#ifndef _DHCP_H_
#define _DHCP_H_

#include "uipopt.h"
#include <stdint.h>

#define DHCPC_SERVER_PORT	67
#define DHCPC_CLIENT_PORT	68

#define DHCP_OFF		0
#define DHCP_START		1
#define DHCP_DISCOVER_SENT	2
#define DHCP_REQUEST_SENT	3
#define DHCP_LEASING		4

void dhcp_start(void) __banked;
void dhcp_stop(void) __banked;
// void dhcp_periodic(void) __banked;
void dhcp_callback(void) __banked;


struct dhcp_state {
	uint8_t state;
	uint32_t transaction_id;
	uint16_t dhcp_timer;
	uint8_t ticks;
	uint16_t opt_ptr;
	uint8_t current_ip[4];
	uint8_t server[4];
	uint8_t router[4];
	uint8_t subnet[4];
	uint8_t dns[4];
	uint8_t broadcast[4];
	uint32_t lease;
	uint32_t rebind;
	uint32_t renewal;

	struct uip_udp_conn *conn;
};

typedef struct dhcp_state uip_udp_appstate_t;

/* Finally we define the application function to be called by uIP. */
#ifndef UIP_UDP_APPCALL
#define UIP_UDP_APPCALL dhcp_callback
#endif /* UIP_APPCALL */

#endif
