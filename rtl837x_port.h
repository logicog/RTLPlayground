#ifndef _RTL837X_PORT_H_
#define _RTL837X_PORT_H_

uint8_t port_l2_forget(void) __banked;
void port_l2_learned(void) __banked;
void port_stats_print(void) __banked;
void vlan_setup(void) __banked;

#endif
