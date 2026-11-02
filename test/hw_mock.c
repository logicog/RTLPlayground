/*
 * hw_mock.c - the ASIC edge: SFR storage, register file, table engine.
 *
 * The shim declares every SFR extern so that including rtl837x_sfr.h from many
 * translation units does not define them repeatedly. Undefining the four
 * keywords here and re-including the header emits the definitions once, so
 * the mock tracks the real register list without a hand-kept copy.
 */
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

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

#include "rtl837x_common.h"
#include "rtlos_regs.h"
#include "hw_mock.h"

/* The RTL836x TA engine below needs this die's constants; rtlos_regs.h only
 * picks the header per SOC_FAMILY, so most host binaries build without it.
 * It is pure declarations for this die's register map, so an unconditional
 * include is safe. */
#include "rtl836x_regs.h"

uint8_t sfr_data[4];
unsigned long hw_reads, hw_writes;

static uint32_t      regfile[0x10000];
static uint32_t      vlan_tbl[4096];
static struct hw_l2  l2[HW_L2_MAX];
static uint64_t      counters[16][64];

void hw_reset(void)
{
	memset(regfile, 0, sizeof(regfile));
	memset(vlan_tbl, 0, sizeof(vlan_tbl));
	memset(l2, 0, sizeof(l2));
	memset(counters, 0, sizeof(counters));
	memset(sfr_data, 0, sizeof(sfr_data));
	hw_l2_8370_reset();
	hw_reads = hw_writes = 0;
}

void     hw_reg_set(uint16_t addr, uint32_t v) { regfile[addr] = v; }
uint32_t hw_reg_get(uint16_t addr)             { return regfile[addr]; }
uint32_t hw_vlan_word(uint16_t vid)            { return vlan_tbl[vid & 0xfff]; }
void     hw_counter_set(uint8_t port, uint8_t counter, uint64_t value) { counters[port & 0xf][counter & 0x3f] = value; }

/* ---- L2 table -------------------------------------------------------- */

static int l2_cmp(const void *x, const void *y)
{
	const struct hw_l2 *a = x, *b = y;
	if (a->present != b->present)
		return a->present ? -1 : 1;
	return (int)a->idx - (int)b->idx;
}

static void l2_sort(void) { qsort(l2, HW_L2_MAX, sizeof(l2[0]), l2_cmp); }

int hw_l2_count(void)
{
	int n = 0;
	for (int i = 0; i < HW_L2_MAX; i++)
		n += l2[i].present;
	return n;
}

const struct hw_l2 *hw_l2_at(int n)
{
	l2_sort();
	return (n >= 0 && n < hw_l2_count()) ? &l2[n] : 0;
}

static void l2_key(const struct hw_l2 *e, uint8_t mac[6], uint16_t *vid)
{
	mac[0] = e->b >> 8; mac[1] = e->b;
	mac[2] = e->a >> 24; mac[3] = e->a >> 16; mac[4] = e->a >> 8; mac[5] = e->a;
	*vid = ((e->b >> 16) & 0xfff);
}

const struct hw_l2 *hw_l2_find(const uint8_t mac[6], uint16_t vid)
{
	for (int i = 0; i < HW_L2_MAX; i++) {
		uint8_t m[6]; uint16_t v;
		if (!l2[i].present)
			continue;
		l2_key(&l2[i], m, &v);
		if (v == vid && !memcmp(m, mac, 6))
			return &l2[i];
	}
	return 0;
}

static struct hw_l2 *l2_free(void)
{
	for (int i = 0; i < HW_L2_MAX; i++)
		if (!l2[i].present)
			return &l2[i];
	return 0;
}

static uint16_t l2_hash(const uint8_t mac[6], uint16_t vid)
{
	uint16_t h = vid;
	for (int i = 0; i < 6; i++)
		h = (uint16_t)(h * 31 + mac[i]);
	return h & 0xfff;
}

void hw_l2_put(uint16_t idx, const uint8_t mac[6], uint16_t vid, uint16_t port_or_pmask, bool is_static)
{
	struct hw_l2 *e = (struct hw_l2 *)hw_l2_find(mac, vid);
	bool mc = mac[0] & 1;
	if (!e)
		e = l2_free();
	e->present = true;
	e->idx = idx & 0xfff;
	e->a = ((uint32_t)mac[2] << 24) | ((uint32_t)mac[3] << 16) | ((uint32_t)mac[4] << 8) | mac[5];
	e->b = (0x20u << 24) | ((uint32_t)(vid & 0xfff) << 16) | ((uint32_t)mac[0] << 8) | mac[1];
	if (mc) {
		e->b |= (uint32_t)(port_or_pmask & 0x3) << 30;
		e->c = (port_or_pmask >> 2) & 0xff;
	} else {
		e->b |= (uint32_t)(port_or_pmask & 0x3) << 30;
		e->c = (port_or_pmask >> 2) & 0x3;
	}
	if (is_static)
		e->c |= 1u << 16;
}

void hw_l2_decode(const struct hw_l2 *e, uint8_t mac[6], uint16_t *vid, uint16_t *port_or_pmask,
		  bool *valid, bool *is_static, bool *mc)
{
	l2_key(e, mac, vid);
	*valid = (e->b >> 29) & 1;
	*is_static = (e->c >> 16) & 1;
	*mc = mac[0] & 1;
	if (*mc)
		*port_or_pmask = ((e->b >> 30) & 0x3) | ((e->c & 0xff) << 2);
	else
		*port_or_pmask = ((e->b >> 30) & 0x3) | ((e->c & 0x3) << 2);
}

/* ---- the RTL836x TA engine (see doc/l2.md) --------------------------- */

/*
 * The RTL836x die drives its L2 through the TA window (0x0500 control, words
 * staged in WRDATA 0x0510-0x0516, answers in RDDATA 0x0520-0x0526, lookup
 * status in 0x0502) rather than through the three-word TBL interface of the
 * 8372/3. The engine below stores seven-word entries and behaves like the
 * measured die:
 *
 *   WRITE          places the entry at the slot its key hashes to and leaves
 *                  the stored words in the read window;
 *   read by address (method 1) exports the entry at the addressed slot, or
 *                  zero words for an empty slot;
 *   read by key    (method 0) answers 0x0502 with 0x1000 (FOUND) | slot for
 *                  an entry the walk would report (age left and live flag
 *                  set), and leaves 0x0502 alone on a miss, as measured.
 *
 * The one measured quirk reproduced here is the WRITE normalization: with the
 * live flag staged, an age field of 0 or one above 6 is stored as the
 * fabric's fresh age (6), the spa field and word 3 verbatim. A release
 * staging -- age and live flag cleared -- is stored as staged, so the entry
 * stops being held and the by-key lookup misses it.
 */
#define HW_L2_8370_MAX 8
struct hw_l2_8370 {
	bool     used;
	uint16_t idx;
	uint16_t words[7];
};

static struct hw_l2_8370 rtl836x_l2[HW_L2_8370_MAX];

static uint16_t l2_8370_hash_words(uint16_t w0, uint16_t w1, uint16_t w2, uint16_t fid)
{
	const uint8_t mac[6] = { (uint8_t)(w2 >> 8), (uint8_t)w2,
				 (uint8_t)(w1 >> 8), (uint8_t)w1,
				 (uint8_t)(w0 >> 8), (uint8_t)w0 };
	uint16_t h = fid;
	for (int i = 0; i < 6; i++)
		h = (uint16_t)(h * 31 + mac[i]);
	return h & 0xfff;
}

/* The slot the mock engine's key hash places mac/fid in. Tests seed there so
 * a later firmware write of the same key lands on the seeded entry. */
uint16_t hw_l2_8370_hash(const uint8_t mac[6], uint16_t fid)
{
	uint16_t w0 = (uint16_t)(((uint16_t)mac[4] << 8) | mac[5]);
	uint16_t w1 = (uint16_t)(((uint16_t)mac[2] << 8) | mac[3]);
	uint16_t w2 = (uint16_t)(((uint16_t)mac[0] << 8) | mac[1]);
	return l2_8370_hash_words(w0, w1, w2, fid);
}

void hw_l2_8370_reset(void)
{
	memset(rtl836x_l2, 0, sizeof(rtl836x_l2));
}

static struct hw_l2_8370 *l2_8370_at(uint16_t idx)
{
	for (int i = 0; i < HW_L2_8370_MAX; i++)
		if (rtl836x_l2[i].used && rtl836x_l2[i].idx == idx)
			return &rtl836x_l2[i];
	return 0;
}

static struct hw_l2_8370 *l2_8370_find_by_key(uint16_t w0, uint16_t w1, uint16_t w2,
					      uint16_t fid, bool held_only)
{
	for (int i = 0; i < HW_L2_8370_MAX; i++) {
		struct hw_l2_8370 *e = &rtl836x_l2[i];
		if (!e->used || e->words[0] != w0 || e->words[1] != w1
		    || e->words[2] != w2 || (e->words[3] & 0x0fff) != (fid & 0x0fff))
			continue;
		if (held_only && !(((e->words[4] >> RTL836x_TA_L2_W4_AGE_SHIFT) & RTL836x_TA_L2_W4_AGE_MASK)
				   && (e->words[5] & RTL836x_TA_L2_W5_LIVE)))
			continue;
		return e;
	}
	return 0;
}

/* Seed the die with one entry at `idx`, in the shape the fabric holds after
 * its own fix-ups (seeding is not normalized). W0-W2 carry the MAC, W3 the
 * static flag + FID, W4 the age and spa, W5 the live flag, W6 zero. */
void hw_l2_8370_put(uint16_t idx, const uint8_t mac[6], uint16_t fid,
		    uint8_t age, uint8_t spa, bool live, bool is_static)
{
	struct hw_l2_8370 *e = l2_8370_at(idx);
	if (!e) {
		for (int i = 0; i < HW_L2_8370_MAX && !e; i++)
			if (!rtl836x_l2[i].used)
				e = &rtl836x_l2[i];
	}
	if (!e)
		return;					/* all slots taken: never in these tests */

	e->used    = true;
	e->idx     = idx;
	e->words[0] = (uint16_t)(((uint16_t)mac[4] << 8) | mac[5]);
	e->words[1] = (uint16_t)(((uint16_t)mac[2] << 8) | mac[3]);
	e->words[2] = (uint16_t)(((uint16_t)mac[0] << 8) | mac[1]);
	e->words[3] = (uint16_t)((is_static ? RTL836x_TA_L2_W3_STATIC : 0) | (fid & 0x0fff));
	e->words[4] = (uint16_t)(((uint16_t)(age & 0x1f) << RTL836x_TA_L2_W4_AGE_SHIFT)
				  | ((uint16_t)(spa & 7) << RTL836x_TA_L2_W4_SPA_SHIFT));
	e->words[5] = live ? RTL836x_TA_L2_W5_LIVE : 0;
	e->words[6] = 0;
}

static uint16_t l2_8370_wrdata(uint8_t w)
{
	/* the firmware's WRITE stages each word into the register's high half */
	return (uint16_t)(regfile[RTL836x_TA_WRDATA_REG(w)] >> 16);
}

static void l2_8370_store(void)
{
	uint16_t words[7];

	for (int w = 0; w < 7; w++)
		words[w] = l2_8370_wrdata((uint8_t)w);

	/* The one measured fix-up: with the live flag staged, an age field of 0
	 * or one above 6 comes back stored as the fabric's fresh age; the spa
	 * field and W3 go in verbatim. */
	if (words[5] & RTL836x_TA_L2_W5_LIVE) {
		uint16_t age = (words[4] >> RTL836x_TA_L2_W4_AGE_SHIFT) & RTL836x_TA_L2_W4_AGE_MASK;
		if (age == 0 || age > RTL836x_TA_L2_W4_AGE_MAX)
			age = RTL836x_TA_L2_W4_AGE_MAX;
		words[4] = (uint16_t)((age << RTL836x_TA_L2_W4_AGE_SHIFT) | (words[4] & 0x07ff));
	}

	uint16_t idx = l2_8370_hash_words(words[0], words[1], words[2], words[3] & 0x0fff);
	struct hw_l2_8370 *e = l2_8370_at(idx);
	if (!e) {
		for (int i = 0; i < HW_L2_8370_MAX && !e; i++)
			if (!rtl836x_l2[i].used)
				e = &rtl836x_l2[i];
	}
	if (!e)
		return;

	e->used = true;
	e->idx  = idx;
	memcpy(e->words, words, sizeof(e->words));
	/* like the die, which answers the next read with what it stored */
	for (int w = 0; w < 7; w++)
		regfile[RTL836x_TA_RDDATA_REG(w)] = words[w];
}

static void l2_8370_read_addr(void)
{
	uint16_t idx = (uint16_t)(regfile[RTL836x_TA_ADDR_REG] >> 16) & 0xfff;
	struct hw_l2_8370 *e = l2_8370_at(idx);

	for (int w = 0; w < 7; w++)
		regfile[RTL836x_TA_RDDATA_REG(w)] = e ? e->words[w] : 0;
}

static void l2_8370_read_key(void)
{
	struct hw_l2_8370 *e = l2_8370_find_by_key(
		l2_8370_wrdata(0),
		l2_8370_wrdata(1),
		l2_8370_wrdata(2),
		l2_8370_wrdata(3) & RTL836x_TA_L2_W3_FID_MASK,
		true);
	if (e)
		regfile[RTL836x_TA_LUT_REG] = (uint32_t)(RTL836x_TA_LUT_FOUND | e->idx);
}

/* ---- the engine ------------------------------------------------------ */

static void l2_out(const struct hw_l2 *e, uint16_t idx)
{
	regfile[RTL837x_L2_DATA_OUT_A] = e ? e->a : 0;
	regfile[RTL837x_L2_DATA_OUT_B] = e ? e->b : 0;
	regfile[RTL837x_L2_DATA_OUT_C] = e ? e->c : 0;
	regfile[RTL837x_TBL_DATA_0] = (regfile[RTL837x_TBL_DATA_0] & ~0x1fffu) | (e ? 0x1000 : 0) | (idx & 0xfff);
}

static void table_exec(uint32_t ctrl)
{
	uint16_t idx   = ((ctrl >> 16) & 0xfff);
	uint8_t  type  = ctrl >> 8;
	bool     write = ctrl & TBL_WRITE;

	if (type == TBL_VLAN) {
		if (write)
			vlan_tbl[idx] = regfile[RTL837x_TBL_DATA_IN_A];
		else
			regfile[RTL837x_L2_DATA_OUT_A] = vlan_tbl[idx];
		return;
	}
	if (type != TBL_L2_UNICAST)
		return;

	uint32_t d0     = regfile[RTL837x_TBL_DATA_0];
	uint8_t  method = (d0 >> 14) & 0x3;
	bool     clear  = (d0 >> 18) & 1;

	if (method == TBL_LUTREAD_MAC) {
		struct hw_l2 key = { .present = true, .a = regfile[RTL837x_TBL_DATA_IN_A],
				     .b = regfile[RTL837x_TBL_DATA_IN_B], .c = regfile[RTL837x_TBL_DATA_IN_C] };
		uint8_t mac[6]; uint16_t vid;
		l2_key(&key, mac, &vid);
		struct hw_l2 *e = (struct hw_l2 *)hw_l2_find(mac, vid);
		if (write) {
			if (clear) {
				if (e)
					e->present = false;
				l2_out(0, 0);
				return;
			}
			if (!e) {
				e = l2_free();
				e->idx = l2_hash(mac, vid);
			}
			e->present = true;
			e->a = key.a; e->b = key.b; e->c = key.c;
			l2_out(e, e->idx);
			return;
		}
		l2_out(e, e ? e->idx : 0);
		return;
	}

	/* next-entry reads: the first present entry at or after idx, wrapping */
	l2_sort();
	int n = hw_l2_count();
	struct hw_l2 *e = 0;
	for (int i = 0; i < n; i++)
		if (l2[i].idx >= idx) { e = &l2[i]; break; }
	if (!e && n)
		e = &l2[0];
	if (write) {
		/* a write by index replaces the entry that sits there */
		for (int i = 0; i < n; i++)
			if (l2[i].idx == idx) { e = &l2[i]; break; }
		if (!e)
			e = l2_free();
		e->present = (regfile[RTL837x_TBL_DATA_IN_B] >> 29) & 1;
		e->idx = idx;
		e->a = regfile[RTL837x_TBL_DATA_IN_A];
		e->b = regfile[RTL837x_TBL_DATA_IN_B];
		e->c = regfile[RTL837x_TBL_DATA_IN_C];
		return;
	}
	l2_out(e, e ? e->idx : idx);
}

static void flush_exec(uint32_t ctrl)
{
	uint16_t mask = ctrl & 0x3ff;
	bool dynamic_only = regfile[RTL837x_L2_TBL_FLUSH_CNF] == 0;
	for (int i = 0; i < HW_L2_MAX; i++) {
		uint8_t mac[6]; uint16_t vid, p; bool valid, st, mc;
		if (!l2[i].present)
			continue;
		hw_l2_decode(&l2[i], mac, &vid, &p, &valid, &st, &mc);
		if (mc || (st && dynamic_only))
			continue;
		if ((mask >> p) & 1)
			l2[i].present = false;
	}
}

static void stat_exec(uint32_t v)
{
	uint8_t cnt  = (uint8_t)(((v >> 8) & 0xff) << 3) | ((v >> 5) & 0x7);
	uint8_t port = (v >> 1) & 0xf;
	uint64_t c = counters[port][cnt & 0x3f];
	regfile[RTL837X_STAT_V_HIGH] = (uint32_t)(c >> 32);
	regfile[RTL837X_STAT_V_LOW]  = (uint32_t)c;
}

static void reg_store(uint16_t addr, uint32_t v)
{
	hw_writes++;
	regfile[addr] = v;
	switch (addr) {
	case RTL836x_TA_CTRL_REG:
		/* the CVLAN commands share this control register with the L2 ones
		 * but carry their words in a different layout: plain store. The RTL836x
		 * writes its 16-bit values into the high half of the 32-bit register
		 * (see the asymmetric access in rtl836x_l2_read), so the command is
		 * taken from there too. */
		switch (v >> 16 & 0x1f) {
		case RTL836x_TA_CTRL_L2_WRITE:
			l2_8370_store();
			break;
		case RTL836x_TA_CTRL_L2_READ_ADDR:
			l2_8370_read_addr();
			break;
		case RTL836x_TA_CTRL_L2_READ_MAC:
			l2_8370_read_key();
			break;
		}
		break;
	case RTL837X_TBL_CTRL:
		if (v & TBL_EXECUTE) {
			table_exec(v);
			regfile[addr] = v & ~(uint32_t)TBL_EXECUTE;
		}
		break;
	case RTL837x_L2_TBL_FLUSH_CTRL:
		if (v & L2_TBL_FLUSH_EXEC) {
			flush_exec(v);
			regfile[addr] = v & ~(uint32_t)L2_TBL_FLUSH_EXEC;
		}
		break;
	case RTL837X_STAT_GET:
		if (v & 1) {
			stat_exec(v);
			regfile[addr] = v & ~1u;
		}
		break;
	}
}

static uint32_t sfr_word(void)
{
	return ((uint32_t)SFR_DATA_24 << 24) | ((uint32_t)SFR_DATA_16 << 16) | ((uint32_t)SFR_DATA_8 << 8) | SFR_DATA_0;
}

static void sfr_load(uint32_t v)
{
	SFR_DATA_24 = v >> 24; SFR_DATA_16 = v >> 16; SFR_DATA_8 = v >> 8; SFR_DATA_0 = v;
}

void reg_read(uint16_t addr)
{
	hw_reads++;
	sfr_load(regfile[addr]);
}

void reg_read_m(uint16_t addr)
{
	reg_read(addr);
	sfr_data[0] = SFR_DATA_24; sfr_data[1] = SFR_DATA_16; sfr_data[2] = SFR_DATA_8; sfr_data[3] = SFR_DATA_0;
}

void reg_write(uint16_t addr)
{
	reg_store(addr, sfr_word());
}

void reg_write_m(uint16_t addr)
{
	sfr_load(((uint32_t)sfr_data[0] << 24) | ((uint32_t)sfr_data[1] << 16) | ((uint32_t)sfr_data[2] << 8) | sfr_data[3]);
	reg_write(addr);
}

void reg_bit_set(uint16_t addr, char bit)
{
	reg_read_m(addr);
	sfr_data[3 - (bit >> 3)] |= 1 << (bit & 7);
	reg_write_m(addr);
}

void reg_bit_clear(uint16_t addr, char bit)
{
	reg_read_m(addr);
	sfr_data[3 - (bit >> 3)] &= ~(1 << (bit & 7));
	reg_write_m(addr);
}

uint8_t reg_bit_test(uint16_t addr, char bit)
{
	reg_read_m(addr);
	return (sfr_data[3 - (bit >> 3)] >> (bit & 7)) & 1;
}

void sfr_mask_data(uint8_t n, uint8_t mask, uint8_t set)
{
	sfr_data[3 - n] = (sfr_data[3 - n] & ~mask) | set;
}
