#include <stdint.h>
#include "rtl837x_common.h"
#include "rtl837x_sfr.h"
#include "rtl837x_regs.h"
#include "rtl837x_acl.h"

#pragma codeseg BANK3
#pragma constseg BANK3

#define ACL_PORTS	((uint16_t)((1 << CPU_PORT) - 1))
#define ACL_ALL		((uint16_t)((1 << (CPU_PORT + 1)) - 1))
#define ACL_NONE	0xff
#define ACL_TBL_Y	0x80
#define ACL_ACT_CTRL_OFF	0xff
#define ACL_INFO_TMPL	0x0007
#define ACL_INFO_PORT_SHIFT	11
#define ACL_SLOT_INFO	8
#define ACL_RANGE_KINDS	3
#define ACL_RANGE_REG_STEP	4
#define ACL_RULE_WORDS	5
#define ACL_ACT_WORDS	3

extern __xdata uint8_t sfr_data[4];

__xdata uint16_t acl_kv[ACL_KEYS];
__xdata uint16_t acl_km[ACL_KEYS];
__xdata struct acl_req acl_req[ACL_REQS];
__xdata uint8_t  acl_nreq;
__xdata uint16_t acl_info_v;
__xdata uint16_t acl_info_m;
__xdata uint16_t acl_in_pmask;
__xdata struct acl_act_entry acl_act;
__xdata uint16_t acl_ctrl;
__xdata uint16_t acl_unmatch_drop;
__xdata uint8_t  acl_rule_tmpl[ACL_HW_RULES];
__xdata uint8_t  acl_used[ACL_HW_RULES / 8];
__xdata uint16_t acl_ra;
__xdata uint32_t acl_rv;
__xdata uint16_t acl_vk;
__xdata uint16_t acl_mk;
__xdata struct acl_req acl_rq;

static __xdata struct acl_rule_entry acl_y;
static __xdata struct acl_rule_entry acl_x;
static __xdata struct acl_rule_entry acl_zero;
static __xdata struct acl_range_pool acl_range[ACL_RANGE_KINDS];

static __code const uint8_t acl_key_fts[ACL_KEYS] = {
	ACL_FT_DMAC0, ACL_FT_DMAC1, ACL_FT_DMAC2, ACL_FT_SMAC0, ACL_FT_SMAC1, ACL_FT_SMAC2,
	ACL_FT_ETHERTYPE, ACL_FT_STAG, ACL_FT_CTAG,
	ACL_FT_SIP0, ACL_FT_SIP1, ACL_FT_DIP0, ACL_FT_DIP1,
	ACL_FT_IPTOSPROTO, ACL_FT_L4SPORT, ACL_FT_L4DPORT,
	ACL_FT_SEL0, ACL_FT_SEL0 + 1, ACL_FT_SEL0 + 2, ACL_FT_SEL0 + 3,
	ACL_FT_SEL0 + 4, ACL_FT_SEL0 + 5, ACL_FT_SEL0 + 6, ACL_FT_SEL0 + 7,
	ACL_FT_SEL0 + 8, ACL_FT_SEL0 + 9, ACL_FT_SEL0 + 10, ACL_FT_SEL0 + 11,
	ACL_FT_SEL0 + 12, ACL_FT_SEL0 + 13, ACL_FT_SEL0 + 14, ACL_FT_SEL0 + 15,
	ACL_FT_FIELD_VALID,
};

static __code const uint8_t acl_tmpl[ACL_TEMPLATES][8] = {
	{ACL_FT_DMAC0, ACL_FT_DMAC1, ACL_FT_DMAC2, ACL_FT_SMAC0,
	 ACL_FT_SMAC1, ACL_FT_SMAC2, ACL_FT_ETHERTYPE, ACL_FT_CTAG},
	{ACL_FT_SIP0, ACL_FT_SIP1, ACL_FT_DIP0, ACL_FT_DIP1,
	 ACL_FT_IPTOSPROTO, ACL_FT_L4SPORT, ACL_FT_L4DPORT, ACL_FT_CTAG},
	{ACL_FT_VIDRANGE, ACL_FT_IPRANGE, ACL_FT_PORTRANGE, ACL_FT_FIELD_VALID,
	 ACL_FT_SEL0, ACL_FT_SEL0 + 1, ACL_FT_SEL0 + 2, ACL_FT_SEL0 + 3},
	{ACL_FT_SEL0 + 4, ACL_FT_SEL0 + 5, ACL_FT_SEL0 + 6, ACL_FT_SEL0 + 7,
	 ACL_FT_SEL0 + 8, ACL_FT_SEL0 + 9, ACL_FT_SEL0 + 10, ACL_FT_SEL0 + 11},
	{ACL_FT_SEL0 + 12, ACL_FT_SEL0 + 13, ACL_FT_SEL0 + 14, ACL_FT_SEL0 + 15,
	 ACL_FT_STAG, ACL_FT_CTAG, ACL_FT_ETHERTYPE, ACL_FT_IPTOSPROTO},
};

static __code const uint16_t acl_range_base[ACL_RANGE_KINDS] = {
	RTL837X_ACL_RNG_VID, RTL837X_ACL_RNG_IP, RTL837X_ACL_RNG_PORT
};
static __code const uint8_t acl_range_stride[ACL_RANGE_KINDS] = {4, 12, 8};

void acl_reg(void) __banked
{
	__xdata uint8_t *p = (__xdata uint8_t *)&acl_rv;

	REG_WRITE(acl_ra, p[3], p[2], p[1], p[0]);
}

static void acl_tbl_wait(void)
{
	uint8_t guard = 0;
	do {
		reg_read_m(RTL837X_TBL_CTRL);
	} while ((sfr_data[3] & TBL_EXECUTE) && ++guard);
}

static void acl_tbl_write(uint8_t sel, uint8_t words, uint8_t addr, __xdata uint16_t *src) __reentrant
{
	uint8_t i;

	acl_tbl_wait();
	for (i = 0; i < words; i++)
		REG_WRITE(RTL837x_TBL_DATA_IN_A + (i << 2), src[2 * i + 1] >> 8,
			  src[2 * i + 1], src[2 * i] >> 8, src[2 * i]);
	REG_WRITE(RTL837X_TBL_CTRL, 0, addr, sel, TBL_WRITE | TBL_EXECUTE);
	acl_tbl_wait();
}

static void acl_entry_write(uint8_t idx, __xdata uint16_t *x, __xdata uint16_t *y) __reentrant
{
	acl_tbl_write(TBL_ACL_RULE, ACL_RULE_WORDS, idx, x);
	acl_tbl_write(TBL_ACL_RULE, ACL_RULE_WORDS, ACL_TBL_Y | idx, y);
}

static uint8_t acl_any(void)
{
	for (uint8_t i = 0; i < ACL_HW_RULES / 8; i++)
		if (acl_used[i])
			return 1;
	return 0;
}

static void acl_ports_update(void)
{
	if (acl_any() || acl_unmatch_drop) {
		REG_SET(RTL837X_ACL_UNMATCH_PERMIT, ACL_PORTS & ~acl_unmatch_drop);
		REG_SET(RTL837X_ACL_PORT_EN, ACL_PORTS);
	} else {
		REG_SET(RTL837X_ACL_PORT_EN, 0);
		REG_SET(RTL837X_ACL_UNMATCH_PERMIT, 0);
	}
}

uint8_t acl_key(uint8_t ft) __banked
{
	if (ft <= ACL_FT_CTAG)
		return ft;
	if (ft >= ACL_FT_SIP0 && ft <= ACL_FT_DIP1)
		return ft - ACL_FT_SIP0 + 9;
	if (ft >= ACL_FT_IPTOSPROTO && ft <= ACL_FT_L4DPORT)
		return ft - ACL_FT_IPTOSPROTO + 13;
	if (ft >= ACL_FT_SEL0 && ft < ACL_FT_SEL0 + ACL_SELECTORS)
		return ft - ACL_FT_SEL0 + 16;
	if (ft == ACL_FT_FIELD_VALID)
		return 32;
	return ACL_NONE;
}

static uint8_t acl_slot(uint8_t tmpl, uint8_t ft) __reentrant
{
	for (uint8_t s = 0; s < 8; s++)
		if (acl_tmpl[tmpl][s] == ft)
			return s;
	return ACL_NONE;
}

static uint8_t acl_alt(uint8_t k)
{
	uint8_t ft = acl_key_fts[k];

	if (ft == ACL_FT_SIP1 || ft == ACL_FT_DIP1)
		k--;
	if ((ft == ACL_FT_CTAG || ft == ACL_FT_STAG) && acl_km[k] != 0x0fff)
		return ACL_NONE;
	for (uint8_t r = 0; r < acl_nreq; r++)
		if (acl_req[r].key == k)
			return r;
	return ACL_NONE;
}

static uint8_t acl_fits(uint8_t tmpl, uint8_t ranges) __reentrant
{
	uint8_t k, r;

	for (k = 0; k < ACL_KEYS; k++) {
		if (!acl_km[k] || acl_slot(tmpl, acl_key_fts[k]) != ACL_NONE)
			continue;
		r = ranges ? acl_alt(k) : ACL_NONE;
		if (r == ACL_NONE || acl_slot(tmpl, ACL_FT_VIDRANGE + acl_req[r].kind) == ACL_NONE)
			return 0;
	}
	for (r = 0; r < acl_nreq; r++)
		if (acl_req[r].key == ACL_NONE
		    && acl_slot(tmpl, ACL_FT_VIDRANGE + acl_req[r].kind) == ACL_NONE)
			return 0;
	return 1;
}

static void acl_put(uint8_t s, uint16_t val, uint16_t mask) __reentrant
{
	__xdata uint16_t *y;
	__xdata uint16_t *x;

	if (s == ACL_SLOT_INFO) {
		y = &acl_y.info;
		x = &acl_x.info;
	} else {
		y = &acl_y.field[s];
		x = &acl_x.field[s];
	}
	val &= mask;
	*y = (*y & ~mask) | val;
	*x = (*x & ~mask) | (val ^ mask);
}

static void acl_range_addr(uint8_t kind, uint8_t slot) __reentrant
{
	acl_ra = acl_range_base[kind];
	while (slot--)
		acl_ra += acl_range_stride[kind];
}

static void acl_range_free(uint8_t idx) __reentrant
{
	uint8_t kind, slot;

	for (kind = 0; kind < ACL_RANGE_KINDS; kind++) {
		for (slot = 0; slot < ACL_RANGES; slot++) {
			if (acl_range[kind].owner[slot] != idx + 1)
				continue;
			acl_range[kind].owner[slot] = 0;
			acl_range_addr(kind, slot);
			acl_rv = 0;
			acl_reg();
			if (kind == ACL_RANGE_VID)
				continue;
			acl_ra += ACL_RANGE_REG_STEP;
			acl_reg();
			if (kind == ACL_RANGE_IP) {
				acl_ra += ACL_RANGE_REG_STEP;
				acl_reg();
			}
		}
	}
}

static uint8_t acl_range_alloc(uint8_t idx, __xdata struct acl_req *req) __reentrant
{
	uint8_t kind = req->kind;
	uint8_t slot;
	uint16_t lo, hi;

	for (slot = 0; slot < ACL_RANGES; slot++)
		if (!acl_range[kind].owner[slot])
			break;
	if (slot == ACL_RANGES)
		return 0;
	acl_range[kind].owner[slot] = idx + 1;
	acl_range[kind].bits |= (uint16_t)1 << slot;
	acl_range_addr(kind, slot);
	if (kind == ACL_RANGE_VID) {
		lo = req->lo;
		lo <<= 2;
		lo |= req->type;
		hi = req->hi;
		lo |= hi << 14;
		hi >>= 2;
		((__xdata uint16_t *)&acl_rv)[0] = lo;
		((__xdata uint16_t *)&acl_rv)[1] = hi;
		acl_reg();
		return 1;
	}
	acl_ra += ACL_RANGE_REG_STEP;
	if (kind == ACL_RANGE_IP) {
		acl_rv = req->hi;
		acl_reg();
		acl_ra += ACL_RANGE_REG_STEP;
		acl_rv = req->lo;
		acl_reg();
		acl_ra -= ACL_RANGE_REG_STEP;
	} else {
		acl_rv = req->hi;
		acl_rv <<= 16;
		acl_rv |= req->lo;
		acl_reg();
	}
	acl_ra -= ACL_RANGE_REG_STEP;
	acl_rv = req->type;
	acl_reg();
	return 1;
}

static uint8_t acl_range_check(uint8_t idx, uint8_t tmpl) __reentrant
{
	uint8_t r, kind, slot;
	__xdata struct acl_req *req;

	acl_range[0].need = acl_range[1].need = acl_range[2].need = 0;
	for (r = 0; r < acl_nreq; r++) {
		req = &acl_req[r];
		if (req->key != ACL_NONE && acl_slot(tmpl, acl_key_fts[req->key]) != ACL_NONE)
			continue;
		acl_range[req->kind].need++;
	}
	for (kind = 0; kind < ACL_RANGE_KINDS; kind++)
		for (slot = 0; slot < ACL_RANGES && acl_range[kind].need; slot++)
			if (!acl_range[kind].owner[slot]
			    || acl_range[kind].owner[slot] == idx + 1)
				acl_range[kind].need--;
	return !(acl_range[0].need | acl_range[1].need | acl_range[2].need);
}

static uint8_t acl_encode(uint8_t idx, uint8_t tmpl) __reentrant
{
	__xdata uint16_t *y = (__xdata uint16_t *)&acl_y;
	__xdata uint16_t *x = (__xdata uint16_t *)&acl_x;
	__xdata struct acl_req *req;
	uint8_t s, k;
	uint16_t ports;

	for (s = 0; s < 2 * ACL_RULE_WORDS; s++)
		y[s] = x[s] = 0xffff;
	acl_range[0].bits = acl_range[1].bits = acl_range[2].bits = 0;

	for (s = 0; s < ACL_SLOT_INFO; s++) {
		k = acl_key(acl_tmpl[tmpl][s]);
		if (k == ACL_NONE || !acl_km[k])
			continue;
		acl_put(s, acl_kv[k], acl_km[k]);
	}
	for (k = 0; k < acl_nreq; k++) {
		req = &acl_req[k];
		if (req->key != ACL_NONE && acl_slot(tmpl, acl_key_fts[req->key]) != ACL_NONE)
			continue;
		if (!acl_range_alloc(idx, req)) {
			acl_range_free(idx);
			return 0;
		}
	}
	for (k = 0; k < ACL_RANGE_KINDS; k++) {
		if (!acl_range[k].bits)
			continue;
		acl_put(acl_slot(tmpl, ACL_FT_VIDRANGE + k), acl_range[k].bits, acl_range[k].bits);
	}

	acl_put(ACL_SLOT_INFO, tmpl, ACL_INFO_TMPL);
	acl_put(ACL_SLOT_INFO, acl_info_v, acl_info_m);
	ports = ~(acl_in_pmask ? acl_in_pmask : ACL_PORTS) & ACL_ALL;
	acl_y.info &= ~(uint16_t)(ports << ACL_INFO_PORT_SHIFT);
	acl_y.info_hi &= ~(ports >> (16 - ACL_INFO_PORT_SHIFT));
	return 1;
}

void acl_match_begin(void) __banked
{
	for (uint8_t k = 0; k < ACL_KEYS; k++)
		acl_kv[k] = acl_km[k] = 0;
	acl_nreq = 0;
	acl_info_v = acl_info_m = 0;
	acl_in_pmask = 0;
	acl_act.vlan = acl_act.fwd_qos = acl_act.misc = 0;
	acl_ctrl = 0;
}

void acl_match_key(uint8_t ft) __banked __reentrant
{
	uint8_t k = acl_key(ft);

	acl_kv[k] = (acl_kv[k] & ~acl_mk) | (acl_vk & acl_mk);
	acl_km[k] |= acl_mk;
}

uint8_t acl_match_range(uint8_t key) __banked
{
	__xdata struct acl_req *req;

	if (acl_nreq == ACL_REQS)
		return 0;
	req = &acl_req[acl_nreq++];
	req->kind = acl_rq.kind;
	req->type = acl_rq.type;
	req->key = key;
	req->lo = acl_rq.lo;
	req->hi = acl_rq.hi;
	return 1;
}

uint8_t acl_rule_set(uint8_t idx) __banked __reentrant
{
	uint8_t ranges, tmpl, i;

	for (ranges = 0; ranges < 2; ranges++)
		for (tmpl = 0; tmpl < ACL_TEMPLATES; tmpl++)
			if (acl_fits(tmpl, ranges))
				goto found;
	return ACL_ERR_TEMPLATE;
found:
	if (!acl_range_check(idx, tmpl))
		return ACL_ERR_RANGE;
	if (ACL_IS_USED(idx))
		acl_rule_clear(idx);

	if (!acl_any()) {
		for (i = 0; i < ACL_TEMPLATES; i++) {
			REG_WRITE(RTL837X_ACL_TEMPLATE0_F0_3 + (i << 3), acl_tmpl[i][3],
				  acl_tmpl[i][2], acl_tmpl[i][1], acl_tmpl[i][0]);
			REG_WRITE(RTL837X_ACL_TEMPLATE0_F4_7 + (i << 3), acl_tmpl[i][7],
				  acl_tmpl[i][6], acl_tmpl[i][5], acl_tmpl[i][4]);
		}
	}
	if (!acl_encode(idx, tmpl))
		return ACL_ERR_RANGE;

	acl_tbl_write(TBL_ACL_ACT, ACL_ACT_WORDS, idx, (__xdata uint16_t *)&acl_act);
	REG_SET(RTL837X_ACL_ACT_CTRL + ((uint16_t)idx << 2), acl_ctrl);
	acl_entry_write(idx, (__xdata uint16_t *)&acl_x, (__xdata uint16_t *)&acl_y);

	ACL_SET_USED(idx);
	acl_rule_tmpl[idx] = tmpl;
	acl_ports_update();
	return 0;
}

void acl_rule_clear(uint8_t idx) __banked __reentrant
{
	acl_entry_write(idx, (__xdata uint16_t *)&acl_zero, (__xdata uint16_t *)&acl_zero);
	acl_tbl_write(TBL_ACL_ACT, ACL_ACT_WORDS, idx, (__xdata uint16_t *)&acl_zero);
	REG_SET(RTL837X_ACL_ACT_CTRL + ((uint16_t)idx << 2), ACL_ACT_CTRL_OFF);
	ACL_CLR_USED(idx);
	acl_range_free(idx);
	acl_ports_update();
}

void acl_unmatch_set(void) __banked
{
	acl_unmatch_drop &= ACL_PORTS;
	acl_ports_update();
}

void acl_poll(void) __banked
{
	reg_read_m(RTL837X_ISR_INT_MISC);
	if (!(sfr_data[2] & (ISR_MISC_ACL >> 8)))
		return;
	REG_SET(RTL837X_ISR_INT_MISC, ISR_MISC_ACL);
	print_string("ACL interrupt\n");
}

void acl_counter_reset(void) __banked
{
	REG_SET(RTL837X_ACL_LOG_RST, 0xffffffff);
	REG_SET(RTL837X_ACL_LOG_RST, 0);
}

uint8_t acl_rule_is_and(uint8_t idx) __banked
{
	if (!ACL_IS_USED(idx))
		return 0;
	reg_read_m(RTL837X_ACL_ACT_CTRL + ((uint16_t)idx << 2));
	return !(sfr_data[0] | sfr_data[1] | sfr_data[2] | sfr_data[3]);
}
