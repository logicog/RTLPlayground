#ifndef _LLDP_H_
#define _LLDP_H_

#include <stdint.h>
#include <stdbool.h>

#define LLDP_MIN_ETHERNET_PAYLOAD_LENGTH 46
#define LLDP_ETHERTYPE       0x88cc
#define LLDP_ETHERTYPE_LENGTH 2
/* Largest LLDPDU we send: the mandatory TLVs, every optional TLV with its
 * longest value (port name 31, hostname 23, machine name 30) and End is
 * 156 bytes; test_lldp checks the bound. */
#define LLDP_MAX_PAYLOAD     176
#define LLDP_TX_INTERVAL_TICKS 6000
#define LLDP_TTL_SECONDS     120

/* TLV types */
#define LLDP_TLV_END		0
#define LLDP_TLV_CHASSIS_ID	1
#define LLDP_TLV_PORT_ID	2
#define LLDP_TLV_TTL		3
#define LLDP_TLV_PORT_DESC	4
#define LLDP_TLV_SYS_NAME	5
#define LLDP_TLV_SYS_DESC	6
#define LLDP_TLV_SYS_CAP	7
#define LLDP_TLV_MGMT_ADDR	8
#define LLDP_TLV_ORG		127

#define LLDP_CHASSIS_SUB_MAC	4
#define LLDP_PORT_SUB_MAC	3
#define LLDP_PORT_SUB_LOCAL	7
#define LLDP_CAP_BRIDGE		0x0004

/* Optional TLVs we send, bits of lldp_tlv; the order is that of the names
 * in "lldp tlv <name> on|off" */
#define LLDP_TX_PORTDESC	0x01
#define LLDP_TX_SYSCAP		0x02
#define LLDP_TX_MGMT		0x04
#define LLDP_TX_PVID		0x08
#define LLDP_TX_MACPHY		0x10
#define LLDP_TX_MFS		0x20
#define LLDP_TX_ALL		0x3f
#define LLDP_TX_N		6

/* Neighbour strings are stored printable ASCII without '"' and '\', so they
 * go out verbatim in JSON and on the console. Sizes include the NUL. */
#define LLDP_STR_SIZE	32
#define LLDP_DESC_SIZE	64

struct lldp_nb {
	uint16_t ttl;		/* TTL of the last LLDPDU, 0: no neighbour */
	uint16_t age;		/* seconds since the last LLDPDU */
	uint16_t cap_sup;
	uint16_t cap_en;
	uint16_t pvid;		/* 0: not sent */
	uint8_t chassis_sub;
	uint8_t port_sub;
	uint8_t ip[4];		/* first IPv4 management address, 0.0.0.0: none */
	char chassis[LLDP_STR_SIZE];
	char port[LLDP_STR_SIZE];
	char port_desc[LLDP_STR_SIZE];
	char sys_name[LLDP_STR_SIZE];
	char sys_desc[LLDP_DESC_SIZE];
};

extern __xdata struct lldp_nb lldp_nb[9];
extern __xdata uint8_t lldp_tlv;

void lldp_init(void) __banked;
void lldp_tick(void) __banked;
void lldp_in(void) __banked;
void lldp_parse(void) __banked;
void send_lldp(__xdata uint8_t phys_port) __banked;

#endif
