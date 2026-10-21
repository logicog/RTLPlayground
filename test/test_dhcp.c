/*
 * Host-side test for the DHCP client's packet parser.
 *
 * Compiles the UNMODIFIED dhcp.c and feeds parse_dhcp() OFFER and ACK
 * packets built here. The receive buffer past the end of each packet holds
 * leftovers of an earlier frame, as uip_buf does on the switch, so an option
 * read past the packet shows up as a field taken from those leftovers.
 *
 * Run: make -C test    (exit code 0 = all checks pass)
 */
#include <stdio.h>
#include <string.h>

#undef HTONS
#undef NTOHS

#include "rtl837x_common.h"
#include "dhcp.h"
#include "uip.h"

/* timer.c reads it; the parser tests let no time pass */
volatile uint32_t ticks;

/* Defined in dhcp.c, which has no header entry for them */
extern struct dhcp_state dhcp_state;
void parse_dhcp(void);

static int failures;

#define CHECK(cond, name) do { \
	if (cond) printf("PASS  %s\n", name); \
	else { printf("FAIL  %s\n", name); failures++; } \
} while (0)

/* ---- environment below dhcp.c ------------------------------------------- */

#define FIXED_LEN	240	/* BOOTP header and magic cookie */
#define STALE		0xaa	/* what an earlier frame left in the buffer */

static uint8_t rxbuf[UIP_CONF_BUFFER_ALLOC_SIZE];
void *uip_appdata;
uint16_t uip_len;
uint8_t uip_flags;
struct uip_eth_addr uip_ethaddr = { { 0x1c, 0x2a, 0xa3, 0x1a, 0x6d, 0xec } };
uip_ipaddr_t uip_hostaddr, uip_draddr, uip_netmask;
char hostname[24] = "sw";
volatile uint8_t sfr_data[4];
volatile uint8_t SFR_DATA_0, SFR_DATA_8, SFR_DATA_16, SFR_DATA_24;
static int sends;

void uip_send(const void *data, uint16_t len) { (void)data; (void)len; sends++; }
struct uip_udp_conn *uip_udp_new(uip_ipaddr_t *ripaddr, uint16_t rport)
{
	static struct uip_udp_conn conn;
	(void)ripaddr; (void)rport;
	return &conn;
}
void get_random_32(void) { }
void print_string(const char *p) { (void)p; }
void print_byte(uint8_t a) { (void)a; }
void print_long(uint32_t a) { (void)a; }
void itoa(uint8_t v) { (void)v; }
void write_char(char c) { (void)c; }

/* ---- packet builder ------------------------------------------------------ */

static uint16_t olen;

static void pkt_start(uint8_t type)
{
	memset(rxbuf, STALE, sizeof(rxbuf));
	memset(rxbuf, 0, FIXED_LEN);
	rxbuf[0] = 2;					/* BOOTREPLY */
	memcpy(&rxbuf[4], &dhcp_state.transaction_id, 4);
	rxbuf[16] = 192; rxbuf[17] = 168; rxbuf[18] = 10; rxbuf[19] = 247;	/* yiaddr */
	rxbuf[236] = 0x63; rxbuf[237] = 0x82; rxbuf[238] = 0x53; rxbuf[239] = 0x63;
	olen = 0;
	rxbuf[FIXED_LEN + olen++] = 53;
	rxbuf[FIXED_LEN + olen++] = 1;
	rxbuf[FIXED_LEN + olen++] = type;
}

static void opt(const uint8_t *bytes, uint16_t n)
{
	memcpy(&rxbuf[FIXED_LEN + olen], bytes, n);
	olen += n;
}

/* Hands the packet to the parser, the buffer past its end left stale */
static void deliver(void)
{
	uip_appdata = rxbuf;
	uip_len = FIXED_LEN + olen;
	uip_flags = UIP_NEWDATA;
	parse_dhcp();
}

static void reset_client(void)
{
	memset(&dhcp_state, 0, sizeof(dhcp_state));
	dhcp_state.transaction_id = 0x12345678;
	dhcp_state.state = DHCP_REQUEST_SENT;
	sends = 0;
}

static int ip_is(const uint8_t *ip, uint8_t a, uint8_t b, uint8_t c, uint8_t d)
{
	return ip[0] == a && ip[1] == b && ip[2] == c && ip[3] == d;
}

/* ---- scenarios ----------------------------------------------------------- */

static const uint8_t subnet_24[] = { 1, 4, 255, 255, 255, 0 };
static const uint8_t router_1[] = { 3, 4, 192, 168, 10, 1 };
static const uint8_t lease_1h[] = { 51, 4, 0, 0, 0x0e, 0x10 };
static const uint8_t end[] = { 255 };

static void scenario_well_formed(void)
{
	reset_client();
	pkt_start(5);
	opt(subnet_24, sizeof(subnet_24));
	opt(router_1, sizeof(router_1));
	opt(lease_1h, sizeof(lease_1h));
	opt(end, sizeof(end));
	deliver();
	CHECK(ip_is(dhcp_state.subnet, 255, 255, 255, 0)
	      && ip_is(dhcp_state.router, 192, 168, 10, 1)
	      && dhcp_state.lease == 3600 && dhcp_state.state == DHCP_LEASING,
	      "dhcp: a well-formed ACK sets netmask, gateway and lease");
}

static void scenario_value_past_end(void)
{
	/* The packet ends inside the netmask option's value: its length byte
	 * says 4 but only two value bytes arrived */
	static const uint8_t cut[] = { 1, 4, 255, 255 };

	reset_client();
	memcpy(dhcp_state.subnet, (uint8_t[]){ 255, 255, 255, 0 }, 4);
	pkt_start(5);
	opt(router_1, sizeof(router_1));
	opt(cut, sizeof(cut));
	deliver();
	CHECK(ip_is(dhcp_state.subnet, 255, 255, 255, 0),
	      "dhcp: a netmask cut off by the packet end is not completed from stale bytes");
	CHECK(ip_is(dhcp_state.router, 192, 168, 10, 1),
	      "dhcp: options before a truncated one still apply");
}

static void scenario_length_past_end(void)
{
	/* No END option and the packet stops right after an option code: the
	 * length byte itself lies in the stale bytes */
	static const uint8_t code_only[] = { 3 };

	reset_client();
	pkt_start(5);
	opt(subnet_24, sizeof(subnet_24));
	opt(code_only, sizeof(code_only));
	deliver();
	CHECK(!ip_is(dhcp_state.router, STALE, STALE, STALE, STALE),
	      "dhcp: an option whose length byte is past the packet is ignored");
}

static void scenario_no_end_option(void)
{
	/* Options run to the packet end without END; the stale bytes after
	 * them would parse as option 0xaa, length 0xaa, and so on */
	reset_client();
	pkt_start(5);
	opt(router_1, sizeof(router_1));
	deliver();
	CHECK(ip_is(dhcp_state.router, 192, 168, 10, 1) && !dhcp_state.lease
	      && ip_is(dhcp_state.subnet, 0, 0, 0, 0),
	      "dhcp: parsing stops at the packet end when END is missing");
}

static void scenario_short_packet(void)
{
	reset_client();
	pkt_start(5);
	uip_appdata = rxbuf;
	uip_len = FIXED_LEN;	/* cookie but no message type option */
	uip_flags = UIP_NEWDATA;
	parse_dhcp();
	CHECK(dhcp_state.state == DHCP_REQUEST_SENT && !sends,
	      "dhcp: a packet too short for the message type is ignored");
}

static void scenario_offer_requests(void)
{
	reset_client();
	dhcp_state.state = DHCP_DISCOVER_SENT;
	pkt_start(2);
	opt(subnet_24, sizeof(subnet_24));
	opt(end, sizeof(end));
	deliver();
	CHECK(ip_is(dhcp_state.current_ip, 192, 168, 10, 247) && sends == 1
	      && dhcp_state.state == DHCP_REQUEST_SENT,
	      "dhcp: an OFFER takes the offered address and sends a REQUEST");
}

int main(void)
{
	printf("== dhcp: option parsing stays inside the packet ==\n");
	scenario_well_formed();
	scenario_value_past_end();
	scenario_length_past_end();
	scenario_no_end_option();
	scenario_short_packet();
	scenario_offer_requests();

	printf("\n%s (%d failure%s)\n",
	       failures ? "DHCP: FAILURES" : "DHCP: ALL PASS",
	       failures, failures == 1 ? "" : "s");
	return failures ? 1 : 0;
}
