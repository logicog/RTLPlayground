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
static __xdata uint16_t acl_put_val;
static __xdata uint16_t acl_put_mask;
static __xdata uint16_t acl_tmp;
static __xdata uint16_t acl_tmp2;
static __xdata uint16_t * __xdata acl_py;
static __xdata uint16_t * __xdata acl_px;
static __xdata uint8_t  acl_tmpl_i;
static __xdata uint8_t  acl_idx;
static __xdata uint8_t  acl_i;
static __xdata uint8_t  acl_req_i;
static __xdata uint8_t  acl_slot_i;
static __xdata uint8_t  acl_key_i;
static __xdata uint8_t  acl_range_kind;
static __xdata uint8_t  acl_range_slot;
static __xdata uint8_t  acl_use_ranges;
static __xdata struct acl_req * __xdata acl_cur_req;
static __xdata uint8_t  acl_tbl_sel;
static __xdata uint8_t  acl_tbl_words;
static __xdata uint8_t  acl_tbl_addr;
static __xdata uint16_t * __xdata acl_tbl_src;
static __xdata uint16_t * __xdata acl_tbl_src_y;

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

static void acl_tbl_write(void)
{
	acl_tbl_wait();
	for (acl_i = 0; acl_i < acl_tbl_words; acl_i++)
		REG_WRITE(RTL837x_TBL_DATA_IN_A + (acl_i << 2), acl_tbl_src[2 * acl_i + 1] >> 8,
			  acl_tbl_src[2 * acl_i + 1], acl_tbl_src[2 * acl_i] >> 8, acl_tbl_src[2 * acl_i]);
	REG_WRITE(RTL837X_TBL_CTRL, 0, acl_tbl_addr, acl_tbl_sel, TBL_WRITE | TBL_EXECUTE);
	acl_tbl_wait();
}

static void acl_entry_write(void)
{
	acl_tbl_sel = TBL_ACL_RULE;
	acl_tbl_words = ACL_RULE_WORDS;
	acl_tbl_addr = acl_idx;
	acl_tbl_write();
	acl_tbl_addr = ACL_TBL_Y | acl_idx;
	acl_tbl_src = acl_tbl_src_y;
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
	if (ft == ACL_FT_FIELD_VALID)
		return 32;
	return ACL_NONE;
}

static uint8_t acl_slot(uint8_t ft)
{
	for (uint8_t s = 0; s < 8; s++)
		if (acl_tmpl[acl_tmpl_i][s] == ft)
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
		acl_req_i = acl_use_ranges ? acl_alt(acl_i) : ACL_NONE;
		if (acl_req_i == ACL_NONE || acl_slot(ACL_FT_VIDRANGE + acl_req[acl_req_i].kind) == ACL_NONE)
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
	if (s == ACL_SLOT_INFO) {
		acl_py = &acl_y.info;
		acl_px = &acl_x.info;
	} else {
		acl_py = &acl_y.field[s];
		acl_px = &acl_x.field[s];
	}
	acl_tmp2 = ~acl_put_mask;
	acl_tmp = acl_put_val & acl_put_mask;
	*acl_py &= acl_tmp2;
	*acl_py |= acl_tmp;
	acl_tmp ^= acl_put_mask;
	*acl_px &= acl_tmp2;
	*acl_px |= acl_tmp;
}

static void acl_range_addr(void)
{
	acl_ra = acl_range_base[acl_range_kind];
	for (acl_i = 0; acl_i < acl_range_slot; acl_i++)
		acl_ra += acl_range_stride[acl_range_kind];
}

static void acl_range_free(void)
{
	for (acl_range_kind = 0; acl_range_kind < ACL_RANGE_KINDS; acl_range_kind++) {
		for (acl_range_slot = 0; acl_range_slot < ACL_RANGES; acl_range_slot++) {
			if (acl_range[acl_range_kind].owner[acl_range_slot] != acl_idx + 1)
				continue;
			acl_range[acl_range_kind].owner[acl_range_slot] = 0;
			acl_range_addr();
			acl_rv = 0;
			acl_reg();
			if (acl_range_kind == ACL_RANGE_VID)
				continue;
			acl_ra += ACL_RANGE_REG_STEP;
			acl_reg();
			if (acl_range_kind == ACL_RANGE_IP) {
				acl_ra += ACL_RANGE_REG_STEP;
				acl_reg();
			}
		}
	}
}

static uint8_t acl_range_alloc(void)
{
	acl_range_kind = acl_cur_req->kind;
	for (acl_range_slot = 0; acl_range_slot < ACL_RANGES; acl_range_slot++)
		if (!acl_range[acl_range_kind].owner[acl_range_slot])
			break;
	if (acl_range_slot == ACL_RANGES)
		return 0;
	acl_range[acl_range_kind].owner[acl_range_slot] = acl_idx + 1;
	acl_range[acl_range_kind].bits |= (uint16_t)1 << acl_range_slot;
	acl_range_addr();
	if (acl_range_kind == ACL_RANGE_VID) {
		acl_tmp = acl_cur_req->lo;
		acl_tmp <<= 2;
		acl_tmp |= acl_cur_req->type;
		acl_tmp2 = acl_cur_req->hi;
		acl_put_val = acl_tmp2 << 14;
		acl_tmp |= acl_put_val;
		acl_tmp2 >>= 2;
		((__xdata uint16_t *)&acl_rv)[0] = acl_tmp;
		((__xdata uint16_t *)&acl_rv)[1] = acl_tmp2;
		acl_reg();
		return 1;
	}
	acl_ra += ACL_RANGE_REG_STEP;
	if (acl_range_kind == ACL_RANGE_IP) {
		acl_rv = acl_cur_req->hi;
		acl_reg();
		acl_ra += ACL_RANGE_REG_STEP;
		acl_rv = acl_cur_req->lo;
		acl_reg();
		acl_ra -= ACL_RANGE_REG_STEP;
	} else {
		acl_rv = acl_cur_req->hi;
		acl_rv <<= 16;
		acl_rv |= acl_cur_req->lo;
		acl_reg();
	}
	acl_ra -= ACL_RANGE_REG_STEP;
	acl_rv = acl_cur_req->type;
	acl_reg();
	return 1;
}

static uint8_t acl_range_check(void)
{
	acl_range[0].need = acl_range[1].need = acl_range[2].need = 0;
	for (acl_req_i = 0; acl_req_i < acl_nreq; acl_req_i++) {
		acl_cur_req = &acl_req[acl_req_i];
		if (acl_cur_req->key != ACL_NONE && acl_slot(acl_key_fts[acl_cur_req->key]) != ACL_NONE)
			continue;
		acl_range[acl_cur_req->kind].need++;
	}
	for (acl_range_kind = 0; acl_range_kind < ACL_RANGE_KINDS; acl_range_kind++)
		for (acl_range_slot = 0; acl_range_slot < ACL_RANGES && acl_range[acl_range_kind].need; acl_range_slot++)
			if (!acl_range[acl_range_kind].owner[acl_range_slot]
			    || acl_range[acl_range_kind].owner[acl_range_slot] == acl_idx + 1)
				acl_range[acl_range_kind].need--;
	return !(acl_range[0].need | acl_range[1].need | acl_range[2].need);
}

static uint8_t acl_encode(void)
{
	acl_py = (__xdata uint16_t *)&acl_y;
	acl_px = (__xdata uint16_t *)&acl_x;
	for (acl_slot_i = 0; acl_slot_i < 2 * ACL_RULE_WORDS; acl_slot_i++)
		acl_py[acl_slot_i] = acl_px[acl_slot_i] = 0xffff;
	acl_range[0].bits = acl_range[1].bits = acl_range[2].bits = 0;

	for (acl_slot_i = 0; acl_slot_i < ACL_SLOT_INFO; acl_slot_i++) {
		acl_key_i = acl_key(acl_tmpl[acl_tmpl_i][acl_slot_i]);
		if (acl_key_i == ACL_NONE || !acl_km[acl_key_i])
			continue;
		acl_put_val = acl_kv[acl_key_i];
		acl_put_mask = acl_km[acl_key_i];
		acl_put(acl_slot_i);
	}
	for (acl_req_i = 0; acl_req_i < acl_nreq; acl_req_i++) {
		acl_cur_req = &acl_req[acl_req_i];
		if (acl_cur_req->key != ACL_NONE && acl_slot(acl_key_fts[acl_cur_req->key]) != ACL_NONE)
			continue;
		if (!acl_range_alloc()) {
			acl_range_free();
			return 0;
		}
	}
	for (acl_range_kind = 0; acl_range_kind < ACL_RANGE_KINDS; acl_range_kind++) {
		if (!acl_range[acl_range_kind].bits)
			continue;
		acl_put_val = acl_put_mask = acl_range[acl_range_kind].bits;
		acl_put(acl_slot(ACL_FT_VIDRANGE + acl_range_kind));
	}

	acl_put_val = acl_tmpl_i;
	acl_put_mask = ACL_INFO_TMPL;
	acl_put(ACL_SLOT_INFO);
	acl_put_val = acl_info_v;
	acl_put_mask = acl_info_m;
	acl_put(ACL_SLOT_INFO);
	acl_put_val = ~(acl_in_pmask ? acl_in_pmask : ACL_PORTS) & ACL_ALL;
	acl_y.info &= ~(uint16_t)(acl_put_val << ACL_INFO_PORT_SHIFT);
	acl_y.info_hi &= ~(acl_put_val >> (16 - ACL_INFO_PORT_SHIFT));
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

void acl_match_key(uint8_t ft) __banked
{
	acl_key_i = acl_key(ft);
	acl_kv[acl_key_i] = (acl_kv[acl_key_i] & ~acl_mk) | (acl_vk & acl_mk);
	acl_km[acl_key_i] |= acl_mk;
}

uint8_t acl_match_range(uint8_t key) __banked
{
	if (acl_nreq == ACL_REQS)
		return 0;
	acl_cur_req = &acl_req[acl_nreq++];
	acl_cur_req->kind = acl_rq.kind;
	acl_cur_req->type = acl_rq.type;
	acl_cur_req->key = key;
	acl_cur_req->lo = acl_rq.lo;
	acl_cur_req->hi = acl_rq.hi;
	return 1;
}

uint8_t acl_rule_set(uint8_t idx) __banked
{
	for (acl_use_ranges = 0; acl_use_ranges < 2; acl_use_ranges++)
		for (acl_tmpl_i = 0; acl_tmpl_i < ACL_TEMPLATES; acl_tmpl_i++)
			if (acl_fits())
				goto found;
	return ACL_ERR_TEMPLATE;
found:
	acl_idx = idx;
	if (!acl_range_check())
		return ACL_ERR_RANGE;
	if (ACL_IS_USED(idx))
		acl_rule_clear(idx);
	acl_idx = idx;

	if (!acl_any()) {
		for (acl_i = 0; acl_i < ACL_TEMPLATES; acl_i++) {
			REG_WRITE(RTL837X_ACL_TEMPLATE0_F0_3 + (acl_i << 3), acl_tmpl[acl_i][3],
				  acl_tmpl[acl_i][2], acl_tmpl[acl_i][1], acl_tmpl[acl_i][0]);
			REG_WRITE(RTL837X_ACL_TEMPLATE0_F4_7 + (acl_i << 3), acl_tmpl[acl_i][7],
				  acl_tmpl[acl_i][6], acl_tmpl[acl_i][5], acl_tmpl[acl_i][4]);
		}
	}
	if (!acl_encode())
		return ACL_ERR_RANGE;

	acl_tbl_sel = TBL_ACL_ACT;
	acl_tbl_words = ACL_ACT_WORDS;
	acl_tbl_addr = acl_idx;
	acl_tbl_src = (__xdata uint16_t *)&acl_act;
	acl_tbl_write();
	REG_SET(RTL837X_ACL_ACT_CTRL + ((uint16_t)acl_idx << 2), acl_ctrl);
	acl_tbl_src = (__xdata uint16_t *)&acl_x;
	acl_tbl_src_y = (__xdata uint16_t *)&acl_y;
	acl_entry_write();

	ACL_SET_USED(acl_idx);
	acl_rule_tmpl[acl_idx] = acl_tmpl_i;
	acl_ports_update();
	return 0;
}

void acl_rule_clear(uint8_t idx) __banked
{
	acl_idx = idx;
	acl_tbl_src = acl_tbl_src_y = (__xdata uint16_t *)&acl_zero;
	acl_entry_write();
	acl_tbl_sel = TBL_ACL_ACT;
	acl_tbl_words = ACL_ACT_WORDS;
	acl_tbl_addr = acl_idx;
	acl_tbl_src = (__xdata uint16_t *)&acl_zero;
	acl_tbl_write();
	REG_SET(RTL837X_ACL_ACT_CTRL + ((uint16_t)acl_idx << 2), ACL_ACT_CTRL_OFF);
	ACL_CLR_USED(acl_idx);
	acl_range_free();
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
