#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "rtl837x_common.h"
#include "machine.h"
#include "uip.h"
#include "rtl837x_regs.h"
#include "rtl837x_lacp.h"

/* ---------- platform mocks ---------- */

uint8_t uip_buf[UIP_CONF_BUFFER_ALLOC_SIZE];
uint16_t uip_len;
struct uip_eth_addr uip_ethaddr = { .addr = {0x02,0x11,0x22,0x33,0x44,0x55} };
const struct machine machine = { .min_port = 0, .max_port = 7,
			   .log_to_phys_port = {5, 1, 2, 3, 4, 6, 7, 8, 9} };
struct machine_runtime machine_detected = { .isRTL8373 = 1 };
uint8_t lacpEnabled;
uint16_t management_vlan;	/* mocked; slow-protocol path zeroes it per-frame */

static int verbose = 0;
void print_string(const char *s) { if (verbose) fputs(s, stdout); }
void print_byte(uint8_t b) { if (verbose) printf("%02x", b); }
void print_short(uint16_t v) { if (verbose) printf("%04x", v); }
void write_char(char c) { if (verbose) putchar(c); }

/* Trunk programming mock: record last mask per LAG + call count.
 * hw_members mirrors LAG 0 so the single-aggregator scenarios read naturally. */
static uint16_t hw_members_lag[4];
static int hw_set_calls;
#define hw_members hw_members_lag[0]
void port_lag_members_set(uint8_t lag, uint16_t members)
{
	if (lag < 4)
		hw_members_lag[lag] = members;
	hw_set_calls++;
}

/* Reserved-multicast action mock: delivery of a LACPDU to the CPU rests on
 * the action of the slow-protocols entry alone, so record what the engine
 * sets. The real trap_rma_set() touches only the action field. */
static uint16_t hw_rma_reg;
static uint8_t hw_rma_act;
static int hw_rma_calls;
void trap_rma_set(uint16_t reg, uint8_t act);
void trap_rma_set(uint16_t reg, uint8_t act)
{
	hw_rma_reg = reg;
	hw_rma_act = act;
	hw_rma_calls++;
}

/* ---------- on-wire mirrors (independent re-statement of the layout) ---------- */

struct wire_info {
	uint16_t sys_prio;
	uint8_t  sys[MAC_ADDR_LEN];
	uint16_t key;
	uint16_t port_prio;
	uint16_t port;
	uint8_t  state;
	uint8_t  reserved[3];
};

struct wire_pdu_out {		/* what lacp_send() writes at uip_buf+12 */
	uint8_t  dst[MAC_ADDR_LEN];
	uint8_t  src[MAC_ADDR_LEN];
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

struct wire_pdu_in {		/* forwarded form: the hardware inserts a VLAN tag */
	uint8_t  dst[MAC_ADDR_LEN];
	uint8_t  src[MAC_ADDR_LEN];
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
static void partner_frame_full(uint8_t port, const uint8_t sys[MAC_ADDR_LEN], uint8_t pstate,
			       int echo, uint16_t len, int tagged)
{
	struct wire_pdu_in in;
	memset(&in, 0, sizeof(in));

	in.dst[0]=0x01; in.dst[1]=0x80; in.dst[2]=0xc2; in.dst[5]=0x02;
	memcpy(in.src, sys, MAC_ADDR_LEN);
	in.rtl_tag.tag = HTONS(RTL_FRAME_TAG_ID);
	in.rtl_tag.pmask = HTONS(port);		/* RX: 4-bit ingress port */
	in.vlan_tag.svlan = HTONS(0x8100);	/* the tag's TPID, which marks the body offset */
	in.ethertype = HTONS(0x8809);
	in.subtype = 0x01;
	in.version = 0x01;

	in.tlv_actor = 0x01; in.actor_len = 0x14;
	in.actor.sys_prio = HTONS(0x8000);
	memcpy(in.actor.sys, sys, MAC_ADDR_LEN);
	in.actor.key = HTONS(0x0011);
	in.actor.port_prio = HTONS(0x00ff);
	in.actor.port = HTONS((uint16_t)port + 101);
	in.actor.state = pstate;

	in.tlv_partner = 0x02; in.partner_len = 0x14;
	if (echo) {			/* fresh view of us, from our last TX */
		in.partner.sys_prio = last_tx[port].actor.sys_prio;
		memcpy(in.partner.sys, last_tx[port].actor.sys, MAC_ADDR_LEN);
		in.partner.key = last_tx[port].actor.key;
		in.partner.port = last_tx[port].actor.port;
		in.partner.state = last_tx[port].actor.state;
	}				/* else: zeros = stale view */

	memcpy(uip_buf, &in, sizeof(in));
	if (!tagged) {		/* trapped: no VLAN tag, so the body sits earlier */
		uint8_t *body = uip_buf + RTL_FRAME_DESC_SIZE + RTL_TAG_SIZE;
		memmove(body, body + VLAN_TAG_SIZE,
			sizeof(in) - (RTL_FRAME_DESC_SIZE + RTL_TAG_SIZE + VLAN_TAG_SIZE));
		len -= VLAN_TAG_SIZE;
	}
	uip_len = len;
	lacp_in();
}

static void partner_frame_len(uint8_t port, const uint8_t sys[MAC_ADDR_LEN], uint8_t pstate, int echo, uint16_t len)
{
	partner_frame_full(port, sys, pstate, echo, len, 1);
}

static void partner_frame(uint8_t port, const uint8_t sys[MAC_ADDR_LEN], uint8_t pstate, int echo)
{
	partner_frame_full(port, sys, pstate, echo, sizeof(struct wire_pdu_in), 1);
}

/* The same LACPDU as the reserved-multicast trap delivers it. */
static void partner_frame_trap(uint8_t port, const uint8_t sys[MAC_ADDR_LEN], uint8_t pstate, int echo)
{
	partner_frame_full(port, sys, pstate, echo, sizeof(struct wire_pdu_in), 0);
}

/* Advance time by n system ticks; the LACP timers step every fourth one */
volatile uint32_t ticks;
static void step(int n) { while (n--) { ticks++; lacp_timers(); } }

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

static const uint8_t SYS_A[MAC_ADDR_LEN] = {0x02,0xaa,0xaa,0xaa,0xaa,0x01};
static const uint8_t SYS_B[MAC_ADDR_LEN] = {0x02,0xbb,0xbb,0xbb,0xbb,0x02};

int main(int argc, char **argv)
{
	verbose = (argc > 1 && !strcmp(argv[1], "-v"));

	/* Boot init first: on host, zeroed globals would otherwise read as "every
	 * port belongs to LAG 0" (the firmware calls this from main() the same way) */
	lacp_init();

	/* T1: enable announces on every port with sane field contents */
	lacp_lag_set(0, 0x00ff);
	step(8);
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
	step(8);
	CHECK((lacp_actor_state[0] & P_SYNC) && !(lacp_actor_state[0] & (P_COL|P_DIST))
	      && hw_members == 0,
	      "T2 partner not in sync: actor SYNC only, trunk empty");

	/* T3: partner in sync on ports 0,1 -> full converge, members 0x0003 */
	partner_frame(0, SYS_A, P_ACT|P_AGG|P_TO|P_SYNC, 1);
	partner_frame(1, SYS_A, P_ACT|P_AGG|P_TO|P_SYNC, 1);
	step(8);
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
	step(8);
	CHECK(hw_members == 0x0003 && !(lacp_actor_state[2] & P_SYNC),
	      "T4 mis-cabling: port with different partner system stays out");

	/* T5: register-write economy: stable state must not rewrite the trunk */
	int calls_before = hw_set_calls;
	partner_frame(0, SYS_A, P_ACT|P_AGG|P_TO|P_SYNC, 1);
	partner_frame(1, SYS_A, P_ACT|P_AGG|P_TO|P_SYNC, 1);
	step(64);
	CHECK(hw_set_calls == calls_before,
	      "T5 stable state: no redundant trunk register writes");

	/* T6: stale echo triggers retransmit (update_NTT) */
	int txc = tx_count[0];
	partner_frame(0, SYS_A, P_ACT|P_AGG|P_TO|P_SYNC, 0 /* stale view of us */);
	step(8);
	CHECK(tx_count[0] > txc, "T6 update_NTT: stale partner view causes retransmit");

	/* T7: expiry - partner silent past the short timeout: trunk drains,
	 * aggregator identity is released */
	step(4 * LACP_SHORT_TIMEOUT + 64);	/* past the short timeout */
	CHECK(hw_members == 0 && lacp_agg_valid[0] == 0,
	      "T7 expiry: members drop to 0 and aggregator is released");

	/* T8: re-convergence with a NEW partner system after release */
	/* (needs a fresh announce first so the echo below carries current state) */
	step(4 * 0x0100 + 8);		/* let periodic TX refresh last_tx */
	partner_frame(3, SYS_B, P_ACT|P_AGG|P_TO|P_SYNC, 1);
	partner_frame(3, SYS_B, P_ACT|P_AGG|P_TO|P_SYNC, 1);
	step(8);
	CHECK(hw_members == 0x0008 && (lacp_actor_state[3] & P_COL),
	      "T8 re-election: new partner system forms a fresh aggregate");

	/* T9: lacp off clears the trunk */
	lacp_off();
	CHECK(hw_members == 0 && lacpEnabled == 0, "T9 disable: trunk cleared");

	/* ---- per-LAG scenarios: two independent aggregators ---- */

	/* T10: lag 1 lacp {0,1} + lag 2 lacp {2,3}, both to the SAME partner
	 * system (worst case for sibling scoping): each converges into its own
	 * hardware trunk, and the engine reports enabled. */
	lacp_lag_set(1, 0x0003);	/* ports 0,1 */
	lacp_lag_set(2, 0x000c);	/* ports 2,3 */
	step(8);			/* announce */
	for (int r = 0; r < 2; r++) {	/* two rounds so echoes carry fresh state */
		for (int p = 0; p < 4; p++)
			partner_frame(p, SYS_A, P_ACT|P_AGG|P_TO|P_SYNC, 1);
		step(8);
	}
	CHECK(lacpEnabled == 1
	      && hw_members_lag[1] == 0x0003 && hw_members_lag[2] == 0x000c,
	      "T10 two LAGs: independent aggregates converge on their own trunks");

	/* T11: removing one LAG releases only its trunk; the other keeps running */
	lacp_lag_set(1, 0);
	CHECK(hw_members_lag[1] == 0 && hw_members_lag[2] == 0x000c
	      && lacpEnabled == 1,
	      "T11 remove one LAG: its trunk drains, the other LAG unaffected");

	/* T12: a port in no LACP LAG ignores LACPDUs entirely */
	int rx5_before = tx_count[5];
	partner_frame(5, SYS_B, P_ACT|P_AGG|P_TO|P_SYNC, 1);
	step(8);
	CHECK(!(lacp_actor_state[5] & P_SYNC) && hw_members_lag[2] == 0x000c,
	      "T12 unassigned port: LACPDU ignored, no state change");
	(void)rx5_before;

	/* T13: removing the last LACP LAG shuts the engine down */
	lacp_lag_set(2, 0);
	CHECK(lacpEnabled == 0 && hw_members_lag[2] == 0
	      && hw_rma_reg == RTL837X_RMA2_CONF && hw_rma_act == RTL837X_RMA_ACT_DROP,
	      "T13 last LAG removed: engine off, trunk drained, address dropped");

	/* T14: while the engine runs, the slow-protocols address is trapped to
	 * the CPU. The trap keys on the address alone, so delivery needs no L2
	 * entry and nothing written per VLAN or per PVID. */
	hw_rma_calls = 0;
	lacp_lag_set(0, 0x0003);
	CHECK(hw_rma_calls == 1 && hw_rma_reg == RTL837X_RMA2_CONF
	      && hw_rma_act == RTL837X_RMA_ACT_TRAP,
	      "T14 engine on traps 01:80:c2:00:00:02 to the CPU");

	/* T15: a trapped LACPDU arrives without the VLAN tag a forwarded one
	 * carries, so its body sits four bytes earlier. Both forms must parse. */
	uint16_t rx_before = lacp_rx_count[0];
	lacp_partner_state[0] = 0;		/* all three are written past the ethertype gate */
	memset(lacp_partner_sys[0], 0, MAC_ADDR_LEN);
	partner_frame_trap(0, SYS_A, P_ACT|P_AGG|P_TO|P_SYNC, 1);
	int trap_ok = lacp_rx_count[0] == (uint16_t)(rx_before + 1)
		   && lacp_partner_state[0] == (P_ACT|P_AGG|P_TO|P_SYNC)
		   && !memcmp(lacp_partner_sys[0], SYS_A, MAC_ADDR_LEN);
	partner_frame(0, SYS_A, P_ACT|P_AGG|P_TO|P_SYNC, 1);
	CHECK(trap_ok && lacp_rx_count[0] == (uint16_t)(rx_before + 2),
	      "T15 trapped LACPDU parses like a forwarded one");

	/* T16: the partner changes one cable at a time. Port 1 starts hearing B
	 * while port 0 still hears A, then A goes quiet and B appears on port 0
	 * too. The aggregator must let go of A once no port hears it, even though
	 * port 1 was CURRENT the whole time, or the LAG never aggregates with B. */
	lacp_lag_set(0, 0);
	lacp_lag_set(0, 0x0003);
	step(8);
	for (int r = 0; r < 3; r++) {
		partner_frame(0, SYS_A, P_ACT|P_AGG|P_TO|P_SYNC, 1);
		partner_frame(1, SYS_A, P_ACT|P_AGG|P_TO|P_SYNC, 1);
		step(8);
	}
	for (int t = 0; t < 40; t++) {		/* port 1 moved to B, port 0 still on A */
		partner_frame(0, SYS_A, P_ACT|P_AGG|P_TO|P_SYNC, 1);
		partner_frame(1, SYS_B, P_ACT|P_AGG|P_TO|P_SYNC, 1);
		step(4 * 40);
	}
	int stayed_out = hw_members == 0x0001 && !(lacp_actor_state[1] & P_SYNC);
	for (int t = 0; t < 60; t++)		/* port 0 unplugged from A, past the short timeout */
		{ partner_frame(1, SYS_B, P_ACT|P_AGG|P_TO|P_SYNC, 1); step(4 * 40); }
	for (int t = 0; t < 40; t++) {		/* port 0 now on B as well */
		partner_frame(0, SYS_B, P_ACT|P_AGG|P_TO|P_SYNC, 1);
		partner_frame(1, SYS_B, P_ACT|P_AGG|P_TO|P_SYNC, 1);
		step(4 * 40);
	}
	CHECK(stayed_out && hw_members == 0x0003 && !memcmp(lacp_agg_sys[0], SYS_B, MAC_ADDR_LEN),
	      "T16 re-cabling one port at a time: aggregator moves from A to B");

	/* T17: a group that already has a STATIC trunk is put under LACP with the
	 * same ports (the LAG page's Static -> LACP switch sends only "lag N lacp").
	 * The static trunk must go: until the partner converges the ports are not
	 * an aggregate, and after a reboot the config replays only the LACP line. */
	hw_members_lag[1] = 0x0030;		/* static lag 2: ports 4,5 */
	lacp_lag_set(1, 0x0030);
	step(8);
	CHECK(hw_members_lag[1] == 0 && lacp_members_last[1] == 0 && lacp_lag_ports[1] == 0x0030,
	      "T17 static trunk taken over: cleared while the group negotiates");
	lacp_lag_set(1, 0);

	/* T18: a port named in a second group moves there. The first group's
	 * candidate set drops it, so taking the second group out later leaves no
	 * orphan that group 1 still shows as a candidate but never runs LACP on. */
	lacp_lag_set(1, 0x0030);		/* ports 4,5 */
	step(8);
	for (int r = 0; r < 2; r++) {
		partner_frame(4, SYS_A, P_ACT|P_AGG|P_TO|P_SYNC, 1);
		partner_frame(5, SYS_A, P_ACT|P_AGG|P_TO|P_SYNC, 1);
		step(8);
	}
	int converged = hw_members_lag[1] == 0x0030;
	lacp_lag_set(2, 0x0060);		/* ports 5,6: port 5 moves from group 2 to 3 */
	int moved = lacp_lag_ports[1] == 0x0010 && lacp_port_lag[5] == 2
		    && hw_members_lag[1] == 0x0010;
	lacp_lag_set(2, 0);
	int no_orphan = lacp_port_lag[5] == LACP_LAG_NONE && lacp_lag_ports[1] == 0x0010;
	lacp_lag_set(3, 0x0010);		/* the last port leaves group 2 as well */
	CHECK(converged && moved && no_orphan && lacp_lag_ports[1] == 0 && lacp_agg_valid[1] == 0,
	      "T18 port moves between groups: old group drops it from candidates and trunk");
	lacp_lag_set(3, 0);

	/* T20: taking a group out of LACP leaves nothing of the partner behind
	 * for "lacp show" to report as if the port still heard it. */
	lacp_lag_set(1, 0x0030);
	step(8);
	partner_frame(4, SYS_A, P_ACT|P_AGG|P_TO|P_SYNC, 1);
	step(8);
	int had_partner = lacp_rx_state[4] == LACP_RX_CURRENT && lacp_rx_count[4] > 0;
	lacp_lag_set(1, 0);
	CHECK(had_partner && lacp_partner_state[4] == 0 && lacp_rx_state[4] == LACP_RX_INITIALIZE
	      && lacp_rx_count[4] == 0 && !memcmp(lacp_partner_sys[4], "\0\0\0\0\0\0", MAC_ADDR_LEN),
	      "T20 release: partner state, rx state, counter and system cleared");

	/* T21: applying the same candidate set again (the LAG page resends it
	 * with every Apply) must not tear the trunk down and renegotiate. */
	lacp_lag_set(1, 0x0030);
	step(8);
	for (int r = 0; r < 2; r++) {
		partner_frame(4, SYS_A, P_ACT|P_AGG|P_TO|P_SYNC, 1);
		partner_frame(5, SYS_A, P_ACT|P_AGG|P_TO|P_SYNC, 1);
		step(8);
	}
	int before = hw_set_calls;
	lacp_lag_set(1, 0x0030);
	CHECK(hw_members_lag[1] == 0x0030 && hw_set_calls == before
	      && (lacp_actor_state[4] & P_COL),
	      "T21 same ports applied again: trunk and state untouched");
	lacp_lag_set(1, 0);

	/* T22: a runt on the slow-protocols address must not be parsed as a
	 * LACPDU; the partner block would be read from stale buffer bytes. */
	lacp_lag_set(1, 0x0030);
	step(8);
	partner_frame_len(4, SYS_A, P_ACT|P_AGG|P_TO|P_SYNC, 1, 40);	/* cut before the partner block */
	CHECK(lacp_rx_count[4] == 0 && lacp_rx_state[4] == LACP_RX_INITIALIZE,
	      "T22 truncated LACPDU ignored: the port does not record it");
	lacp_lag_set(1, 0);

	/* T23: the partner asks for the long timeout (Linux's default slow rate).
	 * current_while is armed from OUR LACP_Timeout, which is short, so a
	 * partner that stops speaking LACP with the link up is dropped after the
	 * short timeout, not after two minutes. */
	lacp_lag_set(1, 0x0030);
	step(8);
	for (int r = 0; r < 2; r++) {
		partner_frame(4, SYS_A, P_ACT|P_AGG|P_SYNC, 1);
		partner_frame(5, SYS_A, P_ACT|P_AGG|P_SYNC, 1);
		step(8);
	}
	int slow_converged = hw_members_lag[1] == 0x0030;
	step(4 * LACP_SHORT_TIMEOUT + 64);
	CHECK(slow_converged && hw_members_lag[1] == 0,
	      "T23 partner on slow rate goes quiet: dropped after our short timeout");
	lacp_lag_set(1, 0);

	/* T24: a LACPDU carrying our own system id (two ports of one group
	 * cabled to each other) must not be taken for a partner. */
	lacp_lag_set(1, 0x0030);
	step(8);
	partner_frame(4, uip_ethaddr.addr, P_ACT|P_AGG|P_TO|P_SYNC, 1);
	CHECK(lacp_rx_count[4] == 0 && lacp_rx_state[4] == LACP_RX_INITIALIZE,
	      "T24 own system id: LACPDU ignored");
	lacp_lag_set(1, 0);

	printf("\n%s (%d failure%s)\n", failures ? "SANDBOX: FAILURES" : "SANDBOX: ALL PASS",
	       failures, failures == 1 ? "" : "s");
	return failures ? 1 : 0;
}
