#ifndef _RTL837X_DHCP_H_
#define _RTL837X_DHCP_H_

#include <stdint.h>

#define DHCP_OFF		0
#define DHCP_START		1
#define DHCP_DISCOVER_SENT	2

void dhcp_start(void) __banked;
void dhcp_stop(void) __banked;
void dhcp_periodic(void) __banked;
void dhcp_packet_handler(void) __banked;

#endif
