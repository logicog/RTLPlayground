/*
 * This is a DHCP client implementation for the RTL837x-based switches
 */

// #define REGDBG
// #define DEBUG

#include <stdint.h>
#include "rtl837x_common.h"
#include "dhcp.h"
#include "uip.h"

extern __code struct uip_eth_addr uip_ethaddr;
__xdata uint8_t dhcp_state;
__xdata uint16_t dhcp_timer;
__xdata uint16_t opt_ptr;

#define DHCP_HW_TYPE_ETH	1

#define DHCP_MESSAGE_TYPE 	53
#define DHCP_MESSAGE_TYPE_LEN	1
#define DHCP_MESSAGE_DISCOVER	1
#define DHCP_CLIENT_ID		61
#define DHCP_CLIENT_ID_LEN	7
#define DHCP_REQUEST_IP		50
#define DHCP_REQUEST_IP_LEN	4
#define DHCP_PARAMS		55
#define DHCP_PARAM_SUBNET	1
#define DHCP_PARAM_ROUTER	3
#define DHCP_PARAM_DNS		6
#define DHCP_END		255

#pragma codeseg BANK1
#pragma constseg BANK1

struct dhcp_pkt {
	uint8_t ipv4mc_addr[6];
	uint8_t src_addr[6];
	struct rtl_tag rtl_tag;
	uint16_t ipv4_tag;
	uint8_t hlen;
	uint8_t dscp;
	uint16_t len;
	uint16_t id; // ID filled in by UIP when sending
	uint16_t flags;
	uint8_t ttl;
	uint8_t protocol;
	uint16_t checksum;  // Filled in by the ASIC
	uint8_t src_ip[4];
	uint8_t dst_ip[4];
	uint16_t src_port;
	uint16_t dst_port;
	uint16_t ip_len;
	uint16_t ip_checksum;
	uint8_t bootp_type;
	uint8_t bootp_hw;
	uint8_t bootp_hw_len;
	uint8_t bootp_hops;
	uint32_t bootp_tid;
	uint16_t bootp_delay;
	uint16_t bootp_flags;
	uint8_t bootp_client_ip[4];
	uint8_t bootp_your_ip[4];
	uint8_t bootp_next_server_ip[4];
	uint8_t bootp_relay_ip[4];
	uint8_t bootp_client_addr[6];
	uint8_t bootp_client_pad[10];
	uint8_t bootp_server_name[64];
	uint8_t bootp_file[128];
	uint8_t bootp_cookie[4];
};

#define DHCP_P ((__xdata struct dhcp_pkt *)&uip_buf[0])
#define DHCP_OPT ((__xdata uint8_t *)&uip_buf[sizeof (struct dhcp_pkt)])


void dhcp_send_discover(void)
{
	memset(DHCP_P->ipv4mc_addr, 0xff, 6);
	memcpyc(DHCP_P->src_addr, uip_ethaddr.addr, 6);
	
	DHCP_P->rtl_tag.tag = HTONS(0x8899);
	DHCP_P->rtl_tag.version = 0x04;
	DHCP_P->rtl_tag.reason = 0x00;
	DHCP_P->rtl_tag.flags = 0x0020; // Disable L2 learning
	DHCP_P->rtl_tag.pmask = HTONS(PMASK_9);
	DHCP_P->ipv4_tag = HTONS(0x0800);
	DHCP_P->hlen = 0x45;
	DHCP_P->len = HTONS(0x012c);
	DHCP_P->ttl = 0x80;
	DHCP_P->protocol = 0x11; // UDP
	DHCP_P->src_ip[0] = DHCP_P->src_ip[1] = DHCP_P->src_ip[2] = DHCP_P->src_ip[3] = 0;
	DHCP_P->dst_ip[0] = DHCP_P->dst_ip[1] = DHCP_P->dst_ip[2] = DHCP_P->dst_ip[3] = 0xff;
	DHCP_P->src_port = HTONS(68);
	DHCP_P->dst_port = HTONS(67);
	DHCP_P->ip_len = HTONS(280);
	DHCP_P->bootp_type = 1;
	DHCP_P->bootp_hw = DHCP_HW_TYPE_ETH;
	DHCP_P->bootp_hw = 6;
	DHCP_P->bootp_hops = 0;
	DHCP_P->bootp_tid = 0;
	DHCP_P->bootp_delay = HTONS(0);
	DHCP_P->bootp_flags = 0;
	// Clear fields bootp_client_ip to bootp_file
	memset(DHCP_P->bootp_client_ip, 0, 224);
	memcpyc(DHCP_P->bootp_client_addr, uip_ethaddr.addr, 6);
	DHCP_P->bootp_cookie[0] = 0x63;
	DHCP_P->bootp_cookie[1] = 0x82;
	DHCP_P->bootp_cookie[2] = 0x53;
	DHCP_P->bootp_cookie[3] = 0x63;

	opt_ptr = 0;
	DHCP_OPT[opt_ptr++] = DHCP_MESSAGE_TYPE;
	DHCP_OPT[opt_ptr++] = DHCP_MESSAGE_TYPE_LEN;
	DHCP_OPT[opt_ptr++] = DHCP_MESSAGE_DISCOVER;

	DHCP_OPT[opt_ptr++] = DHCP_CLIENT_ID;
	DHCP_OPT[opt_ptr++] = DHCP_HW_TYPE_ETH;
	DHCP_OPT[opt_ptr++] = DHCP_CLIENT_ID_LEN;
	memcpyc(&DHCP_OPT[opt_ptr], uip_ethaddr.addr, 6);
	opt_ptr += 6;

	DHCP_OPT[opt_ptr++] = DHCP_REQUEST_IP;
	DHCP_OPT[opt_ptr++] = DHCP_REQUEST_IP_LEN;
	memcpyc(&DHCP_OPT[opt_ptr], uip_ethaddr.addr, 6);
	opt_ptr += 6;
	
	DHCP_OPT[opt_ptr++] = DHCP_PARAMS;
	DHCP_OPT[opt_ptr++] = 3;
	DHCP_OPT[opt_ptr++] = DHCP_PARAM_SUBNET;
	DHCP_OPT[opt_ptr++] = DHCP_PARAM_ROUTER;
	DHCP_OPT[opt_ptr++] = DHCP_PARAM_DNS;

	DHCP_OPT[opt_ptr++] = DHCP_END;

	uip_len = sizeof(struct dhcp_pkt) + opt_ptr;
	tcpip_output();
	dhcp_state = DHCP_DISCOVER_SENT;
	dhcp_timer = 10; // Timeout for discover
}


void dhcp_start(void) __banked
{
	print_string("dhcp_start called\n");
	dhcp_state= DHCP_START;
	dhcp_send_discover();
}

void dhcp_stop(void) __banked
{
	print_string("dhcp_stop called\n");
	dhcp_state = DHCP_OFF;
}


void dhcp_periodic(void) __banked
{
	print_string("dhcp_periodic called\n");
	// Check for timer timeout
	if (!--dhcp_timer) {
		switch (dhcp_state) {
		case DHCP_DISCOVER_SENT:
			dhcp_send_discover();
			break;
		default:
			print_string("UNKNOWN STATE\n");
		}
	}
}


void dhcp_packet_handler(void) __banked
{
	// By default we do not send anything out
	uip_len = 0;

#ifdef DEBUG
	print_string("\nIPv4 DHCP packet:\n");
	for (uint8_t i = 0; i < 80; i++) {
		print_byte(uip_buf[i]);
		write_char(' ');
	}
	write_char('\n');
#endif
	if (DHCP_P->protocol != 0x11)
		return;
	print_string("Have DHCP packet\n");
}
