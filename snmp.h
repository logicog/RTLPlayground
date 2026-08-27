/*
 * Minimal SNMPv1/v2c agent for RTL837x switches.
 *
 * Provides read-only access to a subset of MIB-II:
 *  - system group (sysDescr, sysObjectID, sysUpTime, sysContact,
 *    sysName, sysLocation, sysServices)
 *  - interfaces group (ifNumber, ifTable columns: ifIndex, ifDescr,
 *    ifType, ifMtu, ifSpeed, ifPhysAddress, ifAdminStatus,
 *    ifOperStatus, ifInOctets, ifInUcastPkts, ifInErrors,
 *    ifOutOctets, ifOutUcastPkts, ifOutErrors)
 *
 * Wire protocol: SNMPv1 (RFC1157) and SNMPv2c (RFC1901..1908).
 * PDU types accepted: GetRequest, GetNextRequest, GetBulkRequest
 * (GetBulk is served as one repetition per variable-binding).
 * No SetRequest support (agent is read-only). No SNMP traps.
 */
#ifndef _SNMP_H_
#define _SNMP_H_

#include <stdint.h>

#define SNMP_PORT		161
#define SNMP_COMMUNITY_MAX	15

struct snmp_state {
	uint8_t enabled;
	char community[SNMP_COMMUNITY_MAX + 1];	/* NUL-terminated */
	char location[32];
	char contact[32];
	__xdata struct uip_udp_conn *conn;
};

extern __xdata struct snmp_state snmp_state;

void snmp_init(void) __banked __reentrant;
void snmp_start(void) __banked __reentrant;
void snmp_stop(void) __banked __reentrant;
void snmp_callback(uint16_t lport) __banked __reentrant;

#endif
