#ifndef SNMP_INTERNAL_H
#define SNMP_INTERNAL_H
/* Private control interface. Community bytes remain within the two private SNMP implementation
 * modules. Only main-loop code may access this state; never serialize credential length. */
#include <stdint.h>
struct uip_udp_conn;
extern __xdata uint8_t snmp_community[32];
extern __xdata uint8_t snmp_agent_enabled, snmp_community_length;
extern __xdata uint8_t snmp_network[4], snmp_mask[4];
extern __xdata struct uip_udp_conn *__xdata snmp_listener;
#endif
