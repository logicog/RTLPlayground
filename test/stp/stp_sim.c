#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include "rtl837x_common.h"
#include "machine.h"
#include "uip.h"
#include "rtl837x_regs.h"
#include "rtl837x_stp.h"

#define NPORTS	9	/* indices 0..8, front-panel 1..9 */

uint8_t uip_buf[UIP_CONF_BUFFER_SIZE + 2];
uint16_t uip_len;
struct uip_eth_addr uip_ethaddr = { .addr = {0x1c,0x2a,0xa3,0x1a,0x72,0x4e} };
const struct machine machine = { .min_port = 0, .max_port = NPORTS - 1,
			   .log_to_phys_port = {1, 2, 3, 4, 5, 6, 7, 8, 9} };
struct machine_runtime machine_detected = { .isRTL8373 = 1 };
uint8_t sfr_data[4];
bool stp_enabled;
uint16_t management_vlan;
uint8_t cmd_buffer[CMD_BUF_SIZE];
uint8_t cmd_words_len;
uint8_t cmd_words_b[15];
char save_cmd;
uint8_t err_status;
uint8_t atoi_results_u8;

static int verbose;
void print_string(char *s) { if (verbose) fputs(s, stdout); }
void print_byte(uint8_t b) { if (verbose) printf("%02x", b); }
void print_short(uint16_t v) { if (verbose) printf("%04x", v); }
void print_long(uint32_t v) { if (verbose) printf("%08x", v); }
void write_char(char c) { if (verbose) putchar(c); }
void itoa(uint8_t v) { if (verbose) printf("%u", v); }
void print_reg(uint16_t reg) { (void)reg; }

uint8_t cmd_compare(uint8_t start, uint8_t *cmd)
{
	if (start >= cmd_words_len)
		return 0;
	const char *w = (const char *)&cmd_buffer[cmd_words_b[start]];
	size_t n = strlen((const char *)cmd);
	return strncmp(w, (const char *)cmd, n) == 0 && (w[n] == ' ' || w[n] == 0);
}
uint8_t atoi_byte(uint8_t idx)
{
	unsigned v = 0, n = 0;
	while (cmd_buffer[idx + n] >= '0' && cmd_buffer[idx + n] <= '9')
		v = v * 10 + (cmd_buffer[idx + n++] - '0');
	atoi_results_u8 = (uint8_t)v;
	return (uint8_t)n;
}
uint8_t cmd_parse_port_separator(uint8_t idx)
{
	if (!atoi_byte(idx) || atoi_results_u8 < 1 || atoi_results_u8 > NPORTS)
		return 0;
	atoi_results_u8--;
	return 1;
}
static void sim_cmd(const char *line)
{
	memset(cmd_buffer, 0, sizeof(cmd_buffer));
	strncpy((char *)cmd_buffer, line, sizeof(cmd_buffer) - 1);
	cmd_words_len = 0;
	for (uint8_t i = 0; cmd_buffer[i]; i++)
		if (cmd_buffer[i] != ' ' && (i == 0 || cmd_buffer[i - 1] == ' '))
			cmd_words_b[cmd_words_len++] = i;
	err_status = ERR_OK;
	stp_parse();
}
void execute_config(void) { }
int8_t vlan_get(uint16_t vlan) { (void)vlan; return -1; }
uint16_t port_pvid_get(uint8_t port) { (void)port; return 1; }
static int l2mc_calls;
static uint16_t l2mc_pmask;
void port_l2mc_set(uint8_t mac_last, uint16_t vid, uint16_t pmask) { (void)mac_last; (void)vid; l2mc_calls++; l2mc_pmask = pmask; }
uint8_t port_ingress_filter_get(uint8_t port) { (void)port; return 0; }

static int flush_count[NPORTS];
void port_l2_forget_port(uint8_t port) { if (port < NPORTS) flush_count[port]++; }

static uint8_t mstp_reg[4];
static uint16_t sim_lag[4];
uint16_t port_lag_members_get(uint8_t lag) { return sim_lag[lag]; }
static uint16_t sim_links;
static uint8_t sim_speed[NPORTS];	/* nibble as the ASIC reports it: 2 = 1G, 4 = 10G */
static int tx_frames[NPORTS];

struct sim_pkt_out {
	uint8_t stp_addr[6];
	uint8_t src_addr[6];
	struct rtl_tag rtl_tag;
	uint16_t msg_len;
	uint8_t dsap;
	uint8_t ssap;
	uint8_t ctrl;
	uint16_t proto;
	uint8_t version;
	uint8_t bpdu_type;
	uint8_t flags;
	struct bridge root;
	uint32_t root_path_cost;
	struct bridge bridge;
	uint8_t port_prio;
	uint8_t port_id;
	uint16_t age;
	uint16_t age_max;
	uint16_t hello;
	uint16_t fwd_delay;
	uint8_t version1_length;
};
static struct sim_pkt_out last_tx[NPORTS];
static int tc_frames[NPORTS];
static int tcn_frames[NPORTS];

void reg_read_m(uint16_t addr)
{
	memset(sfr_data, 0, 4);
	if (addr == RTL837X_MSTP_STATES)
		memcpy(sfr_data, mstp_reg, 4);
	else if (addr == RTL837X_REG_LINKS_STS) {
		sfr_data[1] = (uint8_t)sim_links;
		sfr_data[2] = (uint8_t)(sim_links >> 8);
	} else if (addr == RTL837X_REG_LINKS || addr == RTL837X_REG_LINKS_89) {
		uint8_t base = (addr == RTL837X_REG_LINKS) ? 0 : 8;
		for (uint8_t k = 0; k < 8 && base + k < NPORTS; k++)
			sfr_data[3 - (k >> 1)] |= (k & 1) ? (uint8_t)(sim_speed[base + k] << 4)
							  : sim_speed[base + k];
	}
}

void reg_write_m(uint16_t addr)
{
	if (addr == RTL837X_MSTP_STATES) {
		memcpy(mstp_reg, sfr_data, 4);
		if (verbose > 1)
			printf("    [mstp <- %02x %02x %02x %02x]\n",
			       mstp_reg[0], mstp_reg[1], mstp_reg[2], mstp_reg[3]);
	}
}

void tcpip_output(void)
{
	struct sim_pkt_out *f = (struct sim_pkt_out *)&uip_buf[RTL_FRAME_DESC_SIZE];
	uint16_t pmask = HTONS(f->rtl_tag.pmask);
	for (uint8_t p = 0; p < NPORTS; p++)
		if (pmask & (1 << p)) {
			tx_frames[p]++;
			if (f->bpdu_type == 0x80)
				tcn_frames[p]++;
			else if (f->flags & 0x01)
				tc_frames[p]++;
			memcpy(&last_tx[p], f, sizeof(*f));
		}
	uip_len = 0;
}

static uint8_t port_state(uint8_t port)
{
	return (mstp_reg[3 - (port >> 2)] >> ((port << 1) & 0x7)) & 0x3;
}

static const char *state_name(uint8_t s)
{
	static const char *n[] = { "off", "discarding", "learning", "forwarding" };
	return n[s & 3];
}

static void links_set(uint16_t mask)
{
	sim_links = mask;
}

static void tick(unsigned n)
{
	while (n--)
		stp_timers();
}

static void secs(unsigned s)
{
	tick(s * STP_HZ);
}

struct sim_bpdu {
	uint8_t port;		/* ingress port index */
	uint8_t root_prio;	/* high byte of the root priority */
	uint8_t root_mac[6];
	uint32_t root_cost;	/* root path cost as advertised */
	uint8_t br_prio;	/* sender's own bridge priority high byte */
	uint8_t br_mac[6];
	uint8_t port_id;	/* sender's port number, 1-based */
	uint8_t port_prio;	/* 0 = 0x80 */
	uint8_t flags;		/* 0 = designated, learning + forwarding */
	uint8_t age;		/* message age, seconds */
	uint8_t maxage;		/* 0 = 20 s, and hello/fwd default with it */
	uint8_t hello;
	uint8_t fwd;
	uint8_t legacy;		/* 1 = 802.1D Config BPDU, 2 = TCN */
	uint8_t msg_len;	/* 0 = the right length for the type */
};

struct sim_pkt_in {
	uint8_t stp_addr[6];
	uint8_t src_addr[6];
	struct rtl_tag rtl_tag;
	struct vlan_tag vlan_tag;
	uint16_t msg_len;
	uint8_t dsap;
	uint8_t ssap;
	uint8_t ctrl;
	uint16_t proto;
	uint8_t version;
	uint8_t bpdu_type;
	uint8_t flags;
	struct bridge root;
	uint32_t root_path_cost;
	struct bridge bridge;
	uint8_t port_prio;
	uint8_t port_id;
	uint16_t age;
	uint16_t age_max;
	uint16_t hello;
	uint16_t fwd_delay;
	uint8_t version1_length;
};

static uint32_t swap32(uint32_t v)
{
	return (v << 24) | ((v & 0xff00) << 8) | ((v >> 8) & 0xff00) | (v >> 24);
}

static void bpdu_in(const struct sim_bpdu *b)
{
	struct sim_pkt_in *f = (struct sim_pkt_in *)uip_buf;

	memset(uip_buf, 0, sizeof(uip_buf));
	memcpy(f->stp_addr, "\x01\x80\xc2\x00\x00\x00", 6);
	memcpy(f->src_addr, b->br_mac, 6);
	f->rtl_tag.pmask = HTONS(b->port);
	f->msg_len = HTONS(b->msg_len ? b->msg_len : (b->legacy == 2 ? 7 : (b->legacy ? 38 : 39)));
	f->dsap = 0x42;
	f->ssap = 0x42;
	f->ctrl = 0x03;
	f->proto = 0;
	f->version = b->legacy ? 0x00 : 0x02;
	f->bpdu_type = b->legacy == 2 ? 0x80 : (b->legacy ? 0x00 : 0x02);
	f->flags = b->flags ? b->flags : (b->legacy ? 0x00 : 0x3c);
	f->root.prio = b->root_prio;
	f->root.ext = 0;
	memcpy(f->root.mac, b->root_mac, 6);
	f->root_path_cost = swap32(b->root_cost);
	f->bridge.prio = b->br_prio;
	f->bridge.ext = 0;
	memcpy(f->bridge.mac, b->br_mac, 6);
	f->port_prio = b->port_prio ? b->port_prio : 0x80;
	f->port_id = b->port_id;
	f->age = b->age;
	f->age_max = b->maxage ? b->maxage : 20;
	f->hello = b->maxage ? b->hello : 2;
	f->fwd_delay = b->maxage ? b->fwd : 15;
	uip_len = sizeof(struct sim_pkt_in);
	stp_in();
}

static int failures;

static void check(int cond, const char *what)
{
	if (!cond) {
		printf("  FAIL: %s\n", what);
		failures++;
	} else if (verbose)
		printf("  ok: %s\n", what);
}

static void check_state(uint8_t port, uint8_t want, const char *what)
{
	uint8_t got = port_state(port);
	if (got != want) {
		printf("  FAIL: %s: port %u is %s, expected %s\n",
		       what, port + 1, state_name(got), state_name(want));
		failures++;
	} else if (verbose)
		printf("  ok: %s (port %u %s)\n", what, port + 1, state_name(got));
}

static void reset_all(void)
{
	memset(mstp_reg, 0, sizeof(mstp_reg));
	memset(sim_lag, 0, sizeof(sim_lag));
	memset(flush_count, 0, sizeof(flush_count));
	memset(tx_frames, 0, sizeof(tx_frames));
	memset(last_tx, 0, sizeof(last_tx));
	memset(tc_frames, 0, sizeof(tc_frames));
	memset(tcn_frames, 0, sizeof(tcn_frames));
	for (uint8_t p = 0; p < NPORTS; p++)
		sim_speed[p] = 2;
	stp_enabled = 1;
	stp_defaults();
	links_set(0);
	stp_setup();
	tick(2 * STP_HZ);
}

static const uint8_t ROOT_MAC[6] = {0x1c,0x2a,0xa3,0x1e,0xc2,0x03};
static const uint8_t PEER_MAC[6] = {0x00,0x82,0x44,0x2a,0x74,0x82};

static void scen_edge_ports(void)
{
	printf("1. a port nobody talks STP on becomes an edge port\n");
	reset_all();
	links_set(1 << 3);
	secs(1);
	check_state(3, 1, "port with fresh carrier starts discarding");
	secs(4);
	check_state(3, 3, "silent port goes forwarding as an edge port");
}

static void scen_root_port(void)
{
	printf("2. the bridge follows the root heard on port 9\n");
	reset_all();
	links_set(1 << 8);
	secs(1);
	struct sim_bpdu root_bpdu = { .port = 8, .root_prio = 0x40, .root_cost = 0,
				      .br_prio = 0x40, .port_id = 2 };
	memcpy(root_bpdu.root_mac, ROOT_MAC, 6);
	memcpy(root_bpdu.br_mac, ROOT_MAC, 6);
	bpdu_in(&root_bpdu);
	check(stp_root_port == 8, "port 9 becomes the root port");
	secs(20);
	bpdu_in(&root_bpdu);
	check_state(8, 3, "root port forwards after the listen period");
}

static void scen_ring(void)
{
	printf("3. a neighbour with a cheaper path to the root must block us\n");
	reset_all();
	links_set((1 << 0) | (1 << 8));
	secs(1);

	struct sim_bpdu from_root = { .port = 8, .root_prio = 0x40, .root_cost = 0,
				      .br_prio = 0x40, .port_id = 2 };
	memcpy(from_root.root_mac, ROOT_MAC, 6);
	memcpy(from_root.br_mac, ROOT_MAC, 6);

	struct sim_bpdu from_peer = { .port = 0, .root_prio = 0x40, .root_cost = 2000,
				      .br_prio = 0x80, .port_id = 1 };
	memcpy(from_peer.root_mac, ROOT_MAC, 6);
	memcpy(from_peer.br_mac, PEER_MAC, 6);

	for (int i = 0; i < 12; i++) {
		bpdu_in(&from_root);
		bpdu_in(&from_peer);
		secs(2);
	}
	check(stp_root_port == 8, "the direct uplink stays the root port");
	check_state(8, 3, "root port forwards");
	check_state(0, 1, "port 1 is discarding (alternate)");
}

static void scen_ring_clears(void)
{
	printf("4. when the better neighbour goes quiet the port comes back\n");
	scen_ring();
	struct sim_bpdu from_root = { .port = 8, .root_prio = 0x40, .root_cost = 0,
				      .br_prio = 0x40, .port_id = 2 };
	memcpy(from_root.root_mac, ROOT_MAC, 6);
	memcpy(from_root.br_mac, ROOT_MAC, 6);
	bpdu_in(&from_root);
	secs(2);
	bpdu_in(&from_root);
	secs(2);
	check_state(0, 1, "port 1 still discards within three of the peer's hellos");
	for (int i = 0; i < 18; i++) {
		bpdu_in(&from_root);
		secs(2);
	}
	check_state(0, 3, "port 1 forwards again once the peer information aged out");
}

static void scen_cheaper_path(void)
{
	printf("5. the cheaper of two paths to the root becomes the root port\n");
	reset_all();
	stp_pcost[0] = 2000;
	links_set((1 << 0) | (1 << 8));
	secs(1);
	struct sim_bpdu from_root_p9 = { .port = 8, .root_prio = 0x40, .root_cost = 0,
					 .br_prio = 0x40, .port_id = 2 };
	memcpy(from_root_p9.root_mac, ROOT_MAC, 6);
	memcpy(from_root_p9.br_mac, ROOT_MAC, 6);
	struct sim_bpdu from_root_p1 = from_root_p9;
	from_root_p1.port = 0;
	from_root_p1.port_id = 7;
	for (int i = 0; i < 12; i++) {
		bpdu_in(&from_root_p9);
		bpdu_in(&from_root_p1);
		secs(2);
	}
	check(stp_root_port == 0, "port 1 wins on cost and becomes the root port");
	check(root_bridge_cost == 2000, "the root path cost follows that port");
	check_state(0, 3, "the root port forwards");
	check_state(8, 1, "the dearer path to the same root is discarding");
	stp_pcost[0] = 0;
}

static void scen_we_are_better(void)
{
	printf("6. a neighbour with a worse path does not block us\n");
	reset_all();
	links_set((1 << 0) | (1 << 8));
	secs(1);
	struct sim_bpdu from_root = { .port = 8, .root_prio = 0x40, .root_cost = 0,
				      .br_prio = 0x40, .port_id = 2 };
	memcpy(from_root.root_mac, ROOT_MAC, 6);
	memcpy(from_root.br_mac, ROOT_MAC, 6);
	struct sim_bpdu from_peer = { .port = 0, .root_prio = 0x40, .root_cost = 200000,
				      .br_prio = 0x80, .port_id = 1, .flags = 0x0c };
	memcpy(from_peer.root_mac, ROOT_MAC, 6);
	memcpy(from_peer.br_mac, PEER_MAC, 6);
	for (int i = 0; i < 18; i++) {
		bpdu_in(&from_root);
		bpdu_in(&from_peer);
		secs(2);
	}
	check(stp_root_port == 8, "the uplink stays the root port");
	check_state(0, 3, "port 1 keeps forwarding");
}

static void scen_alt_survives_link_bounce(void)
{
	printf("7. a blocked port stays blocked when the carrier returns\n");
	scen_ring();
	struct sim_bpdu from_root = { .port = 8, .root_prio = 0x40, .root_cost = 0,
				      .br_prio = 0x40, .port_id = 2 };
	memcpy(from_root.root_mac, ROOT_MAC, 6);
	memcpy(from_root.br_mac, ROOT_MAC, 6);
	struct sim_bpdu from_peer = { .port = 0, .root_prio = 0x40, .root_cost = 2000,
				      .br_prio = 0x80, .port_id = 1 };
	memcpy(from_peer.root_mac, ROOT_MAC, 6);
	memcpy(from_peer.br_mac, PEER_MAC, 6);

	links_set(1 << 8);
	secs(2);
	links_set((1 << 0) | (1 << 8));
	bpdu_in(&from_peer);
	for (int i = 0; i < 15; i++) {
		bpdu_in(&from_root);
		bpdu_in(&from_peer);
		secs(2);
	}
	check_state(0, 1, "port 1 never reaches forwarding while the peer is better");
	check(stp_root_port == 8, "the uplink is still the root port");
}

static void scen_speed_cost(void)
{
	printf("8. the faster of two links to one root wins on cost\n");
	reset_all();
	sim_speed[8] = 4;
	sim_speed[0] = 2;
	links_set((1 << 0) | (1 << 8));
	secs(1);
	struct sim_bpdu from_root = { .port = 8, .root_prio = 0x40, .root_cost = 0,
				      .br_prio = 0x40, .port_id = 2 };
	memcpy(from_root.root_mac, ROOT_MAC, 6);
	memcpy(from_root.br_mac, ROOT_MAC, 6);
	struct sim_bpdu slow = from_root;
	slow.port = 0;
	slow.port_id = 7;
	for (int i = 0; i < 12; i++) {
		bpdu_in(&from_root);
		bpdu_in(&slow);
		secs(2);
	}
	check(stp_root_port == 8, "the 10G port is the root port");
	check(root_bridge_cost == 2000, "its cost is the 10G value, not a flat default");
}

static struct sim_bpdu root_on(uint8_t port)
{
	struct sim_bpdu b = { .port = port, .root_prio = 0x40, .root_cost = 0,
			      .br_prio = 0x40, .port_id = 2 };
	memcpy(b.root_mac, ROOT_MAC, 6);
	memcpy(b.br_mac, ROOT_MAC, 6);
	return b;
}

static const uint8_t THIRD_MAC[6] = {0x00,0x11,0x22,0x33,0x44,0x55};

static void scen_root_times(void)
{
	printf("9. the times of the root travel on and time the listen period\n");
	reset_all();
	stp_pflags[1] &= ~STP_PF_AUTOEDGE;
	links_set(1 << 8);
	secs(1);
	struct sim_bpdu r = root_on(8);
	r.age = 3; r.maxage = 12; r.hello = 1; r.fwd = 8;
	bpdu_in(&r);
	check(stp_root_port == 8, "port 9 is the root port");
	links_set((1 << 1) | (1 << 8));
	for (int i = 0; i < 2; i++) { bpdu_in(&r); secs(1); }
	check_state(1, 1, "a new designated port starts discarding");
	for (int i = 0; i < 5; i++) { bpdu_in(&r); secs(1); }
	check_state(1, 1, "it still discards short of the root's forward delay");
	for (int i = 0; i < 3; i++) { bpdu_in(&r); secs(1); }
	check_state(1, 2, "it learns after the root's 8 s, not our own 15 s");
	for (int i = 0; i < 8; i++) { bpdu_in(&r); secs(1); }
	check_state(1, 3, "and forwards after a second 8 s");
	for (int i = 0; i < 3; i++) { bpdu_in(&r); secs(1); }
	check(tx_frames[1] > 0, "the designated port sends BPDUs");
	check((uint8_t)last_tx[1].age == 4, "the message age is one more than received");
	check((uint8_t)last_tx[1].age_max == 12, "max age is the root's");
	check((uint8_t)last_tx[1].fwd_delay == 8, "forward delay is the root's");
	check((uint8_t)last_tx[1].hello == 2, "hello time stays our own");
}

static void scen_too_old(void)
{
	printf("10. information as old as max age is not used\n");
	reset_all();
	links_set(1 << 8);
	secs(1);
	struct sim_bpdu r = root_on(8);
	r.age = 20; r.maxage = 20; r.hello = 2; r.fwd = 15;
	bpdu_in(&r);
	check(stp_root_port == 0xff, "the switch stays its own root");
	r.age = 19;
	bpdu_in(&r);
	check(stp_root_port == 8, "one second younger it is accepted");
}

static void scen_info_expiry(void)
{
	printf("11. received information lasts three of the sender's hellos\n");
	reset_all();
	links_set(1 << 8);
	secs(1);
	struct sim_bpdu r = root_on(8);
	r.maxage = 20; r.hello = 1; r.fwd = 15;
	bpdu_in(&r);
	check(stp_root_port == 8, "port 9 is the root port");
	tick(2 * STP_HZ + STP_HZ / 2);
	check(stp_root_port == 8, "still the root port 2.5 s later");
	tick(STP_HZ);
	check(stp_root_port == 0xff, "gone after 3 s with a 1 s hello");
}

static void scen_dport_tiebreak(void)
{
	printf("12. equal cost to one bridge: the lower designated port ID wins\n");
	reset_all();
	links_set((1 << 1) | (1 << 2));
	secs(1);
	struct sim_bpdu a = root_on(1); a.port_id = 5;
	struct sim_bpdu b = root_on(2); b.port_id = 2;
	for (int i = 0; i < 12; i++) { bpdu_in(&a); bpdu_in(&b); secs(2); }
	check(stp_root_port == 2, "port 3, which hears port ID 8002, is the root port");
	check_state(1, 1, "port 2, which hears 8005, is discarding");
}

static void scen_rxport_tiebreak(void)
{
	printf("13. same designated port on two of ours: the receiving port ID decides\n");
	reset_all();
	links_set((1 << 3) | (1 << 4));
	secs(1);
	struct sim_bpdu a = root_on(3);
	struct sim_bpdu b = root_on(4);
	for (int i = 0; i < 3; i++) { bpdu_in(&a); bpdu_in(&b); secs(2); }
	check(stp_root_port == 3, "equal port priority: the lower port number, port 4");
	stp_pprio[4] = 0x40;
	for (int i = 0; i < 3; i++) { bpdu_in(&a); bpdu_in(&b); secs(2); }
	check(stp_root_port == 4, "a better priority on port 5 wins");
	stp_pprio[4] = 0x80;
}

static void scen_inferior_info(void)
{
	printf("14. worse information replaces stored only from the same sender\n");
	reset_all();
	links_set((1 << 0) | (1 << 8));
	secs(1);
	struct sim_bpdu r = root_on(8);
	struct sim_bpdu peer = { .port = 0, .root_prio = 0x40, .root_cost = 2000,
				 .br_prio = 0x80, .port_id = 1 };
	memcpy(peer.root_mac, ROOT_MAC, 6);
	memcpy(peer.br_mac, PEER_MAC, 6);
	struct sim_bpdu third = peer;
	third.root_cost = 400000;
	memcpy(third.br_mac, THIRD_MAC, 6);
	for (int i = 0; i < 10; i++) { bpdu_in(&r); bpdu_in(&peer); secs(2); }
	check_state(0, 1, "port 1 is discarding behind the better peer");
	for (int i = 0; i < 2; i++) { bpdu_in(&r); bpdu_in(&peer); bpdu_in(&third); secs(1); }
	check(stp_dcost[0] == 2000, "a worse bridge on the segment does not overwrite the peer");
	check_state(0, 1, "and port 1 keeps discarding");
	peer.root_cost = 400000;
	peer.flags = 0x0c;
	bpdu_in(&peer);
	check(stp_dcost[0] == 400000, "the peer's own worse information does replace it");
	for (int i = 0; i < 18; i++) { bpdu_in(&r); bpdu_in(&peer); secs(2); }
	check_state(0, 3, "port 1 is designated and forwards again");
}

static void scen_root_role_bpdu(void)
{
	printf("15. an RST BPDU sent by a root port is not designated information\n");
	reset_all();
	links_set((1 << 0) | (1 << 8));
	secs(1);
	struct sim_bpdu r = root_on(8);
	struct sim_bpdu peer = { .port = 0, .root_prio = 0x40, .root_cost = 2000,
				 .br_prio = 0x80, .port_id = 1, .flags = 0x08 | 0x30 };
	memcpy(peer.root_mac, ROOT_MAC, 6);
	memcpy(peer.br_mac, PEER_MAC, 6);
	for (int i = 0; i < 18; i++) { bpdu_in(&r); bpdu_in(&peer); secs(2); }
	check_state(0, 3, "port 1 keeps forwarding");
	check(stp_root_port == 8, "the uplink stays the root port");
}

static void scen_alternate_takes_over(void)
{
	printf("16. when the root port's information expires an alternate path takes over\n");
	reset_all();
	stp_pcost[8] = 2000;
	links_set((1 << 0) | (1 << 8));
	secs(1);
	struct sim_bpdu r = root_on(8);
	struct sim_bpdu peer = { .port = 0, .root_prio = 0x40, .root_cost = 2000,
				 .br_prio = 0x80, .port_id = 1 };
	memcpy(peer.root_mac, ROOT_MAC, 6);
	memcpy(peer.br_mac, PEER_MAC, 6);
	for (int i = 0; i < 12; i++) { bpdu_in(&r); bpdu_in(&peer); secs(2); }
	check(stp_root_port == 8, "the 10G uplink is the root port");
	for (int i = 0; i < 4; i++) { bpdu_in(&peer); secs(2); }
	check(stp_root_port == 0, "port 1 is the root port once port 9 went quiet");
	check(memcmp(root_bridge.mac, ROOT_MAC, 6) == 0, "the root is still the same bridge");
	check(root_bridge_cost == 22000, "the cost is the peer's plus port 1's");
	stp_pcost[8] = 0;
}

static void scen_dbridge_tiebreak(void)
{
	printf("17. equal cost through two bridges: the lower bridge ID wins\n");
	reset_all();
	links_set((1 << 1) | (1 << 2));
	secs(1);
	struct sim_bpdu via_peer = { .port = 1, .root_prio = 0x40, .root_cost = 2000,
				     .br_prio = 0x80, .port_id = 1 };
	memcpy(via_peer.root_mac, ROOT_MAC, 6);
	memcpy(via_peer.br_mac, PEER_MAC, 6);
	struct sim_bpdu via_third = via_peer;
	via_third.port = 2;
	memcpy(via_third.br_mac, THIRD_MAC, 6);
	via_third.br_prio = 0x70;
	for (int i = 0; i < 12; i++) { bpdu_in(&via_peer); bpdu_in(&via_third); secs(2); }
	check(stp_root_port == 2, "port 3, behind the bridge with priority 7000, is the root port");
	check_state(1, 1, "port 2, behind 8000, is discarding");
}

static void scen_times_revert(void)
{
	printf("18. back to our own times once we are the root again\n");
	reset_all();
	links_set((1 << 1) | (1 << 8));
	secs(1);
	struct sim_bpdu r = root_on(8);
	r.age = 3; r.maxage = 12; r.hello = 2; r.fwd = 8;
	for (int i = 0; i < 4; i++) { bpdu_in(&r); secs(2); }
	check(stp_root_port == 8, "port 9 is the root port");
	secs(8);
	check(stp_root_port == 0xff, "the switch is its own root again");
	tx_frames[1] = 0;
	secs(3);
	check(tx_frames[1] > 0, "port 2 still sends");
	check((uint8_t)last_tx[1].age == 0, "the message age is 0 from the root");
	check((uint8_t)last_tx[1].age_max == 20, "max age is our own again");
	check((uint8_t)last_tx[1].fwd_delay == 15, "forward delay is our own again");
}

static void ring_with_uplink_on_9(struct sim_bpdu *r, struct sim_bpdu *peer)
{
	reset_all();
	stp_pcost[8] = 2000;
	links_set((1 << 0) | (1 << 8));
	secs(1);
	*r = root_on(8);
	struct sim_bpdu p = { .port = 0, .root_prio = 0x40, .root_cost = 2000,
			      .br_prio = 0x80, .port_id = 1 };
	memcpy(p.root_mac, ROOT_MAC, 6);
	memcpy(p.br_mac, PEER_MAC, 6);
	*peer = p;
	for (int i = 0; i < 12; i++) { bpdu_in(r); bpdu_in(peer); secs(2); }
}

static void scen_tc_via_root_port(void)
{
	printf("19. a port that takes over as root port tells the root about it\n");
	struct sim_bpdu r, peer;
	ring_with_uplink_on_9(&r, &peer);
	check(stp_root_port == 8 && port_state(0) == 1, "port 1 starts as the blocked alternate");
	check(tc_frames[0] == 0, "no TC on port 1 so far");
	links_set(1 << 0);
	int waited = 0;
	while (port_state(0) != 3 && waited < 20 * STP_HZ) { bpdu_in(&peer); tick(STP_HZ / 2); waited += STP_HZ / 2; }
	check(stp_root_port == 0, "port 1 is the root port after port 9 went down");
	check_state(0, 3, "and forwards");
	tick(2);
	check(tc_frames[0] > 0, "port 1 sends a BPDU with TC toward the root right away");
	check((last_tx[0].flags & 0x0c) == 0x08, "that BPDU carries the root port role");
	stp_pcost[8] = 0;
}

static void scen_tc_window_rstp(void)
{
	printf("20. in RSTP the TC window lasts hello time plus one second\n");
	struct sim_bpdu r, peer;
	ring_with_uplink_on_9(&r, &peer);
	links_set(1 << 0);
	int waited = 0;
	while (port_state(0) != 3 && waited < 20 * STP_HZ) { bpdu_in(&peer); tick(STP_HZ / 2); waited += STP_HZ / 2; }
	for (int i = 0; i < 4; i++) { bpdu_in(&peer); secs(1); }
	int seen = tc_frames[0];
	for (int i = 0; i < 6; i++) { bpdu_in(&peer); secs(1); }
	check(seen > 0, "TC was sent");
	check(tc_frames[0] == seen, "and stops once hello plus one second is over");
	stp_pcost[8] = 0;
}

static void scen_tc_propagation(void)
{
	printf("21. a TC from the root side goes out on the other ports, not back\n");
	reset_all();
	stp_pflags[1] &= ~STP_PF_AUTOEDGE;
	links_set((1 << 1) | (1 << 8));
	secs(1);
	struct sim_bpdu r = root_on(8);
	for (int i = 0; i < 18; i++) { bpdu_in(&r); secs(2); }
	check_state(1, 3, "designated port 2 forwards");
	for (int i = 0; i < 3; i++) { bpdu_in(&r); secs(2); }
	memset(flush_count, 0, sizeof(flush_count));
	int tc2 = tc_frames[1], tc9 = tc_frames[8];
	struct sim_bpdu rtc = r; rtc.flags = 0x3d;
	bpdu_in(&rtc);
	tick(2);
	check(tc_frames[1] > tc2, "port 2 passes the TC on at once");
	check(flush_count[1] > 0, "port 2 forgets what it learned");
	check(flush_count[8] == 0, "the port the TC came in on keeps its addresses");
	check(tc_frames[8] == tc9, "no TC is echoed back to the root");
}

static void scen_tc_legacy_tcn(void)
{
	printf("22. with 802.1D neighbours the root port sends TCN until acknowledged\n");
	reset_all();
	stp_rstp = 0;
	stp_pflags[1] &= ~STP_PF_AUTOEDGE;
	links_set(1 << 8);
	secs(1);
	struct sim_bpdu r = root_on(8); r.legacy = 1;
	for (int i = 0; i < 18; i++) { bpdu_in(&r); secs(2); }
	check(stp_root_port == 8, "port 9 is the root port");
	check(tcn_frames[8] > 0, "port 9 entering forwarding is itself reported with TCN");
	struct sim_bpdu ack = r; ack.flags = 0x81;
	bpdu_in(&ack);
	int quiet = tcn_frames[8];
	for (int i = 0; i < 5; i++) { bpdu_in(&r); secs(2); }
	check(tcn_frames[8] == quiet, "acknowledged: no more TCN while nothing changes");
	links_set((1 << 1) | (1 << 8));
	for (int i = 0; i < 18; i++) { bpdu_in(&r); secs(2); }
	check_state(1, 3, "port 2 came up and forwards");
	check(tcn_frames[8] > quiet, "port 9 sends TCN toward the root again");
	bpdu_in(&ack);
	int n = tcn_frames[8];
	for (int i = 0; i < 5; i++) { bpdu_in(&r); secs(2); }
	check(tcn_frames[8] == n, "and stops once the root acknowledged it");
}

static void scen_alt_forgets(void)
{
	printf("23. a port that turns alternate forgets what it learned\n");
	reset_all();
	links_set((1 << 0) | (1 << 8));
	secs(1);
	struct sim_bpdu r = root_on(8);
	for (int i = 0; i < 10; i++) { bpdu_in(&r); secs(2); }
	check_state(0, 3, "port 1 forwards as designated");
	memset(flush_count, 0, sizeof(flush_count));
	struct sim_bpdu peer = { .port = 0, .root_prio = 0x40, .root_cost = 2000,
				 .br_prio = 0x80, .port_id = 1 };
	memcpy(peer.root_mac, ROOT_MAC, 6);
	memcpy(peer.br_mac, PEER_MAC, 6);
	bpdu_in(&peer);
	check_state(0, 1, "port 1 is blocked behind the better peer");
	check(flush_count[0] > 0, "and its addresses are flushed");
}

static void scen_edge_no_tc(void)
{
	printf("24. an edge port coming up raises no topology change\n");
	reset_all();
	links_set((1 << 1) | (1 << 8));
	secs(1);
	struct sim_bpdu r = root_on(8);
	for (int i = 0; i < 10; i++) { bpdu_in(&r); secs(2); }
	for (int i = 0; i < 20; i++) { bpdu_in(&r); secs(2); }
	uint16_t tc = stp_tc_count;
	int tc9 = tc_frames[8], tc2 = tc_frames[1];
	links_set((1 << 1) | (1 << 3) | (1 << 8));
	for (int i = 0; i < 4; i++) { bpdu_in(&r); secs(2); }
	check_state(3, 3, "the host port forwards as an edge port");
	check(stp_tc_count == tc, "the change counter does not move");
	check(tc_frames[8] == tc9 && tc_frames[1] == tc2, "no TC is sent");
}

static void scen_link_loss_no_tc(void)
{
	printf("25. losing a designated link flushes it but sends no TC\n");
	reset_all();
	stp_pflags[1] &= ~STP_PF_AUTOEDGE;
	stp_pflags[2] &= ~STP_PF_AUTOEDGE;
	links_set((1 << 1) | (1 << 2) | (1 << 8));
	secs(1);
	struct sim_bpdu r = root_on(8);
	for (int i = 0; i < 20; i++) { bpdu_in(&r); secs(2); }
	check_state(1, 3, "port 2 forwards");
	memset(flush_count, 0, sizeof(flush_count));
	int tc3 = tc_frames[2], tc9 = tc_frames[8];
	links_set((1 << 2) | (1 << 8));
	for (int i = 0; i < 3; i++) { bpdu_in(&r); secs(1); }
	check(flush_count[1] > 0, "port 2's addresses are flushed");
	check(tc_frames[2] == tc3 && tc_frames[8] == tc9, "no TC goes out");
}

static void scen_proposal_agreement(void)
{
	printf("26. a new designated port proposes and forwards as soon as the neighbour agrees\n");
	reset_all();
	stp_pflags[1] &= ~STP_PF_AUTOEDGE;
	links_set(1 << 8);
	secs(1);
	struct sim_bpdu r = root_on(8);
	for (int i = 0; i < 3; i++) { bpdu_in(&r); secs(2); }
	links_set((1 << 1) | (1 << 8));
	tick(STP_HZ + STP_HZ / 2);
	check_state(1, 1, "port 2 starts discarding");
	check((last_tx[1].flags & 0x02) != 0, "and its BPDU carries the proposal flag");
	check((last_tx[1].flags & 0x0c) == 0x0c, "with the designated role");
	struct sim_bpdu agree = { .port = 1, .root_prio = 0x40, .root_cost = 22000,
				  .br_prio = 0x80, .port_id = 3, .flags = 0x78 };
	memcpy(agree.root_mac, ROOT_MAC, 6);
	memcpy(agree.br_mac, PEER_MAC, 6);
	bpdu_in(&agree);
	tick(2);
	check_state(1, 3, "port 2 forwards right after the agreement");
}

static void scen_no_agreement(void)
{
	printf("27. without an agreement the port waits both forward delays\n");
	reset_all();
	stp_pflags[1] &= ~STP_PF_AUTOEDGE;
	stp_pflags[2] &= ~STP_PF_AUTOEDGE;
	stp_pp2p[2] = 2;
	links_set(1 << 8);
	secs(1);
	struct sim_bpdu r = root_on(8);
	for (int i = 0; i < 3; i++) { bpdu_in(&r); secs(2); }
	links_set((1 << 1) | (1 << 2) | (1 << 8));
	for (int i = 0; i < 5; i++) { bpdu_in(&r); secs(2); }
	check_state(1, 1, "port 2 still discards after 10 s");
	check((last_tx[2].flags & 0x02) == 0, "a port set to p2p off does not propose");
	for (int i = 0; i < 4; i++) { bpdu_in(&r); secs(2); }
	check_state(1, 2, "port 2 learns once the first forward delay is over");
	for (int i = 0; i < 8; i++) { bpdu_in(&r); secs(2); }
	check_state(1, 3, "and forwards after the second");
	stp_pp2p[2] = 0;
}

static void scen_sync_on_proposal(void)
{
	printf("28. a proposal on the root port syncs the designated ports and is agreed\n");
	reset_all();
	stp_pflags[1] &= ~STP_PF_AUTOEDGE;
	links_set((1 << 1) | (1 << 3) | (1 << 8));
	secs(1);
	struct sim_bpdu r = root_on(8);
	for (int i = 0; i < 18; i++) { bpdu_in(&r); secs(2); }
	check_state(1, 3, "designated port 2 forwards");
	check_state(3, 3, "edge port 4 forwards");
	check_state(8, 3, "root port 9 forwards");
	struct sim_bpdu prop = r; prop.flags = 0x3e;
	bpdu_in(&prop);
	tick(2);
	check_state(1, 1, "port 2 is put back to discarding for the sync");
	check((last_tx[1].flags & 0x02) != 0, "and proposes downstream");
	check_state(3, 3, "the edge port is left alone");
	check_state(8, 3, "the root port keeps forwarding");
	check((last_tx[8].flags & 0x40) != 0, "the root port answers with an agreement");
	check((last_tx[8].flags & 0x0c) == 0x08, "in a BPDU with the root role");
}

static void scen_rapid_failover(void)
{
	printf("29. when the root port goes down the alternate forwards at once\n");
	struct sim_bpdu r, peer;
	ring_with_uplink_on_9(&r, &peer);
	check(stp_root_port == 8 && port_state(0) == 1, "port 1 is the blocked alternate");
	links_set(1 << 0);
	for (int i = 0; i < 4; i++) { bpdu_in(&peer); tick(STP_HZ / 2); }
	check(stp_root_port == 0, "port 1 is the root port within two seconds");
	check_state(0, 3, "and already forwards");
	stp_pcost[8] = 0;
}

static void scen_rapid_reroot(void)
{
	printf("30. the uplink coming back takes over without both ports forwarding\n");
	struct sim_bpdu r, peer;
	ring_with_uplink_on_9(&r, &peer);
	stp_pcost[8] = 2000;
	links_set(1 << 0);
	for (int i = 0; i < 6; i++) { bpdu_in(&peer); tick(STP_HZ / 2); }
	check(stp_root_port == 0 && port_state(0) == 3, "port 1 is the forwarding root port");
	links_set((1 << 0) | (1 << 8));
	int both = 0;
	for (int i = 0; i < 12; i++) {
		struct sim_bpdu p = r; p.flags = 0x3e;
		bpdu_in(&p);
		if (port_state(0) == 3 && port_state(8) == 3) both++;
		bpdu_in(&peer);
		if (port_state(0) == 3 && port_state(8) == 3) both++;
		for (int k = 0; k < STP_HZ / 4; k++) {
			tick(1);
			if (port_state(0) == 3 && port_state(8) == 3) both++;
		}
	}
	check(stp_root_port == 8, "port 9 is the root port again");
	check_state(8, 3, "and forwards within three seconds");
	check_state(0, 1, "port 1 is back to discarding");
	check(both == 0, "ports 1 and 9 never forwarded at the same time");
	stp_pcost[8] = 0;
}

static void scen_agreement_other_root(void)
{
	printf("31. an agreement for a different root does not open a port\n");
	reset_all();
	stp_pflags[1] &= ~STP_PF_AUTOEDGE;
	links_set(1 << 8);
	secs(1);
	struct sim_bpdu r = root_on(8);
	for (int i = 0; i < 3; i++) { bpdu_in(&r); secs(2); }
	links_set((1 << 1) | (1 << 8));
	tick(STP_HZ + STP_HZ / 2);
	struct sim_bpdu agree = { .port = 1, .root_prio = 0x40, .root_cost = 22000,
				  .br_prio = 0x80, .port_id = 3, .flags = 0x78 };
	memcpy(agree.root_mac, THIRD_MAC, 6);
	memcpy(agree.br_mac, PEER_MAC, 6);
	bpdu_in(&agree);
	tick(2);
	check_state(1, 1, "port 2 keeps discarding");
}

static void scen_repeated_proposal(void)
{
	printf("32. a proposal repeated every hello syncs only once\n");
	reset_all();
	stp_pflags[1] &= ~STP_PF_AUTOEDGE;
	links_set((1 << 1) | (1 << 8));
	secs(1);
	struct sim_bpdu r = root_on(8);
	for (int i = 0; i < 18; i++) { bpdu_in(&r); secs(2); }
	check_state(1, 3, "designated port 2 forwards");
	struct sim_bpdu prop = r; prop.flags = 0x3e;
	bpdu_in(&prop);
	tick(2);
	check_state(1, 1, "the first proposal syncs port 2");
	struct sim_bpdu agree = { .port = 1, .root_prio = 0x40, .root_cost = 22000,
				  .br_prio = 0x80, .port_id = 3, .flags = 0x78 };
	memcpy(agree.root_mac, ROOT_MAC, 6);
	memcpy(agree.br_mac, PEER_MAC, 6);
	bpdu_in(&agree);
	tick(2);
	check_state(1, 3, "port 2 forwards again after its own agreement");
	int agreements = 0;
	for (int i = 0; i < 5; i++) {
		int before = tx_frames[8];
		bpdu_in(&prop);
		tick(2);
		if (tx_frames[8] > before && (last_tx[8].flags & 0x40)) agreements++;
		secs(2);
		check_state(1, 3, "a repeated proposal does not block port 2 again");
	}
	check(agreements == 5, "every repeated proposal is answered with an agreement");
}

static void scen_dispute(void)
{
	printf("33. a worse neighbour that claims to forward as designated is disputed\n");
	reset_all();
	stp_pflags[1] &= ~STP_PF_AUTOEDGE;
	links_set((1 << 1) | (1 << 8));
	secs(1);
	struct sim_bpdu r = root_on(8);
	for (int i = 0; i < 18; i++) { bpdu_in(&r); secs(2); }
	check_state(1, 3, "designated port 2 forwards");
	struct sim_bpdu quiet = { .port = 1, .root_prio = 0x40, .root_cost = 200000,
				  .br_prio = 0x80, .port_id = 4, .flags = 0x0c };
	memcpy(quiet.root_mac, ROOT_MAC, 6);
	memcpy(quiet.br_mac, PEER_MAC, 6);
	bpdu_in(&quiet);
	tick(2);
	check_state(1, 3, "worse designated info without the learning flag is no dispute");
	struct sim_bpdu claim = quiet; claim.flags = 0x3c;
	bpdu_in(&claim);
	tick(2);
	check_state(1, 1, "with the learning flag set port 2 goes back to discarding");
	int opened = 0;
	for (int i = 0; i < 12; i++) {
		bpdu_in(&r);
		bpdu_in(&claim);
		for (int k = 0; k < 2 * STP_HZ; k++) {
			tick(1);
			if (port_state(1) == 3) opened++;
		}
	}
	check(opened == 0, "and never forwards while the dispute goes on");
	for (int i = 0; i < 18; i++) { bpdu_in(&r); bpdu_in(&quiet); secs(2); }
	check_state(1, 3, "once the neighbour stops claiming, port 2 forwards again");
}

static void scen_learning_state(void)
{
	printf("34. without an agreement a port learns for one forward delay before it forwards\n");
	reset_all();
	stp_pflags[1] &= ~STP_PF_AUTOEDGE;
	links_set(1 << 8);
	secs(1);
	struct sim_bpdu r = root_on(8);
	for (int i = 0; i < 3; i++) { bpdu_in(&r); secs(2); }
	links_set((1 << 1) | (1 << 8));
	for (int i = 0; i < 6; i++) { bpdu_in(&r); secs(2); }
	check_state(1, 1, "port 2 discards for the first forward delay");
	for (int i = 0; i < 3; i++) { bpdu_in(&r); secs(2); }
	check_state(1, 2, "then learns");
	check((last_tx[1].flags & 0x30) == 0x10, "its BPDUs carry learning without forwarding");
	struct sim_bpdu claim = { .port = 1, .root_prio = 0x40, .root_cost = 200000,
				  .br_prio = 0x80, .port_id = 4, .flags = 0x3c };
	memcpy(claim.root_mac, ROOT_MAC, 6);
	memcpy(claim.br_mac, PEER_MAC, 6);
	bpdu_in(&claim);
	tick(2);
	check_state(1, 1, "a dispute while learning sends it back to discarding");
	for (int i = 0; i < 9; i++) { bpdu_in(&r); secs(2); }
	check_state(1, 2, "after a full forward delay it learns again");
	for (int i = 0; i < 8; i++) { bpdu_in(&r); secs(2); }
	check_state(1, 3, "and forwards after the second forward delay");
}

static void scen_reply_to_inferior(void)
{
	printf("35. a designated port answers worse information at once\n");
	reset_all();
	stp_pflags[1] &= ~STP_PF_AUTOEDGE;
	links_set((1 << 1) | (1 << 8));
	secs(1);
	struct sim_bpdu r = root_on(8);
	for (int i = 0; i < 18; i++) { bpdu_in(&r); secs(2); }
	check_state(1, 3, "designated port 2 forwards");
	int n = tx_frames[1], guard = 0;
	while (tx_frames[1] == n && guard++ < 3 * STP_HZ) tick(1);
	check(tx_frames[1] > n, "port 2 sent its regular hello");
	struct sim_bpdu worse = { .port = 1, .root_prio = 0x40, .root_cost = 200000,
				  .br_prio = 0x80, .port_id = 4, .flags = 0x0c };
	memcpy(worse.root_mac, ROOT_MAC, 6);
	memcpy(worse.br_mac, PEER_MAC, 6);
	n = tx_frames[1];
	bpdu_in(&worse);
	tick(2);
	check(tx_frames[1] > n, "the worse BPDU is answered within two ticks, not at the next hello");
	tick(STP_HZ / 2);
	n = tx_frames[1];
	struct sim_bpdu same_root_better = { .port = 1, .root_prio = 0x40, .root_cost = 2000,
					     .br_prio = 0x80, .port_id = 1, .flags = 0x0c };
	memcpy(same_root_better.root_mac, ROOT_MAC, 6);
	memcpy(same_root_better.br_mac, PEER_MAC, 6);
	bpdu_in(&same_root_better);
	tick(2);
	check(tx_frames[1] == n, "better information is not answered");
}

static void scen_backup_role(void)
{
	printf("36. a port hearing another port of this bridge is a blocked backup\n");
	reset_all();
	stp_pflags[2] &= ~STP_PF_AUTOEDGE;
	stp_pflags[3] &= ~STP_PF_AUTOEDGE;
	links_set((1 << 2) | (1 << 3) | (1 << 8));
	secs(1);
	struct sim_bpdu r = root_on(8);
	for (int i = 0; i < 18; i++) { bpdu_in(&r); secs(2); }
	check_state(2, 3, "port 3 forwards");
	check_state(3, 3, "port 4 forwards");
	struct sim_bpdu own = { .port = 2, .root_prio = 0x40, .root_cost = 20000,
				.br_prio = 0x80, .port_id = 4 };
	memcpy(own.root_mac, ROOT_MAC, 6);
	memcpy(own.br_mac, uip_ethaddr.addr, 6);
	bpdu_in(&own);
	tick(2);
	check_state(3, 1, "port 4, the worse of the two, is blocked");
	check(((stp_backup >> 3) & 1) == 1, "and has the backup role");
	check(((stp_backup >> 2) & 1) == 0, "port 3 is not a backup");
	for (int i = 0; i < 20; i++) { bpdu_in(&r); secs(2); }
	check_state(3, 3, "once its own BPDUs stop coming back port 4 forwards again");
	check(((stp_backup >> 3) & 1) == 0, "and is no longer a backup");
	bpdu_in(&own);
	tick(2);
	check(((stp_backup >> 3) & 1) == 1, "a new loop makes it a backup again");
	links_set((1 << 2) | (1 << 8));
	secs(2);
	check(((stp_backup >> 3) & 1) == 0, "losing the link clears the backup role");
}

static void scen_migrate_to_stp(void)
{
	printf("37. a port that hears an 802.1D neighbour sends Config BPDUs\n");
	reset_all();
	stp_pflags[1] &= ~STP_PF_AUTOEDGE;
	links_set((1 << 1) | (1 << 8));
	secs(1);
	struct sim_bpdu r = root_on(8);
	for (int i = 0; i < 3; i++) { bpdu_in(&r); secs(2); }
	check(last_tx[1].version == 2, "port 2 starts with RST BPDUs");
	struct sim_bpdu old = { .port = 1, .root_prio = 0x40, .root_cost = 200000,
				.br_prio = 0x80, .port_id = 4, .legacy = 1 };
	memcpy(old.root_mac, ROOT_MAC, 6);
	memcpy(old.br_mac, PEER_MAC, 6);
	bpdu_in(&old);
	tick(2);
	check(last_tx[1].version == 0 && last_tx[1].bpdu_type == 0, "after the 802.1D BPDU port 2 sends Config BPDUs");
	check((last_tx[1].flags & 0x02) == 0, "without a proposal");
	check(((stp_legacy >> 1) & 1) == 1 && ((stp_legacy >> 8) & 1) == 0, "only port 2 is switched, not the root port");
}

static void scen_migrate_back(void)
{
	printf("38. mcheck and an RST neighbour bring the port back to RSTP\n");
	scen_migrate_to_stp();
	struct sim_bpdu r = root_on(8);
	secs(4);
	stp_port_mcheck(1);
	tick(2);
	check(last_tx[1].version == 2, "mcheck makes port 2 send RST BPDUs again");
	struct sim_bpdu old = { .port = 1, .root_prio = 0x40, .root_cost = 200000,
				.br_prio = 0x80, .port_id = 4, .legacy = 1 };
	memcpy(old.root_mac, ROOT_MAC, 6);
	memcpy(old.br_mac, PEER_MAC, 6);
	bpdu_in(&old);
	tick(STP_HZ);
	check(last_tx[1].version == 2, "an 802.1D BPDU within the migrate time does not switch it yet");
	for (int i = 0; i < 4; i++) { bpdu_in(&r); tick(STP_HZ / 2); }
	tick(2 * STP_HZ);
	check(last_tx[1].version == 0, "once the migrate time is over it falls back to STP");
	secs(4);
	struct sim_bpdu rst = old; rst.legacy = 0; rst.flags = 0x0c;
	bpdu_in(&rst);
	tick(2);
	check(last_tx[1].version == 2, "an RST BPDU after the migrate time switches it back to RSTP");
}

static void scen_legacy_root_tcn(void)
{
	printf("39. behind an 802.1D root port a topology change goes out as TCN\n");
	reset_all();
	stp_pflags[1] &= ~STP_PF_AUTOEDGE;
	links_set(1 << 8);
	secs(1);
	struct sim_bpdu r = root_on(8); r.legacy = 1;
	for (int i = 0; i < 18; i++) { bpdu_in(&r); secs(2); }
	check(stp_root_port == 8, "port 9 is the root port");
	struct sim_bpdu ack = r; ack.flags = 0x81;
	bpdu_in(&ack);
	for (int i = 0; i < 5; i++) { bpdu_in(&r); secs(2); }
	int tcn = tcn_frames[8];
	links_set((1 << 1) | (1 << 8));
	for (int i = 0; i < 18; i++) { bpdu_in(&r); secs(2); }
	check_state(1, 3, "port 2 came up and forwards");
	check(tcn_frames[8] > tcn, "port 9 reports the change with TCN");
	check(last_tx[1].version == 2, "port 2, with no 802.1D neighbour, sends RST BPDUs");
}

static void scen_alternate_agrees(void)
{
	printf("40. an alternate port answers a proposal with an agreement\n");
	struct sim_bpdu r, peer;
	ring_with_uplink_on_9(&r, &peer);
	stp_pflags[2] &= ~STP_PF_AUTOEDGE;
	links_set((1 << 0) | (1 << 2) | (1 << 8));
	for (int i = 0; i < 18; i++) { bpdu_in(&r); bpdu_in(&peer); secs(2); }
	check(stp_root_port == 8 && port_state(0) == 1, "port 1 is the blocked alternate");
	check_state(2, 3, "designated port 3 forwards");
	struct sim_bpdu prop = peer; prop.flags = 0x3e;
	int n = tx_frames[0];
	bpdu_in(&prop);
	tick(2);
	check(tx_frames[0] > n, "port 1 answers at once");
	check((last_tx[0].flags & 0x40) != 0, "with the agreement flag");
	check((last_tx[0].flags & 0x0c) == 0x04, "in a BPDU with the alternate role");
	check((last_tx[0].flags & 0x02) == 0, "and without a proposal of its own");
	check_state(0, 1, "port 1 stays discarding");
	check_state(8, 3, "the root port is not touched by the sync");
	check_state(2, 1, "the first proposal syncs designated port 3");
	for (int i = 0; i < 18; i++) { bpdu_in(&r); bpdu_in(&peer); secs(2); }
	check_state(2, 3, "port 3 forwards again");
	n = tx_frames[0];
	bpdu_in(&prop);
	tick(2);
	check(tx_frames[0] > n && (last_tx[0].flags & 0x40), "a repeated proposal is answered again");
	check_state(2, 3, "but does not sync port 3 a second time");
	n = tx_frames[0];
	for (int i = 0; i < 3; i++) { bpdu_in(&r); bpdu_in(&peer); secs(2); }
	check(tx_frames[0] == n, "without a proposal the alternate port stays silent");
	stp_pcost[8] = 0;
}

static void scen_agreement_from_alternate(void)
{
	printf("41. an agreement from an alternate port opens a designated port\n");
	reset_all();
	stp_pflags[1] &= ~STP_PF_AUTOEDGE;
	links_set(1 << 8);
	secs(1);
	struct sim_bpdu r = root_on(8);
	for (int i = 0; i < 3; i++) { bpdu_in(&r); secs(2); }
	links_set((1 << 1) | (1 << 8));
	tick(STP_HZ + STP_HZ / 2);
	check_state(1, 1, "port 2 proposes from discarding");
	struct sim_bpdu agree = { .port = 1, .root_prio = 0x40, .root_cost = 22000,
				  .br_prio = 0x80, .port_id = 3, .flags = 0x44 };
	memcpy(agree.root_mac, ROOT_MAC, 6);
	memcpy(agree.br_mac, PEER_MAC, 6);
	bpdu_in(&agree);
	tick(2);
	check_state(1, 3, "the agreement in an alternate-role BPDU opens it");
}

static void scen_timer_relation(void)
{
	printf("42. bridge times must keep 2*(fwd-1) >= maxage >= 2*(hello+1)\n");
	reset_all();
	save_cmd = 1;
	sim_cmd("stp fwd 30");
	sim_cmd("stp maxage 40");
	check(stp_fwddelay_s == 30 && stp_maxage_s == 40, "fwd 30 then maxage 40 is accepted");
	check(err_status == ERR_OK, "an accepted value reports no error, so it is kept in the command history");
	sim_cmd("stp fwd 20");
	check(stp_fwddelay_s == 30, "fwd 20 under maxage 40 is refused");
	check(err_status != ERR_OK, "a refused value reports an error, so it stays out of the command history");
	sim_cmd("stp port 3 cost x");
	check(err_status != ERR_OK, "a malformed command reports an error too");
	sim_cmd("stp maxage 6");
	sim_cmd("stp hello 3");
	check(stp_maxage_s == 6 && stp_hello_s == 2, "hello 3 under maxage 6 is refused");
	sim_cmd("stp fwd 4");
	sim_cmd("stp maxage 7");
	check(stp_fwddelay_s == 4 && stp_maxage_s == 6, "maxage 7 over fwd 4 is refused");
	stp_defaults();
	save_cmd = 0;
	sim_cmd("stp maxage 40");
	sim_cmd("stp fwd 30");
	check(stp_maxage_s == 40 && stp_fwddelay_s == 30, "a saved config may pass through a broken order");
	stp_setup();
	check(stp_maxage_s == 40 && stp_fwddelay_s == 30, "and keeps its values when the result is consistent");
	sim_cmd("stp fwd 10");
	stp_setup();
	check(stp_hello_s == 2 && stp_maxage_s == 20 && stp_fwddelay_s == 15, "an inconsistent result falls back to 2/20/15 when STP starts");
	save_cmd = 1;
}

static void scen_recent_root(void)
{
	printf("43. ports that were root a moment ago are held back when a new root port forwards\n");
	reset_all();
	stp_pflags[1] &= ~STP_PF_AUTOEDGE;
	stp_pflags[2] &= ~STP_PF_AUTOEDGE;
	links_set((1 << 1) | (1 << 8));
	secs(1);
	struct sim_bpdu r9 = root_on(8);
	for (int i = 0; i < 18; i++) { bpdu_in(&r9); secs(2); }
	check(stp_root_port == 8, "port 9 is the root port");
	check_state(1, 3, "designated port 2 forwards");
	stp_rrwhile[1] = 10 * STP_HZ;
	stp_pcost[2] = 2000;
	links_set((1 << 1) | (1 << 2) | (1 << 8));
	secs(1);
	struct sim_bpdu r3 = root_on(2);
	bpdu_in(&r3);
	tick(2);
	check(stp_root_port == 2, "port 3, the cheaper path, is the new root port");
	check_state(2, 3, "and forwards at once");
	check_state(8, 1, "the old root port 9 is discarding");
	check_state(1, 1, "port 2, a recent root port too, is put back to discarding");
	struct sim_bpdu agree = { .port = 1, .root_prio = 0x40, .root_cost = 22000,
				  .br_prio = 0x80, .port_id = 3, .flags = 0x78 };
	memcpy(agree.root_mac, ROOT_MAC, 6);
	memcpy(agree.br_mac, PEER_MAC, 6);
	bpdu_in(&agree);
	tick(2);
	check_state(1, 1, "an agreement does not open it while it is a recent root");
	for (int i = 0; i < 6; i++) { bpdu_in(&r3); secs(2); }
	bpdu_in(&agree);
	tick(2);
	check_state(1, 3, "once that time is over the agreement opens it");
	stp_pcost[2] = 0;
}

static void scen_recent_backup(void)
{
	printf("44. a port that was a backup a moment ago does not rush into forwarding as root\n");
	reset_all();
	links_set(1 << 8);
	secs(1);
	struct sim_bpdu r = root_on(8);
	for (int i = 0; i < 3; i++) { bpdu_in(&r); secs(2); }
	stp_pflags[3] &= ~STP_PF_AUTOEDGE;
	stp_pcost[3] = 2000;
	stp_rbwhile[3] = 3 * STP_HZ;
	links_set((1 << 3) | (1 << 8));
	secs(1);
	struct sim_bpdu r4 = root_on(3);
	bpdu_in(&r4);
	tick(2);
	check(stp_root_port == 3, "port 4 is the new root port");
	check_state(3, 1, "but stays discarding while it is a recent backup");
	for (int i = 0; i < 4; i++) { bpdu_in(&r4); secs(1); }
	check_state(3, 3, "and forwards once that time is over");
	stp_pcost[3] = 0;
}

static void scen_short_bpdu(void)
{
	printf("45. BPDUs shorter than their type allows are ignored\n");
	reset_all();
	links_set(1 << 8);
	secs(1);
	struct sim_bpdu r = root_on(8);
	r.msg_len = 38;
	bpdu_in(&r);
	check(stp_root_port == 0xff, "an RST BPDU of 38 bytes is ignored");
	r.msg_len = 0;
	bpdu_in(&r);
	check(stp_root_port == 8, "at 39 bytes it is taken");
	reset_all();
	links_set(1 << 8);
	secs(1);
	struct sim_bpdu c = root_on(8); c.legacy = 1; c.msg_len = 37;
	bpdu_in(&c);
	check(stp_root_port == 0xff, "a Config BPDU of 37 bytes is ignored");
	c.msg_len = 0;
	bpdu_in(&c);
	check(stp_root_port == 8, "at 38 bytes it is taken");
	int n = tx_frames[8];
	struct sim_bpdu tcn = c; tcn.legacy = 2; tcn.msg_len = 6;
	bpdu_in(&tcn);
	check(tx_frames[8] == n, "a TCN of 6 bytes gets no TCA");
	tcn.msg_len = 0;
	bpdu_in(&tcn);
	check(tx_frames[8] > n, "a TCN of 7 bytes is acknowledged");
}

static void scen_port_roles(void)
{
	printf("46. the reported role covers every port, a port without link is disabled\n");
	struct sim_bpdu r, peer;
	ring_with_uplink_on_9(&r, &peer);
	check(stp_port_role(8) == 1, "port 9 is reported as root");
	check(stp_port_role(0) == 3, "port 1 as alternate");
	check(stp_port_role(4) == 0, "port 5, which has no link, as disabled");
	links_set((1 << 0) | (1 << 4) | (1 << 8));
	secs(2);
	check(stp_port_role(4) == 2, "port 5 is designated once its link is up");
	stp_pflags[4] &= ~STP_PF_ENABLED;
	check(stp_port_role(4) == 0, "and disabled again when STP is off on it");
	stp_pflags[4] |= STP_PF_ENABLED;
	stp_pcost[8] = 0;
}

static void scen_no_tx_without_link(void)
{
	printf("47. no BPDU goes out on a port without link\n");
	reset_all();
	links_set(1 << 8);
	secs(1);
	struct sim_bpdu r = root_on(8);
	for (int i = 0; i < 5; i++) { bpdu_in(&r); secs(2); }
	check(tx_frames[1] == 0, "port 2 without link sent nothing in ten seconds");
	stp_pflags[1] &= ~STP_PF_AUTOEDGE;
	links_set((1 << 1) | (1 << 8));
	for (int i = 0; i < 2; i++) { bpdu_in(&r); secs(2); }
	check(tx_frames[1] > 0, "once its link is up it sends");
}

static void scen_counters(void)
{
	printf("48. per-port BPDU and TC counters, and stp clear\n");
	reset_all();
	stp_pflags[1] &= ~STP_PF_AUTOEDGE;
	links_set((1 << 1) | (1 << 8));
	struct sim_bpdu r = root_on(8);
	for (int i = 0; i < 40; i++) { bpdu_in(&r); secs(1); }
	check(stp_cnt[STP_CNT_RX][8] == 40 && stp_cnt[STP_CNT_RX][1] == 0, "each BPDU taken in is counted on its own port");
	check(stp_cnt[STP_CNT_TX][1] > 0 && stp_cnt[STP_CNT_TX][0] == 0, "BPDUs sent are counted, none on a port without link");
	int sent = 1;
	for (int p = 0; p < NPORTS; p++)
		sent &= stp_cnt[STP_CNT_TX][p] == (uint32_t)tx_frames[p];
	check(sent, "BPDUs sent match the frames on the wire on every port");
	check(stp_cnt[STP_CNT_TCRX][8] == 0, "a BPDU without the TC flag is not counted as TC");
	int same = 1, any = 0;
	for (int p = 0; p < NPORTS; p++) {
		same &= stp_cnt[STP_CNT_TCTX][p] == (uint32_t)(tc_frames[p] + tcn_frames[p]);
		any |= tc_frames[p] + tcn_frames[p];
	}
	check(any && same, "TC sent matches the TC and TCN frames on the wire");
	r.flags = 0x3d;
	bpdu_in(&r);
	check(stp_cnt[STP_CNT_TCRX][8] == 1, "a BPDU with the TC flag is counted as TC");
	struct sim_bpdu tcn = r;
	tcn.legacy = 2;
	bpdu_in(&tcn);
	check(stp_cnt[STP_CNT_TCRX][8] == 2 && stp_cnt[STP_CNT_RX][8] == 42, "and so is a TCN");
	sim_cmd("stp clear");
	int zero = err_status == ERR_OK;
	for (int k = 0; k < STP_CNT_N; k++)
		for (int p = 0; p < NPORTS; p++)
			zero &= stp_cnt[k][p] == 0;
	check(zero, "stp clear zeroes every counter on every port");
}

static void scen_path_cost_method(void)
{
	printf("49. short path costs follow 802.1D-1998\n");
	reset_all();
	links_set(1 << 8);
	struct sim_bpdu r = root_on(8);
	for (int i = 0; i < 3; i++) { bpdu_in(&r); secs(1); }
	check(root_bridge_cost == 20000, "long method: a 1G root port costs 20000");
	sim_cmd("stp pathcost short");
	check(err_status == ERR_OK && stp_pcost_short == 1, "stp pathcost short is accepted");
	for (int i = 0; i < 2; i++) { bpdu_in(&r); secs(1); }
	check(root_bridge_cost == 4, "short method: the same port costs 4");
	sim_speed[8] = 4;
	for (int i = 0; i < 2; i++) { bpdu_in(&r); secs(1); }
	check(root_bridge_cost == 2, "and a 10G port costs 2");
	sim_cmd("stp pathcost medium");
	check(err_status != ERR_OK && stp_pcost_short == 1, "an unknown method is refused");
	sim_cmd("stp pathcost long");
	sim_speed[8] = 2;
}

static void scen_bpdu_handling(void)
{
	printf("50. BPDU handling while STP is off: flood or filter\n");
	reset_all();
	stp_enabled = 0;
	l2mc_calls = 0;
	stp_off();
	check(l2mc_calls && (l2mc_pmask & PMASK_9) == PMASK_9, "by default BPDUs are flooded when STP stops");
	l2mc_calls = 0;
	sim_cmd("stp bpdu filter");
	check(err_status == ERR_OK && l2mc_calls && l2mc_pmask == PMASK_CPU,
	      "filter keeps them on the CPU, at once while STP is off");
	stp_enabled = 1;
	stp_setup();
	l2mc_calls = 0;
	sim_cmd("stp bpdu flood");
	check(l2mc_calls == 0, "while STP runs the setting only waits");
	stp_enabled = 0;
	stp_off();
	check((l2mc_pmask & PMASK_9) == PMASK_9, "and applies when STP stops");
	sim_cmd("stp bpdu drop");
	check(err_status != ERR_OK, "an unknown handling is refused");
	stp_enabled = 1;
}

static void scen_last_tc(void)
{
	printf("51. time since the last topology change\n");
	reset_all();
	links_set(1 << 8);
	struct sim_bpdu tcn = root_on(8);
	tcn.legacy = 2;
	secs(3);
	uint16_t before = stp_tc_count;
	bpdu_in(&tcn);
	check(stp_tc_count != before, "a TCN moves the topology change counter");
	secs(5);
	check(stp_tc_secs >= 4 && stp_tc_secs <= 5, "the clock counts the seconds since then");
	bpdu_in(&tcn);
	secs(1);
	check(stp_tc_secs <= 1, "and starts again at the next change");
}

static void scen_lag(void)
{
	printf("52. a link aggregation group is one port of the tree\n");
	reset_all();
	links_set((1 << 1) | (1 << 2) | (1 << 8));
	secs(1);
	struct sim_bpdu before = root_on(2);
	bpdu_in(&before);
	check(stp_root_port == 2 && stp_info_while[2], "port 3 hears the root before it joins a group");
	sim_lag[0] = (1 << 1) | (1 << 2);
	secs(1);
	check(stp_info_while[2] == 0, "joining the group drops what the port heard on its own");
	check(stp_ent_id(STP_LAG_BASE) && !stp_ent_id(1) && !stp_ent_id(2),
	      "the group replaces its member ports");
	check(port_state(1) == 1 && port_state(2) == 1, "both members start discarding together");

	struct sim_bpdu b = root_on(2);
	bpdu_in(&b);
	check(stp_root_port == STP_LAG_BASE, "a BPDU on either member reaches the group");
	struct sim_bpdu c = root_on(1);
	c.root_cost = 4;
	bpdu_in(&c);
	check(stp_root_port == STP_LAG_BASE, "and the other member feeds the same port");
	secs(31);
	bpdu_in(&b);
	check(port_state(1) == 3 && port_state(2) == 3, "both members forward as the root port");

	sim_cmd("stp port 2 cost 5");
	check(err_status != ERR_OK, "a member port cannot be set on its own");
	sim_cmd("stp lag 1 cost 5");
	check(err_status == ERR_OK && stp_pcost[STP_LAG_BASE] == 5, "the group takes the setting");
	sim_cmd("stp lag 5 cost 5");
	check(err_status != ERR_OK, "a group number past the last one is refused");

	memset(tx_frames, 0, sizeof(tx_frames));
	tcn_frames[1] = 0;
	struct sim_bpdu t = root_on(2);
	t.legacy = 2;
	bpdu_in(&t);
	check(tx_frames[1] == 1 && tx_frames[2] == 0, "a reply leaves through the lowest member");
	check(last_tx[1].port_id == STP_LAG_BASE + 1, "with the group's own port id");

	memset(flush_count, 0, sizeof(flush_count));
	links_set((1 << 2) | (1 << 8));
	secs(1);
	check(port_state(2) == 3 && stp_root_port == STP_LAG_BASE,
	      "losing one member keeps the group forwarding");
	check(flush_count[2] == 0, "and is no topology change");
	memset(tx_frames, 0, sizeof(tx_frames));
	bpdu_in(&t);
	check(tx_frames[1] == 0 && tx_frames[2] == 1, "replies move to the member that still has a link");

	sim_lag[0] = 0;
	links_set((1 << 1) | (1 << 2) | (1 << 8));
	secs(1);
	check(!stp_ent_id(STP_LAG_BASE) && stp_ent_id(1) && stp_ent_id(2),
	      "dissolving the group gives the ports back");
	check(stp_root_port != STP_LAG_BASE, "and the group stops being the root port");
	check(port_state(1) == 1 && port_state(2) == 1, "the ports start over discarding");
}

int main(int argc, char **argv)
{
	verbose = argc > 1 && argv[1][0] == '-' ? (argv[1][1] == 'd' ? 2 : 1) : 0;
	scen_edge_ports();
	scen_root_port();
	scen_ring();
	scen_ring_clears();
	scen_cheaper_path();
	scen_we_are_better();
	scen_alt_survives_link_bounce();
	scen_speed_cost();
	scen_root_times();
	scen_too_old();
	scen_info_expiry();
	scen_dport_tiebreak();
	scen_rxport_tiebreak();
	scen_inferior_info();
	scen_root_role_bpdu();
	scen_alternate_takes_over();
	scen_dbridge_tiebreak();
	scen_times_revert();
	scen_tc_via_root_port();
	scen_tc_window_rstp();
	scen_tc_propagation();
	scen_tc_legacy_tcn();
	scen_alt_forgets();
	scen_edge_no_tc();
	scen_link_loss_no_tc();
	scen_proposal_agreement();
	scen_no_agreement();
	scen_sync_on_proposal();
	scen_rapid_failover();
	scen_rapid_reroot();
	scen_agreement_other_root();
	scen_repeated_proposal();
	scen_dispute();
	scen_learning_state();
	scen_reply_to_inferior();
	scen_backup_role();
	scen_migrate_to_stp();
	scen_migrate_back();
	scen_legacy_root_tcn();
	scen_alternate_agrees();
	scen_agreement_from_alternate();
	scen_timer_relation();
	scen_recent_root();
	scen_recent_backup();
	scen_short_bpdu();
	scen_port_roles();
	scen_no_tx_without_link();
	scen_counters();
	scen_path_cost_method();
	scen_bpdu_handling();
	scen_last_tc();
	scen_lag();
	if (failures) {
		printf("\n%d check(s) failed\n", failures);
		return 1;
	}
	printf("\nall scenarios passed\n");
	return 0;
}
