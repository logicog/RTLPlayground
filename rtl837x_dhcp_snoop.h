#ifndef _RTL837X_DHCP_SNOOP_H_
#define _RTL837X_DHCP_SNOOP_H_

#include <stdint.h>

#define DHCP_SNOOP_RULE_DROP	64
#define DHCP_SNOOP_RULE_SERVER	65
#define DHCP_SNOOP_RULE_CLIENT	66
#define DHCP_SERVER_PORT	67
#define DHCP_CLIENT_PORT	68

#define DHCP_BIND_MAX		16
#define DHCP_BIND_FREE		0
#define DHCP_BIND_PENDING	1
#define DHCP_BIND_BOUND		2
#define DHCP_BIND_PORT_NONE	0xff
#define DHCP_PENDING_SECS	60
#define DHCP_LEASE_INFINITE	0xffffffff

struct dhcp_binding {
	uint8_t  mac[6];
	uint8_t  ip[4];
	uint16_t vid;
	uint8_t  port;
	uint8_t  state;
	uint32_t secs;
};

extern __xdata uint8_t  dhcp_snoop_on;
extern __xdata uint16_t dhcp_snoop_trust;
extern __xdata struct dhcp_binding dhcp_bind[DHCP_BIND_MAX];

void dhcp_snoop_apply(void) __banked;
void dhcp_snoop_in(void) __banked;
void dhcp_snoop_tick(void) __banked;
void dhcp_snoop_show(void) __banked;

#endif
