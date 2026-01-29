/*
 * This is a DHCP client implementation for the RTL837x-based switches
 */

// #define REGDBG
// #define DEBUG

#include <stdint.h>
#include "rtl837x_sfr.h"
#include "rtl837x_common.h"
#include "dhcp.h"
#include "uip.h"
#include "uip/uip.h"

extern __code struct uip_eth_addr uip_ethaddr;
__xdata struct dhcp_state dhcp_state;
__xdata uip_ipaddr_t server;

#define BOOTP_REQUEST		1
#define BOOTP_REPY		2

#define DHCP_HW_TYPE_ETH	1

#define DHCP_SUBNET_MASK 	1
#define DHCP_SUBNET_MASK_LEN 	4
#define DHCP_ROUTER	 	3
#define DHCP_ROUTER_LEN 	4
#define DHCP_DNS	 	6
#define DHCP_DNS_LEN	 	4
#define DHCP_BROADCAST 		28
#define DHCP_BROADCAST_LEN	4
#define DHCP_SERVER_ID		54
#define DHCP_SERVER_ID_LEN	4
#define DHCP_MESSAGE_TYPE 	53
#define DHCP_MESSAGE_TYPE_LEN	1
#define DHCP_MESSAGE_DISCOVER	1
#define DHCP_MESSAGE_OFFER	2
#define DHCP_MESSAGE_REQUEST	3
#define DHCP_MESSAGE_NACK	4
#define DHCP_MESSAGE_ACK	5
#define DHCP_LEASE 		51
#define DHCP_LEASE_LEN		4
#define DHCP_RENEWAL 		58
#define DHCP_RENEWAL_LEN	4
#define DHCP_REBIND 		59
#define DHCP_REBIND_LEN		4
#define DHCP_CLIENT_ID		61
#define DHCP_CLIENT_ID_LEN	7
#define DHCP_REQUEST_IP		50
#define DHCP_REQUEST_IP_LEN	4
#define DHCP_CLIENT_NAME	12
#define DHCP_PARAMS		55
#define DHCP_VENDOR_ID		60
#define DHCP_CLIENT_ID		61
#define DHCP_PARAM_SUBNET	1
#define DHCP_PARAM_ROUTER	3
#define DHCP_PARAM_DNS		6
#define DHCP_END		255

#define LEASE_TIME		43200
#define RENEWAL_TIME		21600
#define REBIND_TIME		21600

#pragma codeseg BANK2
#pragma constseg BANK2


struct dhcp_pkt {
	uint8_t type;
	uint8_t hw;
	uint8_t hw_len;
	uint8_t hops;
	uint32_t tid;
	uint16_t delay;
	uint16_t flags;
	uint8_t client_ip[4];
	uint8_t your_ip[4];
	uint8_t next_server_ip[4];
	uint8_t relay_ip[4];
	uint8_t client_addr[6];
	uint8_t client_pad[10];
	uint8_t server_name[64];
	uint8_t file[128];
	uint8_t cookie[4];
};

#define DHCP_P ((__xdata struct dhcp_pkt *)uip_appdata)
#define DHCP_OPT ((__xdata uint8_t *)(uip_appdata) + sizeof (struct dhcp_pkt))

__xdata uint32_t long_value;
__xdata struct dhcpd_cstate cstates[DHCPD_MAX_CLIENTS];
__xdata uint8_t client_idx;

void dhcp_print_ip(uint8_t *a)
{
	itoa(a[0]); write_char('.');
	itoa(a[1]); write_char('.');
	itoa(a[2]); write_char('.');
	itoa(a[3]);
}


void dhcp_prepare_msg(void)
{
	DHCP_P->type = BOOTP_REQUEST;
	DHCP_P->hw = DHCP_HW_TYPE_ETH;
	DHCP_P->hw_len = 6;
	DHCP_P->hops = 0;

	DHCP_P->tid = dhcp_state.transaction_id;  // In network byte order
	DHCP_P->delay = HTONS(0);
	DHCP_P->flags = 0;
	// Clear fields client_ip to bootp_file
	memset(DHCP_P->client_ip, 0, 224);
	memcpyc(DHCP_P->client_addr, uip_ethaddr.addr, 6);
	DHCP_P->cookie[0] = 0x63;
	DHCP_P->cookie[1] = 0x82;
	DHCP_P->cookie[2] = 0x53;
	DHCP_P->cookie[3] = 0x63;
}


void dhcp_addopt_client_id(void)
{
	DHCP_OPT[dhcp_state.opt_ptr++] = DHCP_CLIENT_ID;
	DHCP_OPT[dhcp_state.opt_ptr++] = DHCP_CLIENT_ID_LEN;
	DHCP_OPT[dhcp_state.opt_ptr++] = DHCP_HW_TYPE_ETH;
	memcpyc(&DHCP_OPT[dhcp_state.opt_ptr], uip_ethaddr.addr, 6);
	dhcp_state.opt_ptr += 6;
}


void dhcp_addopt_request_ip(void)
{
	DHCP_OPT[dhcp_state.opt_ptr++] = DHCP_REQUEST_IP;
	DHCP_OPT[dhcp_state.opt_ptr++] = DHCP_REQUEST_IP_LEN;
	DHCP_OPT[dhcp_state.opt_ptr++] = dhcp_state.current_ip[0];
	DHCP_OPT[dhcp_state.opt_ptr++] = dhcp_state.current_ip[1];
	DHCP_OPT[dhcp_state.opt_ptr++] = dhcp_state.current_ip[2];
	DHCP_OPT[dhcp_state.opt_ptr++] = dhcp_state.current_ip[3];
}


void dhcp_addopt_server_id(void)
{
	DHCP_OPT[dhcp_state.opt_ptr++] = DHCP_SERVER_ID;
	DHCP_OPT[dhcp_state.opt_ptr++] = DHCP_SERVER_ID_LEN;
	DHCP_OPT[dhcp_state.opt_ptr++] = dhcp_state.server[0];
	DHCP_OPT[dhcp_state.opt_ptr++] = dhcp_state.server[1];
	DHCP_OPT[dhcp_state.opt_ptr++] = dhcp_state.server[2];
	DHCP_OPT[dhcp_state.opt_ptr++] = dhcp_state.server[3];
}


void dhcp_addopt_subnet(void)
{
	DHCP_OPT[dhcp_state.opt_ptr++] = DHCP_SUBNET_MASK;
	DHCP_OPT[dhcp_state.opt_ptr++] = DHCP_SUBNET_MASK_LEN;
	DHCP_OPT[dhcp_state.opt_ptr++] = dhcp_state.subnet[0];
	DHCP_OPT[dhcp_state.opt_ptr++] = dhcp_state.subnet[1];
	DHCP_OPT[dhcp_state.opt_ptr++] = dhcp_state.subnet[2];
	DHCP_OPT[dhcp_state.opt_ptr++] = dhcp_state.subnet[3];
}


void dhcp_addopt_router(void)
{
	DHCP_OPT[dhcp_state.opt_ptr++] = DHCP_ROUTER;
	DHCP_OPT[dhcp_state.opt_ptr++] = DHCP_ROUTER_LEN;
	DHCP_OPT[dhcp_state.opt_ptr++] = dhcp_state.router[0];
	DHCP_OPT[dhcp_state.opt_ptr++] = dhcp_state.router[1];
	DHCP_OPT[dhcp_state.opt_ptr++] = dhcp_state.router[2];
	DHCP_OPT[dhcp_state.opt_ptr++] = dhcp_state.router[3];
}


void dhcp_addopt_lease(void)
{
	DHCP_OPT[dhcp_state.opt_ptr++] = DHCP_LEASE;
	DHCP_OPT[dhcp_state.opt_ptr++] = DHCP_LEASE_LEN;
	DHCP_OPT[dhcp_state.opt_ptr++] = 0;
	DHCP_OPT[dhcp_state.opt_ptr++] = 0;
	DHCP_OPT[dhcp_state.opt_ptr++] = LEASE_TIME >> 8;
	DHCP_OPT[dhcp_state.opt_ptr++] = LEASE_TIME & 0xff;
}


void dhcp_addopt_renewal(void)
{
	DHCP_OPT[dhcp_state.opt_ptr++] = DHCP_RENEWAL;
	DHCP_OPT[dhcp_state.opt_ptr++] = DHCP_RENEWAL_LEN;
	DHCP_OPT[dhcp_state.opt_ptr++] = 0;
	DHCP_OPT[dhcp_state.opt_ptr++] = 0;
	DHCP_OPT[dhcp_state.opt_ptr++] = RENEWAL_TIME >> 8;
	DHCP_OPT[dhcp_state.opt_ptr++] = RENEWAL_TIME & 0xff;
}


void dhcp_addopt_rebind(void)
{
	DHCP_OPT[dhcp_state.opt_ptr++] = DHCP_REBIND;
	DHCP_OPT[dhcp_state.opt_ptr++] = DHCP_REBIND_LEN;
	DHCP_OPT[dhcp_state.opt_ptr++] = 0;
	DHCP_OPT[dhcp_state.opt_ptr++] = 0;
	DHCP_OPT[dhcp_state.opt_ptr++] = REBIND_TIME >> 8;
	DHCP_OPT[dhcp_state.opt_ptr++] = REBIND_TIME & 0xff;
}


void dhcp_send_discover(void)
{
	print_string("dhcp_send_discover called\n");
	dhcp_prepare_msg();

	dhcp_state.opt_ptr = 0;
	DHCP_OPT[dhcp_state.opt_ptr++] = DHCP_MESSAGE_TYPE;
	DHCP_OPT[dhcp_state.opt_ptr++] = DHCP_MESSAGE_TYPE_LEN;
	DHCP_OPT[dhcp_state.opt_ptr++] = DHCP_MESSAGE_DISCOVER;

	dhcp_addopt_client_id();
	dhcp_addopt_request_ip();

	DHCP_OPT[dhcp_state.opt_ptr++] = DHCP_PARAMS;
	DHCP_OPT[dhcp_state.opt_ptr++] = 3;
	DHCP_OPT[dhcp_state.opt_ptr++] = DHCP_PARAM_SUBNET;
	DHCP_OPT[dhcp_state.opt_ptr++] = DHCP_PARAM_ROUTER;
	DHCP_OPT[dhcp_state.opt_ptr++] = DHCP_PARAM_DNS;

	DHCP_OPT[dhcp_state.opt_ptr++] = DHCP_END;
	// Padding to 300 bytes
	DHCP_OPT[dhcp_state.opt_ptr++] = 0;
	DHCP_OPT[dhcp_state.opt_ptr++] = 0;
	DHCP_OPT[dhcp_state.opt_ptr++] = 0;
	DHCP_OPT[dhcp_state.opt_ptr++] = 0;
	DHCP_OPT[dhcp_state.opt_ptr++] = 0;
	DHCP_OPT[dhcp_state.opt_ptr++] = 0;

	uip_udp_send(sizeof(struct dhcp_pkt) + dhcp_state.opt_ptr);
	dhcp_state.state = DHCP_DISCOVER_SENT;
	dhcp_state.ticks = SYS_TICK_HZ;
	dhcp_state.dhcp_timer = 30; // Timeout for discover
}


void dhcp_send_request(void)
{
	print_string("dhcp_send_request called\n");
	dhcp_prepare_msg();

	dhcp_state.opt_ptr = 0;
	DHCP_OPT[dhcp_state.opt_ptr++] = DHCP_MESSAGE_TYPE;
	DHCP_OPT[dhcp_state.opt_ptr++] = DHCP_MESSAGE_TYPE_LEN;
	DHCP_OPT[dhcp_state.opt_ptr++] = DHCP_MESSAGE_REQUEST;

	dhcp_addopt_client_id();
	dhcp_addopt_request_ip();
	dhcp_addopt_server_id();

	DHCP_OPT[dhcp_state.opt_ptr++] = DHCP_PARAMS;
	DHCP_OPT[dhcp_state.opt_ptr++] = 3;
	DHCP_OPT[dhcp_state.opt_ptr++] = DHCP_PARAM_SUBNET;
	DHCP_OPT[dhcp_state.opt_ptr++] = DHCP_PARAM_ROUTER;
	DHCP_OPT[dhcp_state.opt_ptr++] = DHCP_PARAM_DNS;

	DHCP_OPT[dhcp_state.opt_ptr++] = DHCP_END;
	// Padding to 300 bytes
	DHCP_OPT[dhcp_state.opt_ptr++] = 0;
	DHCP_OPT[dhcp_state.opt_ptr++] = 0;
	DHCP_OPT[dhcp_state.opt_ptr++] = 0;
	DHCP_OPT[dhcp_state.opt_ptr++] = 0;
	DHCP_OPT[dhcp_state.opt_ptr++] = 0;
	DHCP_OPT[dhcp_state.opt_ptr++] = 0;

	uip_udp_send(sizeof(struct dhcp_pkt) + dhcp_state.opt_ptr);
	dhcp_state.state = DHCP_REQUEST_SENT;
	dhcp_state.ticks = SYS_TICK_HZ;
	dhcp_state.dhcp_timer = 30; // Timeout for request
}


void dhcp_send_reply(uint8_t rtype)
{
	print_string("dhcp_send_reply called\n");
	dhcp_prepare_msg();
	DHCP_P->type = BOOTP_REPY;
	DHCP_P->client_addr[0] = cstates[client_idx].mac[0]; DHCP_P->client_addr[1] = cstates[client_idx].mac[1];
	DHCP_P->client_addr[2] = cstates[client_idx].mac[2]; DHCP_P->client_addr[3] = cstates[client_idx].mac[3];
	DHCP_P->client_addr[4] = cstates[client_idx].mac[4]; DHCP_P->client_addr[5] = cstates[client_idx].mac[5];

	if (rtype != DHCP_MESSAGE_NACK) {
		DHCP_P->your_ip[0] = dhcp_state.server[0];
		DHCP_P->your_ip[1] = dhcp_state.server[1];
		DHCP_P->your_ip[2] = dhcp_state.server[2];
		DHCP_P->your_ip[3] = DHCPD_START_IP + client_idx;
	}

	dhcp_state.opt_ptr = 0;
	DHCP_OPT[dhcp_state.opt_ptr++] = DHCP_MESSAGE_TYPE;
	DHCP_OPT[dhcp_state.opt_ptr++] = DHCP_MESSAGE_TYPE_LEN;
	DHCP_OPT[dhcp_state.opt_ptr++] = rtype;

	if (rtype != DHCP_MESSAGE_NACK) {
		dhcp_addopt_subnet();
		dhcp_addopt_router();
		dhcp_addopt_server_id();
		dhcp_addopt_rebind();
		dhcp_addopt_lease();
		dhcp_addopt_renewal();
	}

	DHCP_OPT[dhcp_state.opt_ptr++] = DHCP_END;
	uip_udp_send(sizeof(struct dhcp_pkt) + dhcp_state.opt_ptr);
}


void ip_opt(uint8_t * __xdata ip)
{
	dhcp_state.opt_ptr++;
	uint8_t len = DHCP_OPT[dhcp_state.opt_ptr++];
	*ip++ = DHCP_OPT[dhcp_state.opt_ptr++];
	*ip++ = DHCP_OPT[dhcp_state.opt_ptr++];
	*ip++ = DHCP_OPT[dhcp_state.opt_ptr++];
	*ip++ = DHCP_OPT[dhcp_state.opt_ptr++];
	 // There may be more than one IP option, such as 2 DNS servers advertised
	dhcp_state.opt_ptr += len - 4;
}


void long_opt(void)
{
	dhcp_state.opt_ptr++;
	dhcp_state.opt_ptr++;
	long_value =  DHCP_OPT[dhcp_state.opt_ptr++];
	long_value <<= 8;
	long_value |= DHCP_OPT[dhcp_state.opt_ptr++];
	long_value <<= 8;
	long_value |= DHCP_OPT[dhcp_state.opt_ptr++];
	long_value <<= 8;
	long_value |= DHCP_OPT[dhcp_state.opt_ptr++];
}


void print_txt_opt(void)
{
	dhcp_state.opt_ptr++;
	for (uint8_t l = DHCP_OPT[dhcp_state.opt_ptr++]; l ; l--)
		write_char(DHCP_OPT[dhcp_state.opt_ptr++]);
}


void print_eth_opt(void)
{
	dhcp_state.opt_ptr++;
	for (uint8_t l = DHCP_OPT[dhcp_state.opt_ptr++]; l ; l--)
		print_byte(DHCP_OPT[dhcp_state.opt_ptr++]);
}


void parse_opts(void)
{
	while (DHCP_OPT[dhcp_state.opt_ptr] && DHCP_OPT[dhcp_state.opt_ptr] != DHCP_END) {
		switch(DHCP_OPT[dhcp_state.opt_ptr]) {
		case DHCP_SUBNET_MASK:
			ip_opt(&dhcp_state.subnet[0]);
			break;
		case DHCP_ROUTER:
			ip_opt(&dhcp_state.router[0]);
			break;
		case DHCP_DNS:
			ip_opt(&dhcp_state.dns[0]);
			break;
		case DHCP_SERVER_ID:
			ip_opt(&dhcp_state.server[0]);
			break;
		case DHCP_BROADCAST:
			ip_opt(&dhcp_state.broadcast[0]);
			break;
		case DHCP_REQUEST_IP:
			ip_opt(&dhcp_state.current_ip[0]);
			break;
		case DHCP_LEASE:
			long_opt();
			dhcp_state.lease = long_value;
			break;
		case DHCP_REBIND:
			long_opt();
			dhcp_state.rebind = long_value;
			break;
		case DHCP_RENEWAL:
			long_opt();
			dhcp_state.renewal = long_value;
			break;
		case DHCP_CLIENT_NAME:
			print_string("Client name: ");
			print_txt_opt();
			write_char('\n');
			break;
		case DHCP_VENDOR_ID:
			print_string("Vendor ID: ");
			print_txt_opt();
			write_char('\n');
			break;
		case DHCP_CLIENT_ID:
			print_string("Client ID: ");
			print_eth_opt();
			write_char('\n');
			break;
		case DHCP_PARAMS:
			print_string("PARAMS request (ignored)\n");
			dhcp_state.opt_ptr++;
			dhcp_state.opt_ptr += DHCP_OPT[dhcp_state.opt_ptr];
			dhcp_state.opt_ptr++;
			break;
		case DHCP_END:
			break;
		default:
			print_string("Unknown DHCP option: "); print_byte(DHCP_OPT[dhcp_state.opt_ptr]); write_char('\n');
			dhcp_state.opt_ptr++;
			dhcp_state.opt_ptr += DHCP_OPT[dhcp_state.opt_ptr];
			dhcp_state.opt_ptr++;
		}
	}
}


void find_client(void)
{
	uint8_t i;
	for (i = 0; i < DHCPD_MAX_CLIENTS; i++) {
		if (cstates[i].mac[0] == DHCP_P->client_addr[0] && cstates[i].mac[1] == DHCP_P->client_addr[1]
			&& cstates[i].mac[2] == DHCP_P->client_addr[2] && cstates[i].mac[3] == DHCP_P->client_addr[3]
			&& cstates[i].mac[4] == DHCP_P->client_addr[4] && cstates[i].mac[5] == DHCP_P->client_addr[5]
		)
			break;
	}
	if (i < DHCPD_MAX_CLIENTS) {
		client_idx = i;
		return;
	}
	
	client_idx = 255;
}


void find_slot(void)
{
	for (client_idx = 0; client_idx < DHCPD_MAX_CLIENTS; client_idx++) {
		if (!cstates[client_idx].cstate)
			return;
	}
	client_idx = 255;
	return;
}


void parse_dhcp_response(void)
{
	if (!DHCP_P->tid == dhcp_state.transaction_id)
		return;
	if (DHCP_P->cookie[0] != 0x63 || DHCP_P->cookie[1] != 0x82 || DHCP_P->cookie[2] != 0x53 || DHCP_P->cookie[3] != 0x63)
		return;

	dhcp_state.opt_ptr = 0;
	if (DHCP_OPT[dhcp_state.opt_ptr++] != DHCP_MESSAGE_TYPE || DHCP_OPT[dhcp_state.opt_ptr++] != DHCP_MESSAGE_TYPE_LEN)
		return;
	if (DHCP_OPT[dhcp_state.opt_ptr] == DHCP_MESSAGE_OFFER) {
		dhcp_state.opt_ptr++;
		dhcp_state.current_ip[0] = DHCP_P->your_ip[0];
		dhcp_state.current_ip[1] = DHCP_P->your_ip[1];
		dhcp_state.current_ip[2] = DHCP_P->your_ip[2];
		dhcp_state.current_ip[3] = DHCP_P->your_ip[3];
		parse_opts();
		print_string("DHCP offer received for IP "); dhcp_print_ip(dhcp_state.current_ip);
		write_char('\n');
		dhcp_send_request();
	} else if (DHCP_OPT[dhcp_state.opt_ptr++] == DHCP_MESSAGE_ACK) {
		parse_opts();
		print_string("DHCP ACK, our IP is "); dhcp_print_ip(dhcp_state.current_ip);
		write_char('\n');
		print_string("DHCP netmask "); dhcp_print_ip(dhcp_state.subnet);
		write_char('\n');
		print_string("DHCP gateway "); dhcp_print_ip(dhcp_state.router);
		write_char('\n');
		print_string("DHCP lease-time ");
		print_long(dhcp_state.lease);
		write_char('\n');
		uip_ipaddr(&uip_hostaddr, dhcp_state.current_ip[0], dhcp_state.current_ip[1], dhcp_state.current_ip[2], dhcp_state.current_ip[3]);
		uip_ipaddr(&uip_draddr, dhcp_state.router[0], dhcp_state.router[1], dhcp_state.router[2], dhcp_state.router[3]);
		uip_ipaddr(&uip_netmask, dhcp_state.subnet[0], dhcp_state.subnet[1], dhcp_state.subnet[2], dhcp_state.subnet[3]);
		dhcp_state.state = DHCP_LEASING;
		dhcp_state.ticks = SYS_TICK_HZ;
		dhcp_state.dhcp_timer = dhcp_state.renewal > 0xffff ? 0xffff : dhcp_state.renewal;
	}
}


void parse_dhcp_request(void)
{
	print_string("parse_dhcp_request called\n");
	if (DHCP_P->cookie[0] != 0x63 || DHCP_P->cookie[1] != 0x82 || DHCP_P->cookie[2] != 0x53 || DHCP_P->cookie[3] != 0x63)
		return;

	dhcp_state.opt_ptr = 0;
	if (DHCP_OPT[dhcp_state.opt_ptr++] != DHCP_MESSAGE_TYPE || DHCP_OPT[dhcp_state.opt_ptr++] != DHCP_MESSAGE_TYPE_LEN)
		return;
	if (DHCP_OPT[dhcp_state.opt_ptr] == DHCP_MESSAGE_DISCOVER) {
		dhcp_state.opt_ptr++;
		find_client();
		if (client_idx == 255)
			find_slot();
		// If there is no empty slot, we play possum and do not answer to the request
		if (client_idx == 255)
			return;

		cstates[client_idx].cstate = CSTATE_OFFERED;
		cstates[client_idx].mac[0] = DHCP_P->client_addr[0]; cstates[client_idx].mac[1] = DHCP_P->client_addr[1];
		cstates[client_idx].mac[2] = DHCP_P->client_addr[2]; cstates[client_idx].mac[3] = DHCP_P->client_addr[3];
		cstates[client_idx].mac[4] = DHCP_P->client_addr[4]; cstates[client_idx].mac[5] = DHCP_P->client_addr[5];
		dhcp_state.transaction_id = DHCP_P->tid;
		parse_opts();
		dhcp_send_reply(DHCP_MESSAGE_OFFER);
	} else if (DHCP_OPT[dhcp_state.opt_ptr++] == DHCP_MESSAGE_REQUEST) {
		find_client();
		if (client_idx == 255) {
			dhcp_send_reply(DHCP_MESSAGE_NACK);
			return;
		}
		parse_opts();
		dhcp_send_reply(DHCP_MESSAGE_ACK);
	}
}


void dhcp_start(void) __banked
{
	uip_ipaddr(server, 255,255,255,255);
	dhcp_state.conn = uip_udp_new(&server, HTONS(DHCP_SERVER_PORT));
	dhcp_state.current_ip[0] = dhcp_state.current_ip[1] = dhcp_state.current_ip[2] = dhcp_state.current_ip[3] = 0;
	if(dhcp_state.conn) {
		uip_udp_bind(dhcp_state.conn, HTONS(DHCP_CLIENT_PORT));
	} else {
		print_string("dhcp_start failed to set up socket\n");
		return;
	}
	get_random_32();
	dhcp_state.transaction_id = SFR_DATA_U32;
	dhcp_state.state = DHCP_START;
	print_string("dhcp_start done\n");
}


void dhcpd_start(void) __banked
{
	memset(&cstates[0], 0, sizeof (struct dhcpd_cstate) * DHCPD_MAX_CLIENTS);
	dhcp_state.conn = uip_udp_new(0, 0);
	if(dhcp_state.conn) {
		uip_udp_bind(dhcp_state.conn, HTONS(DHCP_SERVER_PORT));
	} else {
		print_string("dhcpd_start failed to set up socket\n");
		return;
	}
	dhcp_state.state = DHCP_SERVER;

	dhcp_state.server[1] = uip_hostaddr[0] >> 8; dhcp_state.server[0] = uip_hostaddr[0] & 0xff;
	dhcp_state.server[3] = uip_hostaddr[1] >> 8; dhcp_state.server[2] = uip_hostaddr[1] & 0xff;

	dhcp_state.router[1] = uip_draddr[0] >> 8; dhcp_state.router[0] = uip_draddr[0] & 0xff;
	dhcp_state.router[3] = uip_draddr[1] >> 8; dhcp_state.router[2] = uip_draddr[1] & 0xff;

	dhcp_state.subnet[1] = uip_netmask[0] >> 8; dhcp_state.subnet[0] = uip_netmask[0] & 0xff;
	dhcp_state.subnet[3] = uip_netmask[1] >> 8; dhcp_state.subnet[2] = uip_netmask[1] & 0xff;

	dhcp_state.broadcast[0] = dhcp_state.router[0]; dhcp_state.broadcast[1] = dhcp_state.router[1];
	dhcp_state.broadcast[2] = dhcp_state.router[2]; dhcp_state.broadcast[3] = 0xff;

	for (uint8_t i = 0; i < DHCPD_MAX_CLIENTS; i++) {
		cstates[i].cstate = CSTATE_NONE;
	}
	// TODO: DNS, correct broadcast address

	print_string("dhcpd_start done\n");
}


void dhcp_stop(void) __banked
{
	print_string("dhcp_stop called\n");
	uip_udp_remove(dhcp_state.conn);
	dhcp_state.state = DHCP_OFF;
}

void dhcpd_stop(void) __banked
{
	print_string("dhcpd_stop called\n");
	uip_udp_remove(dhcp_state.conn);
	dhcp_state.state = DHCP_OFF;
}


void dhcp_callback(void) __banked
{
	if (!dhcp_state.state)
		return;
	if (uip_closed()) {
		print_string("Closed\n");
		return;
	} else if (dhcp_state.state == DHCP_SERVER && uip_newdata()) {
		parse_dhcp_request();
	} else if (uip_newdata()) {
		parse_dhcp_response();
	} else {
		if (dhcp_state.state == DHCP_START) {
			dhcp_send_discover();
		} else if (!--dhcp_state.ticks) {
//			print_string("Timer: "); print_short(dhcp_state.ticks); write_char(' '); print_short(dhcp_state.dhcp_timer);
			dhcp_state.dhcp_timer--;
			dhcp_state.ticks = SYS_TICK_HZ;
		}
		if (!dhcp_state.dhcp_timer) {
			switch (dhcp_state.state) {
			case DHCP_DISCOVER_SENT:
				dhcp_send_discover();
				break;
			case DHCP_LEASING:
			case DHCP_REQUEST_SENT:
				dhcp_send_request();
				break;
			default:
				print_string("UNKNOWN STATE\n");
			}
		}
	}
}
