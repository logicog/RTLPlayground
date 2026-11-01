#ifndef _LLDP_H_
#define _LLDP_H_

#include <stdint.h>

#define LLDP_MIN_ETHERNET_PAYLOAD_LENGTH 46
#define LLDP_ETHERTYPE       0x88cc
#define LLDP_ETHERTYPE_LENGTH 2
#define LLDP_MAX_FRAME       256
#define LLDP_TX_INTERVAL_S   30
#define LLDP_HOLD_MULTIPLIER 4

#define LLDP_PORT_ID_TLV_TYPE 2
#define LLDP_PORT_ID_PREFIX "Port "
#define LLDP_PORT_ID_PREFIX_LENGTH 5
#define LLDP_PORT_ID_TLV_LENGTH (1 + LLDP_PORT_ID_PREFIX_LENGTH + 1)
#define LLDP_PORT_ID_TLV_SUBTYPE 0x05

#define LLDP_TTL_TLV_TYPE 3
#define LLDP_TTL_TLV_LENGTH 2
#define LLDP_TTL_TTL_SECONDS (LLDP_HOLD_MULTIPLIER * LLDP_TX_INTERVAL_S)

#define LLDP_CHASSIS_ID_TLV_TYPE 1
#define LLDP_CHASSIS_ID_TLV_LENGTH 7
#define LLDP_CHASSIS_ID_TLV_SUBTYPE 4

#define LLDP_PORT_DESC_TLV_TYPE 4
#define LLDP_SYSNAME_TLV_TYPE 5
#define LLDP_SYSDESC_TLV_TYPE 6

#define LLDP_CAPABILITIES_TLV_TYPE 7
#define LLDP_CAPABILITIES_TLV_LENGTH 4
#define LLDP_CAPABILITY_BRIDGE 0x0004

#define LLDP_MGMT_ADDR_TLV_TYPE 8
#define LLDP_MGMT_ADDR_TLV_LENGTH 12
#define LLDP_MGMT_ADDR_STR_LENGTH 5
#define LLDP_MGMT_ADDR_SUBTYPE_IPV4 1
#define LLDP_MGMT_IF_NUMBERING_UNKNOWN 1

#define LLDP_MAX_PAYLOAD 128

void lldp_init(void) __banked;
void lldp_tick(void) __banked;
void lldp_send(void) __banked;
void lldp_shutdown(uint16_t ports) __banked;
void lldp_set_addresses(void) __banked;
void lldp_set_rtl_wrapper(void) __banked;
uint8_t lldp_sysname(__xdata uint8_t *p) __banked;
uint8_t lldp_sysdesc(__xdata uint8_t *p) __banked;
uint8_t lldp_portdesc(__xdata uint8_t *p, uint8_t port) __banked;

#endif
