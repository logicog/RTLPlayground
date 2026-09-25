#ifndef _RTL837X_ACL_H_
#define _RTL837X_ACL_H_

#include <stdint.h>

#define ACL_RULES		64
#define ACL_HW_RULES		96
#define ACL_TEMPLATES		5
#define ACL_RANGES		16
#define ACL_METERS		64
#define ACL_COUNTERS		32
#define ACL_SELECTORS		16
#define ACL_KEYS		32
#define ACL_REQS		6

#define ACL_FT_DMAC0		0x00
#define ACL_FT_DMAC1		0x01
#define ACL_FT_DMAC2		0x02
#define ACL_FT_SMAC0		0x03
#define ACL_FT_SMAC1		0x04
#define ACL_FT_SMAC2		0x05
#define ACL_FT_ETHERTYPE	0x06
#define ACL_FT_STAG		0x07
#define ACL_FT_CTAG		0x08
#define ACL_FT_SIP0		0x10
#define ACL_FT_SIP1		0x11
#define ACL_FT_DIP0		0x12
#define ACL_FT_DIP1		0x13
#define ACL_FT_VIDRANGE		0x30
#define ACL_FT_IPRANGE		0x31
#define ACL_FT_PORTRANGE	0x32
#define ACL_FT_FIELD_VALID	0x33
#define ACL_FT_IPTOSPROTO	0x34
#define ACL_FT_L4SPORT		0x35
#define ACL_FT_L4DPORT		0x36
#define ACL_FT_SEL0		0x40

#define ACL_RNG_VID		0
#define ACL_RNG_IP		1
#define ACL_RNG_PORT		2

#define ACL_VID_CVID		1
#define ACL_VID_SVID		2
#define ACL_IP_SIP		1
#define ACL_IP_DIP		2
#define ACL_IP_SIP6		3
#define ACL_IP_DIP6		4
#define ACL_PORT_SPORT		1
#define ACL_PORT_DPORT		2

#define ACL_INFO_CTAG		0x0008
#define ACL_INFO_STAG		0x0010
#define ACL_INFO_PPPOE		0x0020
#define ACL_INFO_L3		0x00c0
#define ACL_INFO_L4		0x0700
#define ACL_L3_OTHER		0x0000
#define ACL_L3_ARP		0x0040
#define ACL_L3_IPV4		0x0080
#define ACL_L3_IPV6		0x00c0
#define ACL_L4_OTHER		0x0000
#define ACL_L4_ICMP		0x0100
#define ACL_L4_IGMP		0x0200
#define ACL_L4_TCP		0x0300
#define ACL_L4_UDP		0x0400
#define ACL_L4_NONE		0x0700

#define ACL_ACT_CVLAN		0x01
#define ACL_ACT_SVLAN		0x02
#define ACL_ACT_PRI		0x04
#define ACL_ACT_RMK		0x08
#define ACL_ACT_POLIC_LOG	0x10
#define ACL_ACT_FWD		0x20
#define ACL_ACT_INT		0x40
#define ACL_ACT_BYPASS		0x80
#define ACL_ACT_NOT		0x100

#define ACL_FWD_COPY		0
#define ACL_FWD_REDIRECT	1
#define ACL_FWD_MIRROR		2
#define ACL_FWD_TRAP		3

#define ACL_ERR_TEMPLATE	1
#define ACL_ERR_RANGE		2

struct acl_req {
	uint8_t  kind;
	uint8_t  type;
	uint8_t  key;
	uint32_t lo;
	uint32_t hi;
};

extern __xdata uint16_t acl_kv[ACL_KEYS];
extern __xdata uint16_t acl_km[ACL_KEYS];
extern __xdata struct acl_req acl_req[ACL_REQS];
extern __xdata uint8_t  acl_nreq;
extern __xdata uint16_t acl_info_v;
extern __xdata uint16_t acl_info_m;
extern __xdata uint16_t acl_in_pmask;
extern __xdata uint32_t acl_act[3];
extern __xdata uint16_t acl_ctrl;
extern __xdata uint16_t acl_unmatch_drop;
extern __xdata uint8_t  acl_used[ACL_HW_RULES / 8];
extern __xdata uint8_t  acl_rule_tmpl[ACL_HW_RULES];

extern __xdata uint16_t acl_ra;
extern __xdata uint32_t acl_rv;
extern __xdata uint16_t acl_vk;
extern __xdata uint16_t acl_mk;
extern __xdata struct acl_req acl_rq;

void acl_reg(void) __banked;
uint8_t acl_key(uint8_t ft) __banked;
void acl_match_begin(void) __banked;
void acl_match_key(uint8_t ft) __banked;
uint8_t acl_match_range(uint8_t key) __banked;
uint8_t acl_rule_set(uint8_t idx) __banked;
void acl_rule_clear(uint8_t idx) __banked;
void acl_unmatch_set(void) __banked;
void acl_counter_reset(void) __banked;
void acl_cmd(void) __banked;

#endif
