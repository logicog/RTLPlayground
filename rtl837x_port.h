#ifndef _RTL837X_PORT_H_
#define _RTL837X_PORT_H_

uint8_t port_l2_forget(void) __banked;
void port_l2_learned(void) __banked;
void port_stats_print(void) __banked;
void vlan_setup(void) __banked;
void port_pvid_set(uint8_t port, __xdata uint16_t pvid) __banked;
void vlan_create(uint16_t vlan, uint16_t members, uint16_t tagged) __banked;
void vlan_delete(uint16_t vlan) __banked;
void port_mirror_set(register uint8_t port, __xdata uint16_t rx_pmask, __xdata uint16_t tx_pmask) __banked;

#endif
