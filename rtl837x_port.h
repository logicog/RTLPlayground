#ifndef _RTL837X_PORT_H_
#define _RTL837X_PORT_H_

#include <stdint.h>
#include <stdbool.h>
#include "rtlos_regs.h"

#define STAT_COUNTER_TX_PKTS	46
#define STAT_COUNTER_RX_PKTS	47
#define STAT_COUNTER_ERR_PKTS	48

/*
 * STAT_GET -- latch one MIB counter and wait for the MIB engine to drop BUSY.
 *
 * THE WAIT MUST BE BOUNDED, and on the RTL836x this register must not be
 * touched at all: 0x0f60 bit0 stays 1 on that die, so an unbounded poll
 * wedges the CPU (console dead, power-cycle to recover). The web path shares
 * the macro -- send_status() and send_counters() call it many times -- so a
 * stuck BUSY froze the whole board with a browser open.
 *
 * RTL836x reads its counters through the 0x1000 window instead (see
 * rtlos_regs.h), so skip the latch entirely here. Other machines: bound the
 * BUSY poll (TABLE_WAIT_TOUT, rtl837x_port.c).
 */
#if SOC_FAMILY == RTL836X
#define STAT_GET(cnt, port) do { (void)(cnt); (void)(port); } while (0)
#else
#define STAT_GET(cnt, port) do { \
	REG_WRITE(RTL837X_STAT_GET, 0x00, 0x00, cnt >> 3, (cnt << 5) | (port << 1) | 1); \
	do { \
		reg_read_m(RTL837X_STAT_GET); \
	} while (sfr_data[3] & 0x1); \
	} while (0)
#endif

// Possible values for ingress filter type
typedef enum {
	VLAN_ALL = INGR_ALLOW_ALL,
	VLAN_TAGGED = INGR_ALLOW_TAGGED,
	VLAN_UNTAGGED = INGR_ALLOW_UNTAGGED,
	VLAN_INVALID = 3
} vlan_ingress_mode_e;

// Since we lack a way to force enum to be 1 byte,
// This is a typedef for the VLAN ingress filter type
// which holds vlan_ingress_mode_e values
typedef uint8_t vlan_ingress_mode_t;

struct vlan_settings {
	uint16_t vlan;
	uint16_t members;
	uint16_t tagged;
};

/* 
 * Port EEE settings
 */
#define EEE_100 	0x01
#define EEE_1000	0x04
#define EEE_2G5		0x10
#define EEE_5G		0x20
#define EEE_10G		0x40
#define EEE_NORESET	0x80

extern __xdata struct vlan_settings vlan_settings;

uint8_t port_l2_forget(void) __banked;
void port_l2_learned(void) __banked;
void port_stats_print(void) __banked;
void port_media_show(uint8_t port) __banked;
int8_t vlan_get(uint16_t vlan) __banked;

__xdata uint16_t vlan_name(uint16_t vlan) __banked;
void vlan_name_remove(uint16_t vlan) __banked;
void vlan_setup(void) __banked;
void port_pvid_default_unset(void) __banked;
void port_vlan_filter_default_on(void) __banked;
void port_vlan_egress_original(void) __banked;
void port_pvid_set(uint8_t port, __xdata uint16_t pvid) __banked;
uint16_t port_pvid_get(uint8_t port) __banked;
void port_l2mc_set(uint8_t mac_last, __xdata uint16_t vid, __xdata uint16_t pmask) __banked;
void port_l2_forget_port(uint8_t port) __banked;
void vlan_create(void) __banked;
void vlan_delete(uint16_t vlan) __banked;
void vlan_dump(void) __banked;
void port_mirror_set(uint8_t port, __xdata uint16_t rx_pmask, __xdata uint16_t tx_pmask) __banked;
void port_mirror_del(void) __banked;
bool port_ingress_filter(__xdata uint8_t port, __xdata vlan_ingress_mode_t type) __banked;
void port_l2_setup(void) __banked;
uint16_t port_lag_members_get(uint8_t lag) __banked;
#define PORT_LAG_NONE 0xff
uint8_t port_lag_of(uint8_t port) __banked;
void port_lag_members_set(__xdata uint8_t lag, __xdata uint16_t members) __banked;
void port_lag_hash_set(__xdata uint8_t lag, __xdata uint8_t hash) __banked;
void port_eee_enable_all(__xdata uint8_t speed) __banked;
void port_eee_disable_all(void) __banked;
void port_eee_status_all(void) __banked;
void port_eee_enable(__xdata uint8_t port, __xdata uint8_t speed) __banked;
void port_eee_disable(uint8_t port) __banked;
void port_eee_status(uint8_t port) __banked;
void print_port_ingress_filter_mode(vlan_ingress_mode_t mode) __banked;
bool port_ingress_vlan_filter_set(uint8_t port, __xdata bool enabled) __banked;
bool port_ingress_vlan_filter_get(uint8_t port) __banked;
vlan_ingress_mode_t port_ingress_filter_get(__xdata uint8_t port) __banked;
void port_isolate(uint8_t port, __xdata uint16_t pmask) __banked;
uint16_t port_isolation_get(uint8_t port) __banked;
void port_l2_static_mgmt(__xdata uint8_t *mac, __xdata uint16_t vlan, __xdata bool remove_entry) __banked;

#define L2_WALK_END 0xffff
#define L2_WALK_PORT_UNKNOWN 0xff
struct l2_walk_entry {
	uint8_t mac[6];
	uint16_t fid;		/* forwarding id the entry was learned under (= VLAN) */
	uint8_t port;		/* logical port, or L2_WALK_PORT_UNKNOWN */
	uint8_t is_static;
	uint8_t is_mcast;
	uint16_t portmask;	/* member mask when is_mcast (0 = unreadable here) */
};
extern __xdata struct l2_walk_entry l2_walk;
uint16_t rtl836x_l2_walk_next(__xdata uint16_t *slot) __banked __reentrant;
uint8_t rtl836x_l2_delete_slot(uint16_t slot) __banked __reentrant;
extern __xdata uint8_t mib_value[8];
void rtl836x_mib_reset(void) __banked;
#define MIB_ST_TX_GOOD	0
#define MIB_ST_RX_GOOD	1
#define MIB_ST_TX_BAD	2
#define MIB_ST_RX_BAD	3
uint8_t rtl836x_mib_read(uint8_t idx, uint8_t port) __banked __reentrant;
uint8_t rtl836x_mib_slot(uint8_t slot, uint8_t port) __banked __reentrant;
uint8_t rtl836x_mib_status(uint8_t which, uint8_t port) __banked __reentrant;
uint32_t rtl836x_mib_result(void) __banked __reentrant;

void nic8370_init(void) __banked;
bool nic8370_rx_fetch(void) __banked;
void nic8370_tx(void) __banked;
void nic8370_status(void) __banked;
#endif
