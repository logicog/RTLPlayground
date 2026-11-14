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

static uint32_t regmem[0x10000 / 4];

static uint32_t *reg_of(uint16_t a)
{
	if (a == RTL837X_REG_HW_CONF)
		return &reg_hw_conf;
	if (a == RTL837X_PIN_MUX_1)
		return &reg_pin_mux_1;
	return &regmem[a >> 2];
}

#define RING RTL837X_NIC_RX_RING_BYTES
static uint8_t asic[RING];
static uint16_t rx_slot[16];
static int rx_count, rx_cur, nic_resets, tx_count;
static uint8_t txlog[8][1600];
static struct { uint8_t mask, dev; uint16_t reg, val; } smi[8];
static int smi_count;

static void rx_regs(void)
{
	regmem[RTL837X_REG_NIC_RX_BUFF_DATA >> 2] = rx_count - rx_cur;
	regmem[RTL837X_REG_CPU_RX_CURR_PKT >> 2] = rx_cur < rx_count ? rx_slot[rx_cur] : 0;
}

static void reg_written(uint16_t a, uint32_t v)
{
	if (a == RTL837X_REG_RESET) {
		if (v & 1) {
			regmem[a >> 2] = 0;
			longjmp(escape, 4);
		}
		if (v & (1 << RESET_NIC_BIT))
			nic_resets++;
		regmem[a >> 2] = v & ~(1u << RESET_NIC_BIT);
	} else if (a == RTL837X_REG_NIC_RXCMD) {
		rx_cur++;
		rx_regs();
	}
}

static bool sim_nic(uint8_t *b, uint16_t ring, uint8_t words)
{
	if (ring & 0x8000) {
		if (tx_count < 8)
			memcpy(txlog[tx_count], b, (size_t)words * 8);
		tx_count++;
		return true;
	}
	for (size_t i = 0; i < (size_t)words * 8; i++)
		b[i] = ring < RING ? asic[(ring + i) % RING] : 0xd7;
	return true;
}

static void rx_frame(uint16_t slot, uint8_t port, const uint8_t *f, uint16_t len)
{
	uint32_t pos = (uint32_t)slot * 8;
	uint8_t h[8] = { 0, 0, 0, port, len, len >> 8, 0, 0 };
	for (int i = 0; i < 8; i++)
		asic[(pos + i) % RING] = h[i];
	for (uint16_t i = 0; i < len; i++)
		asic[(pos + 8 + i) % RING] = f[i];
	rx_slot[rx_count++] = slot;
	rx_regs();
}

#define NIC_GO(b, r, w) sim_nic(b, r, w)

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
		reg_written(addr_v, v);
	} else if (exec_v == SFR_EXEC_WRITE_SMI && smi_count < 8) {
		smi[smi_count].mask = SFR_SMI_PHYMASK;
		smi[smi_count].dev = SFR_SMI_DEV;
		smi[smi_count].reg = SFR_SMI_REG_U16;
		smi[smi_count].val = (uint16_t)sd_v[2] << 8 | sd_v[3];
		smi_count++;
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
#include "../loader/net.c"

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

static size_t rx_gate = (size_t)-1;
static int rx_delay;

static void uart_rx(void)
{
	uart_step();
	if (rx_head == rx_gate && rx_delay) {
		rx_delay--;
		return;
	}
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
	rx_gate = (size_t)-1;
	rx_delay = 0;
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


static const uint8_t peer_mac[6] = { 0x02, 0xaa, 0xbb, 0xcc, 0xdd, 0x01 };
static const uint8_t own_mac[6] = { 0x06, 0x22, 0xee, 0x55, 0x66, 0x77 };

static uint16_t eth_hdr(uint8_t *f, bool tagged, uint16_t type)
{
	uint16_t o = 12;
	memset(f, 0xff, 6);
	memcpy(f + 6, peer_mac, 6);
	if (tagged) {
		const uint8_t tag[12] = { 0x88, 0x99, 0x04, 0x00, 0x00, 0x00, 0x00, 0x05, 0x81, 0x00, 0x00, 0x01 };
		memcpy(f + o, tag, sizeof(tag));
		o += sizeof(tag);
	}
	f[o] = type >> 8;
	f[o + 1] = type;
	return o + 2;
}

static uint16_t arp_req(uint8_t *f, bool tagged, const uint8_t *target)
{
	uint16_t o = eth_hdr(f, tagged, 0x0806);
	const uint8_t h[8] = { 0, 1, 8, 0, 6, 4, 0, 1 };
	memcpy(f + o, h, 8);
	memcpy(f + o + 8, peer_mac, 6);
	memcpy(f + o + 14, (uint8_t[]){ 192, 168, 2, 9 }, 4);
	memset(f + o + 18, 0, 6);
	memcpy(f + o + 24, target, 4);
	return o + 28;
}

static uint16_t ip_csum(const uint8_t *p, size_t n)
{
	uint32_t s = 0;
	for (size_t i = 0; i < n; i += 2)
		s += (uint32_t)p[i] << 8 | (i + 1 < n ? p[i + 1] : 0);
	while (s >> 16)
		s = (s & 0xffff) + (s >> 16);
	return ~s & 0xffff;
}

static uint16_t ping_req(uint8_t *f, bool tagged, const uint8_t *target, uint16_t data)
{
	uint16_t o = eth_hdr(f, tagged, 0x0800);
	uint8_t *ip4 = f + o;
	uint16_t tl = 20 + 8 + data;
	memset(ip4, 0, tl);
	ip4[0] = 0x45;
	ip4[2] = tl >> 8;
	ip4[3] = tl;
	ip4[8] = 128;
	ip4[9] = 1;
	memcpy(ip4 + 12, (uint8_t[]){ 192, 168, 2, 9 }, 4);
	memcpy(ip4 + 16, target, 4);
	uint16_t c = ip_csum(ip4, 20);
	ip4[10] = c >> 8;
	ip4[11] = c;
	uint8_t *ic = ip4 + 20;
	ic[0] = 8;
	ic[4] = 0x12;
	ic[5] = 0x34;
	ic[7] = 7;
	for (uint16_t i = 0; i < data; i++)
		ic[8 + i] = i * 3;
	c = ip_csum(ic, 8 + data);
	ic[2] = c >> 8;
	ic[3] = c;
	return o + tl;
}

static void nic_reset_model(void)
{
	memset(asic, 0, sizeof(asic));
	rx_count = rx_cur = nic_resets = tx_count = smi_count = 0;
	rx_regs();
	regmem[RTL837X_REG_CHIP_UUID >> 2] = 0x11223344;
	regmem[RTL837X_REG_CHIP_LOT_NO >> 2] = 0x55667788;
	regmem[RTL837X_REG_CPU_TX_CURR_PKT >> 2] = 0;
	net_used = false;
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


	{
		static uint8_t f[1600], ping[1600];
		uint16_t n;
		uint16_t pn;
		const uint8_t own_ip[4] = { 192, 168, 2, 2 };

		nic_reset_model();
		for (int i = 0; i < 3; i++)
			boot();
		nic_reset_model();
		n = arp_req(f, true, own_ip);
		rx_frame(RING / 8 - 1, 5, f, n);
		n = arp_req(f, false, (uint8_t[]){ 192, 168, 2, 3 });
		rx_frame(10, 6, f, n);
		pn = ping_req(ping, false, own_ip, 56);
		rx_frame(30, 4, ping, pn);
		xm_arm(NULL, 0, 0);
		type("c");
		type("net\r");
		rx_gate = rx_tail;
		rx_delay = 50;
		type("q");
		check(boot() == 4 && strstr(out, "MAC 06:22:ee:55:66:77 IP 192.168.2.2\n") && strstr(out, "rx port 05 type 0806"),
			"net brings the ports up with a MAC from the chip UUID and reads a CPU-tagged frame");
		check(rx_cur == 3 && strstr(out, "rx port 06 type 0806") && strstr(out, "rx port 04 type 0800") && !strstr(out, "rx error"),
			"net reads every queued frame, also one whose data wraps around the ring");
		check(tx_count == 2, "net answers the ARP request and the ping for its address and nothing else");
		{
			uint8_t *d = txlog[0], *e = d + 8, *a = e + 14;
			check(d[1] == 0x07 && (d[4] | d[5] << 8) == 42 && !memcmp(e, peer_mac, 6) && !memcmp(e + 6, own_mac, 6)
				&& e[12] == 0x08 && e[13] == 0x06 && a[7] == 2 && !memcmp(a + 8, own_mac, 6) && !memcmp(a + 14, own_ip, 4)
				&& !memcmp(a + 18, peer_mac, 6) && !memcmp(a + 24, (uint8_t[]){ 192, 168, 2, 9 }, 4),
				"the ARP reply carries the own MAC and IP and goes back to the asker");
		}
		{
			uint8_t *d = txlog[1], *e = d + 8, *ip4 = e + 14, *q = ping + 14;
			uint16_t tl = 20 + 8 + 56;
			check((d[4] | d[5] << 8) == 14 + tl && !memcmp(e, peer_mac, 6) && e[12] == 0x08 && e[13] == 0x00
				&& !memcmp(ip4 + 12, own_ip, 4) && !memcmp(ip4 + 16, q + 12, 4) && ip_csum(ip4, 20) == 0
				&& ip4[20] == 0 && ip_csum(ip4 + 20, tl - 20) == 0 && !memcmp(ip4 + 24, q + 24, tl - 24),
				"the echo reply swaps the addresses, keeps id, sequence and data, and has valid checksums");
		}
		check((regmem[RTL837X_CFG_PHY_MDI_REVERSE >> 2] & 0x0f) == 0x0c && regmem[RTL837X_CFG_PHY_TX_POLARITY_SWAP >> 2] == 0,
			"the pair order of the built-in PHYs is set, the TX polarity only on an N chip");
		check(smi_count == 2 && smi[0].mask == 0xf0 && smi[0].dev == (31 << 3 | 2) && smi[0].reg == 0xa610 && smi[0].val == 0x2858
			&& smi[1].mask == 0xf0 && smi[1].val == 0x2058, "only the built-in PHYs 4-7 are written, off and on again");
		{
			bool iso = true;
			for (int p = 0; p < 9; p++)
				iso = iso && regmem[(RTL837X_PORT_ISOLATION_BASE + p * 4) >> 2] == 0x200;
			for (int p = 4; p < 8; p++)
				iso = iso && regmem[(0x1238 + p * 0x100) >> 2] == 0xf77;
			iso = iso && regmem[0x1538 >> 2] == 0 && regmem[0x1a38 >> 2] == 0;
			check(iso && nic_resets == 1 && (reg_hw_conf & 0x1001) == 0x1001 && (regmem[0x632c >> 2] & 0x000f0000) == 0x000f0000
				&& (regmem[(RTL837X_REG_MAC_FORCE_MODE + 36) >> 2] & 3) == 3,
				"every port only reaches the CPU, the NIC is reset and enabled, ports 4-7 get MAC access");
		}
		check(strstr(out, "Console idle") && !started && loader_intact() && app_is(a2),
			"leaving the console after net resets the chip instead of jumping to the application");

		nic_reset_model();
		regmem[RTL837X_REG_CHIP_ID >> 2] = 0x00837000;
		n = arp_req(f, false, own_ip);
		rx_frame(0, 4, f, n);
		n = arp_req(f, false, (uint8_t[]){ 10, 0, 0, 9 });
		rx_frame(8, 4, f, n);
		xm_arm(NULL, 0, 0);
		type("c");
		type("net 300.1.1.1\r");
		type("net 10.0.0.9\r");
		rx_gate = rx_tail;
		rx_delay = 50;
		type("q");
		check(boot() == 4 && strstr(out, "?\n") && strstr(out, "IP 10.0.0.9\n") && tx_count == 1 && !memcmp(txlog[0] + 8 + 14 + 14, (uint8_t[]){ 10, 0, 0, 9 }, 4),
			"net takes an address, refuses a bad one and answers only for the new address");
		check(regmem[RTL837X_CFG_PHY_TX_POLARITY_SWAP >> 2] == 0x596a, "an N chip gets the TX polarity swap of the application");
		regmem[RTL837X_REG_CHIP_ID >> 2] = 0;
		ip[0] = 192; ip[1] = 168; ip[2] = 2; ip[3] = 2;
		net_used = false;
		confirm();
	}

	printf("%s: %d failed\n", fails ? "FAIL" : "OK", fails);
	return fails != 0;
}
