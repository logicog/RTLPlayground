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

__xdata struct dhcp_state dhcp_state;
__xdata struct dhcpd_state dhcpd_state;
__xdata uip_ipaddr_t server;
__xdata uip_ipaddr_t dhcpd_bcast;

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
#define DHCP_PARAMS		55
#define DHCP_PARAM_SUBNET	1
#define DHCP_PARAM_ROUTER	3
#define DHCP_PARAM_DNS		6
#define DHCP_END		255

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

/*
#define DHCP_P ((__xdata struct dhcp_pkt *)&uip_buf[RTL_TAG_SIZE + VLAN_TAG_SIZE])
#define DHCP_OPT ((__xdata uint8_t *)&uip_buf[RTL_TAG_SIZE + VLAN_TAG_SIZE + sizeof (struct dhcp_pkt)])
*/

#define DHCP_P ((__xdata struct dhcp_pkt *)uip_appdata)
#define DHCP_OPT ((__xdata uint8_t *)(uip_appdata) + sizeof (struct dhcp_pkt))

__xdata uint32_t long_value;


void dhcp_print_ip(uint8_t *a)
{
	itoa(a[0]); write_char('.');
	itoa(a[1]); write_char('.');
	itoa(a[2]); write_char('.');
	itoa(a[3]);
}


void dhcp_prepare_request(void)
{
	DHCP_P->type = 1;
	DHCP_P->hw = DHCP_HW_TYPE_ETH;
	DHCP_P->hw_len = 6;
	DHCP_P->hops = 0;

	DHCP_P->tid = HTONS(dhcp_state.transaction_id);
	DHCP_P->delay = HTONS(0);
	DHCP_P->flags = 0;
	// Clear fields client_ip to bootp_file
	memset(DHCP_P->client_ip, 0, 224);
	memcpy(DHCP_P->client_addr, uip_ethaddr.addr, 6);
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
	memcpy(&DHCP_OPT[dhcp_state.opt_ptr], uip_ethaddr.addr, 6);
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
	memcpy(&DHCP_OPT[dhcp_state.opt_ptr], uip_ethaddr.addr, 4);
}


void dhcp_addopt_server_id(void)
{
	DHCP_OPT[dhcp_state.opt_ptr++] = DHCP_SERVER_ID;
	DHCP_OPT[dhcp_state.opt_ptr++] = DHCP_SERVER_ID_LEN;
	DHCP_OPT[dhcp_state.opt_ptr++] = dhcp_state.server[0];
	DHCP_OPT[dhcp_state.opt_ptr++] = dhcp_state.server[1];
	DHCP_OPT[dhcp_state.opt_ptr++] = dhcp_state.server[2];
	DHCP_OPT[dhcp_state.opt_ptr++] = dhcp_state.server[3];
	memcpy(&DHCP_OPT[dhcp_state.opt_ptr], uip_ethaddr.addr, 4);
}


void dhcp_send_discover(void)
{
	print_string("dhcp_send_discover called\n");
	dhcp_prepare_request();

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
	dhcp_prepare_request();

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


void parse_dhcp(void)
{
	if (!DHCP_P->tid == HTONS(dhcp_state.transaction_id))
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


void dhcp_start(void) __banked
{
	uip_ipaddr(server, 255,255,255,255);
	dhcp_state.conn = uip_udp_new(&server, HTONS(DHCPC_SERVER_PORT));
	dhcp_state.current_ip[0] = dhcp_state.current_ip[1] = dhcp_state.current_ip[2] = dhcp_state.current_ip[3] = 0;
	if(dhcp_state.conn) {
		uip_udp_bind(dhcp_state.conn, HTONS(DHCPC_CLIENT_PORT));
	} else {
		print_string("dhcp_start failed to set up socket\n");
		return;
	}
	get_random_32();
	dhcp_state.transaction_id = SFR_DATA_U32;
	dhcp_state.state = DHCP_START;
	print_string("dhcp_start done\n");
}


void dhcp_stop(void) __banked
{
	print_string("dhcp_stop called\n");
	uip_udp_remove(dhcp_state.conn);
	dhcp_state.state = DHCP_OFF;
}


static void dhcpc_callback_inner(void)
{
	if (!dhcp_state.state)
		return;
	if (uip_closed()) {
		print_string("Closed\n");
		return;
	} else if (uip_newdata()) {
		parse_dhcp();
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

	// By default we do not send anything out
	uip_len = 0;
}


/*
 * Compare two 6 byte hardware addresses. Returns 0 when equal.
 */
static uint8_t mac_cmp(__xdata uint8_t *a, __xdata uint8_t *b)
{
	for (uint8_t i = 0; i < 6; i++)
		if (a[i] != b[i])
			return 1;
	return 0;
}


/*
 * Look up an existing lease for the given MAC address. Returns the
 * pool index, or 0xff if no lease exists.
 */
static uint8_t dhcpd_find_lease(__xdata uint8_t *mac)
{
	for (uint8_t i = 0; i < dhcpd_state.pool_count; i++) {
		if (dhcpd_state.leases[i].expires && !mac_cmp(dhcpd_state.leases[i].mac, mac))
			return i;
	}
	return 0xff;
}


/*
 * Allocate a new lease for the given MAC address. Prefers the first
 * entry whose lease has expired. Returns the pool index or 0xff when
 * the pool is full.
 */
static uint8_t dhcpd_alloc_lease(__xdata uint8_t *mac)
{
	for (uint8_t i = 0; i < dhcpd_state.pool_count; i++) {
		if (!dhcpd_state.leases[i].expires) {
			memcpy(dhcpd_state.leases[i].mac, mac, 6);
			dhcpd_state.leases[i].expires = dhcpd_state.lease_time;
			return i;
		}
	}
	return 0xff;
}


/*
 * Add an IPv4 option to the reply option buffer.
 */
static void dhcpd_addopt_ip(uint8_t code, __xdata uint8_t *ip)
{
	DHCP_OPT[dhcp_state.opt_ptr++] = code;
	DHCP_OPT[dhcp_state.opt_ptr++] = 4;
	DHCP_OPT[dhcp_state.opt_ptr++] = ip[0];
	DHCP_OPT[dhcp_state.opt_ptr++] = ip[1];
	DHCP_OPT[dhcp_state.opt_ptr++] = ip[2];
	DHCP_OPT[dhcp_state.opt_ptr++] = ip[3];
}


/*
 * Add a 32-bit option to the reply option buffer.
 */
static void dhcpd_addopt_u32(uint8_t code, uint32_t v)
{
	DHCP_OPT[dhcp_state.opt_ptr++] = code;
	DHCP_OPT[dhcp_state.opt_ptr++] = 4;
	DHCP_OPT[dhcp_state.opt_ptr++] = (v >> 24) & 0xff;
	DHCP_OPT[dhcp_state.opt_ptr++] = (v >> 16) & 0xff;
	DHCP_OPT[dhcp_state.opt_ptr++] = (v >> 8) & 0xff;
	DHCP_OPT[dhcp_state.opt_ptr++] = v & 0xff;
}


/*
 * Scan the options of an incoming DHCP packet and return the DHCP
 * message type (DISCOVER, REQUEST, RELEASE, ...). Returns 0 if no
 * message type option was found.
 */
static uint8_t dhcpd_parse_msg_type(void)
{
	uint16_t p = 0;
	while (DHCP_OPT[p] != DHCP_END) {
		uint8_t code = DHCP_OPT[p++];
		if (!code) // PAD option has no length
			continue;
		uint8_t len = DHCP_OPT[p++];
		if (code == DHCP_MESSAGE_TYPE && len == DHCP_MESSAGE_TYPE_LEN)
			return DHCP_OPT[p];
		p += len;
		if (p > 300) // Safety limit for malformed packets
			return 0;
	}
	return 0;
}


/*
 * Build a BOOTP reply (OFFER or ACK) for the client that issued the
 * current request. The chosen IP is taken from pool index idx.
 */
static void dhcpd_build_reply(uint8_t idx, uint8_t msg_type)
{
	// Preserve client MAC and transaction id before clearing the buffer
	__xdata uint32_t req_tid = DHCP_P->tid;
	__xdata uint8_t req_mac[6];
	memcpy(req_mac, DHCP_P->client_addr, 6);

	DHCP_P->type = 2; // BOOTREPLY
	DHCP_P->hw = DHCP_HW_TYPE_ETH;
	DHCP_P->hw_len = 6;
	DHCP_P->hops = 0;
	DHCP_P->tid = req_tid;
	DHCP_P->delay = 0;
	DHCP_P->flags = 0;
	// Clear all fields from client_ip through the end of the bootp file area
	memset(DHCP_P->client_ip, 0, 224);
	// your_ip = offered address (same /24 as our own address)
	DHCP_P->your_ip[0] = uip_hostaddr[0] & 0xff;
	DHCP_P->your_ip[1] = uip_hostaddr[0] >> 8;
	DHCP_P->your_ip[2] = uip_hostaddr[1] & 0xff;
	DHCP_P->your_ip[3] = dhcpd_state.pool_first + idx;
	memcpy(DHCP_P->client_addr, req_mac, 6);
	DHCP_P->cookie[0] = 0x63;
	DHCP_P->cookie[1] = 0x82;
	DHCP_P->cookie[2] = 0x53;
	DHCP_P->cookie[3] = 0x63;

	dhcp_state.opt_ptr = 0;
	DHCP_OPT[dhcp_state.opt_ptr++] = DHCP_MESSAGE_TYPE;
	DHCP_OPT[dhcp_state.opt_ptr++] = DHCP_MESSAGE_TYPE_LEN;
	DHCP_OPT[dhcp_state.opt_ptr++] = msg_type;

	// Server identifier = our IP address
	__xdata uint8_t our_ip[4];
	our_ip[0] = uip_hostaddr[0] & 0xff;
	our_ip[1] = uip_hostaddr[0] >> 8;
	our_ip[2] = uip_hostaddr[1] & 0xff;
	our_ip[3] = uip_hostaddr[1] >> 8;
	dhcpd_addopt_ip(DHCP_SERVER_ID, our_ip);

	// Subnet mask
	__xdata uint8_t mask[4];
	mask[0] = uip_netmask[0] & 0xff;
	mask[1] = uip_netmask[0] >> 8;
	mask[2] = uip_netmask[1] & 0xff;
	mask[3] = uip_netmask[1] >> 8;
	dhcpd_addopt_ip(DHCP_SUBNET_MASK, mask);

	// Default router = our IP
	dhcpd_addopt_ip(DHCP_ROUTER, our_ip);

	// Lease time
	dhcpd_addopt_u32(DHCP_LEASE, dhcpd_state.lease_time);

	DHCP_OPT[dhcp_state.opt_ptr++] = DHCP_END;
	// Pad reply to at least 300 bytes
	while (dhcp_state.opt_ptr < 60) {
		DHCP_OPT[dhcp_state.opt_ptr++] = 0;
	}

	uip_udp_send(sizeof(struct dhcp_pkt) + dhcp_state.opt_ptr);
}


/*
 * DHCP server request handler. Inspects the inbound message type and
 * either allocates a lease (OFFER) or confirms an existing one (ACK).
 * RELEASE and DECLINE messages free the corresponding lease.
 */
static void dhcpd_handle_request(void)
{
	if (DHCP_P->type != 1) // Must be BOOTREQUEST
		return;
	if (DHCP_P->cookie[0] != 0x63 || DHCP_P->cookie[1] != 0x82 ||
	    DHCP_P->cookie[2] != 0x53 || DHCP_P->cookie[3] != 0x63)
		return;

	__xdata uint8_t mac[6];
	memcpy(mac, DHCP_P->client_addr, 6);

	uint8_t msg_type = dhcpd_parse_msg_type();
	print_string("dhcpd RX type "); print_byte(msg_type); write_char('\n');

	switch (msg_type) {
	case DHCP_MESSAGE_DISCOVER: {
		uint8_t idx = dhcpd_find_lease(mac);
		if (idx == 0xff)
			idx = dhcpd_alloc_lease(mac);
		if (idx == 0xff) {
			print_string("dhcpd pool exhausted\n");
			return;
		}
		print_string("dhcpd OFFER .");
		print_byte(dhcpd_state.pool_first + idx); write_char('\n');
		dhcpd_build_reply(idx, DHCP_MESSAGE_OFFER);
		break;
	}
	case DHCP_MESSAGE_REQUEST: {
		uint8_t idx = dhcpd_find_lease(mac);
		if (idx == 0xff) {
			// Unknown client: allocate fresh
			idx = dhcpd_alloc_lease(mac);
		}
		if (idx == 0xff)
			return;
		dhcpd_state.leases[idx].expires = dhcpd_state.lease_time;
		print_string("dhcpd ACK .");
		print_byte(dhcpd_state.pool_first + idx); write_char('\n');
		dhcpd_build_reply(idx, DHCP_MESSAGE_ACK);
		break;
	}
	case 7: // DHCPRELEASE
	case 4: { // DHCPDECLINE
		uint8_t idx = dhcpd_find_lease(mac);
		if (idx != 0xff)
			dhcpd_state.leases[idx].expires = 0;
		break;
	}
	default:
		break;
	}
}


/*
 * Age out DHCP server leases. Called once per second from the main
 * idle loop when the DHCP server is running.
 */
void dhcpd_tick(void) __banked
{
	if (!dhcpd_state.enabled)
		return;
	for (uint8_t i = 0; i < dhcpd_state.pool_count; i++) {
		if (dhcpd_state.leases[i].expires)
			dhcpd_state.leases[i].expires--;
	}
}


void dhcpd_start(void) __banked
{
	if (dhcpd_state.enabled) {
		print_string("dhcpd already running\n");
		return;
	}
	if (!dhcpd_state.pool_count) {
		// Install a sensible default pool: .100-.107 on the same /24
		dhcpd_state.pool_first = 100;
		dhcpd_state.pool_count = 8;
	}
	if (!dhcpd_state.lease_time)
		dhcpd_state.lease_time = 3600; // 1 hour

	// Clear any leftover lease state
	for (uint8_t i = 0; i < DHCPD_POOL_MAX; i++)
		dhcpd_state.leases[i].expires = 0;

	uip_ipaddr(dhcpd_bcast, 255, 255, 255, 255);
	dhcpd_state.conn = uip_udp_new(&dhcpd_bcast, HTONS(DHCPC_CLIENT_PORT));
	if (!dhcpd_state.conn) {
		print_string("dhcpd failed to create UDP socket\n");
		return;
	}
	uip_udp_bind(dhcpd_state.conn, HTONS(DHCPC_SERVER_PORT));
	dhcpd_state.enabled = 1;
	print_string("dhcpd enabled, pool .");
	print_byte(dhcpd_state.pool_first); print_string(" +");
	print_byte(dhcpd_state.pool_count); write_char('\n');
}


void dhcpd_stop(void) __banked
{
	if (!dhcpd_state.enabled)
		return;
	if (dhcpd_state.conn)
		uip_udp_remove(dhcpd_state.conn);
	dhcpd_state.conn = 0;
	dhcpd_state.enabled = 0;
	print_string("dhcpd stopped\n");
}


void dhcpd_set_pool(uint8_t first_octet, uint8_t count) __banked
{
	if (count > DHCPD_POOL_MAX)
		count = DHCPD_POOL_MAX;
	dhcpd_state.pool_first = first_octet;
	dhcpd_state.pool_count = count;
	for (uint8_t i = 0; i < DHCPD_POOL_MAX; i++)
		dhcpd_state.leases[i].expires = 0;
}


void dhcpd_set_lease_time(uint32_t seconds) __banked
{
	dhcpd_state.lease_time = seconds;
}


uint8_t dhcpd_active_leases(void) __banked
{
	uint8_t n = 0;
	for (uint8_t i = 0; i < dhcpd_state.pool_count; i++) {
		if (dhcpd_state.leases[i].expires)
			n++;
	}
	return n;
}


static void dhcpd_callback_inner(void)
{
	if (!dhcpd_state.enabled)
		return;
	if (uip_newdata()) {
		dhcpd_handle_request();
	} else {
		// Make sure no stale data is sent out during periodic polls
		uip_len = 0;
	}
}


void dhcp_callback(void) __banked
{
	// Dispatch between DHCP client and DHCP server based on which
	// uIP UDP connection invoked the callback.
	if (dhcpd_state.enabled && uip_udp_conn == dhcpd_state.conn) {
		dhcpd_callback_inner();
		return;
	}
	dhcpc_callback_inner();
}
