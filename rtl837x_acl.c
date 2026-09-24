#include <stdint.h>
#include "rtl837x_common.h"
#include "rtl837x_sfr.h"
#include "rtl837x_regs.h"
#include "rtl837x_acl.h"

#pragma codeseg BANK3
#pragma constseg BANK3

extern __xdata uint8_t sfr_data[4];

__xdata uint8_t  acl_field;
__xdata uint8_t  acl_value[6];
__xdata uint16_t acl_in_pmask;
__xdata uint16_t acl_out_pmask;

__xdata uint8_t  acl_used[ACL_RULES / 8];
__xdata uint16_t acl_data[10];
__xdata uint16_t acl_care[10];
__xdata uint16_t acl_act[6];

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

void acl_rule_set(uint8_t idx) __banked
{
	uint16_t in = acl_in_pmask ? acl_in_pmask : (PMASK_9 & ~PMASK_CPU);
	uint16_t out = ~in & 0x3ff;

	if (acl_used[idx >> 3] & (1 << (idx & 7)))
		acl_rule_clear(idx);

	for (uint8_t i = 0; i < 10; i++)
		acl_data[i] = acl_care[i] = 0xffff;
	if (acl_field == ACL_FIELD_ETHERTYPE) {
		acl_field_set(ACL_FIELD_ETHERTYPE, ((uint16_t)acl_value[0] << 8) | acl_value[1]);
	} else {
		acl_field_set(acl_field, ((uint16_t)acl_value[4] << 8) | acl_value[5]);
		acl_field_set(acl_field + 1, ((uint16_t)acl_value[2] << 8) | acl_value[3]);
		acl_field_set(acl_field + 2, ((uint16_t)acl_value[0] << 8) | acl_value[1]);
	}
	acl_data[8] &= ~(0x0007 | (uint16_t)(out << 11));
	acl_data[9] &= ~(out >> 5);

	if (!acl_any()) {
		REG_SET(RTL837X_ACL_TEMPLATE0_LO, 0x00060504);
		REG_SET(RTL837X_ACL_TEMPLATE0_HI, 0x03020100);
	}

	for (uint8_t i = 0; i < 6; i++)
		acl_act[i] = 0;
	acl_act[3] = (RTL837X_ACL_FWD_REDIRECT << 1) | (acl_out_pmask << 5);
	acl_tbl_write(idx, TBL_ACL_ACT, acl_act, 3);
	REG_SET(RTL837X_ACL_ACT_CTRL + ((uint16_t)idx << 2), RTL837X_ACL_ACT_FWD);

	acl_tbl_write(idx, TBL_ACL_RULE, acl_care, 5);
	acl_tbl_write(0x80 | idx, TBL_ACL_RULE, acl_data, 5);

	if (!acl_any()) {
		REG_SET(RTL837X_ACL_UNMATCH_PERMIT, PMASK_9);
		REG_SET(RTL837X_ACL_PORT_EN, PMASK_9 & ~PMASK_CPU);
	}
	acl_used[idx >> 3] |= 1 << (idx & 7);
}

void acl_rule_clear(uint8_t idx) __banked
{
	for (uint8_t i = 0; i < 10; i++)
		acl_data[i] = 0;
	acl_tbl_write(idx, TBL_ACL_RULE, acl_data, 5);
	acl_tbl_write(0x80 | idx, TBL_ACL_RULE, acl_data, 5);
	acl_used[idx >> 3] &= ~(1 << (idx & 7));

	if (!acl_any()) {
		REG_SET(RTL837X_ACL_PORT_EN, 0);
		REG_SET(RTL837X_ACL_UNMATCH_PERMIT, 0);
	}
}
