#include <stdint.h>
#include "rtl837x_common.h"
#include "rtl837x_sfr.h"
#include "rtl837x_regs.h"
#include "rtl837x_acl.h"

#pragma codeseg BANK3
#pragma constseg BANK3

#define ACL_PORTS	((uint16_t)((1 << CPU_PORT) - 1))

extern __xdata uint8_t sfr_data[4];

__xdata uint16_t acl_in_pmask;
__xdata uint16_t acl_out_pmask;
__xdata uint8_t  acl_fwd;

__xdata uint8_t  acl_used[ACL_RULES / 8];
__xdata uint16_t acl_data[10];
__xdata uint16_t acl_care[10];
__xdata uint16_t acl_act[6];
static __xdata uint16_t acl_zero[10];

static void acl_tbl_wait(void)
{
	uint8_t guard = 0;
	do {
		reg_read_m(RTL837X_TBL_CTRL);
	} while ((sfr_data[3] & TBL_EXECUTE) && ++guard);
}

static void acl_tbl_write(uint8_t entry, uint8_t table, __xdata uint16_t *h, uint8_t words)
{
	acl_tbl_wait();
	for (uint8_t i = 0; i < words; i++)
		REG_WRITE(RTL837x_TBL_DATA_IN_A + (i << 2), h[2 * i + 1] >> 8, h[2 * i + 1],
			  h[2 * i] >> 8, h[2 * i]);
	REG_WRITE(RTL837X_TBL_CTRL, 0, entry, table, TBL_WRITE | TBL_EXECUTE);
	acl_tbl_wait();
}

static uint8_t acl_any(void)
{
	for (uint8_t i = 0; i < ACL_RULES / 8; i++)
		if (acl_used[i])
			return 1;
	return 0;
}

static void acl_field_set(uint8_t f, uint16_t v)
{
	acl_data[f] = v;
	acl_care[f] = ~v;
}

void acl_match_begin(void) __banked
{
	for (uint8_t i = 0; i < 10; i++)
		acl_data[i] = acl_care[i] = 0xffff;
}

void acl_match_mac(uint8_t field, __xdata uint8_t *mac) __banked
{
	acl_field_set(field, ((uint16_t)mac[4] << 8) | mac[5]);
	acl_field_set(field + 1, ((uint16_t)mac[2] << 8) | mac[3]);
	acl_field_set(field + 2, ((uint16_t)mac[0] << 8) | mac[1]);
}

void acl_match_ethertype(uint16_t type) __banked
{
	acl_field_set(ACL_FIELD_ETHERTYPE, type);
}

void acl_rule_set(uint8_t idx) __banked
{
	uint16_t in = acl_in_pmask ? acl_in_pmask : ACL_PORTS;
	uint16_t out = ~in & ((1 << (CPU_PORT + 1)) - 1);

	if (acl_used[idx >> 3] & (1 << (idx & 7)))
		acl_rule_clear(idx);

	acl_data[8] &= ~(0x0007 | (uint16_t)(out << 11));
	acl_data[9] &= ~(out >> 5);

	if (!acl_any()) {
		REG_SET(RTL837X_ACL_TEMPLATE0_F0_3,
			ACL_TEMPLATE(ACL_FT_DMAC0, ACL_FT_DMAC1, ACL_FT_DMAC2, ACL_FT_SMAC0));
		REG_SET(RTL837X_ACL_TEMPLATE0_F4_7,
			ACL_TEMPLATE(ACL_FT_SMAC1, ACL_FT_SMAC2, ACL_FT_ETHERTYPE, ACL_FT_DMAC0));
	}

	for (uint8_t i = 0; i < 6; i++)
		acl_act[i] = 0;
	acl_act[3] = (acl_fwd << 1) | (acl_out_pmask << 5);
	acl_tbl_write(idx, TBL_ACL_ACT, acl_act, 3);
	REG_SET(RTL837X_ACL_ACT_CTRL + ((uint16_t)idx << 2), RTL837X_ACL_ACT_FWD);

	acl_tbl_write(idx, TBL_ACL_RULE, acl_care, 5);
	acl_tbl_write(0x80 | idx, TBL_ACL_RULE, acl_data, 5);

	if (!acl_any()) {
		REG_SET(RTL837X_ACL_UNMATCH_PERMIT, ACL_PORTS);
		REG_SET(RTL837X_ACL_PORT_EN, ACL_PORTS);
	}
	acl_used[idx >> 3] |= 1 << (idx & 7);
}

void acl_rule_clear(uint8_t idx) __banked
{
	acl_tbl_write(idx, TBL_ACL_RULE, acl_zero, 5);
	acl_tbl_write(0x80 | idx, TBL_ACL_RULE, acl_zero, 5);
	acl_tbl_write(idx, TBL_ACL_ACT, acl_zero, 3);
	REG_SET(RTL837X_ACL_ACT_CTRL + ((uint16_t)idx << 2), 0);
	acl_used[idx >> 3] &= ~(1 << (idx & 7));

	if (!acl_any()) {
		REG_SET(RTL837X_ACL_PORT_EN, 0);
		REG_SET(RTL837X_ACL_UNMATCH_PERMIT, 0);
	}
}
