#ifndef _LLDP_H_
#define _LLDP_H_

#include <stdint.h>
#include <stdbool.h>

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
#define LLDP_END_TLV_TYPE 0
#define LLDP_MULTICAST_ADDR "\x01\x80\xc2\x00\x00\x0e"

#define LLDP_NB_MAX 16
#define LLDP_NB_PORT_ID_LENGTH 12
#define LLDP_NB_SYS_NAME_LENGTH 16
#define LLDP_NB_JSON_LENGTH 107	/* separator + worst-case neighbour entry in /lldp.json */

struct lldp_neighbor {
    uint8_t port;		/* logical ingress port */
    uint8_t chassis[MAC_ADDR_LEN];	/* Chassis ID, MAC address subtype */
    char port_id[LLDP_NB_PORT_ID_LENGTH + 1];	/* NUL terminated, printable ASCII */
    char sys_name[LLDP_NB_SYS_NAME_LENGTH + 1];	/* NUL terminated, printable ASCII */
    uint16_t ttl;		/* seconds until the entry expires, 0 = free slot */
};

extern __xdata struct lldp_neighbor lldp_nb[LLDP_NB_MAX];

void lldp_init(void) __banked;
void lldp_tick(void) __banked;
void lldp_send(void) __banked;
void lldp_shutdown(uint16_t ports) __banked;
bool lldp_in(void) __banked;
void lldp_neighbors_to_html(void) __banked;
void lldp_show(void) __banked;
void lldp_set_addresses(void) __banked;
void lldp_set_rtl_wrapper(void) __banked;
uint8_t lldp_sysname(__xdata uint8_t *p) __banked;
uint8_t lldp_sysdesc(__xdata uint8_t *p) __banked;
uint8_t lldp_portdesc(__xdata uint8_t *p, uint8_t port) __banked;

#endif
