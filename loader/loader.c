#include <8051.h>
#include <stdint.h>
#include <stdbool.h>

#include "../rtl837x_sfr.h"
#include "../rtl837x_regs.h"
#include "../rtl837x_common.h"
#include "../rtl837x_flash.h"
#include "layout.h"

#define XM_SOH	0x01
#define XM_STX	0x02
#define XM_EOT	0x04
#define XM_ACK	0x06
#define XM_NAK	0x15
#define XM_CAN	0x18

#define TICKS_1S	159

extern __xdata struct flash_region_t flash_region;
extern __xdata uint32_t flash_size;


__xdata uint8_t buf[256];
__xdata uint16_t crc;
__xdata uint16_t crct[256];
__xdata uint8_t slotbuf[LDR_SLOT_BUF];
#ifndef ALIAS_WIN
#define ALIAS_WIN(a)	(*(__code const uint8_t *)(a))
#define ALIAS_RAM	((__code const uint8_t *)LDR_ALIAS_RAM)
#define ALIAS_JUMP(a)	((void (*)(void))(a))()
#endif

static bool small;
static bool direct_boot;
static bool direct_nonzero;
static bool direct_other;
static uint16_t direct_want;
__xdata uint16_t xcrc;
__xdata uint16_t scrc;
__xdata uint32_t xlen;
static uint8_t alias_bank;
static uint16_t alias_code;
__code const uint8_t * __code const hexd = "0123456789abcdef";

unsigned char __sdcc_external_startup(void)
{
	MAP_FLASH_PAGE_TO_XDATA = 0;
	return 0;
}

static void put(uint8_t c)
{
	while (!TI);
	TI = 0;
	SBUF = c;
}

void write_char(char c)
{
	if (c == '\n')
		put('\r');
	put(c);
}

void print_string(__code const char *p)
{
	while (*p)
		write_char(*p++);
}

void print_byte(uint8_t a)
{
	write_char(hexd[a >> 4]);
	write_char(hexd[a & 0xf]);
}

static void reg_rd(uint16_t a)
{
	SFR_REG_ADDR_U16 = a;
	SFR_EXEC_GO = SFR_EXEC_READ_REG;
	while (SFR_EXEC_STATUS != 0);
}

static void reg_wr(uint16_t a)
{
	SFR_REG_ADDR_U16 = a;
	SFR_EXEC_GO = SFR_EXEC_WRITE_REG;
	while (SFR_EXEC_STATUS != 0);
}

static void reg_modify(uint16_t a, uint8_t clr0, uint8_t set0, uint8_t set8)
{
	uint8_t d24, d16, d8, d0;

	reg_rd(a);
	d24 = SFR_DATA_24;
	d16 = SFR_DATA_16;
	d8 = SFR_DATA_8;
	d0 = SFR_DATA_0;
	SFR_DATA_24 = d24;
	SFR_DATA_16 = d16;
	SFR_DATA_8 = d8 | set8;
	SFR_DATA_0 = (d0 & ~clr0) | set0;
	reg_wr(a);
}

static void setup_clock(void)
{
	reg_modify(RTL837X_REG_HW_CONF, 0x30, 0x00, 0x01);
	reg_modify(RTL837X_PIN_MUX_1, 0x00, 0x01, 0x00);
}

static void serial_init(void)
{
	TMOD = (TMOD & 0x0f) | 0x20;
	CKCON |= 0x10;
	PCON |= 0x80;
	SCON = 0x50;
	TH1 = 0xef;
	TCON |= 0x40;
	RI = 0;
	TI = 1;
}

static __data uint8_t th0_last;

static void timer0_start(void)
{
	TMOD = (TMOD & 0xf0) | 0x01;
	TH0 = 0;
	TL0 = 0;
	th0_last = 0;
	TR0 = 1;
}

static bool tick(void)
{
	uint8_t h = TH0;
	bool t = h < th0_last;

	th0_last = h;
	return t;
}

static int16_t get_timeout(uint16_t ticks)
{
	while (1) {
		if (RI) {
			RI = 0;
			return SBUF;
		}
		if (tick()) {
			if (!ticks--)
				return -1;
		}
	}
}

static bool wait_key(uint8_t k, uint16_t ticks)
{
	timer0_start();
	while (ticks) {
		if (RI) {
			RI = 0;
			if (SBUF == k)
				return true;
		}
		if (tick())
			ticks--;
	}
	return false;
}

static void crc_table(void)
{
	uint16_t c;
	uint8_t i = 0;
	uint8_t j;

	do {
		c = i;
		for (j = 0; j < 8; j++)
			c = (c & 1) ? (c >> 1) ^ 0xa001 : c >> 1;
		crct[i] = c;
	} while (++i);
}

static void readb(uint32_t a, uint16_t n)
{
	flash_region.addr = a;
	flash_region.len = n;
	flash_read_bulk(buf);
}

static void program(uint32_t a, uint16_t n)
{
	flash_region.addr = a;
	flash_region.len = n;
	flash_write_bytes(buf);
}

static void erase(uint32_t a)
{
	flash_region.addr = a;
	flash_sector_erase();
}

static void crc_range(uint32_t a, uint32_t end)
{
	uint16_t n;
	uint16_t i;

	while (a < end) {
		n = (end - a) > sizeof(buf) ? sizeof(buf) : (end - a);
		readb(a, n);
		for (i = 0; i < n; i++)
			crc = (crc >> 8) ^ crct[(uint8_t)crc ^ buf[i]];
		a += n;
	}
}

static bool is_all(uint32_t a, uint32_t end, uint8_t v)
{
	uint16_t i;

	while (a < end) {
		readb(a, sizeof(buf));
		for (i = 0; i < sizeof(buf); i++)
			if (buf[i] != v)
				return false;
		a += sizeof(buf);
	}
	return true;
}

static void copy(uint32_t src, uint32_t dst, uint32_t len)
{
	uint16_t n;

	while (len) {
		n = len > sizeof(buf) ? sizeof(buf) : len;
		if (!(dst & 0xfff))
			erase(dst);
		readb(src, n);
		program(dst, n);
		src += n;
		dst += n;
		len -= n;
	}
}

static bool has_meta(uint32_t a)
{
	readb(a, 7);
	return buf[0] == 'R' && buf[1] == 'T' && buf[2] == 'L' && buf[3] == 'B';
}

static uint16_t app_crc(uint32_t base, uint32_t home)
{
	crc = 0;
	crc_range(home, home + LDR_HOME_LEN);
	crc_range(base + LDR_APP_A, base + LDR_APP_A_END);
	crc_range(base + LDR_APP_B, base + LDR_APP_B_END);
	return crc;
}

static bool app_valid(void)
{
	uint16_t want;

	if (!has_meta(LDR_META))
		return false;
	want = buf[4] | ((uint16_t)buf[5] << 8);
	return app_crc(0, LDR_SLOT) == want;
}

static void drop_upload(void)
{
	uint32_t a;

	for (a = LDR_UPLOAD + 0x3000; a > LDR_UPLOAD; a -= 0x1000)
		erase(a);
	erase(LDR_UPLOAD);
}

static void install(uint32_t home)
{
	uint16_t want;

	print_string("Installing application\n");
	want = app_crc(LDR_UPLOAD, home);
	erase(LDR_META);
	copy(home, LDR_SLOT, LDR_HOME_LEN);
	copy(LDR_UPLOAD + LDR_APP_A, LDR_APP_A, LDR_APP_A_END - LDR_APP_A);
	copy(LDR_UPLOAD + LDR_APP_B, LDR_APP_B, LDR_APP_B_END - LDR_APP_B);
	if (app_crc(0, LDR_SLOT) != want) {
		print_string("Verify failed, image kept\n");
		return;
	}
	buf[0] = 'R';
	buf[1] = 'T';
	buf[2] = 'L';
	buf[3] = 'B';
	buf[4] = want;
	buf[5] = want >> 8;
	buf[6] = LDR_LAYOUT;
	program(LDR_META, 7);
	erase(LDR_BOOTCNT);
	drop_upload();
	print_string("Installed\n");
}

static void handle_upload(void)
{
	readb(LDR_UPLOAD, 2);
	if (buf[0] != 0x00 || buf[1] != 0x40) {
		if (!is_all(LDR_UPLOAD, LDR_UPLOAD + 0x4000, 0xff))
			drop_upload();
		return;
	}

	print_string("Checking uploaded image\n");
	crc = 0;
	crc_range(LDR_UPLOAD, LDR_UPLOAD + LDR_IMAGE_LEN);
	if (crc != 0xb001) {
		print_string("Bad checksum, image dropped\n");
		drop_upload();
		return;
	}
	if (has_meta(LDR_UPLOAD + LDR_META)) {
		if (buf[6] != LDR_LAYOUT) {
			print_string("Image needs another loader, dropped\n");
			drop_upload();
			return;
		}
		install(LDR_UPLOAD + LDR_SLOT);
	} else if (is_all(LDR_UPLOAD + LDR_DATA, LDR_UPLOAD + LDR_DATA_END, 0)) {
		install(LDR_UPLOAD + 2);
	} else {
		print_string("Image uses the loader data area, dropped\n");
		drop_upload();
	}
}

static uint8_t bootcount(void)
{
	uint8_t n;

	readb(LDR_BOOTCNT, 64);
	if (buf[0] != 'B' || buf[1] != 'C') {
		erase(LDR_BOOTCNT);
		buf[0] = 'B';
		buf[1] = 'C';
		buf[2] = 0;
		program(LDR_BOOTCNT, 3);
		return 0;
	}
	for (n = 2; n < 64 && buf[n] == 0; n++);
	if (n == 64)
		return n - 2;
	buf[0] = 0;
	program(LDR_BOOTCNT + n, 1);
	return n - 2;
}

static void store(uint32_t off)
{
	uint8_t i;
	uint32_t dst = off;

	for (i = 0; i < 128; i++)
		xcrc = (xcrc >> 8) ^ crct[(uint8_t)xcrc ^ buf[i]];
	if (!small) {
		dst = LDR_UPLOAD + off;
	} else if (off < LDR_SLOT_BUF) {
		if (!off) {
			erase(LDR_META);
			direct_boot = false;
			direct_nonzero = false;
			direct_other = false;
		}
		for (i = 0; i < 128; i++) {
			slotbuf[(uint16_t)off + i] = buf[i];
			if (off || i >= 2)
				scrc = (scrc >> 8) ^ crct[(uint8_t)scrc ^ buf[i]];
		}
		return;
	} else if (off >= LDR_DATA && off < LDR_DATA_END) {
		if (off == LDR_META && buf[0] == 'R' && buf[1] == 'T' && buf[2] == 'L' && buf[3] == 'B') {
			direct_other = buf[6] != LDR_LAYOUT;
			direct_boot = !direct_other;
			direct_want = buf[4] | ((uint16_t)buf[5] << 8);
		}
		for (i = 0; i < 128; i++) {
			if (buf[i])
				direct_nonzero = true;
		}
		if (off < LDR_SLOT || !direct_boot)
			return;
	} else if (off >= LDR_APP_B_END) {
		return;
	} else {
		for (i = 0; i < 128; i++)
			scrc = (scrc >> 8) ^ crct[(uint8_t)scrc ^ buf[i]];
	}
	if (!(dst & 0xfff))
		erase(dst);
	program(dst, 128);
}

static void direct_finish(void)
{
	uint16_t a;
	uint16_t n;
	uint16_t want;

	if (xlen != LDR_IMAGE_LEN || xcrc != 0xb001 || slotbuf[0] != 0x00 || slotbuf[1] != 0x40) {
		print_string("Bad checksum, image dropped\n");
		return;
	}
	if (direct_other) {
		print_string("Image needs another loader, dropped\n");
		return;
	}
	if (!direct_boot) {
		if (direct_nonzero) {
			print_string("Image uses the loader data area, dropped\n");
			return;
		}
		for (a = 0; a < LDR_SLOT_BUF; a += 0x1000)
			erase(LDR_SLOT + a);
		for (a = 0; a < LDR_HOME_LEN; a += n) {
			n = LDR_HOME_LEN - a > 256 ? 256 : LDR_HOME_LEN - a;
			flash_region.addr = LDR_SLOT + a;
			flash_region.len = n;
			flash_write_bytes(slotbuf + 2 + a);
		}
	}
	want = app_crc(0, LDR_SLOT);
	if (want != (direct_boot ? direct_want : scrc)) {
		print_string("Verify failed\n");
		return;
	}
	buf[0] = 'R';
	buf[1] = 'T';
	buf[2] = 'L';
	buf[3] = 'B';
	buf[4] = want;
	buf[5] = want >> 8;
	buf[6] = LDR_LAYOUT;
	program(LDR_META, 7);
	erase(LDR_BOOTCNT);
	print_string("Installed\n");
}

static bool xmodem(void)
{
	uint32_t off = 0;
	uint8_t blk = 1;
	uint8_t idle = 0;
	uint8_t b, nb, sum, i;
	int16_t c;
	bool started = false;

	xcrc = 0;
	scrc = 0;
	xlen = 0;
	timer0_start();
	put(XM_NAK);
	while (1) {
		c = get_timeout(started ? TICKS_1S : 3 * TICKS_1S);
		if (c < 0) {
			if (++idle > (started ? 10 : 100))
				return false;
			put(XM_NAK);
			continue;
		}
		idle = 0;
		if (c == XM_EOT) {
			put(XM_ACK);
			return true;
		}
		if (c == XM_CAN)
			return false;
		if (c == XM_STX) {
			put(XM_CAN);
			put(XM_CAN);
			return false;
		}
		if (c != XM_SOH)
			continue;
		started = true;

		c = get_timeout(TICKS_1S);
		b = c;
		c = get_timeout(TICKS_1S);
		nb = c;
		sum = 0;
		for (i = 0; i < 128 && c >= 0; i++) {
			c = get_timeout(TICKS_1S);
			buf[i] = c;
			sum += buf[i];
		}
		if (c >= 0)
			c = get_timeout(TICKS_1S);
		if (c < 0 || (uint8_t)(b ^ nb) != 0xff) {
			put(XM_NAK);
			continue;
		}
		if ((uint8_t)c != sum) {
			put(XM_NAK);
			continue;
		}
		if (b == (uint8_t)(blk - 1) && off) {
			put(XM_ACK);
			continue;
		}
		if (b != blk) {
			put(XM_NAK);
			continue;
		}
		if (off >= LDR_IMAGE_LEN) {
			put(XM_CAN);
			return false;
		}
		store(off);
		off += 128;
		xlen = off;
		blk++;
		put(XM_ACK);
	}
}

static bool load_slot(void)
{
	uint16_t want;
	uint16_t off;

	crc = 0;
	crc_range(LDR_SLOT, LDR_SLOT + LDR_HOME_LEN);
	want = crc;
	for (off = 0; off < sizeof(slotbuf); off += sizeof(buf)) {
		flash_region.addr = LDR_SLOT + off;
		flash_region.len = sizeof(buf);
		flash_read_bulk(slotbuf + off);
	}
	crc = 0;
	for (off = 0; off < LDR_HOME_LEN; off++)
		crc = (crc >> 8) ^ crct[(uint8_t)crc ^ slotbuf[off]];
	return crc == want;
}

static bool alias_find(void)
{
	uint8_t bank = PSBANK;
	uint16_t c;
	uint16_t i;

	for (alias_bank = 1; alias_bank < 0x40; alias_bank++) {
		PSBANK = alias_bank;
		for (c = 0x4000 + LDR_ALIAS_FLASH; c >= 0x4000; c += 0x4000) {
			for (i = 0; i < LDR_ALIAS_LEN; i++) {
				if (ALIAS_WIN(c + i) != ALIAS_RAM[i])
					break;
			}
			if (i == LDR_ALIAS_LEN) {
				alias_code = c;
				PSBANK = bank;
				return true;
			}
		}
	}
	PSBANK = bank;
	return false;
}

static void start_app(void)
{
	if (!load_slot()) {
		print_string("Application copy failed\n");
		return;
	}
	if (!alias_find()) {
		print_string("Loader window check failed, the loader must be reflashed\n");
		timer0_start();
		get_timeout(10 * TICKS_1S);
		return;
	}
	print_string("Starting application\n");
	TR0 = 0;
	while (!TI);
	PSBANK = alias_bank;
	ALIAS_JUMP(alias_code);
}

static void recover(void)
{
	print_string("XMODEM upload of a firmware image, 115200 8N1\n");
	if (xmodem()) {
		get_timeout(TICKS_1S);
		print_string("\nReceived\n");
		if (small)
			direct_finish();
	} else {
		print_string("\nNo image received, 128 byte blocks with checksum only\n");
	}
	if (!small)
		handle_upload();
}

void main(void)
{
	CKCON = 0;
	SFR_b9 = 0x00;
	SFR_ba = 0x80;
	IE = 0;
	EIE = 0;

	setup_clock();
	serial_init();
	flash_init(0);
	crc_table();

	print_string("\nRTLPlayground loader\n");
	flash_read_jedecid();
	small = flash_size < LDR_UPLOAD + LDR_IMAGE_LEN;
	if (small)
		print_string("Small flash, updates over XMODEM only\n");
	else
		handle_upload();

	if (bootcount() >= LDR_RECOVERY_BOOTS) {
		print_string("Recovery boot, press x within 3 s for XMODEM\n");
		if (wait_key('x', 3 * TICKS_1S))
			recover();
	}

	while (1) {
		while (!app_valid()) {
			print_string("No valid application\n");
			recover();
		}
		start_app();
		recover();
	}
}
