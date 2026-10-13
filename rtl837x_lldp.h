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
#define LLDP_PORT_ID_TLV_LENGTH 0x02
#define LLDP_PORT_ID_TLV_SUBTYPE 0x07

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
#define LLDP_MGMT_ADDR_TLV_TYPE 8
#define LLDP_MGMT_ADDR_SUBTYPE_IPV4 1
#define LLDP_MGMT_ADDR_IPV4_LENGTH 5

#define LLDP_END_TLV_TYPE 0
#define LLDP_MULTICAST_ADDR "\x01\x80\xc2\x00\x00\x0e"

#define LLDP_CHASSIS_ID_SUBTYPE_NETWORK_ADDR 5
#define LLDP_ADDR_FAMILY_IPV4 1
#define LLDP_NETWORK_ADDR_IPV4_LENGTH 5

#define LLDP_NB_MAX 16
#define LLDP_NB_CHASSIS_LENGTH 16
#define LLDP_NB_PORT_ID_LENGTH 12
#define LLDP_NB_SYS_NAME_LENGTH 16
#define LLDP_NB_PORT_DESC_LENGTH 16

#define LLDP_ORG_TLV_TYPE 127
#define LLDP_ORG_TLV_HEADER_LENGTH 4
#define LLDP_8021_SUBTYPE_PVID 1
#define LLDP_8023_SUBTYPE_LINK_AGG 3
#define LLDP_8023_SUBTYPE_MAX_FRAME 4
#define LLDP_AGG_CAPABLE 0x01
#define LLDP_AGG_ACTIVE 0x02
#define LLDP_AGG_ANNOUNCED 0x80

#define LLDP_WARN_MTU 0x01
#define LLDP_WARN_AGG 0x02
#define LLDP_WARN_DUPLICATE 0x04
#define PORT_MAX_LEN_MASK 0x3fff
#define LLDP_NB_JSON_LENGTH 221	/* separator + worst-case neighbour entry in /lldp.json */

struct lldp_neighbor {
    uint8_t port;		/* logical ingress port */
    uint8_t chassis_subtype;	/* Chassis ID subtype, 4 = MAC address, 5 = network address */
    uint8_t chassis_len;	/* bytes used in chassis[] */
    uint8_t chassis[LLDP_NB_CHASSIS_LENGTH];	/* Chassis ID without the subtype byte */
    char port_id[LLDP_NB_PORT_ID_LENGTH + 1];	/* NUL terminated, printable ASCII */
    char sys_name[LLDP_NB_SYS_NAME_LENGTH + 1];	/* NUL terminated, printable ASCII */
    char port_desc[LLDP_NB_PORT_DESC_LENGTH + 1];	/* NUL terminated, printable ASCII */
    uint8_t mgmt_addr[4];	/* IPv4 management address, all zero = none */
    uint16_t caps;		/* enabled System Capabilities bits */
    uint16_t pvid;		/* 802.1 Port VLAN ID, 0 = not announced */
    uint16_t mtu;		/* 802.3 Maximum Frame Size, 0 = not announced */
    uint8_t agg;		/* 802.3 Link Aggregation status, LLDP_AGG_* bits */
    uint8_t warn;		/* LLDP_WARN_* bits as of the last frame */
    uint16_t ttl;		/* seconds until the entry expires, 0 = free slot */
};

extern __xdata struct lldp_neighbor lldp_nb[LLDP_NB_MAX];

void lldp_init(void) __banked;
void lldp_tick(void) __banked;
void lldp_send(void) __banked;
bool lldp_in(void) __banked;
void lldp_neighbors_to_html(void) __banked;
void lldp_show(void) __banked;
void lldp_set_addresses(void) __banked;
void lldp_set_rtl_wrapper(void) __banked;
uint8_t lldp_sysname(__xdata uint8_t *p) __banked;
uint8_t lldp_sysdesc(__xdata uint8_t *p) __banked;

#endif
