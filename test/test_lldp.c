/*
 * test_lldp.c - rtl837x_lldp.c compiled unmodified: the LLDPDU it sends,
 * the parser fed with well-formed and malformed frames, ageing, the CLI and
 * the size and shape of /lldp.json. ASan/UBSan catch any overrun of the
 * frame or the neighbour strings.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "rtl837x_common.h"
#include "rtl837x_regs.h"
#include "rtl837x_lldp.h"
#include "machine.h"
#include "phy.h"
#include "uip.h"
#include <stdlib.h>	/* after uip.h: macOS <stdlib.h> defines HTONS too */

/* SFR storage, as test/hw_mock.c does it */
#undef __sfr
#undef __sfr16
#undef __sfr32
#define __sfr   volatile uint8_t
#define __sfr16 volatile uint16_t
#define __sfr32 volatile uint32_t
#include "rtl837x_sfr.h"

/* ---------- platform mocks ---------- */

uint8_t uip_buf[UIP_CONF_BUFFER_ALLOC_SIZE];
uint16_t uip_len;
struct uip_eth_addr uip_ethaddr = { .addr = {0x02, 0x11, 0x22, 0x33, 0x44, 0x55} };
uip_ipaddr_t uip_hostaddr;
#define MNAME "Test switch with 29 chars...."
const struct machine machine = { .machine_name = MNAME, .min_port = 0, .max_port = 8,
	.log_to_phys_port = {1, 2, 3, 4, 5, 6, 7, 8, 9}, .is_sfp = {0, 0, 0, 0, 0, 0, 0, 0, 1} };
char hostname[24];
char port_names[9][PORT_NAME_SIZE];
bool lldp_enabled;
uint8_t outbuf[TCP_OUTBUF_SIZE];
uint16_t slen;
uint8_t cmd_buffer[CMD_BUF_SIZE];
uint8_t cmd_words_len;
uint8_t cmd_words_b[15];
uint8_t err_status;
uint8_t sfr_data[4];

static int fails;
#define CHECK(c) do { if (!(c)) { printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

/* Console */
static char con[8192];
static size_t con_len;
void write_char(char c) { if (con_len < sizeof(con) - 1) con[con_len++] = c, con[con_len] = 0; }
void print_string(const char *s) { while (*s) write_char(*s++); }
void print_string_x(char *s) { print_string(s); }
void itoa_short(uint16_t v) { char b[8]; snprintf(b, sizeof b, "%u", v); print_string(b); }
void print_phys_port(uint8_t p) { itoa_short(machine.log_to_phys_port[p]); }
void print_ip(uint8_t *ip) { char b[20]; snprintf(b, sizeof b, "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]); print_string(b); }

uint16_t strtox(uint8_t *dst, const char *s) { size_t n = strlen(s); memcpy(dst, s, n); return n; }
bool strstart(const uint8_t *a, const uint8_t *b) { return !strncmp((const char *)a, (const char *)b, strlen((const char *)b)); }
uint16_t strlen_x(const char *s) { return strlen(s); }
void memcpyc(uint8_t *dst, const uint8_t *src, uint16_t len) { memcpy(dst, src, len); }

/* Hardware: link bitmap, speed code per port, PHY registers */
static uint16_t links = 0x1ff;
static uint8_t speed[9] = {2, 2, 2, 2, 5, 1, 0, 2, 4};
static uint16_t phy_bmcr_an = 0x1000, phy_adv = 0x01e0, phy_gbcr = 0x0200, phy_physr = 0x0008;
void reg_read_m(uint16_t reg)
{
	memset(sfr_data, 0, 4);
	if (reg == RTL837X_REG_LINKS_STS) {
		sfr_data[1] = links;
		sfr_data[2] = links >> 8;
	} else if (reg == RTL837X_REG_LINKS || reg == RTL837X_REG_LINKS_89) {
		uint8_t base = reg == RTL837X_REG_LINKS ? 0 : 8;
		for (int i = 0; i < 8 && base + i < 9; i++)
			sfr_data[3 - (i >> 1)] |= speed[base + i] << ((i & 1) * 4);
	} else if (reg >= RTL8373_REG_MAC_L2_PORT_MAX_LEN) {
		SFR_DATA_U16 = 1522;
	}
}
void phy_read(uint8_t phy_id, uint8_t dev_id, uint16_t reg)
{
	if (dev_id == PHY_MMD_AN && reg == PHY_ANEG_CTRL) SFR_DATA_U16 = phy_bmcr_an;
	else if (dev_id == PHY_MMD_AN && reg == PHY_ANEG_ADV) SFR_DATA_U16 = phy_adv;
	else if (reg == PHY_MMD31_GBCR) SFR_DATA_U16 = phy_gbcr;
	else if (reg == PHY_MMD31_PHYSR) SFR_DATA_U16 = phy_physr;
	else SFR_DATA_U16 = 0;
}
uint16_t port_pvid_get(uint8_t port) { return 100 + port; }

static uint8_t tx_frame[9][1600];
static uint16_t tx_len[9];
static int tx_count;
void tcpip_output(void)
{
	uint8_t *f = uip_buf + RTL_FRAME_DESC_SIZE;
	uint16_t pmask = (f[18] << 8) | f[19];
	int port = __builtin_ctz(pmask);
	CHECK(uip_len <= sizeof(tx_frame[0]));
	memcpy(tx_frame[port], f, uip_len);
	tx_len[port] = uip_len;
	tx_count++;
}
static uint16_t rma_reg; static uint8_t rma_act;
void trap_rma_set(uint16_t reg, uint8_t act) { rma_reg = reg; rma_act = act; }

/* ---------- helpers ---------- */

static void tick_seconds(int s) { for (int i = 0; i < s * SYS_TICK_HZ; i++) lldp_tick(); }

static void send_all(void)
{
	memset(tx_len, 0, sizeof tx_len);
	tx_count = 0;
	for (int i = 0; i < LLDP_TX_INTERVAL_TICKS; i++)
		lldp_tick();
}

/* Find TLV `type` (and org OUI/subtype for 127) in an LLDPDU payload */
static const uint8_t *find_tlv(const uint8_t *pl, int len, int type, const char *oui, int sub, int *tl)
{
	int p = 0;
	while (p + 2 <= len) {
		int t = pl[p] >> 1, l = ((pl[p] & 1) << 8) | pl[p + 1];
		if (t == 0) return NULL;
		if (t == type && (type != 127 || (!memcmp(pl + p + 2, oui, 3) && pl[p + 5] == sub))) {
			*tl = l;
			return pl + p + 2;
		}
		p += 2 + l;
	}
	return NULL;
}

/* RX frame under construction: addresses, RTL tag, [VLAN], EtherType, TLVs */
static uint8_t rx[UIP_CONF_BUFFER_SIZE];	/* the largest frame the NIC hands over */
static int rx_n;
static void rx_start(int port, int vlan)
{
	memset(rx, 0, sizeof rx);
	memcpy(rx, "\x01\x80\xc2\x00\x00\x0e", 6);
	memcpy(rx + 6, "\x00\xaa\xbb\xcc\xdd\xee", 6);
	rx[12] = 0x88; rx[13] = 0x99; rx[14] = 4;
	rx[18] = 0; rx[19] = port;
	rx_n = 20;
	if (vlan) { rx[rx_n++] = 0x81; rx[rx_n++] = 0; rx[rx_n++] = 0; rx[rx_n++] = 1; }
	rx[rx_n++] = 0x88; rx[rx_n++] = 0xcc;
}
static void rx_tlv(int type, int len, const void *v)
{
	rx[rx_n++] = (type << 1) | (len >> 8);
	rx[rx_n++] = len;
	memcpy(rx + rx_n, v, len);
	rx_n += len;
}
static void rx_mandatory(const char *chassis, const char *port, int ttl)
{
	uint8_t b[300];
	b[0] = 7; strcpy((char *)b + 1, chassis); rx_tlv(1, 1 + strlen(chassis), b);
	b[0] = 5; strcpy((char *)b + 1, port); rx_tlv(2, 1 + strlen(port), b);
	b[0] = ttl >> 8; b[1] = ttl; rx_tlv(3, 2, b);
}
static void rx_deliver_len(int len)
{
	memset(uip_buf, 0x5a, sizeof uip_buf);
	memcpy(uip_buf, rx, len);
	uip_len = len;
	lldp_in();
	CHECK(uip_len == 0);
}
static void rx_deliver(void) { rx_tlv(0, 0, ""); rx_deliver_len(rx_n); }

static int printable(const char *s, int size)
{
	int i;
	for (i = 0; i < size && s[i]; i++)
		if (s[i] < 0x20 || s[i] > 0x7e || s[i] == '"' || s[i] == '\\') return 0;
	return i < size;
}
static int nb_sane(const struct lldp_nb *n)
{
	return printable(n->chassis, LLDP_STR_SIZE) && printable(n->port, LLDP_STR_SIZE)
	    && printable(n->port_desc, LLDP_STR_SIZE) && printable(n->sys_name, LLDP_STR_SIZE)
	    && printable(n->sys_desc, LLDP_DESC_SIZE);
}

static void cli(const char *line)
{
	memset(cmd_buffer, 0, sizeof cmd_buffer);
	strcpy((char *)cmd_buffer, line);
	cmd_words_len = 0;
	for (int i = 0; cmd_buffer[i]; i++)
		if (cmd_buffer[i] != ' ' && (i == 0 || cmd_buffer[i - 1] == ' '))
			cmd_words_b[cmd_words_len++] = i;
	err_status = 0;
	con_len = 0; con[0] = 0;
	lldp_parse();
}

static const char *body(void)
{
	outbuf[slen] = 0;
	const char *p = strstr((const char *)outbuf, "\r\n\r\n");
	return p ? p + 4 : "";
}

static int json_valid(const char *s)
{
	FILE *f = fopen("build/lldp_test.json", "w");
	if (!f) return 0;
	fputs(s, f);
	fclose(f);
	return !system("python3 -c 'import json,sys; json.load(open(sys.argv[1]))' build/lldp_test.json");
}

static void reset(void)
{
	lldp_init();
	memset(port_names, 0, sizeof port_names);
	strcpy(hostname, "sw-test");
	uint8_t ip[4] = {10, 0, 0, 7};
	memcpy(uip_hostaddr, ip, 4);
	links = 0x1ff;
	con_len = 0;
}

/* ---------- tests ---------- */

static void t_tx(void)
{
	printf("[test] LLDPDU sent with every optional TLV\n");
	reset();
	lldp_enabled = 1;
	strcpy(port_names[0], "uplink");
	send_all();
	CHECK(tx_count == 9);
	uint8_t *f = tx_frame[0];
	const uint8_t *pl = f + 22;
	int len = tx_len[0] - 22, tl;
	const uint8_t *v;
	CHECK(!memcmp(f, "\x01\x80\xc2\x00\x00\x0e", 6));
	CHECK(f[20] == 0x88 && f[21] == 0xcc);
	CHECK(len >= LLDP_MIN_ETHERNET_PAYLOAD_LENGTH);
	/* Mandatory TLVs first, in order */
	CHECK(pl[0] == 0x02 && pl[1] == 7 && pl[2] == 4 && !memcmp(pl + 3, uip_ethaddr.addr, 6));
	CHECK(pl[9] == 0x04 && pl[10] == 2 && pl[11] == 7 && pl[12] == '1');
	CHECK(pl[13] == 0x06 && pl[14] == 2 && pl[15] == 0 && pl[16] == 120);
	CHECK((v = find_tlv(pl, len, 4, 0, 0, &tl)) && tl == 6 && !memcmp(v, "uplink", 6));
	CHECK((v = find_tlv(pl, len, 5, 0, 0, &tl)) && tl == 7 && !memcmp(v, "sw-test", 7));
	CHECK((v = find_tlv(pl, len, 6, 0, 0, &tl)) && tl == 29 && !memcmp(v, MNAME, 29));
	CHECK((v = find_tlv(pl, len, 7, 0, 0, &tl)) && tl == 4 && !memcmp(v, "\x00\x04\x00\x04", 4));
	CHECK((v = find_tlv(pl, len, 8, 0, 0, &tl)) && tl == 12
	      && !memcmp(v, "\x05\x01\x0a\x00\x00\x07\x01\x00\x00\x00\x00\x00", 12));
	CHECK((v = find_tlv(pl, len, 127, "\x00\x80\xc2", 1, &tl)) && tl == 6 && v[4] == 0 && v[5] == 100);
	/* 1G full duplex copper, autoneg on, 10/100 HD+FD and 1000 FD advertised */
	CHECK((v = find_tlv(pl, len, 127, "\x00\x12\x0f", 1, &tl)) && tl == 9 && v[4] == 3
	      && v[5] == 0x6c && v[6] == 0x01 && v[7] == 0 && v[8] == 30);
	CHECK((v = find_tlv(pl, len, 127, "\x00\x12\x0f", 4, &tl)) && tl == 6 && v[4] == 0x05 && v[5] == 0xf2);
	/* No port name: no Port Description; MAU of 2.5G copper, 100M half duplex, 10G SFP */
	CHECK(!find_tlv(tx_frame[1] + 22, tx_len[1] - 22, 4, 0, 0, &tl));
	CHECK((v = find_tlv(tx_frame[4] + 22, tx_len[4] - 22, 127, "\x00\x12\x0f", 1, &tl)) && v[8] == 103);
	CHECK((v = find_tlv(tx_frame[8] + 22, tx_len[8] - 22, 127, "\x00\x12\x0f", 1, &tl)) && v[4] == 0 && v[8] == 33);
	phy_physr = 0;
	send_all();
	CHECK((v = find_tlv(tx_frame[5] + 22, tx_len[5] - 22, 127, "\x00\x12\x0f", 1, &tl)) && v[8] == 15);
	phy_physr = 0x0008;

	printf("[test] TLV mask, blocked port, no IP, LLDP off\n");
	cli("lldp tlv portdesc off"); cli("lldp tlv syscap off"); cli("lldp tlv mgmt off");
	cli("lldp tlv pvid off"); cli("lldp tlv macphy off"); cli("lldp tlv mfs off");
	CHECK(lldp_tlv == 0);
	lldp_logical_port_status &= ~PORT_BIT(2);
	send_all();
	CHECK(tx_count == 8 && !tx_len[2]);
	pl = tx_frame[0] + 22; len = tx_len[0] - 22;
	CHECK(!find_tlv(pl, len, 4, 0, 0, &tl) && !find_tlv(pl, len, 7, 0, 0, &tl) && !find_tlv(pl, len, 8, 0, 0, &tl)
	      && !find_tlv(pl, len, 127, "\x00\x80\xc2", 1, &tl) && find_tlv(pl, len, 5, 0, 0, &tl));
	lldp_tlv = LLDP_TX_ALL;
	memset(uip_hostaddr, 0, 4);
	send_all();
	CHECK(!find_tlv(tx_frame[0] + 22, tx_len[0] - 22, 8, 0, 0, &tl));
	lldp_enabled = 0;
	send_all();
	CHECK(tx_count == 0);

	printf("[test] longest LLDPDU fits the payload buffer\n");
	reset();
	lldp_enabled = 1;
	memset(port_names[0], 'p', PORT_NAME_SIZE - 1);
	memset(hostname, 'h', sizeof hostname - 1);
	send_all();
	CHECK(tx_len[0] - 22 == 155);	/* 156 with a 30 character machine name */
	CHECK(tx_len[0] - 22 <= LLDP_MAX_PAYLOAD);

	printf("[test] what we send parses back as a neighbour\n");
	memcpy(rx, tx_frame[3], tx_len[3]);
	rx[19] = 6;
	rx_deliver_len(tx_len[3]);
	struct lldp_nb *n = &lldp_nb[6];
	CHECK(n->ttl == 120 && n->chassis_sub == 4 && !strcmp(n->chassis, "02:11:22:33:44:55"));
	CHECK(n->port_sub == 7 && !strcmp(n->port, "4") && n->cap_sup == 4 && n->cap_en == 4);
	CHECK(!memcmp(n->ip, "\x0a\x00\x00\x07", 4) && n->pvid == 103 && !strcmp(n->sys_desc, MNAME));
	CHECK(strlen(n->sys_name) == 23 && n->port_desc[0] == 0);
}

static void t_rx(void)
{
	struct lldp_nb *n = &lldp_nb[2];
	uint8_t b[600];

	printf("[test] well-formed LLDPDU, VLAN tagged\n");
	reset();
	rx_start(2, 1);
	rx_mandatory("core-sw", "Gi1/0/24", 120);
	rx_tlv(4, 6, "uplnk\"");
	rx_tlv(5, 4, "core");
	rx_tlv(6, 9, "Line\none\\");
	rx_tlv(7, 4, "\x00\x14\x00\x04");
	rx_tlv(8, 12, "\x05\x01\xc0\xa8\x01\x02\x02\x00\x00\x00\x05\x00");
	rx_tlv(127, 6, "\x00\x80\xc2\x01\x00\x2a");
	rx_tlv(127, 9, "\x00\x12\x0f\x01\x03\x6c\x01\x00\x1e");
	rx_deliver();
	CHECK(n->ttl == 120 && n->age == 0);
	CHECK(!strcmp(n->chassis, "core-sw") && !strcmp(n->port, "Gi1/0/24") && n->chassis_sub == 7 && n->port_sub == 5);
	CHECK(!strcmp(n->port_desc, "uplnk.") && !strcmp(n->sys_name, "core") && !strcmp(n->sys_desc, "Line.one."));
	CHECK(n->cap_sup == 0x14 && n->cap_en == 4 && !memcmp(n->ip, "\xc0\xa8\x01\x02", 4) && n->pvid == 42);

	printf("[test] MAC chassis as hex, binary port ID as hex\n");
	rx_start(2, 0);
	rx_tlv(1, 7, "\x04\x41\x42\x43\x44\x45\x46");
	rx_tlv(2, 4, "\x02\x00\x01\xff");
	rx_tlv(3, 2, "\x00\x78");
	rx_deliver();
	CHECK(!strcmp(n->chassis, "41:42:43:44:45:46") && !strcmp(n->port, "00:01:ff"));
	CHECK(n->sys_name[0] == 0 && n->pvid == 0 && n->ip[0] == 0);

	printf("[test] oversized strings are truncated, NUL-terminated and printable\n");
	rx_start(2, 0);
	memset(b, 'c', sizeof b); b[0] = 7;
	rx_tlv(1, 200, b);
	memset(b, 0x01, sizeof b); b[0] = 1;
	rx_tlv(2, 255, b);
	rx_tlv(3, 2, "\x00\x78");
	memset(b, 0xff, sizeof b);
	rx_tlv(4, 300, b);
	memset(b, '"', sizeof b);
	rx_tlv(5, 100, b);
	memset(b, 'd', sizeof b);
	rx_tlv(6, 511, b);	/* longest TLV length there is */
	rx_deliver();
	CHECK(n->ttl == 120 && nb_sane(n));
	CHECK(strlen(n->chassis) == LLDP_STR_SIZE - 1 && strlen(n->port) == 29 && !strncmp(n->port, "01:01:", 6));
	CHECK(strlen(n->port_desc) == LLDP_STR_SIZE - 1 && strlen(n->sys_name) == LLDP_STR_SIZE - 1);
	CHECK(strlen(n->sys_desc) == LLDP_DESC_SIZE - 1);

	printf("[test] malformed LLDPDUs are dropped and keep the old neighbour\n");
	rx_start(2, 0);
	rx_mandatory("keep", "me", 99);
	rx_deliver();
	CHECK(n->ttl == 99);
#define DROPPED(...) do { rx_start(2, 0); __VA_ARGS__; CHECK(n->ttl == 99 && !strcmp(n->chassis, "keep")); } while (0)
	/* TLV running past the frame end */
	DROPPED(rx_mandatory("x", "y", 10); rx_tlv(5, 4, "abcd"); rx_deliver_len(rx_n - 1));
	/* header of a TLV cut by the frame end, after a valid Port ID */
	DROPPED(rx_tlv(1, 2, "\x07x"); rx_tlv(2, 2, "\x07y"); rx[rx_n++] = 0x06; rx_deliver_len(rx_n));
	/* missing TTL */
	DROPPED(rx_tlv(1, 2, "\x07x"); rx_tlv(2, 2, "\x07y"); rx_deliver());
	/* wrong order */
	DROPPED(rx_tlv(2, 2, "\x07y"); rx_tlv(1, 2, "\x07x"); rx_tlv(3, 2, "\x00\x0a"); rx_deliver());
	/* optional TLV before TTL */
	DROPPED(rx_tlv(1, 2, "\x07x"); rx_tlv(2, 2, "\x07y"); rx_tlv(5, 1, "n"); rx_tlv(3, 2, "\x00\x0a"); rx_deliver());
	/* duplicate Chassis ID */
	DROPPED(rx_mandatory("x", "y", 10); rx_tlv(1, 2, "\x07z"); rx_deliver());
	/* Chassis ID with no ID, Port ID with no ID, short TTL */
	DROPPED(rx_tlv(1, 1, "\x07"); rx_tlv(2, 2, "\x07y"); rx_tlv(3, 2, "\x00\x0a"); rx_deliver());
	DROPPED(rx_tlv(1, 2, "\x07x"); rx_tlv(2, 1, "\x07"); rx_tlv(3, 2, "\x00\x0a"); rx_deliver());
	DROPPED(rx_tlv(1, 2, "\x07x"); rx_tlv(2, 2, "\x07y"); rx_tlv(3, 1, "\x0a"); rx_deliver());
	/* End before the mandatory TLVs; empty LLDPDU; no LLDPDU at all */
	DROPPED(rx_tlv(1, 2, "\x07x"); rx_deliver());
	DROPPED(rx_deliver());
	DROPPED(rx_mandatory("x", "y", 10); rx_deliver_len(21));
	DROPPED(rx_mandatory("x", "y", 10); rx_deliver_len(0));
	/* wrong EtherType */
	DROPPED(rx_mandatory("x", "y", 10); rx[21] = 0xcd; rx_deliver());
	/* source port outside the switch: CPU port, garbage */
	DROPPED(rx[19] = 9; rx_mandatory("x", "y", 10); rx_deliver());
	DROPPED(rx[19] = 0xff; rx_mandatory("x", "y", 10); rx_deliver());

	printf("[test] short optional TLVs are ignored, the LLDPDU is kept\n");
	rx_start(2, 0);
	rx_mandatory("short", "opts", 50);
	rx_tlv(7, 3, "\x00\x14\x00");
	rx_tlv(8, 5, "\x05\x01\xc0\xa8\x01");
	rx_tlv(8, 18, "\x11\x02\x20\x01\x0d\xb8\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01");
	rx_tlv(127, 5, "\x00\x80\xc2\x01\x00");
	rx_tlv(127, 3, "\x00\x80\xc2");
	rx_tlv(9, 0, "");
	rx_deliver();
	CHECK(n->ttl == 50 && !strcmp(n->chassis, "short") && n->cap_sup == 0 && n->ip[0] == 0 && n->pvid == 0);

	printf("[test] LLDPDU ending without End TLV is accepted\n");
	rx_start(2, 0);
	rx_mandatory("noend", "p", 60);
	rx_deliver_len(rx_n);
	CHECK(n->ttl == 60 && !strcmp(n->chassis, "noend"));

	printf("[test] shutdown LLDPDU (TTL 0) removes the neighbour\n");
	rx_start(2, 0);
	rx_mandatory("noend", "p", 0);
	rx_deliver();
	CHECK(n->ttl == 0);

	printf("[test] random frames never overrun and leave sane strings\n");
	srand(1);
	for (int it = 0; it < 20000; it++) {
		rx_start(rand() % 16, rand() & 1);
		if (rand() & 1)
			rx_mandatory("r", "s", rand() & 0xff);
		int extra = rand() % 400;
		for (int i = 0; i < extra; i++)
			rx[rx_n++] = (it & 3) ? rand() : (rand() & 1 ? 0xff : 0x00);
		rx_deliver_len(rx_n);
	}
	for (int p = 0; p < 9; p++)
		CHECK(!lldp_nb[p].ttl || nb_sane(&lldp_nb[p]));
}

static void t_age(void)
{
	printf("[test] ageing: TTL runs out, link down clears\n");
	reset();
	rx_start(1, 0); rx_mandatory("a", "b", 3); rx_deliver();
	rx_start(4, 0); rx_mandatory("c", "d", 100); rx_deliver();
	tick_seconds(2);
	CHECK(lldp_nb[1].ttl == 3 && lldp_nb[1].age == 2 && lldp_nb[4].age == 2);
	tick_seconds(1);
	CHECK(lldp_nb[1].ttl == 0 && lldp_nb[4].ttl == 100);
	links &= ~PORT_BIT(4);
	tick_seconds(1);
	CHECK(lldp_nb[4].ttl == 0);
}

static void t_cli(void)
{
	printf("[test] CLI\n");
	reset();
	cli("lldp on");
	CHECK(lldp_enabled && rma_reg == RTL837X_RMA0E_CONF && rma_act == RTL837X_RMA_ACT_TRAP && !err_status);
	cli("lldp tlv mfs off");
	CHECK(lldp_tlv == (LLDP_TX_ALL & ~LLDP_TX_MFS) && !err_status);
	cli("lldp tlv mfs on");
	CHECK(lldp_tlv == LLDP_TX_ALL);
	cli("lldp tlv bogus on");
	CHECK(err_status && lldp_tlv == LLDP_TX_ALL);
	cli("lldp tlv mfs");
	CHECK(err_status);
	cli("lldp tlv mfsx off");
	CHECK(err_status && lldp_tlv == LLDP_TX_ALL);
	cli("lldp tlv portdesc maybe");
	CHECK(err_status);
	cli("lldp bogus");
	CHECK(err_status && lldp_enabled);
	rx_start(3, 0); rx_mandatory("ch", "pt", 120); rx_tlv(5, 4, "peer"); rx_deliver();
	cli("lldp");
	CHECK(!err_status && strstr(con, "LLDP on, TLVs: portdesc on syscap on mgmt on pvid on macphy on mfs on\n"));
	CHECK(strstr(con, "Port 4: send, neighbour \"peer\" port \"pt\" chassis ch ip 0.0.0.0 age 0/120\n"));
	cli("lldp off");
	CHECK(!lldp_enabled && rma_act == RTL837X_RMA_ACT_FORWARD);
}

static void t_json(void)
{
	printf("[test] /lldp.json\n");
	reset();
	lldp_enabled = 1;
	send_lldp(0);
	CHECK(!strcmp(body(), "{\"on\":1,\"tlv\":63,\"ports\":[{\"p\":1,\"tx\":1},{\"p\":2,\"tx\":1},{\"p\":3,\"tx\":1},"
		"{\"p\":4,\"tx\":1},{\"p\":5,\"tx\":1},{\"p\":6,\"tx\":1},{\"p\":7,\"tx\":1},{\"p\":8,\"tx\":1},{\"p\":9,\"tx\":1}]}"));
	rx_start(2, 0);
	rx_mandatory("ch", "pt", 120);
	rx_tlv(4, 2, "pd"); rx_tlv(5, 2, "sn"); rx_tlv(6, 2, "sd");
	rx_tlv(7, 4, "\x00\x14\x00\x04");
	rx_tlv(8, 12, "\x05\x01\xc0\xa8\x01\x02\x02\x00\x00\x00\x05\x00");
	rx_tlv(127, 6, "\x00\x80\xc2\x01\x00\x2a");
	rx_deliver();
	send_lldp(3);
	CHECK(!strcmp(body(), "{\"p\":3,\"tx\":1,\"pvid\":102,\"mfs\":1522,\"nb\":{\"ch\":\"ch\",\"pi\":\"pt\",\"sn\":\"sn\","
		"\"ip\":\"192.168.1.2\",\"cap\":4,\"age\":0,\"cs\":7,\"ps\":5,\"pd\":\"pd\",\"sd\":\"sd\",\"sc\":20,\"vid\":42,\"ttl\":120}}"));
	CHECK(json_valid(body()));
	send_lldp(42);
	CHECK(!strcmp(body(), "{}"));

	printf("[test] /lldp.json worst case: 9 neighbours with every field at its longest\n");
	for (int p = 0; p < 9; p++) {
		struct lldp_nb *n = &lldp_nb[p];
		memset(n, 0, sizeof *n);
		n->ttl = n->age = n->cap_sup = n->cap_en = 65535;
		n->pvid = 4094;
		n->chassis_sub = n->port_sub = 255;
		memset(n->ip, 255, 4);
		memset(n->chassis, 'c', LLDP_STR_SIZE - 1);
		memset(n->port, 'p', LLDP_STR_SIZE - 1);
		memset(n->port_desc, 'd', LLDP_STR_SIZE - 1);
		memset(n->sys_name, 'n', LLDP_STR_SIZE - 1);
		memset(n->sys_desc, 's', LLDP_DESC_SIZE - 1);
	}
	send_lldp(0);
	printf("  list: %u bytes of %u\n", slen, TCP_OUTBUF_SIZE);
	CHECK(slen <= TCP_OUTBUF_SIZE * 3 / 4);
	CHECK(json_valid(body()));
	send_lldp(9);
	printf("  port: %u bytes\n", slen);
	CHECK(slen <= TCP_OUTBUF_SIZE / 2);
	CHECK(json_valid(body()));
}

int main(void)
{
	t_tx();
	t_rx();
	t_age();
	t_cli();
	t_json();
	if (fails) {
		printf("%d check(s) failed\n", fails);
		return 1;
	}
	printf("all LLDP checks passed\n");
	return 0;
}
