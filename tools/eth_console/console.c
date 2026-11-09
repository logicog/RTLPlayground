/*
 * Ethernet console for the TP-Link TL-SG108PE V5 (RTL8370N).
 *
 * A full-flash image that replaces the TP-Link boot code: it starts from cold
 * reset at CODE 0x0000, does the boot code's chip setup itself, initializes the
 * switch the way the boot code's loader mode does, then answers EtherType 0x88B5
 * frames (register, PHY, XDATA and SPI flash access, reboot) and broadcasts a
 * beacon with the port status every ~2 s. Everything is polled; interrupts stay off.
 *
 * Protocol: see console.py. Build:
 *   ./gen_phy.py flash_dump.bin > build/phy_patch.h
 *   sdcc -mmcs51 -Ibuild -o build/console.ihx console.c
 *   ./mkimage.py build/console.ihx build/console_up.bin flash_dump.bin build/console_flash.bin
 */
#include <stdint.h>
#include <string.h>
#ifdef RTLP_HOST_TEST
#include "../../test/rtl8370n_hw.h"
#else
#include <8051.h>
#include "../../rtl837x_sfr.h"	/* same SFR block as the RTL837x: register access, NIC DMA, SPI flash */
#include "phy_patch.h"
#endif

#define VERSION 2

/*
 * Bounded busy-wait: `timed_out` is set if cond still holds after ~65k polls.
 * Nothing here may hang forever, or the console goes silent with no clue why.
 * cond may call reg_read() and rdsr(), which keep their own counters.
 */
static uint16_t wait_n;
static __bit timed_out;
static __bit io_failed;	/* A timed-out controller must not receive another command until reset. */
#define WAIT_WHILE(cond) do { wait_n = 1; while (!io_failed && (cond) && ++wait_n); timed_out = io_failed || !wait_n; } while (0)

static uint16_t reg_read(uint16_t reg)
{
	uint16_t t = 1;

	if (io_failed)
		return 0;
	SFR_REG_ADDRH = reg >> 8;
	SFR_REG_ADDRL = reg;
	SFR_EXEC_GO = SFR_EXEC_READ_REG;
	do {
		__asm__("nop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop");
	} while (SFR_EXEC_STATUS && ++t);
	if (!t) {
		io_failed = 1;
		return 0;
	}
	return (SFR_DATA_8 << 8) | SFR_DATA_0;	/* RTL8370N: 16-bit read data in 0xa6:0xa7 */
}

static void reg_write(uint16_t reg, uint16_t val)
{
	uint16_t t = 1;

	if (io_failed)
		return;
	SFR_REG_ADDRH = reg >> 8;
	SFR_REG_ADDRL = reg;
	SFR_DATA_24 = val >> 8;			/* RTL8370N: 16-bit write data in 0xa4:0xa5 */
	SFR_DATA_16 = val;
	SFR_EXEC_GO = SFR_EXEC_WRITE_REG;
	do {
		__asm__("nop\n\tnop\n\tnop");
	} while (SFR_EXEC_STATUS && ++t);
	if (!t)
		io_failed = 1;
}

/* SDK rtl8367c_setAsicRegBits: val is shifted to the mask's lowest bit */
static void reg_bits(uint16_t reg, uint16_t mask, uint16_t val)
{
	uint16_t m = mask;

	while (!(m & 1)) {
		m >>= 1;
		val <<= 1;
	}
	reg_write(reg, (reg_read(reg) & ~mask) | (val & mask));
}

/* PHY OCP access as the boot code does it (bank 2 8997/8a6f): OCP MSBs in 0x1d15[5:0] */
static uint16_t phy_addr(uint8_t phy, uint16_t ocp)
{
	reg_bits(0x1d15, 0x003f, ocp >> 10);
	return 0x2000 | ((ocp >> 6) & 0x0f) << 8 | (uint16_t)phy << 5 | ((ocp >> 1) & 0x1f);
}

static uint16_t phy_read(uint8_t phy, uint16_t ocp)
{
	return reg_read(phy_addr(phy, ocp));
}

static void phy_write(uint8_t phy, uint16_t ocp, uint16_t val)
{
	reg_write(phy_addr(phy, ocp), val);
}

/* Log kept in XDATA, also sent as frames; console.py `log` reads it back */
#define LOG_SIZE 1024
static __xdata uint8_t log_buf[LOG_SIZE];
static __xdata uint16_t log_len;

static void log_char(char c)
{
	if (log_len < LOG_SIZE)
		log_buf[log_len++] = c;
}

static void log_str(const char *s)
{
	while (*s)
		log_char(*s++);
}

static void log_hex16(uint16_t v)
{
	for (uint8_t i = 0; i < 4; i++, v <<= 4)
		log_char("0123456789abcdef"[v >> 12]);
}

static void log_reg(const char *name, uint16_t v)
{
	log_str(name);
	log_char('=');
	log_hex16(v);
	log_char('\n');
}

/*
 * Switch init: the boot code's loader-mode sequence (bank 2 b02_7ffb, 4fd2,
 * b02_9130), which follows the Realtek SDK's rtk_switch_init() for RTL8370B.
 */
static uint16_t switch_init(void)
{
	uint16_t id, v;
	uint8_t i;

	reg_write(0x13c2, 0x0249);
	id = reg_read(0x1300);
	reg_write(0x13c2, 0x0000);
	log_reg("chip_id", id);
	if (id != 0x0652 && id != 0x6368) {
		log_str("not RTL8370B, switch init skipped\n");
		return id;
	}

	reg_write(0x131c, 0x1076);	/* M10_FORCE_SELECT (b02_7da6) */
	reg_write(0x18e4, 0x07ff);	/* RLDP_CTRL4 */

	/* _rtk_switch_init_8370b (b02_7ad9): PHY patch broadcast to all PHYs */
	v = reg_read(0x1d01);
	reg_write(0x1d01, (v | 0x3be0) & 0xffe0);
	for (i = 0; i < PHY_PATCH_LEN; i++)
		phy_write(0, phy_patch[i][0], phy_patch[i][1]);
	reg_write(0x1d01, v);
	for (i = 0; i < 8; i++)
		phy_write(i, 0xa4b4, long_link[i]);

	/* Common part of _rtk_switch_init, minus rate limits, LUT, RMA and mirror settings */
	reg_bits(0x1200, 0x6000, 3);	/* MAX_LENGTH_LIMINT_IPG */
	reg_bits(0x0884, 0x0003, 3);	/* MAX_LEN_RX_TX */
	reg_bits(0x06eb, 0x0001, 1);	/* ACL_ACCESS_MODE */
	reg_write(0x03fa, 0x0007);
	reg_bits(0x08c8, 0x00c0, 3);	/* unknown unicast DA: per port */

	reg_write(0x130c, reg_read(0x130c) | 0x0050);	/* 4fd2 */

	/* rtk_port_phyEnableAll_set(ENABLED) (b02_9130) */
	reg_bits(0x130f, 0x0020, 0);	/* PHY_AD.PDNPHY */
	for (i = 0; i < 8; i++)
		phy_write(i, 0xa400, (phy_read(i, 0xa400) & ~0x0800) | 0x0200);

	/* Port LEDs in hardware mode, as the TP-Link firmware runs them */
	reg_write(0x1b24, 0xffff);
	reg_write(0x1b25, 0xffff);
	reg_write(0x1b00, 0x1472);
	reg_write(0x1b03, 0x0960);	/* LED1 Spd1000/Act (green), LED2 Spd100(10)/Act (orange) */
	reg_write(0x1b08, 0);
	reg_write(0x1b0a, 0);
	reg_write(0x1b0c, 0);

	/*
	 * GPIO as the stock runtime leaves it (POE.md): 1, 5, 8, 9 = PoE enable ports 1-4 (high = on),
	 * 7 = PoE MCU line, 2 = PoE LEDs. Output value before enable, so no line glitches low; without
	 * the mode bits these pins are LED outputs and port 2's PoE enable followed a port LED.
	 */
	reg_write(0x1d1d, reg_read(0x1d1d) | 0x03a6);	/* GPIO_67C_O_X0 */
	reg_write(0x1d21, reg_read(0x1d21) | 0x03a6);	/* GPIO_67C_OE_X0 */
	reg_write(0x1d25, reg_read(0x1d25) | 0x03a6);	/* GPIO_MODE_67C_X0 */

	log_str("switch init done\n");
	return id;
}

/* NIC, boot code FUN_CODE_4f41 and NIC.md. DMA between XDATA and the NIC buffer:
 * RING bit 15 set = XDATA -> NIC; CTRL = length in 8-byte units, 0 when done */
static void dma(__xdata uint8_t *x, uint16_t nic, uint8_t units)
{
	if (io_failed) {
		timed_out = 1;
		return;
	}
	SFR_NIC_DATA_L = (uint16_t)x;
	SFR_NIC_DATA_H = (uint16_t)x >> 8;
	SFR_NIC_RING_L = nic;
	SFR_NIC_RING_H = nic >> 8;
	SFR_NIC_CTRL = units;
	wait_n = 1;
	while (SFR_NIC_CTRL && ++wait_n);
	timed_out = !wait_n;
	if (timed_out)
		io_failed = 1;
}

static void nic_init(void)
{
	reg_write(0x1322, reg_read(0x1322) | 0x0020);	/* NIC_RST */
	reg_write(0x130c, reg_read(0x130c) | 0x0400);	/* NIC_ENABLE */
	reg_write(0x1a46, 0x00ff);	/* RXSTOPR 0x2ff: RX ring 0x0000-0x17ff */
	reg_write(0x1a47, 0x0002);
	reg_write(0x1a44, 0x00fe);	/* TXSTOPR 0x3fe */
	reg_write(0x1a45, 0x0003);
	reg_write(0x1a48, 0x0004);	/* RXFSTR */
	reg_write(0x1a17, 0x0000);	/* GCR */
	reg_write(0x1a5c, 0x0001);	/* DROP_MODE */
	reg_write(0x1a16, 0x0001);	/* TXCR: TX enable */
	reg_write(0x1a15, 0x0003);	/* RXCR1: RX enable, strip CRC */
	/* CPU port 7, tag only trapped frames: as hlyi's Sodola SL-SG008W build (3:b373) and the TP-Link runtime */
	reg_write(0x121a, 0x00bb);	/* CPU_CTRL */
	reg_write(0x1219, 0x0080);	/* CPU_PORT_MASK */
}



static __xdata __at(0x2000) uint8_t rxb[0x600];
static __xdata __at(0x2600) uint8_t txb[8 + 0xf8];	/* descriptor + frame */
static uint8_t tx_seq;
static uint16_t tx_last;
static __bit tx_started;

/* Returns 0 if sent, 1 if the NIC was not ready */
static uint8_t nic_tx(uint8_t n)
{
	uint16_t p;
	if (io_failed)
		return 1;

	for (; n < 60; n++)
		txb[8 + n] = 0;
	if (tx_started) {
		WAIT_WHILE(reg_read(0x1a56) == tx_last);	/* previous frame taken */
		if (timed_out)
			return 1;
	}
	WAIT_WHILE(((reg_read(0x1a09) << 8 | reg_read(0x1a08)) << 3) < n + 8);
	if (timed_out)
		return 1;
	p = (reg_read(0x1a53) << 8 | reg_read(0x1a52)) << 3;
	txb[0] = tx_seq++;
	txb[1] = 0x07;
	txb[2] = txb[3] = txb[4] = txb[5] = 0;
	txb[6] = 0;
	txb[7] = n;
	dma(txb, p | 0x8000, (n + 15) >> 3);
	if (timed_out)
		return 1;
	tx_last = reg_read(0x1a56);
	tx_started = 1;
	reg_write(0x1a0d, 1);
	return io_failed ? 1 : 0;
}

static __xdata __at(0x2700) uint8_t rx_hdr[8];	/* DMA targets stay 8-byte aligned, as in the boot code */

/* Returns frame length, 0 if none or dropped */
static uint16_t nic_rx(void)
{
	uint16_t p, n;

	if (!reg_read(0x1a04) && !reg_read(0x1a05))
		return 0;
	p = (reg_read(0x1a51) << 8 | reg_read(0x1a50)) << 3;
	dma(rx_hdr, p, 1);
	if (timed_out)
		return 0;
	n = rx_hdr[6] << 8 | rx_hdr[7];
	if (n > sizeof(rxb) || n < 14 || (rx_hdr[1] & 1)) {
		reg_write(0x1a0c, 1);
		return 0;
	}
	p += 8;
	if (p >= 0x1800)
		p -= 0x1800;
	dma(rxb, p, (n + 7) >> 3);
	if (timed_out)
		return 0;
	reg_write(0x1a0c, 1);
	return io_failed ? 0 : n;
}

/* Frame layout: DA, SA, 0x88b5, op, seq, status, data */
#define F_DA	8
#define F_SA	14
#define F_TYPE	20
#define F_OP	22
#define F_SEQ	23
#define F_STAT	24
#define F_DATA	25
#define DATA_MAX	(sizeof(txb) - F_DATA)

#define OP_REG_READ	0x01
#define OP_REG_WRITE	0x02
#define OP_PHY_READ	0x03
#define OP_PHY_WRITE	0x04
#define OP_XDATA_READ	0x05
#define OP_XDATA_WRITE	0x06
#define OP_FLASH_READ	0x07
#define OP_FLASH_ARM	0x08
#define OP_FLASH_ERASE	0x09
#define OP_FLASH_PROG	0x0a
#define OP_LOG		0x0b
#define OP_REBOOT	0x0e
#define OP_INFO		0x0f
#define OP_BEACON	0x7e
#define REPLY		0x80

#define ST_OK		0
#define ST_BAD_OP	1
#define ST_BAD_ARG	2
#define ST_DENIED	3
#define ST_TIMEOUT	4

static __code const uint8_t my_mac[6] = {0x02, 0x83, 0x70, 0x00, 0x00, 0x01};
static uint16_t chip_id;
static uint16_t uptime_s;

static void frame_start(const __xdata uint8_t *da, uint8_t op, uint8_t seq, uint8_t status)
{
	for (uint8_t i = 0; i < 6; i++) {
		txb[F_DA + i] = da ? da[i] : 0xff;
		txb[F_SA + i] = my_mac[i];
	}
	txb[F_TYPE] = 0x88;
	txb[F_TYPE + 1] = 0xb5;
	txb[F_OP] = op;
	txb[F_SEQ] = seq;
	txb[F_STAT] = status;
}

static uint8_t put16(uint8_t at, uint16_t v)
{
	txb[F_DATA + at] = v >> 8;
	txb[F_DATA + at + 1] = v;
	return at + 2;
}

static uint8_t info(void)
{
	uint8_t n = put16(0, VERSION);

	n = put16(n, chip_id);
	n = put16(n, uptime_s);
	for (uint8_t i = 0; i < 8; i++)
		n = put16(n, reg_read(0x1352 + i));	/* PORTn_STATUS: link bit 4, speed bits 1:0 */
	return put16(n, log_len);
}

static void beacon(void)
{
	frame_start(0, OP_BEACON, 0, ST_OK);
	nic_tx(F_DATA - 8 + info());
}

/*
 * SPI flash, as the TP-Link boot code does it (common CODE 0x0b09-0x0c3a, FLASH.md).
 * Erase and program cannot reach the identity block and the settings.
 */
static __bit flash_armed;

static uint8_t rdsr(void)
{
	uint16_t t;

	for (t = 1; SFR_FLASH_EXEC_BUSY && t; t++);
	SFR_FLASH_TCONF = 0x11;
	SFR_FLASH_CMD_R = 0x05;
	SFR_FLASH_EXEC_GO = 1;
	for (t = 1; SFR_FLASH_EXEC_BUSY && t; t++);
	return t ? SFR_FLASH_DATA0 : 0xff;	/* 0xff reads as busy */
}

/* Erase takes up to ~300 ms: allow 16 rounds of ~65k status reads */
static __bit wait_wip(void)
{
	for (uint8_t i = 0; i < 16; i++) {
		WAIT_WHILE(rdsr() & 0x01);
		if (!timed_out)
			return 0;
	}
	return 1;
}

static __bit wren(void)
{
	if (wait_wip())
		return 1;
	SFR_FLASH_TCONF = 0x18;
	SFR_FLASH_CMD = 0x06;
	SFR_FLASH_EXEC_GO = 1;
	WAIT_WHILE(!(rdsr() & 0x02));
	return timed_out;
}

static void flash_single_io(void)
{
	SFR_FLASH_DUMMYCYCLES = 0;
	SFR_FLASH_MODEB = 0;
}

/* Back to the dual-I/O 0xbb read the CODE fetches use */
static void flash_read_mode(void)
{
	SFR_FLASH_MODEB = 0x18;
	SFR_FLASH_CMD_R = 0xbb;
	SFR_FLASH_DUMMYCYCLES = 4;
}

static void flash_addr(uint32_t a)
{
	SFR_FLASH_ADDR16 = a >> 16;
	SFR_FLASH_ADDR8 = a >> 8;
	SFR_FLASH_ADDR0 = a;
}

static void flash_read(uint32_t addr, __xdata uint8_t *buf, uint8_t n)
{
	flash_single_io();
	wait_wip();
	flash_read_mode();
	SFR_FLASH_TCONF = 0x04;
	while (n) {
		flash_addr(addr);
		SFR_FLASH_EXEC_GO = 1;
		WAIT_WHILE(SFR_FLASH_EXEC_BUSY);
		*buf++ = SFR_FLASH_DATA0;
		if (!--n)
			break;
		*buf++ = SFR_FLASH_DATA8;
		if (!--n)
			break;
		*buf++ = SFR_FLASH_DATA16;
		if (!--n)
			break;
		*buf++ = SFR_FLASH_DATA24;
		--n;
		addr += 4;
	}
}

/* Everything below the identity block and settings: [0, 0x1fc000). No addition, so nothing can wrap. */
static uint8_t flash_allowed(uint32_t addr, uint16_t n)
{
	if (!flash_armed)
		return ST_DENIED;
	if (addr < 0x1fc000 && n <= 0x1fc000 - addr)
		return ST_OK;
	return ST_DENIED;
}

static uint8_t flash_erase_4k(uint32_t addr)
{
	uint8_t st = flash_allowed(addr, 0x1000);

	if (st || (addr & 0xfff))
		return st ? st : ST_BAD_ARG;
	flash_single_io();
	if (wren()) {
		flash_read_mode();
		return ST_TIMEOUT;
	}
	SFR_FLASH_TCONF = 0x08;
	SFR_FLASH_CMD = 0x20;
	flash_addr(addr);
	SFR_FLASH_EXEC_GO = 1;
	st = wait_wip() ? ST_TIMEOUT : ST_OK;
	flash_read_mode();
	return st;
}

static uint8_t flash_program(uint32_t addr, __xdata uint8_t *buf, uint8_t n)
{
	uint8_t k, st = flash_allowed(addr, n);

	if (st || !n)
		return st;
	flash_single_io();
	while (n) {
		k = n < 4 ? n : 4;
		if (k > (uint8_t)(0x100 - (uint8_t)addr) && (uint8_t)addr)
			k = 0x100 - (uint8_t)addr;	/* stop at the page end */
		if (wren()) {
			st = ST_TIMEOUT;
			break;
		}
		SFR_FLASH_CMD = 0x02;
		SFR_FLASH_TCONF = 0x08 | k;
		flash_addr(addr);
		SFR_FLASH_DATA0 = buf[0];
		SFR_FLASH_DATA8 = buf[1];
		SFR_FLASH_DATA16 = buf[2];
		SFR_FLASH_DATA24 = buf[3];
		SFR_FLASH_EXEC_GO = 1;
		addr += k;
		buf += k;
		n -= k;
	}
	if (wait_wip())
		st = ST_TIMEOUT;
	flash_read_mode();
	return st;
}

static void reboot(void)
{
	reg_write(0x1322, 0x0001);	/* CHIP_RESET.CHIP_RST */
	while (1);
}

static void handle(uint16_t len)
{
	__xdata uint8_t *a = rxb + 16;	/* request arguments */
	uint8_t op = rxb[14], status = ST_OK, n = 0;
	uint16_t r = (uint16_t)a[0] << 8 | a[1];
	uint32_t fa = (uint32_t)a[0] << 16 | (uint32_t)a[1] << 8 | a[2];	/* casts: SDCC shifts bytes as signed int */

	if (len < 16 || rxb[12] != 0x88 || rxb[13] != 0xb5 || (op & REPLY))
		return;
	frame_start(0, op | REPLY, rxb[15], ST_OK);	/* broadcast: reaches the host even if it sent a wrong SA */

	switch (op) {
	case OP_REG_WRITE:
		reg_write(r, a[2] << 8 | a[3]);
		/* fall through */
	case OP_REG_READ:
		n = put16(0, reg_read(r));
		break;
	case OP_PHY_WRITE:
		phy_write(a[0], a[1] << 8 | a[2], a[3] << 8 | a[4]);
		/* fall through */
	case OP_PHY_READ:
		n = put16(0, phy_read(a[0], a[1] << 8 | a[2]));
		break;
	case OP_XDATA_READ:
		if (a[2] > DATA_MAX) {
			status = ST_BAD_ARG;
			break;
		}
		for (n = 0; n < a[2]; n++)
			txb[F_DATA + n] = ((__xdata uint8_t *)r)[n];
		break;
	case OP_XDATA_WRITE:	/* addr(2) count(1) data */
		if (len < 19 + a[2]) {
			status = ST_BAD_ARG;
			break;
		}
		for (uint8_t i = 0; i < a[2]; i++)
			((__xdata uint8_t *)r)[i] = a[3 + i];
		break;
	case OP_FLASH_READ:
		if (a[3] > DATA_MAX) {
			status = ST_BAD_ARG;
			break;
		}
		flash_read(fa, txb + F_DATA, a[3]);
		n = a[3];
		break;
	case OP_FLASH_ARM:
		flash_armed = r == 0x5afe;
		flash_single_io();
		n = put16(0, rdsr());	/* status register: BP bits block erase/program */
		flash_read_mode();
		break;
	case OP_FLASH_ERASE:
		status = flash_erase_4k(fa);
		break;
	case OP_FLASH_PROG:	/* addr(3) count(1) data */
		if (len < 20 + a[3]) {
			status = ST_BAD_ARG;
			break;
		}
		status = flash_program(fa, a + 4, a[3]);
		break;
	case OP_LOG:	/* reply: length, then that many log bytes from offset r */
		if (r > log_len)
			r = log_len;
		n = log_len - r;
		if (n > (uint16_t)(DATA_MAX - 2))
			n = DATA_MAX - 2;
		memcpy(txb + F_DATA + 2, log_buf + r, n);
		n = put16(0, n) + n;
		break;
	case OP_REBOOT:
		nic_tx(F_DATA - 8);	/* acknowledge, and let it leave before the reset */
		WAIT_WHILE(reg_read(0x1a56) == tx_last);
		reboot();
		break;	/* not reached on hardware */
	case OP_INFO:
		n = info();
		break;
	default:
		status = ST_BAD_OP;
	}
	txb[F_STAT] = status;
	nic_tx(F_DATA - 8 + n);
}

/* Runs before SDCC's RAM init */
unsigned char __sdcc_external_startup(void)
{
	IE = 0;
	MAP_FLASH_PAGE_TO_XDATA = 0;	/* reset leaves XDATA 0x4000-0xbfff mapped to flash */
	return 0;
}

void main(void)
{
	uint16_t ticks = 0;

	IE = 0;
	/* Chip setup the TP-Link boot code did (b01_4140, 5056, 0e97); hlyi's setup_clock uses the same values */
	reg_write(0x1336, 0x0041);	/* DW8051_RDY: DW8051_RATE 4 (62.5 MHz), READY */
	reg_write(0x1d32, 0x0004);	/* IO_MISC_FUNC: UART_EN */
	reg_write(0x130c, 0x1470);	/* MISCELLANEOUS_CONFIGURE0: FLASH, NIC, PON_LIGHT, DW8051, AUTOLOAD */
	SFR_FLASH_CONFIG = 9;		/* flash controller, as rtl837x_flash.c and the boot code */
	SFR_FLASH_CONF_RCMD = 0xbb;
	SFR_FLASH_CONF_DIV = 4;

	/* 5 ms Timer 2 tick at 62.5 MHz / 12, as hlyi's setup_timer2 (0:07a1); polled, ET2 stays off */
	T2CON = 0;
	CKCON &= ~0x20;
	RCAP2_U16 = 0x9a47;
	T2_U16 = 0x9a47;
	T2CON = 0x04;	/* TR2 */

	log_str("rtl8370n console\n");
	log_reg("nic_gcr", reg_read(0x1a17));	/* hlyi waits for bit 4 on cold boot (3:b221) */
	nic_init();
	chip_id = switch_init();
	beacon();

	while (1) {
		uint16_t len = nic_rx();

		if (len)
			handle(len);
		if (T2CON & 0x80) {	/* TF2 */
			T2CON &= ~0x80;
			if (++ticks == 400) {	/* 2 s */
				ticks = 0;
				uptime_s += 2;
				beacon();
			}
		}
	}
}
