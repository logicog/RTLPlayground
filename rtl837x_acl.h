#ifndef _RTL837X_ACL_H_
#define _RTL837X_ACL_H_

#include <stdint.h>

#define ACL_RULES		64

#define ACL_FIELD_DMAC		0
#define ACL_FIELD_SMAC		3
#define ACL_FIELD_ETHERTYPE	6

extern __xdata uint8_t  acl_field;
extern __xdata uint8_t  acl_value[6];
extern __xdata uint16_t acl_in_pmask;
extern __xdata uint16_t acl_out_pmask;

void acl_rule_set(uint8_t idx) __banked;
void acl_rule_clear(uint8_t idx) __banked;

#endif
