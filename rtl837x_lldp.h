#ifndef _LLDP_H_
#define _LLDP_H_

#include <stdint.h>

#define LLDP_MIN_ETHERNET_PAYLOAD_LENGTH 46
#define LLDP_ETHERTYPE       0x88cc
#define LLDP_ETHERTYPE_LENGTH 2
#define LLDP_MAX_FRAME       256
#define LLDP_MAC_ADDR_LEN    6
#define LLDP_TX_INTERVAL_SEC 30

#define LLDP_PORT_ID_TLV_TYPE 2
#define LLDP_PORT_ID_TLV_LENGTH 0x02
#define LLDP_PORT_ID_TLV_SUBTYPE 0x07

#define LLDP_TTL_TLV_TYPE 3
#define LLDP_TTL_TLV_LENGTH 2
#define LLDP_TTL_TTL_SECONDS 120

#define LLDP_CHASSIS_ID_TLV_TYPE 1
#define LLDP_CHASSIS_ID_TLV_LENGTH 7
#define LLDP_CHASSIS_ID_TLV_SUBTYPE 4

#define LLDP_SYSNAME_TLV_TYPE 5
#define LLDP_SYSDESC_TLV_TYPE 6

void lldp_init(void) __banked;
void lldp_tick(void) __banked;
void lldp_send(void) __banked;
void lldp_set_addresses(void) __banked;
void lldp_set_rtl_wrapper(void) __banked;
void lldp_sysname(uint8_t *p, uint16_t *len) __banked;
void lldp_sysdesc(uint8_t *p, uint16_t *len) __banked;

#endif
