/*
 * Host-side sandbox for the RTL837x LACP implementation.
 *
 * Compiles the UNMODIFIED rtl837x_lacp.c against shim headers (see shim/) and
 * drives it with synthetic LACPDUs from a simulated partner switch/bond. This
 * exercises the real state machines that ship in the firmware image - only
 * the platform below them (frame I/O, trunk registers, console) is mocked.
 *
 * Run: make -C test/lacp    (exit code 0 = all scenarios pass)
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "rtl837x_common.h"
#include "machine.h"
#include "uip.h"
#include "rtl837x_lacp.h"

/* ---------- platform mocks ---------- */

uint8_t uip_buf[UIP_CONF_BUFFER_SIZE + 2];
uint16_t uip_len;
struct uip_eth_addr uip_ethaddr = { .addr = {0x02,0x11,0x22,0x33,0x44,0x55} };
struct machine machine = { .min_port = 0, .max_port = 7 };
struct machine_runtime machine_detected = { .isRTL8373 = 1 };
uint8_t lacpEnabled;
unsigned char sfr_data[4];	/* mocked register read buffer */
uint16_t management_vlan;	/* mocked; slow-protocol path zeroes it per-frame */

static int verbose = 0;
void print_string(char *s) { if (verbose) fputs(s, stdout); }
void print_byte(uint8_t b) { if (verbose) printf("%02x", b); }
void print_short(uint16_t v) { if (verbose) printf("%04x", v); }
void write_char(char c) { if (verbose) putchar(c); }

/* Trunk programming mock: record last mask + call count */
static uint16_t hw_members;
static int hw_set_calls;
void port_lag_members_set(uint8_t lag, uint16_t members)
{
	(void)lag;
	hw_members = members;
	hw_set_calls++;
}

/* Port-isolation mock: record the last mask written per port */
static uint16_t hw_isolation[10];
void port_isolate(uint8_t port, uint16_t pmask)
{
	if (port < 10)
		hw_isolation[port] = pmask;
}

/* ---------- on-wire mirrors (independent re-statement of the layout) ---------- */

struct wire_info {
	uint16_t sys_prio;
	uint8_t  sys[6];
	uint16_t key;
	uint16_t port_prio;
	uint16_t port;
	uint8_t  state;
	uint8_t  reserved[3];
};

struct wire_pdu_out {		/* what lacp_send() writes at uip_buf+12 */
	uint8_t  dst[6];
	uint8_t  src[6];
	struct rtl_tag rtl_tag;
	uint16_t ethertype;
	uint8_t  subtype;
	uint8_t  version;
	uint8_t  tlv_actor, actor_len;
	struct wire_info actor;
	uint8_t  tlv_partner, partner_len;
	struct wire_info partner;
	uint8_t  tlv_collector, collector_len;
	uint16_t collector_max_delay;
	uint8_t  collector_reserved[12];
	uint8_t  tlv_terminator, terminator_len;
	uint8_t  terminator_reserved[50];
};

struct wire_pdu_in {		/* what lacp_in() parses at uip_buf+0 (RX adds VLAN tag) */
	uint8_t  dst[6];
	uint8_t  src[6];
	struct rtl_tag rtl_tag;
	struct vlan_tag vlan_tag;
	uint16_t ethertype;
	uint8_t  subtype;
	uint8_t  version;
	uint8_t  tlv_actor, actor_len;
	struct wire_info actor;
	uint8_t  tlv_partner, partner_len;
	struct wire_info partner;
};

_Static_assert(sizeof(struct wire_info) == 18, "info TLV body must be 18");
_Static_assert(sizeof(struct wire_pdu_out) == 132, "TX pdu must be 132");
_Static_assert(sizeof(struct rtl_tag) == 8, "rtl tag must be 8");

/* ---------- TX capture ---------- */

static struct wire_pdu_out last_tx[10];
static int tx_count[10];

void tcpip_output(void)
{
	struct wire_pdu_out *p = (struct wire_pdu_out *)&uip_buf[RTL_FRAME_DESC_SIZE];
	uint16_t pmask = HTONS(p->rtl_tag.pmask);
	for (int port = 0; port < 10; port++) {
		if (pmask & (1u << port)) {
			last_tx[port] = *p;
			tx_count[port]++;
		}
	}
	uip_len = 0;
}

/* ---------- simulated partner ---------- */

/* Deliver a LACPDU into the switch on `port`, from partner system `sys`.
 * pstate = partner's Actor_State flags. If `echo`, the partner echoes the
 * switch's last-seen actor block back (fresh view) - required for quiescence. */
static void partner_frame(uint8_t port, const uint8_t sys[6], uint8_t pstate, int echo)
{
	struct wire_pdu_in in;
	memset(&in, 0, sizeof(in));

	in.dst[0]=0x01; in.dst[1]=0x80; in.dst[2]=0xc2; in.dst[5]=0x02;
	memcpy(in.src, sys, 6);
	in.rtl_tag.tag = HTONS(RTL_FRAME_TAG_ID);
	in.rtl_tag.pmask = HTONS(port);		/* RX: 4-bit ingress port */
	in.ethertype = HTONS(0x8809);
	in.subtype = 0x01;
	in.version = 0x01;

	in.tlv_actor = 0x01; in.actor_len = 0x14;
	in.actor.sys_prio = HTONS(0x8000);
	memcpy(in.actor.sys, sys, 6);
	in.actor.key = HTONS(0x0011);
	in.actor.port_prio = HTONS(0x00ff);
	in.actor.port = HTONS((uint16_t)port + 101);
	in.actor.state = pstate;

	in.tlv_partner = 0x02; in.partner_len = 0x14;
	if (echo) {			/* fresh view of us, from our last TX */
		in.partner.sys_prio = last_tx[port].actor.sys_prio;
		memcpy(in.partner.sys, last_tx[port].actor.sys, 6);
		in.partner.key = last_tx[port].actor.key;
		in.partner.port = last_tx[port].actor.port;
		in.partner.state = last_tx[port].actor.state;
	}				/* else: zeros = stale view */

	memcpy(uip_buf, &in, sizeof(in));
	lacp_in();
}

/* Advance time: each lacp_timers() call is one main-loop tick */
static void ticks(int n) { while (n--) lacp_timers(); }

/* ---------- scenario runner ---------- */

static int failures;
#define CHECK(cond, name) do { \
	if (cond) printf("PASS  %s\n", name); \
	else { printf("FAIL  %s\n", name); failures++; } \
} while (0)

#define P_ACT   LACP_STATE_ACTIVITY
#define P_AGG   LACP_STATE_AGGREGATION
#define P_SYNC  LACP_STATE_SYNC
#define P_COL   LACP_STATE_COLLECTING
#define P_DIST  LACP_STATE_DISTRIBUTING
#define P_TO    LACP_STATE_TIMEOUT	/* partner requests short (fast) timeout */

static const uint8_t SYS_A[6] = {0x02,0xaa,0xaa,0xaa,0xaa,0x01};
static const uint8_t SYS_B[6] = {0x02,0xbb,0xbb,0xbb,0xbb,0x02};

int main(int argc, char **argv)
{
	verbose = (argc > 1 && !strcmp(argv[1], "-v"));

	/* T1: enable announces on every port with sane field contents */
	lacp_cmd(1);
	ticks(8);
	int all_tx = 1, sane = 1;
	for (int i = machine.min_port; i <= machine.max_port; i++) {
		if (!tx_count[i]) all_tx = 0;
		if (last_tx[i].ethertype != HTONS(0x8809) || last_tx[i].subtype != 1
		    || last_tx[i].dst[5] != 0x02
		    || !(last_tx[i].actor.state & (P_ACT|P_AGG|P_TO)))
			sane = 0;
	}
	CHECK(all_tx && sane, "T1 enable: LACPDU announced on all ports, well-formed");

	/* T2: partner without SYNC -> actor SYNC only, no members yet */
	partner_frame(0, SYS_A, P_ACT|P_AGG|P_TO, 1);
	partner_frame(1, SYS_A, P_ACT|P_AGG|P_TO, 1);
	ticks(8);
	CHECK((lacp_actor_state[0] & P_SYNC) && !(lacp_actor_state[0] & (P_COL|P_DIST))
	      && hw_members == 0,
	      "T2 partner not in sync: actor SYNC only, trunk empty");

	/* T3: partner in sync on ports 0,1 -> full converge, members 0x0003 */
	partner_frame(0, SYS_A, P_ACT|P_AGG|P_TO|P_SYNC, 1);
	partner_frame(1, SYS_A, P_ACT|P_AGG|P_TO|P_SYNC, 1);
	ticks(8);
	CHECK(hw_members == 0x0003
	      && (lacp_actor_state[0] & (P_SYNC|P_COL|P_DIST)) == (P_SYNC|P_COL|P_DIST)
	      && (lacp_actor_state[1] & (P_SYNC|P_COL|P_DIST)) == (P_SYNC|P_COL|P_DIST),
	      "T3 convergence: both ports collecting/distributing, trunk 0x0003");

	/* T3b: partner-block echo must be VERBATIM. A Linux 802.3ad partner accepts
	 * our SYNC only if our Partner TLV mirrors its actor identity exactly; a
	 * hardcoded sys_prio/port_prio (instead of the recorded value) makes Linux
	 * clear partner-SYNC and the bond never distributes. Verify we echo the
	 * partner's own priorities back, not a constant. */
	CHECK(HTONS(last_tx[0].partner.sys_prio) == 0x8000
	      && HTONS(last_tx[0].partner.port_prio) == 0x00ff
	      && HTONS(last_tx[0].partner.key) == 0x0011
	      && HTONS(last_tx[0].partner.port) == (uint16_t)0 + 101,
	      "T3b partner echo: recorded partner priorities/key/port echoed verbatim");

	/* T4: mis-cabling - port 2 sees a DIFFERENT system: must stay out */
	partner_frame(2, SYS_B, P_ACT|P_AGG|P_TO|P_SYNC, 1);
	ticks(8);
	CHECK(hw_members == 0x0003 && !(lacp_actor_state[2] & P_SYNC),
	      "T4 mis-cabling: port with different partner system stays out");

	/* T4b: sibling isolation - the two bond ports (same partner SYS_A) must be
	 * isolated from EACH OTHER so a forwarded LACPDU does not loop back, while
	 * the odd port (SYS_B) keeps default isolation. base = CPU|all = 0x3ff. */
	CHECK(hw_isolation[0] == (0x3ff & ~(1u << 1))    /* port0: everyone but port1 */
	      && hw_isolation[1] == (0x3ff & ~(1u << 0)) /* port1: everyone but port0 */
	      && hw_isolation[2] == 0x3ff,               /* port2: different partner, untouched */
	      "T4b sibling isolation: bond ports isolated from each other, odd port default");

	/* T5: register-write economy: stable state must not rewrite the trunk */
	int calls_before = hw_set_calls;
	partner_frame(0, SYS_A, P_ACT|P_AGG|P_TO|P_SYNC, 1);
	partner_frame(1, SYS_A, P_ACT|P_AGG|P_TO|P_SYNC, 1);
	ticks(64);
	CHECK(hw_set_calls == calls_before,
	      "T5 stable state: no redundant trunk register writes");

	/* T6: stale echo triggers retransmit (update_NTT) */
	int txc = tx_count[0];
	partner_frame(0, SYS_A, P_ACT|P_AGG|P_TO|P_SYNC, 0 /* stale view of us */);
	ticks(8);
	CHECK(tx_count[0] > txc, "T6 update_NTT: stale partner view causes retransmit");

	/* T7: expiry - partner silent past the short timeout: trunk drains,
	 * aggregator identity is released */
	ticks(4 * 0x0300 + 64);		/* > LACP_SHORT_TIMEOUT work-ticks */
	CHECK(hw_members == 0 && lacp_agg_valid == 0,
	      "T7 expiry: members drop to 0 and aggregator is released");

	/* T8: re-convergence with a NEW partner system after release */
	/* (needs a fresh announce first so the echo below carries current state) */
	ticks(4 * 0x0100 + 8);		/* let periodic TX refresh last_tx */
	partner_frame(3, SYS_B, P_ACT|P_AGG|P_TO|P_SYNC, 1);
	partner_frame(3, SYS_B, P_ACT|P_AGG|P_TO|P_SYNC, 1);
	ticks(8);
	CHECK(hw_members == 0x0008 && (lacp_actor_state[3] & P_COL),
	      "T8 re-election: new partner system forms a fresh aggregate");

	/* T9: lacp off clears the trunk */
	lacp_cmd(0);
	CHECK(hw_members == 0 && lacpEnabled == 0, "T9 disable: trunk cleared");

	printf("\n%s (%d failure%s)\n", failures ? "SANDBOX: FAILURES" : "SANDBOX: ALL PASS",
	       failures, failures == 1 ? "" : "s");
	return failures ? 1 : 0;
}
