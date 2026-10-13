#ifndef _LLDP_H_
#define _LLDP_H_

#include <stdint.h>

#define LLDP_MIN_ETHERNET_PAYLOAD_LENGTH 46
#define LLDP_ETHERTYPE       0x88cc
#define LLDP_ETHERTYPE_LENGTH 2
#define LLDP_MAX_FRAME       256
#define LLDP_TX_INTERVAL_TICKS 6000

#define LLDP_PORT_ID_TLV_TYPE 2
#define LLDP_PORT_ID_PREFIX "Port "
#define LLDP_PORT_ID_PREFIX_LENGTH 5
#define LLDP_PORT_ID_TLV_LENGTH (1 + LLDP_PORT_ID_PREFIX_LENGTH + 1)
#define LLDP_PORT_ID_TLV_SUBTYPE 0x05

#define LLDP_TTL_TLV_TYPE 3
#define LLDP_TTL_TLV_LENGTH 2
#define LLDP_TTL_TTL_SECONDS 120

#define LLDP_CHASSIS_ID_TLV_TYPE 1
#define LLDP_CHASSIS_ID_TLV_LENGTH 7
#define LLDP_CHASSIS_ID_TLV_SUBTYPE 4

#define LLDP_PORT_DESC_TLV_TYPE 4
#define LLDP_ORG_TLV_TYPE 127
#define LLDP_ORG_TLV_HEADER_LENGTH 4
#define LLDP_8021_SUBTYPE_PVID 1
#define LLDP_8023_SUBTYPE_MAC_PHY 1
#define LLDP_8023_SUBTYPE_LINK_AGG 3
#define LLDP_AUTONEG_SUPPORTED 0x01
#define LLDP_AUTONEG_ENABLED 0x02
#define LLDP_AUTONEG_CAP_10BASE_T 0x4000
#define LLDP_AUTONEG_CAP_10BASE_T_FD 0x2000
#define LLDP_AUTONEG_CAP_100BASE_TX 0x0800
#define LLDP_AUTONEG_CAP_100BASE_TX_FD 0x0400
#define LLDP_AUTONEG_CAP_1000BASE_T_FD 0x0001
#define LLDP_MAU_UNKNOWN 0
#define LLDP_MAU_10BASE_T_HD 10
#define LLDP_MAU_10BASE_T_FD 11
#define LLDP_MAU_100BASE_TX_HD 15
#define LLDP_MAU_100BASE_TX_FD 16
#define LLDP_MAU_1000BASE_X_FD 22
#define LLDP_MAU_1000BASE_T_HD 29
#define LLDP_MAU_1000BASE_T_FD 30
#define PHYSR_SPEED(v) ((((v) & 0x0600) >> 7) | (((v) & 0x0030) >> 4))
#define PHYSR_SPEED_10M 0
#define PHYSR_SPEED_100M 1
#define PHYSR_SPEED_1000M 2
#define PHYSR_SPEED_DOWN 7
#define PHYSR_FULL_DUPLEX 0x0008
#define PHY_ANEG_CTRL_ENABLE 0x1000
#define LINKS_CODE_1000M 2
#define PHY_ANEG_ADV_10BASE_T_HD 0x0020
#define PHY_ANEG_ADV_10BASE_T_FD 0x0040
#define PHY_ANEG_ADV_100BASE_TX_HD 0x0080
#define PHY_ANEG_ADV_100BASE_TX_FD 0x0100
#define PHY_GBCR_1000BASE_T_FD 0x0200
#define PORT_MAX_LEN_MASK 0x3fff
#define LLDP_8023_SUBTYPE_MAX_FRAME 4
#define LLDP_AGG_CAPABLE 0x01
#define LLDP_AGG_ACTIVE 0x02
#define LLDP_SYSNAME_TLV_TYPE 5
#define LLDP_SYSDESC_TLV_TYPE 6

void lldp_init(void) __banked;
void lldp_tick(void) __banked;
void lldp_send(void) __banked;
void lldp_set_addresses(void) __banked;
void lldp_set_rtl_wrapper(void) __banked;
uint8_t lldp_sysname(__xdata uint8_t *p) __banked;
uint8_t lldp_sysdesc(__xdata uint8_t *p) __banked;
uint8_t lldp_portdesc(__xdata uint8_t *p, uint8_t port) __banked;
uint8_t lldp_port_org_tlvs(__xdata uint8_t *p, uint8_t port) __banked __reentrant;

#endif
