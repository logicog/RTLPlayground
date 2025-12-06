#ifndef _RTL837X_PORT_H_
#define _RTL837X_PORT_H_

#include <stdint.h>

#define STAT_COUNTER_TX_PKTS 0x2e
#define STAT_COUNTER_RX_PKTS 0x2f
#define STAT_COUNTER_ERR_PKTS 0x30

#define STAT_GET(cnt, port) \
	REG_WRITE(RTL837X_STAT_GET, 0x00, 0x00, cnt >> 3, (cnt << 5) | (port << 1) | 1); \
	do { \
		reg_read_m(RTL837X_STAT_GET); \
	} while (sfr_data[3] & 0x1);

uint8_t port_l2_forget(void) __banked;
void port_l2_learned(void) __banked;
void port_stats_print(void) __banked;
int8_t vlan_get(register uint16_t vlan) __banked;
__xdata uint16_t vlan_name(register uint16_t vlan) __banked;
void vlan_setup(void) __banked;
void port_pvid_set(uint8_t port, __xdata uint16_t pvid) __banked;
void vlan_create(register uint16_t vlan, register uint16_t members, register uint16_t tagged) __banked;
void vlan_delete(uint16_t vlan) __banked;
void port_mirror_set(register uint8_t port, __xdata uint16_t rx_pmask, __xdata uint16_t tx_pmask) __banked;
void port_mirror_del(void) __banked;
void port_ingress_filter(register uint8_t port, uint8_t type) __banked;
void port_l2_setup(void) __banked;
void port_lag_members_set(__xdata uint8_t lag, __xdata uint16_t members) __banked;
void port_lag_hash_set(__xdata uint8_t lag, __xdata uint8_t hash) __banked;
void port_eee_enable_all(void) __banked;
void port_eee_disable_all(void) __banked;
void port_eee_status_all(void) __banked;
void port_eee_enable(uint8_t port) __banked;
void port_eee_disable(uint8_t port) __banked;
void port_eee_status(uint8_t port) __banked;
#endif
