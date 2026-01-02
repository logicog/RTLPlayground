#ifndef _DHCP_H_
#define _DHCP_H_

#include "uipopt.h"
#include <stdint.h>
#define DHCPD_MAX_CLIENTS	20
#define DHCPD_START_IP		100

#define DHCP_SERVER_PORT	67
#define DHCP_CLIENT_PORT	68

#define DHCP_OFF		0
#define DHCP_START		1
#define DHCP_DISCOVER_SENT	2
#define DHCP_REQUEST_SENT	3
#define DHCP_LEASING		4
#define DHCP_SERVER		5

#define CSTATE_NONE		0
#define CSTATE_OFFERED		1
#define CSTATE_LEASED		2

void dhcp_start(void) __banked;
void dhcp_stop(void) __banked;
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

struct dhcpd_cstate {
	uint8_t cstate;
	uint16_t timer;
	uint32_t transaction_id;
	uint8_t mac[6];
	uint8_t ip[4];
};


void dhcpd_start(void) __banked;
void dhcpd_stop(void) __banked;

typedef struct dhcp_state uip_udp_appstate_t;

/* Finally we define the application function to be called by uIP. */
#ifndef UIP_UDP_APPCALL
#define UIP_UDP_APPCALL dhcp_callback
#endif /* UIP_APPCALL */

#endif
