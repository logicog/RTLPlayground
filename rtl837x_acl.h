#ifndef _RTL837X_ACL_H_
#define _RTL837X_ACL_H_

#include <stdint.h>

#define ACL_RULES		64

#define ACL_FT_DMAC0		0x00
#define ACL_FT_DMAC1		0x01
#define ACL_FT_DMAC2		0x02
#define ACL_FT_SMAC0		0x03
#define ACL_FT_SMAC1		0x04
#define ACL_FT_SMAC2		0x05
#define ACL_FT_ETHERTYPE	0x06

#define ACL_TEMPLATE(f0, f1, f2, f3) \
	((f3) * 0x1000000UL + (f2) * 0x10000UL + (f1) * 0x100U + (f0))

#define ACL_FIELD_DMAC		0
#define ACL_FIELD_SMAC		3
#define ACL_FIELD_ETHERTYPE	6

extern __xdata uint16_t acl_in_pmask;
extern __xdata uint16_t acl_out_pmask;
extern __xdata uint8_t  acl_fwd;

void acl_match_begin(void) __banked;
void acl_match_mac(uint8_t field, __xdata uint8_t *mac) __banked;
void acl_match_ethertype(uint16_t type) __banked;
void acl_rule_set(uint8_t idx) __banked;
void acl_rule_clear(uint8_t idx) __banked;

#endif
