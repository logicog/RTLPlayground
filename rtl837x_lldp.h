#ifndef _LLDP_H_
#define _LLDP_H_

#include <stdint.h>

#define LLDP_MIN_ETHERNET_PAYLOAD_LENGTH 46
#define LLDP_ETHERTYPE       0x88cc
#define LLDP_ETHERTYPE_LENGTH 2
#define LLDP_MAX_FRAME       256
#define LLDP_TX_INTERVAL_TICKS 6000

#define LLDP_PORT_ID_TLV_TYPE 2
#define LLDP_PORT_ID_TLV_LENGTH 0x02
#define LLDP_PORT_ID_TLV_SUBTYPE 0x07

#define LLDP_TTL_TLV_TYPE 3
#define LLDP_TTL_TLV_LENGTH 2
#define LLDP_TTL_TTL_SECONDS 120

#define LLDP_CHASSIS_ID_TLV_TYPE 1
#define LLDP_CHASSIS_ID_TLV_LENGTH 7
#define LLDP_CHASSIS_ID_TLV_SUBTYPE 4

#define LLDP_END_TLV_TYPE 0
#define LLDP_PORT_DESC_TLV_TYPE 4
#define LLDP_SYSNAME_TLV_TYPE 5
#define LLDP_SYSDESC_TLV_TYPE 6
#define LLDP_SYSCAP_TLV_TYPE 7
#define LLDP_MGMT_ADDR_TLV_TYPE 8

/* Binary Chassis/Port ID subtypes, shown as colon-separated hex; the others are text */
#define LLDP_CHASSIS_ID_SUBTYPE_MAC 4
#define LLDP_CHASSIS_ID_SUBTYPE_ADDR 5
#define LLDP_PORT_ID_SUBTYPE_MAC 3
#define LLDP_PORT_ID_SUBTYPE_ADDR 4
#define LLDP_PORT_ID_SUBTYPE_CIRCUIT 6

#define LLDP_MGMT_ADDR_FAMILY_IPV4 1

#define LLDP_NB_PORTS 10
#define LLDP_NB_STR_SIZE 32	/* including the NUL; longer TLVs are truncated */

/* Indices into lldp_neighbor.str */
#define LLDP_NB_CHASSIS_ID 0
#define LLDP_NB_PORT_ID 1
#define LLDP_NB_PORT_DESC 2
#define LLDP_NB_SYS_NAME 3
#define LLDP_NB_STRS 4

/*
 * What the link partner on a port announced in its last LLDPDU. Strings are
 * sanitized to JSON-safe printable ASCII on reception. ttl counts down once
 * a second; an entry with ttl 0 is unused.
 */
struct lldp_neighbor {
	uint16_t ttl;
	char str[LLDP_NB_STRS][LLDP_NB_STR_SIZE];
	uint16_t sys_cap;
	uint16_t en_cap;
	uint8_t mgmt_ip[4];	/* 0.0.0.0: none announced */
};

extern __xdata struct lldp_neighbor lldp_neighbors[LLDP_NB_PORTS];

void lldp_init(void) __banked;
void lldp_tick(void) __banked;
void lldp_in(void) __banked;
void lldp_neighbors_clear(void) __banked;
void lldp_show(void) __banked;
void lldp_neighbors_json(void) __banked;
void lldp_send(void) __banked;
void lldp_set_addresses(void) __banked;
void lldp_set_rtl_wrapper(void) __banked;
uint8_t lldp_sysname(__xdata uint8_t *p) __banked;
uint8_t lldp_sysdesc(__xdata uint8_t *p) __banked;

#endif
