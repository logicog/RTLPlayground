/*
 * Driver for the Realtek RTL8238B PoE+ controller(s).
 * This code is in the Public Domain.
 *
 * The keepLink KP-9000-9XHPML-X carries two RTL8238B PSE controllers on I2C
 * bus 0 (GPIO46 = SCL0, GPIO47 = SDA0). The 7-bit device address selects the
 * port bank with A0: 0x20 drives ports 0-3, 0x21 drives ports 4-7.
 *
 * The RTL8238B has an integrated MCU whose PoE application firmware is volatile: it
 * must be uploaded over I2C at every power-on, after which the MCU runs autonomously
 * and powers detected PDs. poe_bringup() performs that upload plus the per-port enable.
 *
 * Two I2C transports are used: the SoC HW I2C master engine (poe_raw_read, for the per-port
 * telemetry reads in poe_get_port(), programmed like the SFP EEPROM access in sfp_send_data())
 * and a GPIO bit-bang on SCL0/SDA0 (the bb_* helpers) for the firmware upload - one
 * continuous transaction, longer than the HW engine's 16-byte limit. See doc/poe-rtl8238b.md.
 */

#include <stdint.h>
#include <8051.h>		// standard 8051 SFRs: IE / EA (global interrupt enable)
#include "rtl837x_common.h"
#include "rtl837x_sfr.h"
#include "rtl837x_regs.h"
#include "rtl837x_flash.h"
#include "poe.h"
#include "machine.h"

#pragma codeseg BANK2
#pragma constseg BANK2

/* This RTL8238B driver compiles only when a board selects it (POE_CHIP_RTL8238B, set in
 * machine.h). It implements the chip-agnostic PoE interface declared in poe.h; a different
 * PSE would be a separate poe_<chip>.c implementing the same interface. */
#ifdef POE_CHIP_RTL8238B

extern __xdata uint8_t sfr_data[4];
extern __xdata struct flash_region_t flash_region;
extern __code struct machine machine;

// EA (bit 7 of the standard 8051 IE register, from <8051.h>) is the global interrupt
// enable. It is cleared only around the bit-banged image upload (see poe_bb_upload), so no
// ISR can stretch an inter-bit/inter-byte gap and make the chip's command parser miss the
// download-arm. The HW-I2C telemetry path (poe_raw_read) runs with interrupts on.

// RWOP bit in RTL837X_REG_I2C_CTRL: set for a write transaction (clear = read)
#define I2C_RWOP_WRITE	0x04

// I2C_CTRL status bits (after the trigger self-clears). Bit layout matches the
// mainline RTL9300 I2C master: bit0 = trigger/busy, bit1 = transfer failed
// (target NACK), bit2 = RWOP.
#define I2C_STS_BUSY	0x01
#define I2C_STS_FAIL	0x02

// PSE application firmware image. The RTL8238B MCU firmware is volatile (mask bootloader
// only) and must be uploaded at every power-on. The image is supplied by the user (extracted
// from their device's own OEM firmware with tools/poe/extract_rtl8238b_image.py - see
// doc/poe-rtl8238b.md) and embedded by the Makefile (fileadder) at this flash offset. It is
// self-describing: the upload reads its length from the 0x8239 container header, so there is
// no image-variant knowledge in the firmware.
#define PSE_IMG_ADDR	0x60000UL	// flash offset where the Makefile embeds the PSE image (= RTL8238B_IMAGE_LOCATION)
#define PSE_IMG_MAX	0x8000		// sanity bound on the header-declared image length

#define PSE_CHUNK	12		// image bytes read from flash per chunk during the stream

__xdata uint8_t poe_addr;		// target I2C device address (0x20 / 0x21)
__xdata uint8_t poe_resp[16];		// bytes read back by poe_raw_read()
__xdata uint8_t poe_reg;		// register / memory address to read or write
__xdata uint8_t poe_rd_len;		// number of bytes for poe_raw_read() (1..16)
__xdata uint8_t poe_status;		// I2C_CTRL low byte after the last transaction
__xdata uint8_t poe_timeout;		// 1 if the engine never cleared the trigger bit


// Scratch variables kept in xdata rather than as function locals: the 8051
// internal-RAM overlay segment (OSEG) is full, so locals on these call chains
// overflow it at link time.
static __xdata uint8_t poe_i;
static __xdata uint16_t poe_wait;
static __xdata uint8_t poe_try;

/*
 * Route GPIO46/47 to I2C SCL0/SDA0 and reset the I2C master engine.
 * Mirrors the bus-0 branches of setup_i2c(); needed because poe_init() runs
 * before setup_i2c() has configured the engine (poe_raw_read, for telemetry).
 */
static void poe_bus0_setup(void)
{
	REG_SET(RTL837X_REG_I2C_MST_IF_CTRL, 0);
	REG_SET(RTL837X_REG_I2C_CTRL2, 0);

	reg_read_m(RTL837X_PIN_MUX_1);
	// SCL bus 0: bits 7-8 -> 0b01 = SCL
	sfr_mask_data(0, 0x80, 0x80);
	sfr_mask_data(1, 0x01, 0x00);
	// SDA bus 0: bits 9-10 -> 0b01 = SDA
	sfr_mask_data(1, 0x06, 0x02);
	reg_write_m(RTL837X_PIN_MUX_1);
}

/*
 * Read poe_rd_len bytes from register poe_reg of poe_addr into poe_resp.
 * Mirrors the multi-byte read in sfp_send_data(): one memory-address byte is
 * sent with the RWOP bit cleared (read); bytes arrive little-endian in the
 * contiguous data words from RTL837X_REG_I2C_OUT.
 */
void poe_raw_read(void)
{
	poe_bus0_setup();

	// memory-address width = 1, data width = poe_rd_len (field = len - 1),
	// device address in bits 3-9, RWOP cleared => read.
	REG_WRITE(RTL837X_REG_I2C_CTRL, 0x00,
		  (0x1 << (I2C_MEM_ADDR_WIDTH - 16)) | ((poe_rd_len - 1) & 0x0f),
		  poe_addr >> 5,
		  (poe_addr << 3) & 0xff);

	REG_WRITE(RTL837X_REG_I2C_IN, 0, 0, 0, poe_reg);

	reg_bit_set(RTL837X_REG_I2C_CTRL, 0);
	// Bounded poll: don't spin forever if the bus wedges. poe_wait wraps to 0
	// after 65535 tries -> give up rather than hang the firmware.
	poe_wait = 0;
	do {
		reg_read_m(RTL837X_REG_I2C_CTRL);
	} while ((sfr_data[3] & I2C_STS_BUSY) && ++poe_wait);
	poe_status = sfr_data[3];
	poe_timeout = (sfr_data[3] & I2C_STS_BUSY) ? 1 : 0;

	for (poe_i = 0; poe_i < poe_rd_len; poe_i++) {
		if (!(poe_i & 0x3))
			reg_read_m(RTL837X_REG_I2C_OUT + poe_i);
		poe_resp[poe_i] = sfr_data[3 - (poe_i & 0x3)];
	}
}

// --- PSE application image upload ---

__xdata uint8_t poe_prep_status;	// reg 0xec after the prep 0xec (deferred print)
__xdata uint8_t poe_sent18;		// 1 = 0x18 prep block was sent
__xdata uint8_t poe_cfg74ca;		// 1 = per-port pre-load config pass (clears reg 0x10/0x14 fields on 0x20/0x21) before arm
__xdata uint16_t poe_postwait;		// quiet settle (5ms ticks) after the verdict, before the post-load reads (0 = none)
__xdata uint8_t poe_cfg74ca_restore;	// 1 = after upload, run the per-port ENABLE pass (all ports) - powers the ports
__xdata uint8_t poe_port_num;		// poe_get_port() input: port 0-7 (poe.h interface global)

// Normalized per-port status, filled by poe_get_port() (the interface in poe.h).
__xdata uint8_t  poe_st_admin;
__xdata uint8_t  poe_st_on;
__xdata uint8_t  poe_st_class;
__xdata uint8_t  poe_st_volt;
__xdata uint16_t poe_st_ma;
static __xdata uint8_t  poe_local;	// poe_get_port(): per-port channel index 0-3 within a controller
static __xdata uint32_t poe_calc;	// poe_get_port(): 32-bit scratch for the current scaling
static __xdata uint8_t poe_74ca_set;	// bb_74ca_prep mode: 0 = clear the per-port fields, 1 = set/enable them
__xdata uint8_t poe_load_nack;		// set if a frame was NACKed / timed out
__xdata uint16_t poe_load_off;		// byte offset reached (where it stopped on error)
__xdata uint8_t poe_img_valid;		// 1 = flash image passed the container check (magic+length eq)
__xdata uint8_t poe_res11;		// reg 0x0B (reg11) result-mailbox low byte, read after load
__xdata uint8_t poe_res12;		// reg 0x0C (reg12) result-mailbox high byte, read after load
__xdata uint8_t poe_prep2_status;	// reg 0xec AFTER the 0x18+0xec prep, BEFORE the image (mode-switch probe)
__xdata uint8_t poe_post_1b;		// reg 0x1b read AFTER the upload (did the chip change identity/state?)
__xdata uint8_t poe_post_e8;		// reg 0xe8 read AFTER the upload (selector/state)
__xdata uint8_t poe_post00[8];		// reg 0x00-0x07 snapshot AFTER the upload (live status zone)
static __xdata uint8_t poe_post10[4];	// reg 0x10-0x13 raw read AFTER the upload (did the app set a port mode?)
static __xdata uint8_t poe_post14[4];	// reg 0x14-0x17 raw read AFTER the upload (the per-port config region)
__xdata uint8_t poe_ack_ec1;		// ACK of the first 0xec prep command byte
__xdata uint8_t poe_ack_18b;		// ACK of the 0x18 (arm) command byte
__xdata uint8_t poe_ack_f4b;		// ACK of the 0xf4 (download) command byte
__xdata uint8_t poe_busrec;		// 1 = run an I2C bus-recovery (9 SCL pulses + STOP) before the upload
__xdata uint8_t poe_eaoff;		// 1 = CLR EA (disable all interrupts) across the prep + image stream
// Download-arm payloads: the controller's firmware-download mode is armed by writing
// these 4-byte values to the 0xec and (alternate) 0x18 command registers. All-zero
// payloads do NOT arm it. Byte order = wire order: poe_arm[0] is clocked out FIRST.
__xdata uint8_t poe_arm1[4] = { 0x03, 0x00, 0x00, 0x00 };	// 0xec arm payload
__xdata uint8_t poe_arm2[4] = { 0x00, 0x00, 0x20, 0x39 };	// 0x18 (alternate arm) payload

static __xdata uint8_t poe_fbuf[16];	// image bytes read from flash
static __xdata uint16_t poe_ilen;
static __xdata uint32_t poe_ibase;
static __xdata uint8_t poe_n;
static __xdata uint8_t poe_j;

// --- Image upload: bit-banged I2C on GPIO46 (SCL) / GPIO47 (SDA) ---
//
// The image is streamed as ONE continuous bit-bang I2C transaction:
//   START | (0x20<<1) | 0xf4 | <entire image> | STOP, MSB-first, ACK clock after every
// byte. 0xf4 is the controller's firmware-download data-port command. One transaction
// because the image is far larger than the HW I2C engine's 16-byte-per-transfer limit.

#define BB_SCL_BIT	14		// GPIO46 % 32, in the GPIO 32-63 registers
#define BB_SDA_BIT	15		// GPIO47 % 32
#define BB_PSE_DL_CMD	0xf4		// firmware-download data-port command byte

// Bit-bang I2C byte/bit state machine (GPIO46/47). Grouped into one struct to keep the global
// namespace uncluttered; every field is scratch used only inside the bb_* helpers.
static __xdata struct poe_bb_state {
	uint8_t  bit;		// bit loop counter
	uint8_t  ack;		// 1 = ACK (SDA low) sampled on the last write byte
	uint16_t spin;		// per-byte delay spin counter
	uint8_t  out;		// byte to shift out (bb_write_byte input)
	uint8_t  in;		// byte shifted in (bb_read_byte output)
	uint8_t  n;		// read loop counter
	uint8_t  ackout;	// 1 = master sends ACK after read byte (else NACK)
} bbs;
static __xdata uint8_t poe_rdslave = 0x20;	// slave address bb_read_reg() targets (0x20 / 0x21)

// --- Bit-bang I2C on GPIO46 (SCL) / GPIO47 (SDA), push-pull drive ---
static void bb_scl(uint8_t hi)
{
	if (hi)
		reg_bit_set(RTL837X_REG_GPIO_32_63_OUTPUT, BB_SCL_BIT);
	else
		reg_bit_clear(RTL837X_REG_GPIO_32_63_OUTPUT, BB_SCL_BIT);
}

static void bb_sda(uint8_t hi)
{
	// Push-pull drive of both levels (in GPIO mode the pad pull-up may be off and there
	// may be no strong external pull-up). SDA is released to input only for the ACK/read bit.
	if (hi)
		reg_bit_set(RTL837X_REG_GPIO_32_63_OUTPUT, BB_SDA_BIT);
	else
		reg_bit_clear(RTL837X_REG_GPIO_32_63_OUTPUT, BB_SDA_BIT);
}

static uint8_t bb_sda_read(void)
{
	reg_read_m(RTL837X_REG_GPIO_32_63_INPUT);
	return (sfr_data[2] & 0x80) ? 1 : 0;	// GPIO47 = bit 15 = sfr_data[2] bit7
}

/* Route GPIO46/47 to plain GPIO; both lines push-pull, idle-high. */
static void bb_setup(void)
{
	reg_read_m(RTL837X_PIN_MUX_1);
	sfr_mask_data(0, 0x80, 0x00);	// SCL bus0 function bits -> 0b00 = GPIO
	sfr_mask_data(1, 0x01, 0x00);
	sfr_mask_data(1, 0x06, 0x00);	// SDA bus0 function bits -> 0b00 = GPIO
	reg_write_m(RTL837X_PIN_MUX_1);

	reg_bit_set(RTL837X_REG_GPIO_32_63_OUTPUT, BB_SDA_BIT);		// SDA idle high
	reg_bit_set(RTL837X_REG_GPIO_32_63_OUTPUT, BB_SCL_BIT);		// SCL idle high
	reg_bit_set(RTL837X_REG_GPIO_32_63_DIRECTION, BB_SDA_BIT);	// SDA output
	reg_bit_set(RTL837X_REG_GPIO_32_63_DIRECTION, BB_SCL_BIT);	// SCL output
}

static void bb_start(void)
{
	bb_sda(1);
	bb_scl(1);
	bb_sda(0);		// SDA falls while SCL high = START
	bb_scl(0);
}

static void bb_stop(void)
{
	bb_sda(0);
	bb_scl(1);
	bb_sda(1);		// SDA rises while SCL high = STOP
}

/* Clock out bbs.out MSB-first, then one ACK clock (sampling SDA into bbs.ack).
 * Parameterless (globals) to keep the 8051 overlay segment from overflowing. */
static void bb_write_byte(void)
{
	for (bbs.bit = 0; bbs.bit < 8; bbs.bit++) {
		bb_sda(bbs.out & 0x80);
		bb_scl(1);
		bb_scl(0);
		bbs.out <<= 1;
	}
	// 9th clock = ACK: release SDA to input so the target can pull it low.
	bb_sda(1);							// drive latch high, then release
	reg_bit_clear(RTL837X_REG_GPIO_32_63_DIRECTION, BB_SDA_BIT);	// INPUT to sample the ACK
	bb_scl(1);
	bbs.ack = bb_sda_read() ? 0 : 1;
	bb_scl(0);
	reg_bit_set(RTL837X_REG_GPIO_32_63_DIRECTION, BB_SDA_BIT);	// resume push-pull drive
}

/* Read one byte MSB-first into bbs.in; master drives ACK if bbs.ackout else NACK. */
static void bb_read_byte(void)
{
	reg_bit_clear(RTL837X_REG_GPIO_32_63_DIRECTION, BB_SDA_BIT);	// release SDA (input)
	bbs.in = 0;
	for (bbs.bit = 0; bbs.bit < 8; bbs.bit++) {
		bb_scl(1);
		bbs.in <<= 1;
		if (bb_sda_read())
			bbs.in |= 1;
		bb_scl(0);
	}
	// master ACK/NACK on the 9th clock
	bb_sda(bbs.ackout ? 0 : 1);
	reg_bit_set(RTL837X_REG_GPIO_32_63_DIRECTION, BB_SDA_BIT);	// resume push-pull drive
	bb_scl(1);
	bb_scl(0);
}

/*
 * Bit-bang I2C read of poe_rd_len bytes from register poe_reg of slave 0x20 into
 * poe_resp:  START | 0x40 | reg | reSTART | 0x41 | read (ACK..ACK,NACK) | STOP.
 * Status read after each download command (write reg, repeated START, read N).
 */
static void bb_read_reg(void)
{
	bb_start();
	bbs.out = poe_rdslave << 1;	bb_write_byte();	// write address (poe_rdslave = 0x20/0x21)
	bbs.out = poe_reg;	bb_write_byte();
	bb_start();					// repeated START
	bbs.out = (poe_rdslave << 1) | 1; bb_write_byte();	// read address
	for (bbs.n = 0; bbs.n < poe_rd_len; bbs.n++) {
		bbs.ackout = (bbs.n < (poe_rd_len - 1)) ? 1 : 0;
		bb_read_byte();
		poe_resp[bbs.n] = bbs.in;
	}
	bb_stop();
}

/*
 * One short bit-bang command transaction: START | 0x40 | cmd | <4-byte payload> | STOP.
 * Command 0xec then 0x18 (each a separate transaction) are issued before streaming the
 * image; together they switch the controller from register mode into firmware-download
 * mode, so the subsequent 0xf4 stream is loaded into the MCU instead of a register window.
 */
static __xdata uint8_t poe_bbcmd;
static __xdata uint8_t poe_ack_cmd;	// ACK of the command byte (0xec/0x18) in the last bb_cmd4
static void bb_cmd4(void)
{
	bb_start();
	bbs.out = 0x20 << 1;	bb_write_byte();	// slave 0x20, write
	bbs.out = poe_bbcmd;	bb_write_byte();
	poe_ack_cmd = bbs.ack;			// 1 = chip ACKed the command byte
	// 4-byte payload: reg 0xec <- poe_arm1, reg 0x18 <- poe_arm2. poe_arm*[0] is clocked
	// out first. bb_cmd4 is only ever called with 0xec or 0x18, so 0x18 is the implicit else.
	if (poe_bbcmd == 0xec) {
		bbs.out = poe_arm1[0];	bb_write_byte();
		bbs.out = poe_arm1[1];	bb_write_byte();
		bbs.out = poe_arm1[2];	bb_write_byte();
		bbs.out = poe_arm1[3];	bb_write_byte();
	} else {
		bbs.out = poe_arm2[0];	bb_write_byte();
		bbs.out = poe_arm2[1];	bb_write_byte();
		bbs.out = poe_arm2[2];	bb_write_byte();
		bbs.out = poe_arm2[3];	bb_write_byte();
	}
	bb_stop();
}

/*
 * I2C bus recovery: a slave stuck mid-byte (e.g. after a spurious START it never saw
 * a STOP for) holds SDA low and wedges the bus. Release SDA to input and pulse SCL 9
 * times so the slave can finish/abandon its byte, then issue a clean STOP. Cheap
 * insurance against a wedged bus before the download transaction.
 */
static void bb_busrecover(void)
{
	reg_bit_clear(RTL837X_REG_GPIO_32_63_DIRECTION, BB_SDA_BIT);	// release SDA (input)
	for (bbs.bit = 0; bbs.bit < 9; bbs.bit++) {
		bb_scl(1);
		bb_scl(0);
	}
	reg_bit_set(RTL837X_REG_GPIO_32_63_DIRECTION, BB_SDA_BIT);	// drive SDA again
	bb_stop();							// clean STOP
}

/* Write poe_resp[0..3] (4 bytes) back to register poe_reg of slave poe_rdslave. */
static void bb_write_resp4(void)
{
	bb_start();
	bbs.out = poe_rdslave << 1;	bb_write_byte();
	bbs.out = poe_reg;		bb_write_byte();
	bbs.out = poe_resp[0];		bb_write_byte();
	bbs.out = poe_resp[1];		bb_write_byte();
	bbs.out = poe_resp[2];		bb_write_byte();
	bbs.out = poe_resp[3];		bb_write_byte();
	bb_stop();
}

/*
 * Per-port config pass, once per port. Read-modify-write of two per-port register fields
 * on the port's controller:
 *   - reg 0x10 block: the port's 2-bit mode field in byte 2 (reg 0x12)
 *   - reg 0x14 block: the port's 1-bit field      in byte 0 (reg 0x14)
 * Ports 0-3 are on controller 0x20 (field index 3,2,1,0), ports 4-7 on 0x21 (index 0,1,2,3).
 * poe_74ca_set selects the clear pass (run before the image, disables the fields) or the
 * set/enable pass (run after the image, powers the ports). Byte 0 = first I2C byte = LSB.
 */
static void bb_74ca_prep(void)
{
	for (poe_j = 0; poe_j < 8; poe_j++) {
		poe_rdslave = (poe_j < 4) ? 0x20 : 0x21;	// ports 0-3 -> 0x20, 4-7 -> 0x21
		poe_n = (poe_j < 4) ? (3 - poe_j) : (poe_j - 4);	// per-port field index
		// reg 0x10 block: the port's 2-bit mode field in byte 2 (reg 0x12)
		poe_reg = 0x10;	poe_rd_len = 4;	bb_read_reg();
		if (poe_74ca_set)
			poe_resp[2] |= (0x03 << (poe_n << 1));	// set mode field = 3 (enable)
		else
			poe_resp[2] &= ~(0x03 << (poe_n << 1));	// clear it (disable)
		poe_reg = 0x10;	bb_write_resp4();
		// reg 0x14 block: the port's 1-bit field in byte 0 (reg 0x14)
		poe_reg = 0x14;	poe_rd_len = 4;	bb_read_reg();
		if (poe_74ca_set)
			poe_resp[0] |= (0x01 << poe_n);		// set the port's bit = 1 (enable)
		else
			poe_resp[0] &= ~(0x01 << poe_n);	// clear it (disable)
		poe_reg = 0x14;	bb_write_resp4();
	}
	poe_rdslave = 0x20;
}

/* Dump reg 0x10/0x14 (raw 4 bytes) from 0x20 and 0x21 (defined below; used here too). */
static void pp_dump(void);

/*
 * Put the controller into firmware-download mode (prep cmds 0xec, 0x18) then stream the
 * whole PSE image to slave 0x20 by bit-banging GPIO46/47. The image is one START..STOP
 * transaction:
 *   START | 0x40 | 0xf4 | <entire image> | STOP
 * Runs the whole-chip reset and port-disable first. Restores the I2C engine pin-mux on
 * exit so normal register access (/poe.json) keeps working.
 */
void poe_bb_upload(void)
{
	poe_addr = 0x20;
	poe_rdslave = 0x20;	// bb_read_reg() targets 0x20 throughout the upload
	poe_load_nack = 0;
	poe_ibase = PSE_IMG_ADDR;

	// The PSE image is self-describing: a 16-byte LE header with magic word 0x8239 at
	// bytes[2:3], four section-length words, and a 4-byte trailer, so
	//   image_len == 16 + seclen[0]+seclen[1]+seclen[2]+seclen[3] + 4.
	// Take the length straight from the header (no hardcoded size, no image variant) and check
	// the magic. The chip's bootloader self-parses the sections + a trailing CRC, so one bad
	// byte fails the whole load; this just asserts the embedded copy looks intact before we stream.
	flash_region.addr = poe_ibase;
	flash_region.len = 16;
	flash_read_bulk(poe_fbuf);
	poe_ilen = 16 + 4;
	poe_ilen += poe_fbuf[6]  | ((uint16_t)poe_fbuf[7]  << 8);	// seclen[0]
	poe_ilen += poe_fbuf[8]  | ((uint16_t)poe_fbuf[9]  << 8);	// seclen[1]
	poe_ilen += poe_fbuf[10] | ((uint16_t)poe_fbuf[11] << 8);	// seclen[2]
	poe_ilen += poe_fbuf[12] | ((uint16_t)poe_fbuf[13] << 8);	// seclen[3]
	poe_img_valid = (poe_fbuf[2] == 0x39 && poe_fbuf[3] == 0x82
			 && poe_ilen > 16 + 4 && poe_ilen <= PSE_IMG_MAX);

	print_string("PoE: bit-bang upload image, ");
	print_byte(poe_ilen >> 8);
	print_byte(poe_ilen);
	print_string(" bytes, container ");
	print_string(poe_img_valid ? "OK\n" : "BAD (magic/length)!\n");

	bb_setup();

	if (poe_busrec)
		bb_busrecover();	// clear a slave possibly wedged by a spurious START

	// Step0 - whole-chip reset: write reg 0x1a = 0x20 (1 data byte), then STOP.
	bb_start();
	bbs.out = 0x20 << 1;	bb_write_byte();	// addr + W
	bbs.out = 0x1a;		bb_write_byte();	// reg 0x1a
	bbs.out = 0x20;		bb_write_byte();	// = 0x20 (whole-chip reset bit), then STOP
	bb_stop();
	delay(100);		// ~0.5s for the chip to reset back into its bootloader

	// Step1 - disable all ports (reg 0x10 = 0 on 0x20 + 0x21), bit-banged on the same
	// transport so the whole sequence runs continuously on GPIO46/47 (no HW I2C engine,
	// no pin-mux toggle).
	bb_start();
	bbs.out = 0x20 << 1;	bb_write_byte();	// slave 0x20
	bbs.out = 0x10;		bb_write_byte();	// reg 0x10
	bbs.out = 0x00;		bb_write_byte();
	bbs.out = 0x00;		bb_write_byte();
	bbs.out = 0x00;		bb_write_byte();
	bbs.out = 0x00;		bb_write_byte();
	bb_stop();
	bb_start();
	bbs.out = 0x21 << 1;	bb_write_byte();	// slave 0x21
	bbs.out = 0x10;		bb_write_byte();
	bbs.out = 0x00;		bb_write_byte();
	bbs.out = 0x00;		bb_write_byte();
	bbs.out = 0x00;		bb_write_byte();
	bbs.out = 0x00;		bb_write_byte();
	bb_stop();

	// Per-port pre-load config (clear pass): read-modify-write that clears the per-port
	// 2-bit mode field in reg 0x10 and the 1-bit field in reg 0x14 on each controller,
	// before the arm/image. See bb_74ca_prep(). Runs with interrupts ON (CLR EA is later).
	poe_74ca_set = 0;	// clear the fields (the enable pass runs after the upload)
	bb_74ca_prep();

	// INTERRUPTS OFF: bit-bang the ENTIRE prep + image with interrupts disabled, using
	// busy-loops for inter-command delays (never the timer). The chip's I2C slave HW ACKs
	// every byte regardless, but the on-chip command parser that sets the 0xec "armed" bit
	// needs the bytes inside a tight timing window; an ISR (UART/timer) stretching an
	// inter-bit/inter-byte gap can make the parser miss the arm even though the bytes are
	// ACKed. So: CLR EA now, busy-loops only (NO delay() here - delay() needs the timer
	// ISR and would hang), SETB EA after the image STOP.
	if (poe_eaoff)
		EA = 0;	// CLR EA

	// Prep handshake: arm with 0xec -> read status -> if the arm bit (bit2) is still
	// clear, send the alternate arm 0x18 then re-arm 0xec -> then stream the image.
	// NOTE: no printing while interrupts are off (the console UART is interrupt-driven
	// and would deadlock). Capture state, print after SETB EA.
	poe_bbcmd = 0xec;	bb_cmd4();
	poe_ack_ec1 = poe_ack_cmd;			// did the chip ACK the 0xec command byte?
	poe_reg = 0xec;	poe_rd_len = 1;	bb_read_reg();	// status read
	poe_prep_status = poe_resp[0];
	poe_sent18 = 0;
	poe_ack_18b = 0xff;	// 0xff = "not sent"
	if (!(poe_resp[0] & 0x04)) {	// arm bit (bit2) not yet set -> send the alternate arm
		poe_bbcmd = 0x18;	bb_cmd4();
		poe_ack_18b = poe_ack_cmd;		// did the chip ACK the 0x18 (arm) command byte?
		for (bbs.spin = 0; bbs.spin < 1000; bbs.spin++)	// busy-spin (EA is off)
			reg_read_m(RTL837X_REG_GPIO_32_63_INPUT);
		poe_bbcmd = 0xec;	bb_cmd4();	// re-arm before the image
		poe_sent18 = 1;
	}

	// Read reg 0xec right after the prep, before streaming the image: a mode-switch
	// probe. If the 0xec/0x18/0xec handshake put the chip into firmware-download mode,
	// the status here should differ from the post-image value.
	poe_reg = 0xec;	poe_rd_len = 1;	bb_read_reg();
	poe_prep2_status = poe_resp[0];

	bb_start();

	bbs.out = 0x20 << 1;	bb_write_byte();	// slave 0x20, write
	if (!bbs.ack)
		poe_load_nack = 1;		// chip did not ACK its address
	bbs.out = BB_PSE_DL_CMD;	bb_write_byte();	// firmware-download command 0xf4
	poe_ack_f4b = bbs.ack;			// did the chip ACK the 0xf4 command byte?

	for (poe_load_off = 0; poe_load_off < poe_ilen; poe_load_off += PSE_CHUNK) {
		poe_n = (poe_ilen - poe_load_off > PSE_CHUNK)
			? PSE_CHUNK : (uint8_t)(poe_ilen - poe_load_off);
		flash_region.addr = poe_ibase + poe_load_off;
		flash_region.len = poe_n;
		flash_read_bulk(poe_fbuf);
		for (poe_j = 0; poe_j < poe_n; poe_j++) {
			bbs.out = poe_fbuf[poe_j];
			bb_write_byte();
		}
	}

	bb_stop();

	if (poe_eaoff)
		EA = 1;		// SETB EA - re-enable before any printing/delay

	// Deferred prep log.
	print_string("PoE: after 0xec status=0x");
	print_byte(poe_prep_status);
	if (poe_sent18)
		print_string(" (sent 0x18 + 0xec)");
	write_char('\n');
	print_string("PoE: prep status after 0x18 (pre-image) =0x");
	print_byte(poe_prep2_status);
	print_string((poe_prep2_status == poe_prep_status)
		     ? " (unchanged)\n" : " (changed)\n");
	print_string("PoE: cmd ACKs  0xec=");
	write_char(poe_ack_ec1 ? '1' : '0');
	print_string(" 0x18=");
	write_char(poe_ack_18b == 0xff ? '-' : (poe_ack_18b ? '1' : '0'));
	print_string(" 0xf4=");
	write_char(poe_ack_f4b ? '1' : '0');
	print_string(" (1=ACK, 0=NACK/rejected)\n");

	// Verification: the chip validates the image before setting bit5, so poll with
	// delays instead of reading once - 0x18 may be a transient "validating" state.
	// Each bit-bang read is bracketed interrupts-off for clean timing.
	for (poe_try = 0; poe_try < 25; poe_try++) {
		delay(8);			// ~40ms between polls (interrupts on)
		EA = 0;
		poe_reg = 0xec;	poe_rd_len = 4;	bb_read_reg();
		EA = 1;
		if (poe_resp[0] & 0x20)		// bit5 = accepted
			break;
	}
	print_string("PoE: download status ");
	print_byte(poe_resp[0]);
	write_char(' ');
	print_byte(poe_resp[1]);
	write_char(' ');
	print_byte(poe_resp[2]);
	write_char(' ');
	print_byte(poe_resp[3]);
	print_string((poe_resp[0] & 0x20) ? " => ACCEPTED (bit5 set)\n" : " => REJECTED (bit5 clear)\n");

	// Result mailbox: 16-bit result word in reg 0x0B (lo) / 0x0C (hi). Read alongside
	// 0xec - may carry a CRC/verify error code that the bit5 status flag doesn't expose.
	EA = 0;
	poe_reg = 0x0b;	poe_rd_len = 1;	bb_read_reg();
	poe_res11 = poe_resp[0];
	poe_reg = 0x0c;	poe_rd_len = 1;	bb_read_reg();
	poe_res12 = poe_resp[0];
	EA = 1;
	print_string("PoE: result mailbox reg11=0x");
	print_byte(poe_res11);
	print_string(" reg12=0x");
	print_byte(poe_res12);
	write_char('\n');

	// Quiet settle (poe_postwait): leave the bus idle after the verdict so an accepted
	// image can auto-start without our read traffic, then look. delay() needs the timer
	// ISR, so EA is on here.
	if (poe_postwait)
		delay(poe_postwait);

	// Re-read identity/state regs after the upload. If the chip rebooted into a running
	// app (download took effect), reg 0x1b / 0xe8 or the live-status zone 0x00-0x07 should
	// differ from the pre-upload values (0x1b=0x39, 0xe8=0x02, 0x00-0x07 all zero).
	EA = 0;
	poe_reg = 0x1b;	poe_rd_len = 1;	bb_read_reg();	poe_post_1b = poe_resp[0];
	poe_reg = 0xe8;	poe_rd_len = 1;	bb_read_reg();	poe_post_e8 = poe_resp[0];
	poe_reg = 0x10;	poe_rd_len = 4;	bb_read_reg();
	for (poe_j = 0; poe_j < 4; poe_j++) poe_post10[poe_j] = poe_resp[poe_j];
	poe_reg = 0x14;	poe_rd_len = 4;	bb_read_reg();
	for (poe_j = 0; poe_j < 4; poe_j++) poe_post14[poe_j] = poe_resp[poe_j];
	poe_reg = 0x00;	poe_rd_len = 8;	bb_read_reg();
	for (poe_j = 0; poe_j < 8; poe_j++)
		poe_post00[poe_j] = poe_resp[poe_j];
	EA = 1;
	print_string("PoE: post-upload reg 0x1b=0x");
	print_byte(poe_post_1b);
	print_string(" 0xe8=0x");
	print_byte(poe_post_e8);
	print_string(" 0x00-07=");
	for (poe_j = 0; poe_j < 8; poe_j++) {
		print_byte(poe_post00[poe_j]);
		write_char(' ');
	}
	print_string("0x10=");
	for (poe_j = 0; poe_j < 4; poe_j++) print_byte(poe_post10[poe_j]);
	print_string(" 0x14=");
	for (poe_j = 0; poe_j < 4; poe_j++) print_byte(poe_post14[poe_j]);
	write_char('\n');

	// Per-port ENABLE pass: a SECOND config pass AFTER the upload that enables the ports
	// (sets the reg 0x10 2-bit mode field = 3 and the reg 0x14 bit = 1). On the OEM this
	// is driven from saved per-port flash config; we have none, so enable all 8 ports.
	// Same bit-bang transport, mux still GPIO (no engine round-trip that could disturb a
	// just-started app).
	if (poe_cfg74ca_restore) {
		print_string("PoE: enabling all ports (config-restore pass):\n");
		poe_74ca_set = 1;
		bb_74ca_prep();
		poe_74ca_set = 0;
		pp_dump();			// reg 0x10/0x14 raw4 on 0x20/0x21 after the enable pass
		delay(poe_postwait ? poe_postwait : 100);	// settle, let the app act on the enable
		poe_rdslave = 0x20;
		poe_reg = 0x00;	poe_rd_len = 8;	bb_read_reg();
		print_string("PoE: live 0x00-07 after restore =");
		for (poe_j = 0; poe_j < 8; poe_j++) {
			write_char(' ');
			print_byte(poe_resp[poe_j]);
		}
		write_char('\n');
	}

	poe_bus0_setup();			// restore I2C master engine pin-mux
	delay(40);				// let the loader checksum / start the MCU

	print_string("PoE: bit-bang upload done");
	if (poe_load_nack)
		print_string(" (WARN: address NACK)");
	write_char('\n');
}

/*
 * Enable (poe_port_on=1) or disable (=0) PoE on a single port (poe_port_num 0-7) at
 * runtime: read-modify-write that port's 2-bit mode field (reg 0x10 block byte 2 = reg
 * 0x12, value 3=on / 0=off) and 1-bit enable field (reg 0x14) on its controller. This is
 * the same admin-enable register path the bring-up enable pass uses. Verified on hardware
 * to be real auto-detect, NOT force: with mode=3 the controller runs detect/classify/power-up itself
 * and sets the reg 0x10 Power-On status bit only once a genuine PD is present (an empty
 * enabled port stays Power-On=0 / class=0 / 0V) - no MCU app required. Bit-banged; restores
 * the I2C-engine mux on exit. (`poe port <n> <on|off>`.)
 */
void poe_port_set(uint8_t port_num, uint8_t port_on) __banked
{
	bb_setup();
	poe_rdslave = (port_num < 4) ? machine.poe.addr0 : machine.poe.addr1;
	poe_n = (port_num < 4) ? (3 - port_num) : (port_num - 4);	// per-port field index

	poe_reg = 0x10;	poe_rd_len = 4;	bb_read_reg();		// reg 0x10 block: 2-bit mode field (byte 2)
	if (port_on)
		poe_resp[2] |= (0x03 << (poe_n << 1));
	else
		poe_resp[2] &= ~(0x03 << (poe_n << 1));
	poe_reg = 0x10;	bb_write_resp4();

	poe_reg = 0x14;	poe_rd_len = 4;	bb_read_reg();		// reg 0x14 block: 1-bit field (byte 0)
	if (port_on)
		poe_resp[0] |= (0x01 << poe_n);
	else
		poe_resp[0] &= ~(0x01 << poe_n);
	poe_reg = 0x14;	bb_write_resp4();

	poe_bus0_setup();		// restore I2C-engine pin-mux
	print_string("PoE: port ");
	print_byte(port_num + 1);	// report the 1-based port number
	print_string(port_on ? " enabled\n" : " disabled\n");
}

/*
 * Global PoE on/off: enable (on=1) or disable (=0) every port, by applying the per-port admin
 * RMW (poe_port_set) to each in turn. Driven by `poe global <on|off>` and the web UI's "All PoE"
 * control. The port count comes from the machine descriptor.
 */
void poe_global_set(uint8_t on) __banked
{
	for (poe_i = 0; poe_i < machine.poe.n_ports; poe_i++)
		poe_port_set(poe_i, on);
	print_string(on ? "PoE: all ports enabled\n" : "PoE: all ports disabled\n");
}

/*
 * Decode this controller's registers into the normalized per-port status (poe.h), so /poe.json
 * and the web UI need no knowledge of the RTL8238B register layout. Input: poe_port_num (0-7).
 *   admin = reg 0x12 per-port mode field (byte 2 of the reg 0x10 block) != 0
 *   class = high nibble of reg 0x0c+local (nibble 0xc => Class 0, 1..8 => Class 1..8)
 *   volt  = top byte of reg 0x30+4*local, 0.5 V/LSB
 *   ma    = low 16 bits of reg 0x30+4*local, * 125 / 4096
 *   on    = reg 0x10 Power-On bit (4+local) AND real current draw (the chip latches Power-On /
 *           class / voltage after a PD is unplugged, so current is the live "delivering" signal)
 */
void poe_get_port(void) __banked
{
	poe_addr = (poe_port_num < 4) ? machine.poe.addr0 : machine.poe.addr1;
	poe_local = (poe_port_num < 4) ? (3 - poe_port_num) : (poe_port_num - 4);

	poe_reg = 0x10;	poe_rd_len = 4;	poe_raw_read();		// byte0: Power-On bits; byte2 (reg 0x12): mode
	poe_st_admin = ((poe_resp[2] >> (poe_local << 1)) & 0x03) ? 1 : 0;
	poe_st_on = (poe_resp[0] >> (poe_local + 4)) & 0x01;

	poe_reg = 0x30 + (poe_local << 2);	poe_rd_len = 4;	poe_raw_read();
	poe_st_volt = poe_resp[3] >> 1;				// 0.5 V/LSB (top byte)
	poe_calc = (uint32_t)(((uint16_t)poe_resp[1] << 8) | poe_resp[0]) * 125UL;
	poe_st_ma = (uint16_t)(poe_calc >> 12);			// * 125 / 4096

	poe_reg = 0x0c;	poe_rd_len = 4;	poe_raw_read();		// per-port class byte = reg 0x0c+local
	poe_st_class = (poe_resp[poe_local] >> 4) & 0x0f;
	if (poe_st_class == 0x0c)
		poe_st_class = 0;				// 0x0c encodes Class 0
	else if (poe_st_class < 1 || poe_st_class > 8)
		poe_st_class = 0xff;				// none / unknown

	// Only a port actually drawing current is "delivering"; otherwise the latched Power-On /
	// class / voltage are stale (PD unplugged) - report off and hide the measurements.
	if (!(poe_st_on && poe_st_ma > 0)) {
		poe_st_on = 0;
		poe_st_class = 0xff;
	}
}

/*
 * Bring up PoE on both RTL8238B controllers - the full working sequence with the
 * proven options baked in. Called at boot (after PHY init) and by `poe load`. Run on
 * a cold-power-cycled chip; afterwards the controllers' MCU runs the loaded firmware
 * and powers detected PDs autonomously (PoE detection/classification takes a moment).
 */
void poe_bringup(void) __banked
{
	poe_eaoff = 1;			// interrupts off across the prep + image stream
	poe_busrec = 1;			// clear a possibly-wedged slave before the download
	poe_cfg74ca = 1;		// pre-load per-port config (clear pass)
	poe_cfg74ca_restore = 1;	// post-load per-port ENABLE pass (powers the ports)
	poe_postwait = 100;		// ~0.5s settle after the enable pass
	poe_bb_upload();
	poe_postwait = 0;
	poe_cfg74ca_restore = 0;
	poe_cfg74ca = 0;
	poe_busrec = 0;
	poe_eaoff = 0;
}

/*
 * Dump reg 0x10 and reg 0x14 (raw 4 bytes each) from BOTH controllers (0x20, 0x21).
 * Used by poe_bringup() to show the per-port mode / reg 0x14 state after the enable pass.
 */
static void pp_dump(void)
{
	for (poe_j = 0; poe_j < 2; poe_j++) {
		poe_rdslave = poe_j ? 0x21 : 0x20;
		print_string("  0x");	print_byte(poe_rdslave);
		poe_reg = 0x10;	poe_rd_len = 4;	bb_read_reg();
		print_string(" r10=");
		for (poe_n = 0; poe_n < 4; poe_n++) print_byte(poe_resp[poe_n]);
		poe_reg = 0x14;	poe_rd_len = 4;	bb_read_reg();
		print_string(" r14=");
		for (poe_n = 0; poe_n < 4; poe_n++) print_byte(poe_resp[poe_n]);
		write_char('\n');
	}
	poe_rdslave = 0x20;
}

#endif /* POE_CHIP_RTL8238B */
