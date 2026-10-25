/*
 * test_lldp.c - LLDP neighbor discovery: rtl837x_lldp.c is compiled
 * unmodified and fed LLDPDUs as the CPU port delivers them, trapped (no VLAN
 * tag) or forwarded (with one). Checks the neighbor table, ageing on TTL and
 * link loss, and the /lldp_nb.json body.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "rtl837x_common.h"
#include "machine.h"
#include "uip.h"
#include "rtl837x_regs.h"
#include "rtl837x_lldp.h"

/* ---------- platform mocks ---------- */

uint8_t uip_buf[UIP_CONF_BUFFER_ALLOC_SIZE];
uint16_t uip_len;
struct uip_eth_addr uip_ethaddr = { .addr = {0x02,0x11,0x22,0x33,0x44,0x55} };
const struct machine machine = {
			   .log_to_phys_port = {5, 1, 2, 3, 4, 6, 7, 8, 9} };
bool lldp_enabled;
char hostname[24] = "switch";
uint8_t outbuf[TCP_OUTBUF_SIZE];
uint16_t slen;
uint8_t sfr_data[4];

/* Link status: bit n = logical port n has carrier */
static uint16_t links = 0xffff;
void reg_read_m(uint16_t reg)
{
	sfr_data[0] = sfr_data[3] = 0;
	if (reg == RTL837X_REG_LINKS_STS) {
		sfr_data[1] = links;
		sfr_data[2] = links >> 8;
	}
}

void tcpip_output(void) { uip_len = 0; }
void memcpyc(uint8_t *dst, const uint8_t *src, uint16_t len) { memcpy(dst, src, len); }
uint16_t strlen_x(const char *s) { return strlen(s); }

static int verbose = 0;
void print_string(const char *s) { if (verbose) fputs(s, stdout); }
void print_string_x(char *s) { if (verbose) fputs(s, stdout); }
void print_short(uint16_t v) { if (verbose) printf("%04x", v); }
void itoa(uint8_t v) { if (verbose) printf("%u", v); }
void itoa_short(uint16_t v) { if (verbose) printf("%u", v); }
void write_char(char c) { if (verbose) putchar(c); }

/* ---------- frame builder ---------- */

static uint16_t pos;

static void tlv(uint8_t type, const void *val, uint16_t len)
{
	uip_buf[pos++] = (type << 1) | (len >> 8);
	uip_buf[pos++] = len;
	memcpy(&uip_buf[pos], val, len);
	pos += len;
}

static void tlv_sub(uint8_t type, uint8_t subtype, const void *val, uint16_t len)
{
	uint8_t b[300];
	b[0] = subtype;
	memcpy(b + 1, val, len);
	tlv(type, b, len + 1);
}

/* Start an LLDPDU from the partner on logical port `port` */
static void frame_start(uint8_t port, int tagged)
{
	memset(uip_buf, 0, sizeof(uip_buf));
	static const uint8_t dst[] = {0x01, 0x80, 0xc2, 0x00, 0x00, 0x0e};
	memcpy(uip_buf, dst, 6);
	uip_buf[6] = 0x02;
	struct rtl_tag *t = (struct rtl_tag *)&uip_buf[12];
	t->tag = HTONS(RTL_FRAME_TAG_ID);
	t->pmask = HTONS(port);		/* RX: 4-bit ingress port */
	pos = 12 + RTL_TAG_SIZE;
	if (tagged) {
		uip_buf[pos] = 0x81;
		uip_buf[pos + 1] = 0x00;
		pos += VLAN_TAG_SIZE;
	}
	uip_buf[pos++] = 0x88;
	uip_buf[pos++] = 0xcc;
}

static void frame_end(void)
{
	tlv(LLDP_END_TLV_TYPE, "", 0);
	uip_len = pos + 4;	/* FCS */
	lldp_in();
}

static const uint8_t MAC_A[] = {0x00, 0x1b, 0x21, 0xaa, 0xbb, 0xcc};
static const uint8_t TTL_120[] = {0x00, 120};

/* A typical lldpd announcement */
static void partner(uint8_t port, int tagged, const uint8_t ttl[2])
{
	static const uint8_t cap[] = {0x00, 0x14, 0x00, 0x04};		/* bridge+router / bridge */
	static const uint8_t mgmt[] = {5, 1, 192, 168, 1, 7, 2, 0, 0, 0, 1, 0};

	frame_start(port, tagged);
	tlv_sub(LLDP_CHASSIS_ID_TLV_TYPE, LLDP_CHASSIS_ID_SUBTYPE_MAC, MAC_A, 6);
	tlv_sub(LLDP_PORT_ID_TLV_TYPE, 5, "eth0", 4);
	tlv(LLDP_TTL_TLV_TYPE, ttl, 2);
	tlv(LLDP_PORT_DESC_TLV_TYPE, "uplink \"1\"", 10);
	tlv(LLDP_SYSNAME_TLV_TYPE, "core-sw", 7);
	tlv(LLDP_SYSCAP_TLV_TYPE, cap, 4);
	tlv(LLDP_MGMT_ADDR_TLV_TYPE, mgmt, sizeof(mgmt));
	frame_end();
}

static void seconds(int n)
{
	while (n--)
		for (int t = 0; t < SYS_TICK_HZ; t++)
			lldp_tick();
}

static const char *json(void)
{
	lldp_neighbors_json();
	outbuf[slen] = 0;
	const char *p = strstr((const char *)outbuf, "\r\n\r\n");
	return p ? p + 4 : "";
}

static int failures;
#define CHECK(cond, name) do { \
	if (cond) printf("PASS  %s\n", name); \
	else { printf("FAIL  %s\n", name); failures++; } \
} while (0)

int main(int argc, char **argv)
{
	verbose = (argc > 1 && !strcmp(argv[1], "-v"));
	struct lldp_neighbor *nb;

	/* xdata initializers are not applied on the chip: RAM holds leftovers */
	lldp_logical_port_status = 0x3004;
	lldp_init();
	lldp_enabled = 1;
	CHECK(lldp_logical_port_status == 0xffff, "T0 init sends on all ports, whatever RAM held");

	CHECK(!strcmp(json(), "[]"), "T1 no neighbors: empty list");

	partner(2, 0, TTL_120);
	nb = &lldp_neighbors[2];
	CHECK(nb->ttl == 120
	      && !strcmp(nb->str[LLDP_NB_CHASSIS_ID], "00:1b:21:aa:bb:cc")
	      && !strcmp(nb->str[LLDP_NB_PORT_ID], "eth0")
	      && !strcmp(nb->str[LLDP_NB_SYS_NAME], "core-sw")
	      && nb->sys_cap == 0x14 && nb->en_cap == 0x04
	      && nb->mgmt_ip[0] == 192 && nb->mgmt_ip[3] == 7,
	      "T2 trapped LLDPDU: all TLVs stored");
	CHECK(!strcmp(nb->str[LLDP_NB_PORT_DESC], "uplink .1."),
	      "T3 quotes sanitized out of strings");
	CHECK(uip_len == 0, "T4 frame consumed");

	CHECK(!strcmp(json(), "[{\"port\":2,\"ttl\":120,\"ch\":\"00:1b:21:aa:bb:cc\",\"pi\":\"eth0\","
			      "\"pd\":\"uplink .1.\",\"sn\":\"core-sw\",\"ip\":\"192.168.1.7\","
			      "\"cap\":20,\"ecap\":4}]"),
	      "T5 JSON body of one neighbor");

	partner(5, 1, TTL_120);
	CHECK(lldp_neighbors[5].ttl == 120 && !strcmp(lldp_neighbors[5].str[LLDP_NB_PORT_ID], "eth0"),
	      "T6 forwarded LLDPDU with VLAN tag parsed");

	seconds(10);
	CHECK(lldp_neighbors[2].ttl == 110, "T7 TTL counts down once a second");

	links &= ~PORT_BIT(5);
	seconds(1);
	CHECK(lldp_neighbors[5].ttl == 0 && lldp_neighbors[2].ttl == 109,
	      "T8 link down removes only that port's neighbor");
	links = 0xffff;

	static const uint8_t ttl0[] = {0, 0};
	partner(2, 0, ttl0);
	CHECK(lldp_neighbors[2].ttl == 0, "T9 shutdown LLDPDU (TTL 0) removes neighbor");

	static const uint8_t ttl3[] = {0, 3};
	partner(3, 0, ttl3);
	seconds(3);
	CHECK(lldp_neighbors[3].ttl == 0, "T10 neighbor ages out at TTL expiry");

	/* Missing Port ID TLV */
	frame_start(4, 0);
	tlv_sub(LLDP_CHASSIS_ID_TLV_TYPE, LLDP_CHASSIS_ID_SUBTYPE_MAC, MAC_A, 6);
	tlv(LLDP_TTL_TLV_TYPE, TTL_120, 2);
	frame_end();
	CHECK(lldp_neighbors[4].ttl == 0, "T11 LLDPDU without Port ID rejected");

	/* Over-long and truncated TLVs */
	char longname[200];
	memset(longname, 'x', sizeof(longname));
	frame_start(4, 0);
	tlv_sub(LLDP_CHASSIS_ID_TLV_TYPE, 7, "local-chassis", 13);
	tlv_sub(LLDP_PORT_ID_TLV_TYPE, LLDP_PORT_ID_SUBTYPE_MAC, longname, 20);
	tlv(LLDP_TTL_TLV_TYPE, TTL_120, 2);
	tlv(LLDP_SYSNAME_TLV_TYPE, longname, sizeof(longname));
	frame_end();
	nb = &lldp_neighbors[4];
	CHECK(nb->ttl == 120 && strlen(nb->str[LLDP_NB_SYS_NAME]) == LLDP_NB_STR_SIZE - 1
	      && strlen(nb->str[LLDP_NB_PORT_ID]) <= LLDP_NB_STR_SIZE - 1
	      && !strcmp(nb->str[LLDP_NB_CHASSIS_ID], "local-chassis"),
	      "T12 long TLVs truncated to the buffer");

	frame_start(6, 0);
	tlv_sub(LLDP_CHASSIS_ID_TLV_TYPE, LLDP_CHASSIS_ID_SUBTYPE_MAC, MAC_A, 6);
	tlv_sub(LLDP_PORT_ID_TLV_TYPE, 5, "eth1", 4);
	tlv(LLDP_TTL_TLV_TYPE, TTL_120, 2);
	uip_buf[pos++] = LLDP_SYSNAME_TLV_TYPE << 1;
	uip_buf[pos++] = 250;		/* claims more than the frame holds */
	uip_len = pos;
	lldp_in();
	CHECK(lldp_neighbors[6].ttl == 120 && !lldp_neighbors[6].str[LLDP_NB_SYS_NAME][0],
	      "T13 TLV running past the frame end ignored");

	frame_start(7, 0);
	uip_buf[pos - 1] = 0x09;	/* not LLDP */
	tlv_sub(LLDP_CHASSIS_ID_TLV_TYPE, LLDP_CHASSIS_ID_SUBTYPE_MAC, MAC_A, 6);
	tlv_sub(LLDP_PORT_ID_TLV_TYPE, 5, "eth0", 4);
	tlv(LLDP_TTL_TLV_TYPE, TTL_120, 2);
	frame_end();
	CHECK(lldp_neighbors[7].ttl == 0, "T14 wrong EtherType ignored");

	frame_start(12, 0);
	tlv_sub(LLDP_CHASSIS_ID_TLV_TYPE, LLDP_CHASSIS_ID_SUBTYPE_MAC, MAC_A, 6);
	tlv_sub(LLDP_PORT_ID_TLV_TYPE, 5, "eth0", 4);
	tlv(LLDP_TTL_TLV_TYPE, TTL_120, 2);
	frame_end();
	CHECK(uip_len == 0, "T15 invalid ingress port ignored");

	/* Every port with a worst-case neighbor still fits outbuf */
	for (uint8_t p = MACHINE_MIN_PORT; p <= MACHINE_MAX_PORT; p++) {
		frame_start(p, 0);
		tlv_sub(LLDP_CHASSIS_ID_TLV_TYPE, 7, longname, 100);
		tlv_sub(LLDP_PORT_ID_TLV_TYPE, 7, longname, 100);
		tlv(LLDP_TTL_TLV_TYPE, "\xff\xff", 2);
		tlv(LLDP_PORT_DESC_TLV_TYPE, longname, 100);
		tlv(LLDP_SYSNAME_TLV_TYPE, longname, 100);
		tlv(LLDP_SYSCAP_TLV_TYPE, "\xff\xff\xff\xff", 4);
		tlv(LLDP_MGMT_ADDR_TLV_TYPE, "\x05\x01\xff\xff\xff\xff\x02\x00\x00\x00\x01\x00", 12);
		frame_end();
	}
	const char *j = json();
	int n = 0;
	for (const char *q = j; (q = strstr(q, "\"port\":")); q++)
		n++;
	CHECK(n == MACHINE_MAX_PORT - MACHINE_MIN_PORT + 1 && slen < TCP_OUTBUF_SIZE
	      && j[strlen(j) - 1] == ']',
	      "T16 all ports with worst-case neighbors fit the output buffer");
	if (verbose)
		printf("worst case: %u of %u bytes\n", slen, TCP_OUTBUF_SIZE);

	lldp_neighbors_clear();
	CHECK(!strcmp(json(), "[]"), "T17 clear (lldp off) empties the table");

	printf("\n%s (%d failure%s)\n", failures ? "SANDBOX: FAILURES" : "SANDBOX: ALL PASS",
	       failures, failures == 1 ? "" : "s");
	return failures ? 1 : 0;
}
