
#include "uip/uip.h"
#include "udp_apps.h"
#include "sntp.h"

void udp_callbacks(void)
{
	dhcp_callback(uip_udp_conn->lport); 	// let the application decide if this is for it or not
	syslog_callback(uip_udp_conn->lport);	// let the application decide if this is for it or not
	sntp_callback(uip_udp_conn->lport);	// let the application decide if this is for it or not
}