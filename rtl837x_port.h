#ifndef _RTL837X_PORT_H_
#define _RTL837X_PORT_H_

#include <stdint.h>
#include "rtl837x_regs.h"

#define STAT_COUNTER_TX_PKTS	46
#define STAT_COUNTER_RX_PKTS	47
#define STAT_COUNTER_ERR_PKTS	48

#define STAT_GET(cnt, port) \
	REG_WRITE(RTL837X_STAT_GET, 0x00, 0x00, cnt >> 3, (cnt << 5) | (port << 1) | 1); \
	do { \
		reg_read_m(RTL837X_STAT_GET); \
	} while (sfr_data[3] & 0x1);

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

extern __xdata struct vlan_settings vlan_settings;

uint8_t port_l2_forget(void) __banked;
void port_l2_learned(void) __banked;
void port_stats_print(void) __banked;
int8_t vlan_get(register uint16_t vlan) __banked;
__xdata uint16_t vlan_name(register uint16_t vlan) __banked;
void vlan_setup(void) __banked;
void port_pvid_set(uint8_t port, __xdata uint16_t pvid) __banked;
uint16_t port_pvid_get(uint8_t port) __banked;
void vlan_create(void) __banked;
void vlan_delete(uint16_t vlan) __banked;
void vlan_dump(void) __banked;
void port_mirror_set(register uint8_t port, __xdata uint16_t rx_pmask, __xdata uint16_t tx_pmask) __banked;
void port_mirror_del(void) __banked;
bool port_ingress_filter(__xdata uint8_t port, __xdata vlan_ingress_mode_t type) __banked;
void port_l2_setup(void) __banked;
void port_lag_members_set(__xdata uint8_t lag, __xdata uint16_t members) __banked;
void port_lag_hash_set(__xdata uint8_t lag, __xdata uint8_t hash) __banked;
void port_eee_enable_all(__xdata uint8_t speed) __banked;
void port_eee_disable_all(void) __banked;
void port_eee_status_all(void) __banked;
void port_eee_enable(__xdata uint8_t port, __xdata uint8_t speed) __banked;
void port_eee_disable(uint8_t port) __banked;
void port_eee_status(uint8_t port) __banked;
void print_port_ingress_filter_mode(vlan_ingress_mode_t mode) __banked;
bool port_ingress_vlan_filter_set(__xdata uint8_t port, __xdata bool enabled) __banked;
bool port_ingress_vlan_filter_get(__xdata uint8_t port) __banked;
void port_rldp_on(__xdata uint16_t p_ms) __banked;
void port_rldp_off(void) __banked;
uint16_t port_rldp_get(void) __banked;
#endif
