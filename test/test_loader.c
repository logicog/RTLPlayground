#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>

#undef __sfr
#undef __sfr16
#undef __sfr32
#undef __sbit
#define __sfr   volatile uint8_t
#define __sfr16 volatile uint16_t
#define __sfr32 volatile uint32_t
#define __sbit  bool
#include "rtl837x_sfr.h"
#undef __sfr
#undef __sfr16
#undef __sfr32
#undef __sbit
#define __sfr   extern volatile uint8_t
#define __sfr16 extern volatile uint16_t
#define __sfr32 extern volatile uint32_t
#define __sbit  extern bool

volatile bool TR0, EA;
volatile uint8_t TMOD, PCON, SCON, TH1, TCON, TL0, IE;

static bool ti_reg = true, ri_reg, tf0_reg, tf0_stuck;
static uint8_t th0_reg;
static long t0_wraps, rx_after;
static uint8_t tx_reg, rx_reg;
static bool tx_pending;
static uint8_t rxq[1 << 20];
static size_t rx_head, rx_tail;
static void uart_step(void);

static void uart_rx(void);
static bool *ti_lval(void) { uart_step(); return &ti_reg; }
static bool *ri_lval(void) { uart_step(); return &ri_reg; }
static void t0_step(void)
{
	uart_rx();
	if (TR0) {
		th0_reg += 0x40;
		if (!th0_reg) {
			t0_wraps++;
			tf0_stuck = true;
		}
	}
}
static __attribute__((unused)) bool *tf0_lval(void) { t0_step(); if (tf0_stuck) tf0_reg = true; return &tf0_reg; }
static uint8_t *th0_lval(void) { t0_step(); return &th0_reg; }
static uint8_t *sbuf_lval(void)
{
	if (!ti_reg) {
		tx_pending = true;
		return &tx_reg;
	}
	return &rx_reg;
}

#define TI   (*ti_lval())
#define RI   (*ri_lval())
#define TF0  (*tf0_lval())
#define TH0  (*th0_lval())
#define SBUF (*sbuf_lval())

#include "rtl837x_common.h"
#include "rtl837x_regs.h"
#include "../loader/layout.h"

__xdata struct flash_region_t flash_region;
__xdata uint32_t flash_size;
static uint32_t chip_size = 0x200000;
static long drop_writes;
static bool bad_copy;
static bool slot_copied;

static uint8_t loader_byte(uint32_t i)
{
	return (uint8_t)(i * 7 + (i >> 8));
}
extern uint8_t slotbuf[LDR_SLOT_BUF];

#define FLASH_LEN 0x200000
static uint8_t flash[FLASH_LEN];
static long reads_left;
static jmp_buf escape;
static int started;
static char out[65536];
static size_t out_len;

void flash_init(uint8_t enable_dio) { (void)enable_dio; }

void flash_read_jedecid(void) { flash_size = chip_size; }
static bool uid_read;
void flash_read_uid(void) { uid_read = true; }

void flash_read_bulk(__xdata uint8_t *dst)
{
	if (--reads_left < 0)
		longjmp(escape, 2);
	memcpy(dst, flash + flash_region.addr, flash_region.len);
	if (bad_copy && dst >= slotbuf && dst < slotbuf + sizeof(slotbuf))
		dst[3] ^= 0x20;
	flash_region.addr += flash_region.len;
}

void flash_write_bytes(__xdata uint8_t *ptr)
{
	if (drop_writes > 0 && flash_region.addr >= LDR_APP_A && flash_region.addr < LDR_APP_A_END) {
		drop_writes--;
		return;
	}
	for (uint16_t i = 0; i < flash_region.len; i++)
		flash[flash_region.addr + i] &= ptr[i];
}

void flash_sector_erase(void)
{
	memset(flash + (flash_region.addr & ~0xfffUL), 0xff, 0x1000);
}


void app_start(void)
{
	slot_copied = !memcmp(slotbuf, flash + LDR_SLOT, LDR_HOME_LEN);
	started++;
	longjmp(escape, 1);
}

static uint32_t reg_hw_conf = 0x00001001;
static uint32_t reg_pin_mux_1 = 0x40000040;
static uint8_t sd_v[4], sd_l[4] = { 0x5a, 0x5a, 0x5a, 0x5a };
static bool sd_t[4];
static volatile uint16_t addr_v;
static volatile uint8_t exec_v, status_v;

static uint32_t *reg_of(uint16_t a)
{
	static uint32_t other;
	if (a == RTL837X_REG_HW_CONF)
		return &reg_hw_conf;
	if (a == RTL837X_PIN_MUX_1)
		return &reg_pin_mux_1;
	return &other;
}

volatile uint8_t *sd_24(void) { sd_t[0] = true; return &sd_v[0]; }
volatile uint8_t *sd_16(void) { sd_t[1] = true; return &sd_v[1]; }
volatile uint8_t *sd_8(void) { sd_t[2] = true; return &sd_v[2]; }
volatile uint8_t *sd_0(void) { sd_t[3] = true; return &sd_v[3]; }
volatile uint16_t *addr_p(void) { return &addr_v; }
volatile uint8_t *exec_p(void) { return &exec_v; }

volatile uint8_t *status_p(void)
{
	uint32_t *r = reg_of(addr_v);
	if (exec_v == SFR_EXEC_READ_REG) {
		for (int i = 0; i < 4; i++)
			sd_v[i] = *r >> (8 * (3 - i));
	} else if (exec_v == SFR_EXEC_WRITE_REG) {
		uint32_t v = 0;
		for (int i = 0; i < 4; i++) {
			uint8_t b = sd_t[i] ? sd_v[i] : sd_l[i];
			sd_l[i] = b;
			v |= (uint32_t)b << (8 * (3 - i));
		}
		*r = v;
	}
	exec_v = 0;
	memset(sd_t, 0, sizeof(sd_t));
	status_v = 0;
	return &status_v;
}

#define SFR_DATA_24 (*sd_24())
#define SFR_DATA_16 (*sd_16())
#define SFR_DATA_8 (*sd_8())
#define SFR_DATA_0 (*sd_0())
#define SFR_REG_ADDR_U16 (*addr_p())
#define SFR_EXEC_GO (*exec_p())
#define SFR_EXEC_STATUS (*status_p())

static uint32_t wrap = 0x200000;
static uint8_t coderam[0x4000];
static uint8_t jumped_bank;
static uint16_t jumped_code;

static uint8_t flash_win(uint16_t a)
{
	uint8_t b = PSBANK & 0x40 ? 1 : PSBANK;
	uint32_t fa = ((uint32_t)(b - 1) * 0xc000 + a) % wrap;
	return fa < FLASH_LEN ? flash[fa] : 0xff;
}

static void alias_jump(uint16_t a)
{
	jumped_bank = PSBANK;
	jumped_code = a;
	app_start();
}

#define ALIAS_WIN(a) flash_win(a)
#define ALIAS_RAM (coderam + LDR_ALIAS_RAM)
#define ALIAS_JUMP(a) alias_jump(a)

#define main loader_main
#include "../loader/loader.c"
#undef main

static const uint8_t *xm_img;
static int xm_state;
static uint32_t xm_blk;
static uint32_t xm_corrupt, xm_dup, xm_stop;
static bool xm_stx;
static bool xm_done;
static int xm_skip;

static void xm_send(uint32_t blk, bool bad)
{
	uint8_t sum = 0;
	if (rx_tail + 256 > sizeof(rxq))
		longjmp(escape, 3);
	if (xm_stop && blk > xm_stop)
		return;
	rxq[rx_tail++] = xm_stx ? 0x02 : 0x01;
	rxq[rx_tail++] = blk;
	rxq[rx_tail++] = ~blk;
	for (int i = 0; i < 128; i++) {
		uint8_t b = xm_img[(blk - 1) * 128 + i];
		rxq[rx_tail++] = (bad && i == 5) ? b ^ 0x10 : b;
		sum += b;
	}
	rxq[rx_tail++] = sum;
}

static void xm_react(uint8_t c)
{
	if (!xm_img || xm_done)
		return;
	if (c == 0x15 && xm_skip) {
		xm_skip--;
		return;
	}
	if (c == 0x15) {
		if (!xm_state) {
			xm_state = 1;
			xm_blk = 1;
		}
		if (xm_state == 2)
			rxq[rx_tail++] = 0x04;
		else
			xm_send(xm_blk, false);
	} else if (c == 0x06) {
		if (xm_state == 2) {
			xm_done = true;
			return;
		}
		if (xm_blk == xm_dup) {
			xm_dup = 0;
			xm_send(xm_blk, false);
			return;
		}
		if (++xm_blk > LDR_IMAGE_LEN / 128) {
			xm_state = 2;
			rxq[rx_tail++] = 0x04;
			return;
		}
		if (xm_blk == xm_corrupt) {
			xm_corrupt = 0;
			xm_send(xm_blk, true);
		} else {
			xm_send(xm_blk, false);
		}
	}
}

static void uart_step(void)
{
	if (tx_pending) {
		tx_pending = false;
		if (out_len < sizeof(out) - 1 && (tx_reg >= 0x20 || tx_reg == '\n'))
			out[out_len++] = tx_reg;
		xm_react(tx_reg);
		ti_reg = true;
	}
}

static void uart_rx(void)
{
	uart_step();
	if (!ri_reg && rx_head < rx_tail && t0_wraps >= rx_after) {
		rx_reg = rxq[rx_head++];
		ri_reg = true;
	}
}

static void xm_arm(const uint8_t *img, uint32_t corrupt, uint32_t dup)
{
	xm_img = img;
	xm_state = 0;
	xm_done = false;
	xm_corrupt = corrupt;
	xm_dup = dup;
	xm_stop = 0;
	xm_stx = false;
	xm_skip = 0;
	rx_head = rx_tail = 0;
}

static void type(const char *s)
{
	while (*s)
		rxq[rx_tail++] = *s++;
}

static uint16_t crc_of(const uint8_t *p, size_t n, uint16_t c)
{
	while (n--) {
		c ^= *p++;
		for (int i = 0; i < 8; i++)
			c = (c & 1) ? (c >> 1) ^ 0xa001 : c >> 1;
	}
	return c;
}

static void make_app(uint8_t *img, unsigned seed)
{
	srand(seed);
	memset(img, 0, LDR_IMAGE_LEN);
	img[0] = 0x00;
	img[1] = 0x40;
	img[2] = 0x02;
	for (uint32_t i = 3; i < 0x3000; i++)
		img[i] = rand();
	for (uint32_t i = LDR_APP_A; i < 0x2d000; i++)
		img[i] = rand();
	for (uint32_t i = LDR_APP_B; i < 0x66000; i++)
		img[i] = rand();
	for (uint32_t i = 0x70000; i < 0x70040; i++)
		img[i] = 'c';
	uint16_t c = crc_of(img, LDR_IMAGE_LEN - 2, 0) ^ 0xffff;
	img[LDR_IMAGE_LEN - 2] = c;
	img[LDR_IMAGE_LEN - 1] = c >> 8;
}

static uint16_t installed_crc(const uint8_t *home, const uint8_t *base)
{
	uint16_t c = crc_of(home, LDR_HOME_LEN, 0);
	c = crc_of(base + LDR_APP_A, LDR_APP_A_END - LDR_APP_A, c);
	return crc_of(base + LDR_APP_B, LDR_APP_B_END - LDR_APP_B, c);
}

static void install_by_old_updater(const uint8_t *app)
{
	memset(flash, 0xff, FLASH_LEN);
	for (uint32_t i = 0; i < 0x4000; i++)
		flash[i] = loader_byte(i);
	memcpy(flash + LDR_SLOT, app + 2, LDR_HOME_LEN);
	memcpy(flash + LDR_APP_A, app + LDR_APP_A, LDR_APP_A_END - LDR_APP_A);
	memcpy(flash + LDR_APP_B, app + LDR_APP_B, LDR_APP_B_END - LDR_APP_B);
	uint16_t c = installed_crc(app + 2, app);
	memcpy(flash + LDR_META, "RTLB", 4);
	flash[LDR_META + 4] = c;
	flash[LDR_META + 5] = c >> 8;
}

static bool skew;

static int boot(void)
{
	memcpy(coderam, flash + 2, LDR_HOME_LEN);
	if (skew)
		coderam[LDR_ALIAS_RAM + 0x100] ^= 0x01;
	started = 0;
	out_len = 0;
	th0_reg = 0;
	tf0_reg = false;
	tf0_stuck = false;
	t0_wraps = 0;
	reads_left = 20000;
	int r = setjmp(escape);
	if (!r)
		loader_main();
	out[out_len] = 0;
	return r;
}

static int fails;

static void check(bool ok, const char *what)
{
	printf("%s %s\n", ok ? "PASS" : "FAIL", what);
	if (!ok) {
		fails++;
		printf("--- loader output ---\n%s---\n", out);
	}
}

static bool app_is(const uint8_t *app)
{
	return !memcmp(flash + LDR_SLOT, app + 2, LDR_HOME_LEN)
		&& !memcmp(flash + LDR_APP_A, app + LDR_APP_A, LDR_APP_A_END - LDR_APP_A)
		&& !memcmp(flash + LDR_APP_B, app + LDR_APP_B, LDR_APP_B_END - LDR_APP_B);
}

static bool loader_intact(void)
{
	for (uint32_t i = 0; i < 0x4000; i++)
		if (flash[i] != loader_byte(i))
			return false;
	return true;
}

static int unconfirmed(void)
{
	int n = 0;
	if (flash[LDR_BOOTCNT] != 'B' || flash[LDR_BOOTCNT + 1] != 'C')
		return -1;
	while (n < 62 && !flash[LDR_BOOTCNT + 2 + n])
		n++;
	return n;
}

static void confirm(void)
{
	memset(flash + LDR_BOOTCNT, 0xff, 0x1000);
}

int main(void)
{
	static uint8_t a1[LDR_IMAGE_LEN], a2[LDR_IMAGE_LEN], inst[LDR_IMAGE_LEN];
	make_app(a1, 1);
	make_app(a2, 2);


	install_by_old_updater(a1);
	check(boot() == 1 && started == 1, "first boot after the old updater starts the application");
	check(slot_copied, "the application HOME is copied to xdata before the jump");
	check(jumped_bank == 0x2b && jumped_code == 0xb800, "a 2 MB flash runs app_start through bank 0x2b at 0xb800");
	check(reg_pin_mux_1 == 0x40000041 && reg_hw_conf == 0x00001101, "setup_clock writes every byte of the registers it modifies");
	check(unconfirmed() == 1, "first boot writes the counter magic and one mark");
	confirm();

	memcpy(flash + LDR_UPLOAD, flash, 0x4000);
	check(boot() == 1 && jumped_bank == 0x2b && jumped_code == 0xb800, "a copy of the loader in the upload area is dropped and not used as the bank window");
	check(flash[LDR_UPLOAD + 0x3800] == 0xff && flash[LDR_UPLOAD] == 0xff, "a dropped upload loses its first 16 KB");
	confirm();

	memcpy(flash + LDR_UPLOAD, a2, LDR_IMAGE_LEN);
	check(boot() == 1 && app_is(a2), "an uploaded image is installed and started");
	check(flash[LDR_UPLOAD] == 0xff && flash[LDR_UPLOAD + 1] == 0xff, "the upload header is erased after the install");
	check(!memcmp(flash + 0x70000, (uint8_t[]){0xff, 0xff}, 2), "the configuration area is not touched");
	check(loader_intact(), "the loader stays untouched");
	confirm();

	memcpy(flash + LDR_UPLOAD, a1, LDR_IMAGE_LEN);
	memset(flash + LDR_META, 0xff, 0x1000);
	memset(flash + 0x10000, 0x00, 0x8000);
	check(boot() == 1 && app_is(a1), "an install interrupted after the metadata erase is redone");
	confirm();

	memcpy(flash + LDR_UPLOAD, a2, LDR_IMAGE_LEN);
	flash[LDR_UPLOAD + 0x20000] ^= 0x01;
	check(boot() == 1 && app_is(a1), "an upload with a bad checksum is dropped, the old application starts");
	check(flash[LDR_UPLOAD] == 0xff, "the bad upload is erased");
	confirm();

	memcpy(inst, a2, LDR_IMAGE_LEN);
	memcpy(inst + LDR_SLOT, a2 + 2, LDR_HOME_LEN);
	memset(inst + 2, 0x77, 0x3ffe);
	memcpy(inst + LDR_META, "RTLB", 4);
	inst[LDR_META + 6] = LDR_LAYOUT;
	uint16_t c = crc_of(inst, LDR_IMAGE_LEN - 2, 0) ^ 0xffff;
	inst[LDR_IMAGE_LEN - 2] = c;
	inst[LDR_IMAGE_LEN - 1] = c >> 8;
	memcpy(flash + LDR_UPLOAD, inst, LDR_IMAGE_LEN);
	check(boot() == 1 && app_is(a2) && loader_intact() && flash[LDR_UPLOAD + 0x3800] == 0xff, "an install image updates only the application and its loader copy is erased");
	check(flash[LDR_META + 6] == LDR_LAYOUT, "the install records the layout version in the metadata");
	confirm();

	memcpy(inst, a1, LDR_IMAGE_LEN);
	memcpy(inst + LDR_SLOT, a1 + 2, LDR_HOME_LEN);
	memcpy(inst + LDR_META, "RTLB", 4);
	inst[LDR_META + 6] = LDR_LAYOUT + 1;
	c = crc_of(inst, LDR_IMAGE_LEN - 2, 0) ^ 0xffff;
	inst[LDR_IMAGE_LEN - 2] = c;
	inst[LDR_IMAGE_LEN - 1] = c >> 8;
	memcpy(flash + LDR_UPLOAD, inst, LDR_IMAGE_LEN);
	check(boot() == 1 && app_is(a2) && flash[LDR_UPLOAD] == 0xff && strstr(out, "another loader"), "an image for another layout is dropped and the old application starts");
	confirm();

	memcpy(inst, a1, LDR_IMAGE_LEN);
	inst[LDR_DATA + 0x100] = 0x12;
	c = crc_of(inst, LDR_IMAGE_LEN - 2, 0) ^ 0xffff;
	inst[LDR_IMAGE_LEN - 2] = c;
	inst[LDR_IMAGE_LEN - 1] = c >> 8;
	memcpy(flash + LDR_UPLOAD, inst, LDR_IMAGE_LEN);
	check(boot() == 1 && app_is(a2) && flash[LDR_UPLOAD] == 0xff, "an image that uses the loader data area is refused");
	confirm();

	memcpy(inst, a1, LDR_IMAGE_LEN);
	inst[LDR_APP_A_END - 0x100] = 0x34;
	c = crc_of(inst, LDR_IMAGE_LEN - 2, 0) ^ 0xffff;
	inst[LDR_IMAGE_LEN - 2] = c;
	inst[LDR_IMAGE_LEN - 1] = c >> 8;
	memcpy(flash + LDR_UPLOAD, inst, LDR_IMAGE_LEN);
	check(boot() == 1 && app_is(inst) && loader_intact(), "an image with code in bank 5 below the loader data is installed");
	confirm();

	for (int i = 0; i < 3; i++)
		boot();
	check(unconfirmed() == 3 && !strstr(out, "Recovery"), "three unconfirmed boots are counted without recovery");
	check(boot() == 1 && strstr(out, "Recovery boot") && started == 1, "the fourth unconfirmed boot offers recovery, then starts the application");
	check(unconfirmed() == 4, "the application sees four marks, more than LDR_RECOVERY_BOOTS");
	confirm();
	check(boot() == 1 && !strstr(out, "Recovery") && unconfirmed() == 1, "after a confirmation the count starts again");
	confirm();

	flash[LDR_APP_A + 0x1234] ^= 0x80;
	check(boot() == 2 && !started && strstr(out, "No valid application"), "a damaged application is not started");

	xm_arm(a1, 0, 0);
	check(boot() == 1 && started == 1 && xm_done && app_is(a1), "a damaged application is replaced over XMODEM and started");
	check(flash[LDR_UPLOAD] == 0xff, "the XMODEM upload is consumed by the install");
	confirm();

	flash[LDR_SLOT + 0x100] ^= 0x01;
	xm_arm(a2, 7, 2000);
	check(boot() == 1 && xm_done && app_is(a2) && !xm_corrupt && !xm_dup, "XMODEM survives a bad checksum and a repeated block");
	confirm();

	for (int i = 0; i < 3; i++)
		boot();
	xm_arm(a1, 0, 0);
	rxq[rx_tail++] = 'q';
	rxq[rx_tail++] = 'x';
	check(boot() == 1 && xm_done && app_is(a1) && strstr(out, "Recovery boot"), "x in the recovery window, after other bytes, starts XMODEM");
	confirm();

	for (int i = 0; i < 3; i++)
		boot();
	xm_arm(NULL, 0, 0);
	rxq[rx_tail++] = 'q';
	check(boot() == 1 && app_is(a1) && strstr(out, "Recovery boot") && !strstr(out, "XMODEM upload"), "other bytes in the recovery window boot the application");
	confirm();

	for (int i = 0; i < 3; i++)
		boot();
	xm_arm(NULL, 0, 0);
	for (int i = 0; i < 20000; i++)
		rxq[rx_tail++] = 'q';
	check(boot() == 1 && app_is(a1) && strstr(out, "Recovery boot") && rx_head < rx_tail, "endless noise in the recovery window does not hold the boot");
	confirm();

	for (int i = 0; i < 3; i++)
		boot();
	xm_arm(a1, 0, 0);
	rx_after = 2 * TICKS_1S;
	rxq[rx_tail++] = 'x';
	check(boot() == 1 && xm_done && app_is(a1), "x two seconds into the recovery window starts XMODEM although TF0 stays set after an overflow");
	rx_after = 0;
	confirm();

	boot();
	boot();
	memcpy(flash + LDR_UPLOAD, a2, LDR_IMAGE_LEN);
	check(boot() == 1 && app_is(a2) && unconfirmed() == 1, "an install clears the marks of the previous image");
	confirm();

	memcpy(flash + LDR_UPLOAD, a1, LDR_IMAGE_LEN);
	drop_writes = 1;
	xm_arm(NULL, 0, 0);
	check(boot() == 1 && app_is(a1) && strstr(out, "Verify failed") && flash[LDR_UPLOAD] == 0xff, "a failed verify is installed again after the XMODEM window");
	confirm();

	memset(flash + LDR_BOOTCNT + 2, 0x00, 62);
	flash[LDR_BOOTCNT] = 'B';
	flash[LDR_BOOTCNT + 1] = 'C';
	xm_arm(NULL, 0, 0);
	check(boot() == 1 && strstr(out, "Recovery boot") && unconfirmed() == 62, "a full counter keeps the recovery state for the application");
	confirm();

	for (int i = 0; i < 3; i++)
		boot();
	xm_arm(a2, 0, 0);
	xm_stx = true;
	rxq[rx_tail++] = 'x';
	check(boot() == 1 && app_is(a1) && strstr(out, "128 byte blocks"), "an XMODEM-1K sender is refused and the application starts");
	confirm();

	for (int i = 0; i < 3; i++)
		boot();
	xm_arm(a2, 0, 0);
	xm_stop = 100;
	rxq[rx_tail++] = 'x';
	check(boot() == 1 && app_is(a1) && flash[LDR_UPLOAD] == 0xff && flash[LDR_UPLOAD + 1] == 0xff, "an XMODEM transfer cut short leaves no valid upload header");
	confirm();

	skew = true;
	check(boot() == 2 && !started && strstr(out, "Loader window check failed"), "no jump when no bank window shows the loader");
	skew = false;
	confirm();

	static const uint32_t wraps[] = { 0x80000, 0x100000 };
	for (int w = 0; w < 2; w++) {
		wrap = wraps[w];
		check(boot() == 1 && ((uint32_t)(jumped_bank - 1) * 0xc000 + jumped_code) % wrap == LDR_ALIAS_FLASH && jumped_bank < 0x40,
			wrap == 0x80000 ? "a 512 KB flash finds a bank window on the loader" : "a 1 MB flash finds a bank window on the loader");
		confirm();
	}
	wrap = 0x400000;
	check(boot() == 2 && !started && strstr(out, "Loader window check failed"), "a flash with no window on the loader does not jump");
	wrap = 0x200000;
	confirm();

	bad_copy = true;
	check(boot() == 2 && !started && strstr(out, "Application copy failed"), "no jump when the copy in xdata does not match the slot");
	bad_copy = false;
	confirm();

	chip_size = 0x80000;
	wrap = 0x80000;
	memcpy(flash + LDR_UPLOAD, a2, LDR_IMAGE_LEN);
	check(boot() == 1 && app_is(a1) && strstr(out, "Small flash") && !memcmp(flash + LDR_UPLOAD, a2, LDR_IMAGE_LEN), "a flash smaller than the upload area never touches it");
	memset(flash + LDR_UPLOAD, 0xff, LDR_IMAGE_LEN);
	confirm();

	memset(flash + 0x70000, 0x42, 0x100);
	for (int i = 0; i < 3; i++)
		boot();
	xm_arm(a2, 0, 0);
	rxq[rx_tail++] = 'x';
	check(boot() == 1 && app_is(a2) && strstr(out, "Installed") && loader_intact(), "a small flash installs an application over XMODEM in place");
	check(flash[0x70000] == 0x42 && flash[0x700ff] == 0x42 && flash[LDR_UPLOAD] == 0xff, "the direct install leaves the configuration and the upload area alone");
	confirm();

	memcpy(inst, a1, LDR_IMAGE_LEN);
	memcpy(inst + LDR_SLOT, a1 + 2, LDR_HOME_LEN);
	memset(inst + 2, 0x77, 0x3ffe);
	memset(inst + LDR_BOOTCNT, 0xff, 0x1000);
	c = installed_crc(a1 + 2, a1);
	memcpy(inst + LDR_META, "RTLB", 4);
	inst[LDR_META + 4] = c;
	inst[LDR_META + 5] = c >> 8;
	inst[LDR_META + 6] = LDR_LAYOUT + 1;
	c = crc_of(inst, LDR_IMAGE_LEN - 2, 0) ^ 0xffff;
	inst[LDR_IMAGE_LEN - 2] = c;
	inst[LDR_IMAGE_LEN - 1] = c >> 8;
	for (int i = 0; i < 3; i++)
		boot();
	xm_arm(inst, 0, 0);
	rxq[rx_tail++] = 'x';
	check(boot() == 2 && !started && strstr(out, "another loader"), "a small flash refuses an image for another layout over XMODEM");
	inst[LDR_META + 6] = LDR_LAYOUT;
	c = crc_of(inst, LDR_IMAGE_LEN - 2, 0) ^ 0xffff;
	inst[LDR_IMAGE_LEN - 2] = c;
	inst[LDR_IMAGE_LEN - 1] = c >> 8;
	xm_arm(inst, 0, 0);
	check(boot() == 1 && app_is(a1) && loader_intact(), "a small flash installs an install image over XMODEM and keeps its own loader");
	check(flash[LDR_META + 6] == LDR_LAYOUT, "the direct install records the layout version in the metadata");
	confirm();

	memcpy(inst, a2, LDR_IMAGE_LEN);
	inst[0x20000] ^= 0x01;
	for (int i = 0; i < 3; i++)
		boot();
	xm_arm(inst, 0, 0);
	rxq[rx_tail++] = 'x';
	check(boot() == 2 && !started && strstr(out, "Bad checksum"), "a corrupt image over XMODEM on a small flash leaves no valid application");
	xm_arm(a2, 0, 0);
	check(boot() == 1 && app_is(a2) && strstr(out, "No valid application"), "after a failed direct install the next XMODEM transfer restores the application");
	confirm();

	memcpy(inst, a1, LDR_IMAGE_LEN);
	inst[LDR_DATA + 0x100] = 0x12;
	c = crc_of(inst, LDR_IMAGE_LEN - 2, 0) ^ 0xffff;
	inst[LDR_IMAGE_LEN - 2] = c;
	inst[LDR_IMAGE_LEN - 1] = c >> 8;
	for (int i = 0; i < 3; i++)
		boot();
	xm_arm(inst, 0, 0);
	rxq[rx_tail++] = 'x';
	check(boot() == 2 && !started && strstr(out, "loader data area"), "a small flash refuses an image with data in the loader area");
	xm_arm(a2, 0, 0);
	check(boot() == 1 && app_is(a2), "the refused image is replaced by the next transfer");
	confirm();

	for (int i = 0; i < 3; i++)
		boot();
	drop_writes = 1;
	xm_arm(a1, 0, 0);
	rxq[rx_tail++] = 'x';
	check(boot() == 2 && !started && strstr(out, "Verify failed"), "a flash write lost during a direct install is not blessed");
	xm_arm(a1, 0, 0);
	check(boot() == 1 && app_is(a1), "the next transfer after a lost write installs the application");
	confirm();
	chip_size = 0x200000;
	wrap = 0x200000;

	memcpy(inst, a2, LDR_IMAGE_LEN);
	inst[0x20000] ^= 0x01;
	flash[LDR_SLOT + 0x200] ^= 0x01;
	xm_arm(inst, 0, 0);
	check(boot() == 2 && !started && strstr(out, "Bad checksum"), "a corrupt image over XMODEM is dropped and nothing starts");

	install_by_old_updater(a1);
	for (int i = 0; i < 3; i++)
		boot();
	xm_arm(NULL, 0, 0);
	type("c");
	type("inx\x7f" "fo\r");
	type("crc 0 4000\r");
	type("dump 0x3a000 10\r");
	type("zzz\r");
	type("dump\r");
	type("crc 0 4g00\r");
	type("boot\r");
	uid_read = false;
	char want[64];
	snprintf(want, sizeof(want), "%04x", crc_of(flash, 0x4000, 0));
	snprintf(want + 8, sizeof(want) - 8, "%04x ok", installed_crc(a1 + 2, a1));
	check(boot() == 1 && started == 1 && app_is(a1) && strstr(out, "Loader console"), "c in the recovery window opens the console and boot starts the application");
	check(uid_read && strstr(out, "Loader CRC: bad") && strstr(out, want + 8) && strstr(out, "Unconfirmed boots: 04") && strstr(out, "Upload header: ffff"),
		"info reports the flash, the loader, the application and the boot marks, with a deleted character");
	check(strstr(out, want) != NULL, "crc prints the CRC16 of a flash range");
	check(strstr(out, "0003a000: 42 43 00 00 00 00 ff ff ff ff ff ff ff ff ff ff\n") != NULL, "dump prints the flash bytes with their address");
	{
		int q = 0;
		for (char *p = out; (p = strstr(p, "?\n")); p++)
			q++;
		check(q == 3, "an unknown command, a missing argument and a bad number are refused");
	}
	check(unconfirmed() == 4 && loader_intact(), "the console writes nothing to the flash");
	confirm();

	flash[LDR_APP_A + 0x10] ^= 0x01;
	xm_arm(a2, 0, 0);
	xm_skip = 1;
	type("\rxmodem\r");
	check(boot() == 1 && started == 1 && app_is(a2) && strstr(out, "Loader console") && strstr(out, "Console idle"),
		"Enter while XMODEM waits opens the console, xmodem there installs, an idle console leaves");
	confirm();

	for (int i = 0; i < 3; i++)
		boot();
	xm_arm(NULL, 0, 0);
	type("c");
	check(boot() == 1 && started == 1 && app_is(a2) && strstr(out, "Console idle"), "a console without input lets the switch boot");
	confirm();

	printf("%s: %d failed\n", fails ? "FAIL" : "OK", fails);
	return fails != 0;
}
