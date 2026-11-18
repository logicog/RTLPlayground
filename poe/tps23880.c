/*
 * TI TPS23880 PSE driver: 8 channels, as two I2C devices of 4 channels each
 * (POE_TPS_ADDR for channels 1-4, POE_TPS_ADDR + 1 for 5-8).
 *
 * The chip runs its port state machines from SRAM that is lost on power-off:
 * after every power-on the host downloads TI's SRAM and parity images (from
 * SLVC709, see tools/poe/extract_tps23880.py). The build puts them into
 * flash at POE_IMAGE_LOCATION; poe_bringup() checks them and streams them
 * byte by byte through the SRAM data register 0x61. Register 0x41 then reads
 * the image's version.
 *
 * The switch talks to it with its own I2C master on the bus given by
 * POE_I2C_BUS (an I2CBUS() value; setup_i2c() selects its pins).
 * Ports are 2-pair, one channel each; POE_CHANNEL_PORTS maps channel 1-8 to
 * a switch port (0: not wired). POE_RESET_GPIO drives the RESET pin.
 *
 * Register numbers and fields are from TI's tps238x.h (SLVC709) and the
 * TPS23880 datasheet.
 */
#pragma codeseg BANK4
#pragma constseg BANK4

#include <stdint.h>
#include "rtl837x_common.h"
#include "rtl837x_sfr.h"
#include "rtl837x_regs.h"
#include "rtl837x_pins.h"
#include "rtl837x_flash.h"
#include "machine.h"
#include "machine_defs.h"	// I2CBUS() for POE_I2C_BUS
#include "poe.h"

extern __code const struct machine machine;
extern __xdata uint8_t sfr_data[4];
extern __xdata struct flash_region_t flash_region;

#define TPS_POWER_STATUS	0x10	// 3:0 power enabled, 7:4 power good
#define TPS_CH_STATUS		0x0c	// + channel: 7:4 class, 3:0 detection
#define TPS_OP_MODE		0x12	// 2 bits per channel: 00 off, 11 auto
#define TPS_DISCONNECT_EN	0x13	// 3:0 DC disconnect
#define TPS_DET_CLASS_EN	0x14	// 7:4 classification, 3:0 detection
#define TPS_DET_CLASS_RESTART	0x18
#define TPS_POWER_EN		0x19	// 3:0 power on, 7:4 power off
#define TPS_RESET		0x1a
#define TPS_POWER_ALLOC		0x29	// 2-pair allocation per channel pair
#define TPS_ALLOC_2P_30W	0x33	// 30 W on 1/2 and on 3/4
#define TPS_TEMPERATURE		0x2c	// (x * 652 - 20000) / 1000 degrees C
#define TPS_INPUT_VOLTAGE	0x2e	// 14 bits, 3.662 mV
#define TPS_CH_CURRENT		0x30	// + 4 * channel: 14 bits, 61.039 uA
#define TPS_CH_VOLTAGE		0x32	// + 4 * channel: 14 bits, 3.662 mV
#define TPS_FW_REVISION		0x41
#define TPS_SRAM_CONTROL	0x60
#define TPS_SRAM_DATA		0x61
#define TPS_SRAM_ADDR_LSB	0x62
#define TPS_SRAM_ADDR_MSB	0x63

#define TPS_I2C_SPEED		2	// I2C_CTRL2 speed, verified on the REM-8x

#define IMG_HDR_LEN		16
#define IMG_SRAM_LEN		0x4000
#define IMG_PARITY_LEN		0x800

static __code const uint8_t tps_ports[8] = POE_CHANNEL_PORTS;

__xdata uint8_t  poe_port_num;
__xdata uint8_t  poe_st_valid, poe_st_admin, poe_st_on, poe_st_class;
__xdata uint16_t poe_st_mv, poe_st_ma;
__xdata uint8_t  poe_fw;
__xdata uint16_t poe_vin_mv;
__xdata int8_t   poe_temp;

/*
 * All state in xdata: internal RAM is full on this image, so no parameters
 * beyond the first, no bit variables and no locals that need memory.
 */
static __xdata uint8_t tps_dev, tps_reg, tps_len;
static __xdata uint8_t tps_data[4];
static __xdata uint8_t tps_ok;
static __xdata uint8_t tps_ch;		// channel 0-7
static __xdata uint8_t tps_bit, tps_mode, tps_on, tps_k;
static __xdata uint8_t tps_admin;	// bit per channel: enabled; all off until the config enables ports
static __xdata uint8_t tps_buf[16];	// image bytes from flash
static __xdata uint8_t tps_hdr[IMG_HDR_LEN];
static __xdata uint16_t tps_sum;
static __xdata uint16_t tps_i, tps_ofs, tps_cnt, tps_n;
static __xdata uint8_t tps_j;
static __xdata uint32_t tps_calc;

#define TPS_WR(d, r, v)	(tps_dev = (d), tps_reg = (r), tps_data[0] = (v), tps_write())
#define TPS_RD(d, r, l)	(tps_dev = (d), tps_reg = (r), tps_len = (l), tps_read())


/* Start a transfer of tps_len bytes at register tps_reg and wait: 1 if acknowledged */
static uint8_t tps_go(uint8_t write)
{
	REG_WRITE(RTL837X_REG_I2C_CTRL, 0x00,
		  0x1 << (I2C_MEM_ADDR_WIDTH - 16) | (tps_len - 1),
		  (tps_dev >> 5) | POE_I2C_BUS,
		  ((tps_dev << 3) & 0xff) | write | I2C_CTRL_GO);
	tps_n = 0;
	do {
		reg_read(RTL837X_REG_I2C_CTRL);
		if (!--tps_n)
			return 0;	// Controller stuck busy
	} while (SFR_DATA_0 & I2C_CTRL_GO);
	if (SFR_DATA_0 & I2C_CTRL_FAIL)
		return 0;
	return 1;
}


/* Write tps_data[0] to register tps_reg of device tps_dev */
static uint8_t tps_write(void)
{
	REG_SET(RTL837X_REG_I2C_CTRL2, TPS_I2C_SPEED);
	REG_WRITE(RTL837X_REG_I2C_IN, 0, 0, 0, tps_reg);
	REG_WRITE(RTL837X_REG_I2C_OUT, 0, 0, 0, tps_data[0]);
	tps_len = 1;
	tps_ok = tps_go(I2C_CTRL_WRITE);
	REG_SET(RTL837X_REG_I2C_CTRL2, 0);
	return tps_ok;
}


/* Read tps_len (1-4) bytes from register tps_reg on, the first into tps_data[0] */
static uint8_t tps_read(void)
{
	REG_SET(RTL837X_REG_I2C_CTRL2, TPS_I2C_SPEED);
	REG_WRITE(RTL837X_REG_I2C_IN, 0, 0, 0, tps_reg);
	tps_ok = tps_go(0);
	REG_SET(RTL837X_REG_I2C_CTRL2, 0);
	if (!tps_ok)
		return 0;
	reg_read(RTL837X_REG_I2C_OUT);
	tps_data[0] = SFR_DATA_0;
	tps_data[1] = SFR_DATA_8;
	tps_data[2] = SFR_DATA_16;
	tps_data[3] = SFR_DATA_24;
	return 1;
}


/* Read the image header from flash and check it: 0 if usable */
static uint8_t tps_image_check(void)
{
	flash_region.addr = POE_IMAGE_LOCATION;
	flash_region.len = IMG_HDR_LEN;
	flash_read_bulk(tps_hdr);
	if (tps_hdr[0] != 'T' || tps_hdr[1] != 'P' || tps_hdr[2] != 'S' || tps_hdr[3] != 'I')
		return 1;
	if (tps_hdr[4] != (IMG_SRAM_LEN & 0xff) || tps_hdr[5] != (IMG_SRAM_LEN >> 8)
	    || tps_hdr[6] != (IMG_PARITY_LEN & 0xff) || tps_hdr[7] != (IMG_PARITY_LEN >> 8))
		return 2;

	tps_sum = 0;
	for (tps_i = 0; tps_i < IMG_SRAM_LEN + IMG_PARITY_LEN; tps_i += sizeof(tps_buf)) {
		flash_region.addr = POE_IMAGE_LOCATION + IMG_HDR_LEN + tps_i;
		flash_region.len = sizeof(tps_buf);
		flash_read_bulk(tps_buf);
		for (tps_j = 0; tps_j < sizeof(tps_buf); tps_j++)
			tps_sum += tps_buf[tps_j];
	}
	// The header holds the 32-bit sum little endian; its low 16 bits do here
	if ((uint8_t)tps_sum != tps_hdr[12] || (uint8_t)(tps_sum >> 8) != tps_hdr[13])
		return 3;
	return 0;
}


/* Before each download: prepare the SRAM for writing (TPS23880 datasheet) */
static uint8_t tps_sram_prepare(void)
{
	if (!TPS_WR(POE_TPS_ADDR, 0x1d, 0xbc) || !TPS_WR(POE_TPS_ADDR, 0xd7, 0x02)
	    || !TPS_WR(POE_TPS_ADDR, 0x91, 0x00) || !TPS_WR(POE_TPS_ADDR, 0x90, 0x00)
	    || !TPS_WR(POE_TPS_ADDR, 0xd7, 0x00) || !TPS_WR(POE_TPS_ADDR, 0x1d, 0x00))
		return 0;
	return 1;
}


/* Stream tps_cnt bytes of the image from offset tps_ofs to the SRAM data register */
static uint8_t tps_stream(void)
{
	REG_SET(RTL837X_REG_I2C_CTRL2, TPS_I2C_SPEED);
	REG_WRITE(RTL837X_REG_I2C_IN, 0, 0, 0, TPS_SRAM_DATA);
	tps_dev = POE_TPS_ADDR;
	tps_len = 1;
	tps_ok = 1;
	for (tps_i = 0; tps_i < tps_cnt && tps_ok; tps_i += sizeof(tps_buf)) {
		flash_region.addr = POE_IMAGE_LOCATION + IMG_HDR_LEN + tps_ofs + tps_i;
		flash_region.len = sizeof(tps_buf);
		flash_read_bulk(tps_buf);
		for (tps_j = 0; tps_j < sizeof(tps_buf) && tps_ok; tps_j++) {
			REG_WRITE(RTL837X_REG_I2C_OUT, 0, 0, 0, tps_buf[tps_j]);
			tps_ok = tps_go(I2C_CTRL_WRITE);
		}
	}
	REG_SET(RTL837X_REG_I2C_CTRL2, 0);
	return tps_ok;
}


/* The download sequence of the datasheet and TI's tps_UpdateSRAMCode():
 * 0 if the image runs, 1 no answer, 2 wrong version */
static uint8_t tps_download(void)
{
	print_string("PoE: loading TPS23880 SRAM\n");
	if (!TPS_WR(POE_TPS_ADDR, TPS_SRAM_CONTROL, 0x01)	// Reset the address pointer
	    || !TPS_WR(POE_TPS_ADDR, TPS_SRAM_ADDR_LSB, 0x00)
	    || !TPS_WR(POE_TPS_ADDR, TPS_SRAM_ADDR_MSB, 0x80)
	    || !TPS_WR(POE_TPS_ADDR, TPS_SRAM_CONTROL, 0xc4)	// CPU in reset, parity write
	    || !tps_sram_prepare())
		return 1;
	tps_ofs = 0;
	tps_cnt = IMG_PARITY_LEN;
	if (!tps_stream()
	    || !TPS_WR(POE_TPS_ADDR, TPS_SRAM_CONTROL, 0xc5)
	    || !TPS_WR(POE_TPS_ADDR, TPS_SRAM_ADDR_LSB, 0x00)
	    || !TPS_WR(POE_TPS_ADDR, TPS_SRAM_ADDR_MSB, 0x80)
	    || !TPS_WR(POE_TPS_ADDR, TPS_SRAM_CONTROL, 0xc0)	// CPU in reset, SRAM write
	    || !tps_sram_prepare())
		return 1;
	tps_ofs = IMG_PARITY_LEN;
	tps_cnt = IMG_SRAM_LEN;
	if (!tps_stream()
	    || !TPS_WR(POE_TPS_ADDR, TPS_SRAM_CONTROL, 0x18))	// Run from SRAM, parity on
		return 1;
	delay(SYS_TICK_HZ / 50);	// 20 ms, the datasheet asks for 12
	if (!TPS_RD(POE_TPS_ADDR, TPS_FW_REVISION, 1))
		return 1;
	poe_fw = tps_data[0];
	return poe_fw == tps_hdr[8] ? 0 : 2;
}


/* Port setup of both devices, the enabled channels in auto mode: 1 if done */
static uint8_t tps_configure(void)
{
	for (tps_dev = POE_TPS_ADDR; tps_dev <= POE_TPS_ADDR + 1; tps_dev++) {
		tps_on = 0;
		tps_mode = 0;
		for (tps_k = 0; tps_k < 4; tps_k++) {
			tps_ch = ((tps_dev - POE_TPS_ADDR) << 2) + tps_k;
			if (tps_ports[tps_ch] && (tps_admin & (1 << tps_ch))) {
				tps_on |= 1 << tps_k;
				tps_mode |= 3 << (tps_k << 1);
			}
		}
		if (!TPS_WR(tps_dev, TPS_POWER_ALLOC, TPS_ALLOC_2P_30W)
		    || !TPS_WR(tps_dev, TPS_DISCONNECT_EN, 0x0f)
		    || !TPS_WR(tps_dev, TPS_OP_MODE, tps_mode)
		    || !TPS_WR(tps_dev, TPS_DET_CLASS_EN, (tps_on << 4) | tps_on)
		    || !TPS_WR(tps_dev, TPS_POWER_EN, (~tps_on & 0x0f) << 4))
			return 0;
	}
	return 1;
}


uint8_t poe_bringup(void) __banked
{
	poe_fw = 0;
	tps_k = tps_image_check();
	if (tps_k) {
		print_string("PoE: no valid TPS23880 image in flash ("); print_byte(tps_k);
		print_string("), see doc/poe-tps23880.md\n");
		return 1;
	}
	// Reset pulse, then the 50 ms the chip needs before I2C
	gpio_output_setup(POE_RESET_GPIO, 0);
	delay(2);
	gpio_input_setup(POE_RESET_GPIO);	// Released: pulled up on the board
	delay(SYS_TICK_HZ / 10);

	tps_k = tps_download();
	if (tps_k == 1) {
		print_string("PoE: TPS23880 does not answer\n");
		return 2;
	}
	if (tps_k == 2) {
		print_string("PoE: SRAM load failed, version "); print_byte(poe_fw); write_char('\n');
		return 2;
	}
	if (!tps_configure()) {	// The ports as they were enabled: none at boot
		print_string("PoE: configuration failed\n");
		return 3;
	}
	print_string("PoE: TPS23880 SRAM version "); print_byte(poe_fw); write_char('\n');
	return 0;
}


void poe_init(void) __banked
{
	// Still running from before a switch reboot: leave its ports powered
	if (tps_image_check() == 0 && TPS_RD(POE_TPS_ADDR, TPS_FW_REVISION, 1)
	    && tps_data[0] && tps_data[0] == tps_hdr[8]) {
		poe_fw = tps_data[0];
		// Whatever it powered before: off, the config enables ports again.
		// Nothing may stay powered that nobody enabled (an uplink, a PC).
		tps_admin = 0;
		tps_configure();
		print_string("PoE: TPS23880 already running SRAM version "); print_byte(poe_fw); write_char('\n');
		return;
	}
	poe_bringup();
}


/* Channel 0-7 of port tps_k into tps_ch, 0xff if it has none */
static void tps_channel(void)
{
	for (tps_ch = 0; tps_ch < 8; tps_ch++)
		if (tps_k && tps_ports[tps_ch] == tps_k)
			return;
	tps_ch = 0xff;
}


uint8_t poe_port_set(uint8_t port, __xdata uint8_t on) __banked
{
	tps_k = port;
	tps_channel();
	if (tps_ch == 0xff)
		return 1;
	if (on)
		tps_admin |= 1 << tps_ch;
	else
		tps_admin &= ~(1 << tps_ch);
	if (!poe_fw)
		return 0;

	tps_bit = 1 << (tps_ch & 3);
	tps_dev = POE_TPS_ADDR + (tps_ch >> 2);
	if (!TPS_RD(tps_dev, TPS_OP_MODE, 1))
		return 0;
	tps_mode = tps_data[0] & ~(3 << ((tps_ch & 3) << 1));
	if (on) {
		TPS_WR(tps_dev, TPS_OP_MODE, tps_mode | (3 << ((tps_ch & 3) << 1)));
		TPS_RD(tps_dev, TPS_DET_CLASS_EN, 1);
		TPS_WR(tps_dev, TPS_DET_CLASS_EN, tps_data[0] | tps_bit | (tps_bit << 4));
		TPS_WR(tps_dev, TPS_DET_CLASS_RESTART, tps_bit | (tps_bit << 4));
	} else {
		TPS_WR(tps_dev, TPS_POWER_EN, tps_bit << 4);	// Power off now
		TPS_WR(tps_dev, TPS_OP_MODE, tps_mode);
	}
	return 0;
}


void poe_global_set(uint8_t on) __banked
{
	tps_on = on;
	for (tps_j = 0; tps_j < 8; tps_j++)
		if (tps_ports[tps_j])
			poe_port_set(tps_ports[tps_j], tps_on);
}


/* TPS class code to IEEE class, 0xff for none */
static uint8_t tps_class(uint8_t c)
{
	if (c >= 1 && c <= 4)
		return c;
	if (c == 6)
		return 0;
	if (c >= 8 && c <= 0xb)
		return c - 3;
	if (c == 0xc || c == 0xd)
		return 4;
	return 0xff;
}


void poe_get_port(void) __banked
{
	poe_st_valid = 0;
	tps_k = poe_port_num;
	tps_channel();
	if (tps_ch == 0xff)
		return;
	poe_st_valid = 1;
	poe_st_admin = (tps_admin >> tps_ch) & 1;
	poe_st_on = 0;
	poe_st_class = 0xff;
	poe_st_mv = poe_st_ma = 0;
	if (!poe_fw)
		return;

	tps_k = tps_ch & 3;
	tps_dev = POE_TPS_ADDR + (tps_ch >> 2);
	if (TPS_RD(tps_dev, TPS_POWER_STATUS, 1))
		poe_st_on = (tps_data[0] >> (4 + tps_k)) & 1;
	if (TPS_RD(tps_dev, TPS_CH_STATUS + tps_k, 1))
		poe_st_class = tps_class(tps_data[0] >> 4);
	if (!poe_st_on)
		return;
	// Two 2-byte reads, as TI's tps_GetPortMeasurements(): a 4-byte read
	// does not continue from the current into the voltage register
	if (TPS_RD(tps_dev, TPS_CH_CURRENT + (tps_k << 2), 2)) {
		tps_calc = ((uint16_t)(tps_data[1] & 0x3f) << 8) | tps_data[0];
		poe_st_ma = (tps_calc * 61039UL) / 1000000UL;
	}
	if (TPS_RD(tps_dev, TPS_CH_VOLTAGE + (tps_k << 2), 2)) {
		tps_calc = ((uint16_t)(tps_data[1] & 0x3f) << 8) | tps_data[0];
		poe_st_mv = (tps_calc * 3662UL) / 1000UL;
	}
}


/* Raw register access for bring-up: device 0/1 (channels 1-4/5-8); a value
 * above 0xff reads. Returns 0x100 + value read, 0 if written, 0xffff on NACK. */
uint16_t poe_reg(uint8_t dev, __xdata uint8_t reg, __xdata uint16_t val) __banked
{
	tps_dev = POE_TPS_ADDR + (dev & 1);
	tps_reg = reg;
	if (val > 0xff) {
		tps_len = 1;
		return tps_read() ? 0x100 | tps_data[0] : 0xffff;
	}
	tps_data[0] = val;
	return tps_write() ? 0 : 0xffff;
}


uint8_t poe_get_global(void) __banked
{
	if (!TPS_RD(POE_TPS_ADDR, TPS_FW_REVISION, 1))
		return 1;
	poe_fw = tps_data[0];
	if (TPS_RD(POE_TPS_ADDR, TPS_INPUT_VOLTAGE, 2)) {
		tps_calc = ((uint16_t)(tps_data[1] & 0x3f) << 8) | tps_data[0];
		poe_vin_mv = (tps_calc * 3662UL) / 1000UL;
	}
	if (TPS_RD(POE_TPS_ADDR, TPS_TEMPERATURE, 1)) {
		tps_calc = (uint32_t)tps_data[0] * 652;
		poe_temp = tps_calc < 20000 ? -(int8_t)((20000 - tps_calc) / 1000) : (int8_t)((tps_calc - 20000) / 1000);
	}
	return 0;
}
