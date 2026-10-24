
#include "uip/uip.h"
#include "udp_apps.h"

#ifdef SNMP
#include "snmp.h"
/* Keep this non-banked callback in common code, as required by uIP. */
#endif

void udp_callbacks(void)
{
#ifdef SNMP
    if (uip_udp_conn->lport == HTONS(161)) { snmp_callback(); return; }
#endif
	dhcp_callback(uip_udp_conn->lport); 	// let the application decide if this is for it or not
	syslog_callback(uip_udp_conn->lport);	// let the application decide if this is for it or not
	dns_callback(uip_udp_conn->lport);
	ntp_callback(uip_udp_conn->lport);
}