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

// Maximum number of leases handed out by the integrated DHCP server
#define DHCPD_POOL_MAX		8

void dhcp_start(void) __banked;
void dhcp_stop(void) __banked;
// void dhcp_periodic(void) __banked;
void dhcp_callback(void) __banked;

// Integrated DHCP server API
void dhcpd_start(void) __banked;
void dhcpd_stop(void) __banked;
void dhcpd_set_pool(uint8_t first_octet, uint8_t count) __banked;
void dhcpd_set_lease_time(uint32_t seconds) __banked;
uint8_t dhcpd_active_leases(void) __banked;
void dhcpd_tick(void) __banked;


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

struct dhcpd_lease {
	uint8_t mac[6];
	uint32_t expires;	// Seconds remaining on lease; 0 = free
};

struct dhcpd_state {
	uint8_t enabled;
	uint8_t pool_first;	// Last octet of first pool IP
	uint8_t pool_count;	// Number of addresses in the pool (<= DHCPD_POOL_MAX)
	uint32_t lease_time;	// Lease time in seconds
	struct uip_udp_conn *conn;
	struct dhcpd_lease leases[DHCPD_POOL_MAX];
};

typedef struct dhcp_state uip_udp_appstate_t;

/* Finally we define the application function to be called by uIP. */
#ifndef UIP_UDP_APPCALL
#define UIP_UDP_APPCALL dhcp_callback
#endif /* UIP_APPCALL */

#endif
