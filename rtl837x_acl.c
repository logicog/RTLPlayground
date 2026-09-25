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

extern __xdata uint8_t sfr_data[4];

__xdata uint16_t acl_kv[ACL_KEYS];
__xdata uint16_t acl_km[ACL_KEYS];
__xdata struct acl_req acl_req[ACL_REQS];
__xdata uint8_t  acl_nreq;
__xdata uint16_t acl_info_v;
__xdata uint16_t acl_info_m;
__xdata uint16_t acl_in_pmask;
__xdata uint32_t acl_act[3];
__xdata uint16_t acl_ctrl;
__xdata uint16_t acl_unmatch_drop;
__xdata uint8_t  acl_rule_tmpl[ACL_HW_RULES];
__xdata uint8_t  acl_used[ACL_HW_RULES / 8];
__xdata uint16_t acl_ra;
__xdata uint32_t acl_rv;
__xdata uint16_t acl_vk;
__xdata uint16_t acl_mk;
__xdata struct acl_req acl_rq;

static __xdata uint8_t  acl_rng_owner[3][ACL_RANGES];
static __xdata uint16_t acl_y[10];
static __xdata uint16_t acl_x[10];
static __xdata uint16_t acl_zero[10];
static __xdata uint16_t acl_rbits[3];
static __xdata uint16_t acl_v;
static __xdata uint16_t acl_m;
static __xdata uint16_t acl_t1;
static __xdata uint16_t acl_t2;
static __xdata uint8_t  acl_t;
static __xdata uint8_t  acl_idx;
static __xdata uint8_t  acl_i;
static __xdata uint8_t  acl_j;
static __xdata uint8_t  acl_k;
static __xdata uint8_t  acl_s;
static __xdata uint8_t  acl_alt_ok;
static __xdata struct acl_req * __xdata acl_q;
static __xdata uint16_t * __xdata acl_h;
static __xdata uint16_t * __xdata acl_h2;

static __code const uint8_t acl_key_fts[ACL_KEYS] = {
	ACL_FT_DMAC0, ACL_FT_DMAC1, ACL_FT_DMAC2, ACL_FT_SMAC0, ACL_FT_SMAC1, ACL_FT_SMAC2,
	ACL_FT_ETHERTYPE, ACL_FT_STAG, ACL_FT_CTAG,
	ACL_FT_SIP0, ACL_FT_SIP1, ACL_FT_DIP0, ACL_FT_DIP1,
	ACL_FT_IPTOSPROTO, ACL_FT_L4SPORT, ACL_FT_L4DPORT,
	ACL_FT_SEL0, ACL_FT_SEL0 + 1, ACL_FT_SEL0 + 2, ACL_FT_SEL0 + 3,
	ACL_FT_SEL0 + 4, ACL_FT_SEL0 + 5, ACL_FT_SEL0 + 6, ACL_FT_SEL0 + 7,
	ACL_FT_SEL0 + 8, ACL_FT_SEL0 + 9, ACL_FT_SEL0 + 10, ACL_FT_SEL0 + 11,
	ACL_FT_SEL0 + 12, ACL_FT_SEL0 + 13, ACL_FT_SEL0 + 14, ACL_FT_SEL0 + 15,
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

static __code const uint16_t acl_rng_base[3] = {
	RTL837X_ACL_RNG_VID, RTL837X_ACL_RNG_IP, RTL837X_ACL_RNG_PORT
};
static __code const uint8_t acl_rng_stride[3] = {4, 12, 8};

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

static void acl_tbl_write(void)
{
	acl_tbl_wait();
	for (acl_i = 0; acl_i < acl_j; acl_i++)
		REG_WRITE(RTL837x_TBL_DATA_IN_A + (acl_i << 2), acl_h[2 * acl_i + 1] >> 8,
			  acl_h[2 * acl_i + 1], acl_h[2 * acl_i] >> 8, acl_h[2 * acl_i]);
	REG_WRITE(RTL837X_TBL_CTRL, 0, acl_k, acl_s, TBL_WRITE | TBL_EXECUTE);
	acl_tbl_wait();
}

static void acl_entry_write(void)
{
	acl_s = TBL_ACL_RULE;
	acl_j = 5;
	acl_k = acl_idx;
	acl_tbl_write();
	acl_k = 0x80 | acl_idx;
	acl_h = acl_h2;
	acl_tbl_write();
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
	return ACL_NONE;
}

static uint8_t acl_slot(uint8_t ft)
{
	for (uint8_t s = 0; s < 8; s++)
		if (acl_tmpl[acl_t][s] == ft)
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

static uint8_t acl_fits(void)
{
	for (acl_i = 0; acl_i < ACL_KEYS; acl_i++) {
		if (!acl_km[acl_i] || acl_slot(acl_key_fts[acl_i]) != ACL_NONE)
			continue;
		acl_j = acl_alt_ok ? acl_alt(acl_i) : ACL_NONE;
		if (acl_j == ACL_NONE || acl_slot(ACL_FT_VIDRANGE + acl_req[acl_j].kind) == ACL_NONE)
			return 0;
	}
	for (acl_i = 0; acl_i < acl_nreq; acl_i++)
		if (acl_req[acl_i].key == ACL_NONE
		    && acl_slot(ACL_FT_VIDRANGE + acl_req[acl_i].kind) == ACL_NONE)
			return 0;
	return 1;
}

static void acl_put(uint8_t s)
{
	acl_t2 = ~acl_m;
	acl_t1 = acl_v & acl_m;
	acl_y[s] &= acl_t2;
	acl_y[s] |= acl_t1;
	acl_t1 ^= acl_m;
	acl_x[s] &= acl_t2;
	acl_x[s] |= acl_t1;
}

static void acl_rng_addr(void)
{
	acl_ra = acl_rng_base[acl_k];
	for (acl_i = 0; acl_i < acl_j; acl_i++)
		acl_ra += acl_rng_stride[acl_k];
}

static void acl_rng_free(void)
{
	for (acl_k = 0; acl_k < 3; acl_k++) {
		for (acl_j = 0; acl_j < ACL_RANGES; acl_j++) {
			if (acl_rng_owner[acl_k][acl_j] != acl_idx + 1)
				continue;
			acl_rng_owner[acl_k][acl_j] = 0;
			acl_rng_addr();
			acl_rv = 0;
			acl_reg();
			if (acl_k == ACL_RNG_VID)
				continue;
			acl_ra += 4;
			acl_reg();
			if (acl_k == ACL_RNG_IP) {
				acl_ra += 4;
				acl_reg();
			}
		}
	}
}

static uint8_t acl_rng_alloc(void)
{
	acl_k = acl_q->kind;
	for (acl_j = 0; acl_j < ACL_RANGES; acl_j++)
		if (!acl_rng_owner[acl_k][acl_j])
			break;
	if (acl_j == ACL_RANGES)
		return 0;
	acl_rng_owner[acl_k][acl_j] = acl_idx + 1;
	acl_rbits[acl_k] |= (uint16_t)1 << acl_j;
	acl_rng_addr();
	if (acl_k == ACL_RNG_VID) {
		acl_t1 = acl_q->lo;
		acl_t1 <<= 2;
		acl_t1 |= acl_q->type;
		acl_t2 = acl_q->hi;
		acl_v = acl_t2 << 14;
		acl_t1 |= acl_v;
		acl_t2 >>= 2;
		((__xdata uint16_t *)&acl_rv)[0] = acl_t1;
		((__xdata uint16_t *)&acl_rv)[1] = acl_t2;
		acl_reg();
		return 1;
	}
	acl_ra += 4;
	if (acl_k == ACL_RNG_IP) {
		acl_rv = acl_q->hi;
		acl_reg();
		acl_ra += 4;
		acl_rv = acl_q->lo;
		acl_reg();
		acl_ra -= 4;
	} else {
		acl_rv = acl_q->hi;
		acl_rv <<= 16;
		acl_rv |= acl_q->lo;
		acl_reg();
	}
	acl_ra -= 4;
	acl_rv = acl_q->type;
	acl_reg();
	return 1;
}

static uint8_t acl_encode(void)
{
	for (acl_s = 0; acl_s < 10; acl_s++)
		acl_y[acl_s] = acl_x[acl_s] = 0xffff;
	acl_rbits[0] = acl_rbits[1] = acl_rbits[2] = 0;

	for (acl_s = 0; acl_s < 8; acl_s++) {
		acl_k = acl_key(acl_tmpl[acl_t][acl_s]);
		if (acl_k == ACL_NONE || !acl_km[acl_k])
			continue;
		acl_v = acl_kv[acl_k];
		acl_m = acl_km[acl_k];
		acl_put(acl_s);
	}
	for (acl_s = 0; acl_s < acl_nreq; acl_s++) {
		acl_q = &acl_req[acl_s];
		if (acl_q->key != ACL_NONE && acl_slot(acl_key_fts[acl_q->key]) != ACL_NONE)
			continue;
		if (!acl_rng_alloc()) {
			acl_rng_free();
			return 0;
		}
	}
	for (acl_s = 0; acl_s < 3; acl_s++) {
		if (!acl_rbits[acl_s])
			continue;
		acl_v = acl_m = acl_rbits[acl_s];
		acl_put(acl_slot(ACL_FT_VIDRANGE + acl_s));
	}

	acl_v = acl_t;
	acl_m = 0x0007;
	acl_put(8);
	acl_v = acl_info_v;
	acl_m = acl_info_m;
	acl_put(8);
	acl_v = ~(acl_in_pmask ? acl_in_pmask : ACL_PORTS) & ACL_ALL;
	acl_y[8] &= ~(uint16_t)(acl_v << 11);
	acl_y[9] &= ~(acl_v >> 5);
	return 1;
}

void acl_match_begin(void) __banked
{
	for (uint8_t k = 0; k < ACL_KEYS; k++)
		acl_kv[k] = acl_km[k] = 0;
	acl_nreq = 0;
	acl_info_v = acl_info_m = 0;
	acl_in_pmask = 0;
	acl_act[0] = acl_act[1] = acl_act[2] = 0;
	acl_ctrl = 0;
}

void acl_match_key(uint8_t ft) __banked
{
	acl_k = acl_key(ft);
	acl_kv[acl_k] = (acl_kv[acl_k] & ~acl_mk) | (acl_vk & acl_mk);
	acl_km[acl_k] |= acl_mk;
}

uint8_t acl_match_range(uint8_t key) __banked
{
	if (acl_nreq == ACL_REQS)
		return 0;
	acl_q = &acl_req[acl_nreq++];
	acl_q->kind = acl_rq.kind;
	acl_q->type = acl_rq.type;
	acl_q->key = key;
	acl_q->lo = acl_rq.lo;
	acl_q->hi = acl_rq.hi;
	return 1;
}

uint8_t acl_rule_set(uint8_t idx) __banked
{
	acl_idx = idx;
	if (acl_used[acl_idx >> 3] & (1 << (acl_idx & 7)))
		acl_rule_clear(acl_idx);

	for (acl_alt_ok = 0; acl_alt_ok < 2; acl_alt_ok++)
		for (acl_t = 0; acl_t < ACL_TEMPLATES; acl_t++)
			if (acl_fits())
				goto found;
	return ACL_ERR_TEMPLATE;
found:

	if (!acl_any()) {
		for (acl_s = 0; acl_s < ACL_TEMPLATES; acl_s++) {
			REG_WRITE(RTL837X_ACL_TEMPLATE0_F0_3 + (acl_s << 3), acl_tmpl[acl_s][3],
				  acl_tmpl[acl_s][2], acl_tmpl[acl_s][1], acl_tmpl[acl_s][0]);
			REG_WRITE(RTL837X_ACL_TEMPLATE0_F4_7 + (acl_s << 3), acl_tmpl[acl_s][7],
				  acl_tmpl[acl_s][6], acl_tmpl[acl_s][5], acl_tmpl[acl_s][4]);
		}
	}
	if (!acl_encode())
		return ACL_ERR_RANGE;

	acl_s = TBL_ACL_ACT;
	acl_j = 3;
	acl_k = acl_idx;
	acl_h = (__xdata uint16_t *)acl_act;
	acl_tbl_write();
	REG_SET(RTL837X_ACL_ACT_CTRL + ((uint16_t)acl_idx << 2), acl_ctrl);
	acl_h = acl_x;
	acl_h2 = acl_y;
	acl_entry_write();

	acl_used[acl_idx >> 3] |= 1 << (acl_idx & 7);
	acl_rule_tmpl[acl_idx] = acl_t;
	acl_ports_update();
	return 0;
}

void acl_rule_clear(uint8_t idx) __banked
{
	acl_idx = idx;
	acl_h = acl_h2 = acl_zero;
	acl_entry_write();
	acl_s = TBL_ACL_ACT;
	acl_j = 3;
	acl_k = acl_idx;
	acl_h = acl_zero;
	acl_tbl_write();
	REG_SET(RTL837X_ACL_ACT_CTRL + ((uint16_t)acl_idx << 2), 0xff);
	acl_used[acl_idx >> 3] &= ~(1 << (acl_idx & 7));
	acl_rng_free();
	acl_ports_update();
}

void acl_unmatch_set(void) __banked
{
	acl_unmatch_drop &= ACL_PORTS;
	acl_ports_update();
}

void acl_counter_reset(void) __banked
{
	REG_SET(RTL837X_ACL_LOG_RST, 0xffffffff);
	REG_SET(RTL837X_ACL_LOG_RST, 0);
}
