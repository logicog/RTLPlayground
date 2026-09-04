/*
 * LACP (IEEE 802.3ad Clause 43) driver for the RTL837x platform.
 * This code is in the Public Domain.
 */

// #define DEBUG

/* Place this module's code and constants in code bank 2 (cf. rtl837x_igmp.c) */
#pragma codeseg BANK3
#pragma constseg BANK3

#include <stdint.h>
#include "rtl837x_common.h"
#include "rtl837x_sfr.h"
#include "rtl837x_regs.h"
#include "rtl837x_lacp.h"
#include "rtl837x_port.h"
#include "uip.h"
#include "machine.h"

extern __code struct machine machine;
extern __xdata struct machine_runtime machine_detected;	/* runtime chip detection, owned by rtl837x_port.c */
extern __xdata uint16_t management_vlan;	/* owned by rtlplayground.c; suppressed per-frame for slow protocols */
extern __xdata struct uip_eth_addr uip_ethaddr;
extern __xdata uint8_t uip_buf[UIP_CONF_BUFFER_SIZE + 2];
extern __xdata uint8_t sfr_data[4];	/* register-access scratch (cf. rtl837x_igmp.c) */
/* lacpEnabled: extern in rtl837x_lacp.h, owned by rtlplayground.c */

/* 20-byte Actor/Partner information body (802.3ad 43.4.2.2), network byte order */
struct lacp_info {
	uint16_t sys_prio;
	uint8_t  sys[6];
	uint16_t key;
	uint16_t port_prio;
	uint16_t port;
	uint8_t  state;
	uint8_t  reserved[3];
};

/* Outgoing LACPDU (no VLAN tag on TX, as in stp_pkt) */
struct lacpdu {
	uint8_t  dst[6];
	uint8_t  src[6];
	struct rtl_tag rtl_tag;
	uint16_t ethertype;		/* 0x8809 */
	uint8_t  subtype;		/* 0x01   */
	uint8_t  version;		/* 0x01   */
	uint8_t  tlv_actor;		/* 0x01   */
	uint8_t  actor_len;		/* 0x14   */
	struct lacp_info actor;
	uint8_t  tlv_partner;		/* 0x02   */
	uint8_t  partner_len;		/* 0x14   */
	struct lacp_info partner;
	uint8_t  tlv_collector;		/* 0x03   */
	uint8_t  collector_len;		/* 0x10   */
	uint16_t collector_max_delay;
	uint8_t  collector_reserved[12];
	uint8_t  tlv_terminator;	/* 0x00   */
	uint8_t  terminator_len;	/* 0x00   */
	uint8_t  terminator_reserved[50];
};

/* Incoming LACPDU: same, but a VLAN tag sits after the RTL tag (cf. stp_pkt_in) */
struct lacpdu_in {
	uint8_t  dst[6];
	uint8_t  src[6];
	struct rtl_tag rtl_tag;
	struct vlan_tag vlan_tag;
	uint16_t ethertype;
	uint8_t  subtype;
	uint8_t  version;
	uint8_t  tlv_actor;
	uint8_t  actor_len;
	struct lacp_info actor;
	uint8_t  tlv_partner;
	uint8_t  partner_len;
	struct lacp_info partner;
};

#define LACP_O ((__xdata struct lacpdu    *)&uip_buf[RTL_FRAME_DESC_SIZE])
#define LACP_I ((__xdata struct lacpdu_in *)&uip_buf[0])

/* --- Per-port participant state (802.3ad 43.4.7). Indexed by physical port. --- */
__xdata uint8_t  lacp_actor_state[10];		/* our Actor_State flags        */
__xdata uint8_t  lacp_rx_state[10];		/* LACP_RX_* receive machine     */
__xdata uint8_t  lacp_partner_state[10];	/* last Partner_State we saw     */
__xdata uint8_t  lacp_partner_sys[10][6];	/* partner System ID             */
__xdata uint16_t lacp_partner_key[10];		/* partner Key                   */
__xdata uint16_t lacp_partner_port[10];		/* partner Port number           */
/* The Partner block echoes the peer's actor identity verbatim, see
 * doc/link_aggregation.md. */
__xdata uint16_t lacp_partner_sys_prio[10];	/* partner System priority       */
__xdata uint16_t lacp_partner_port_prio[10];	/* partner Port priority         */
__xdata uint16_t lacp_periodic[10];		/* down-counter to next TX       */
__xdata uint16_t lacp_timeout[10];		/* down-counter to partner expiry*/
__xdata uint8_t  lacp_ntt[10];			/* Need-To-Transmit flag         */

/* Tick divider so lacp_timers() may be called on every main-loop tick */
#define LACP_TICK_DIVIDER 3
__xdata uint8_t lacp_clock;

/* Per-LAG state: the hardware trunk groups run LACP independently. */
__xdata uint16_t lacp_lag_ports[LACP_NUM_LAGS];
__xdata uint8_t  lacp_port_lag[10];
__xdata uint8_t  lacp_agg_sys[LACP_NUM_LAGS][6];
__xdata uint8_t  lacp_agg_valid[LACP_NUM_LAGS];
__xdata uint16_t lacp_members_last[LACP_NUM_LAGS];

__xdata uint16_t lacp_scratch_mask;
__xdata uint8_t  lacp_scratch_flag;

/* Per-port RX LACPDU count - primarily a hardware bring-up diagnostic to
 * verify slow-protocol frames actually reach the CPU port (see "lacp show") */
__xdata uint16_t lacp_rx_count[10];

__xdata uint16_t lacp_fdb_vid;
__xdata uint8_t  lacp_fdb_i, lacp_fdb_j, lacp_fdb_guard;



void lacp_mux_update(void) __banked;	/* defined below, used from lacp_in */

static signed char cmpMAC(__xdata uint8_t *m1, __xdata uint8_t *m2) __reentrant
{
	for (uint8_t i = 0; i < 6; i++) {
		if (m1[i] == m2[i])
			continue;
		if (m1[i] < m2[i])
			return -1;
		return 1;
	}
	return 0;
}


/*
 * Selection logic (simplified 43.4.14): a port may join the aggregator when
 * we have fresh partner info, the partner considers the link aggregatable,
 * and the partner is the same system the aggregator was elected for.
 */
static uint8_t lacp_port_selected(uint8_t port)
{
	uint8_t lag = lacp_port_lag[port];
	if (lag == LACP_LAG_NONE)		/* port not in any LACP LAG */
		return 0;
	if (lacp_rx_state[port] != LACP_RX_CURRENT)
		return 0;
	if (!(lacp_partner_state[port] & LACP_STATE_AGGREGATION))
		return 0;
	if (!lacp_agg_valid[lag])
		return 0;
	return cmpMAC(lacp_partner_sys[port], lacp_agg_sys[lag]) == 0;
}


/*
 * Mux machine (simplified coupled control, 43.4.15): SYNC follows selection;
 * COLLECTING+DISTRIBUTING additionally require the partner to be in sync.
 * Returns 1 if the actor state changed (caller sets NTT).
 */
static uint8_t lacp_mux_machine(uint8_t port)
{
	uint8_t old = lacp_actor_state[port];
	uint8_t new = old;

	if (lacp_port_selected(port)) {
		new |= LACP_STATE_SYNC;
		if (lacp_partner_state[port] & LACP_STATE_SYNC)
			new |= LACP_STATE_COLLECTING | LACP_STATE_DISTRIBUTING;
		else
			new &= ~(LACP_STATE_COLLECTING | LACP_STATE_DISTRIBUTING);
	} else {
		new &= ~(LACP_STATE_SYNC | LACP_STATE_COLLECTING | LACP_STATE_DISTRIBUTING);
	}

	if (new == old)
		return 0;
	lacp_actor_state[port] = new;
	return 1;
}

#define port_bit(p) (((uint8_t)1) << (p))

/* All front-panel ports of the detected chip (platform knowledge, one place) */
#define LACP_PMASK_PORTS (machine_detected.isRTL8373 ? PMASK_9 : PMASK_6)


/* Build and emit one LACPDU out physical port `port` (802.3ad 43.4.2). */
void lacp_send(uint8_t port) __banked
{
	memset((__xdata uint8_t *)LACP_O, 0, sizeof(struct lacpdu));
	LACP_O->dst[0] = 0x01; LACP_O->dst[1] = 0x80; LACP_O->dst[2] = 0xc2;
	LACP_O->dst[3] = 0x00; LACP_O->dst[4] = 0x00; LACP_O->dst[5] = LACP_DST5;
	memcpy(LACP_O->src, uip_ethaddr.addr, 6);

	LACP_O->rtl_tag.tag = HTONS(RTL_FRAME_TAG_ID);
	LACP_O->rtl_tag.version = RTL_FRAME_TAG_VERSION;
	LACP_O->rtl_tag.reason = 0x00;
	/* Must go through HTONS like every other tag field: writing 0x0020 raw put
	 * the bits in the wrong byte (on the wire 0x2000 = EFID, not LEARN_DIS), so
	 * the ASIC could not parse the tag and forwarded the frame with the 0x8899
	 * header still attached - the partner then saw ethertype 0x8899 instead of
	 * 0x8809 and ignored the LACPDU. KEEP additionally stops the switch from
	 * rewriting the 802.1Q tag format of a CPU-injected frame (as mainline). */
	LACP_O->rtl_tag.flags = HTONS(RTL_TAG_LEARN_DIS | RTL_TAG_KEEP);
	/* ALLOW cleared => this word is the *forwarding* port mask, i.e. directed
	 * egress to exactly this port (what mainline does). Setting ALLOW instead
	 * makes it an allowance/permission mask on top of a normal lookup, which
	 * for a one-hot mask resolves to an empty egress set and the frame is
	 * silently dropped - verified on hardware. */
	LACP_O->rtl_tag.pmask = HTONS(port_bit(port));	/* egress this port only */

	LACP_O->ethertype = HTONS(SLOW_PROTO_ETHERTYPE);
	LACP_O->subtype = SLOW_PROTO_SUBTYPE_LACP;
	LACP_O->version = LACP_VERSION;

	/* Actor TLV */
	LACP_O->tlv_actor = LACP_TLV_ACTOR;
	LACP_O->actor_len = LACP_TLV_LEN_INFO;
	LACP_O->actor.sys_prio = HTONS(LACP_SYS_PRIO);
	memcpy(LACP_O->actor.sys, uip_ethaddr.addr, 6);
	/* Per-LAG Actor Key so a partner never merges ports of our different LAGs
	 * into one aggregate (only ever called for ports in a LACP LAG). */
	LACP_O->actor.key = HTONS((uint16_t)(lacp_port_lag[port] + 1));
	LACP_O->actor.port_prio = HTONS(LACP_DEF_PORT_PRIO);
	LACP_O->actor.port = HTONS((uint16_t)port + 1);	/* 1-based port id */
	LACP_O->actor.state = lacp_actor_state[port];

	/* Partner TLV: echo the last partner info we recorded, VERBATIM. A Linux
	 * 802.3ad partner (__record_pdu) only accepts the SYNC bit we set above if
	 * this block mirrors its own actor identity exactly - system+priority, key,
	 * port+priority. Priorities must be echoed, not hardcoded: Linux defaults to
	 * system priority 0xffff, so a hardcoded 0 here cleared its partner-SYNC
	 * (0x3f->0x37) and the aggregate never collected/distributed. */
	LACP_O->tlv_partner = LACP_TLV_PARTNER;
	LACP_O->partner_len = LACP_TLV_LEN_INFO;
	LACP_O->partner.sys_prio = HTONS(lacp_partner_sys_prio[port]);
	memcpy(LACP_O->partner.sys, lacp_partner_sys[port], 6);
	LACP_O->partner.key = HTONS(lacp_partner_key[port]);
	LACP_O->partner.port_prio = HTONS(lacp_partner_port_prio[port]);
	LACP_O->partner.port = HTONS(lacp_partner_port[port]);
	LACP_O->partner.state = lacp_partner_state[port];

	/* Collector TLV + Terminator */
	LACP_O->tlv_collector = LACP_TLV_COLLECTOR;
	LACP_O->collector_len = LACP_TLV_LEN_COLLECTOR;
	LACP_O->collector_max_delay = 0;
	LACP_O->tlv_terminator = LACP_TLV_TERMINATOR;
	LACP_O->terminator_len = 0x00;

	lacp_ntt[port] = 0;
	/* Slow-protocol frames are link-local and must egress untagged. With a
	 * management VLAN set, tcpip_output() splices an 802.1Q tag after the SA,
	 * shifting our in-frame rtl_tag out of the position the ASIC expects - so it
	 * fails to parse/strip the CPU tag and the 0x8899 header leaks onto the wire
	 * (the partner then ignores the LACPDU). Suppress the VLAN insert for this
	 * frame only; management traffic keeps its tag. Verified on HW: with the
	 * insert suppressed the LACPDU egresses as clean 0x8809 and the bond
	 * partner converges (partner MAC becomes the switch, both links aggregate). */
	uint16_t saved_mgmt_vlan = management_vlan;
	management_vlan = 0;
	uip_len = sizeof(struct lacpdu);
	tcpip_output();
	management_vlan = saved_mgmt_vlan;
}


/*
 * Process an incoming LACPDU. Called from the rtlplayground.c dispatch when a
 * frame with DA 01:80:C2:00:00:02 and ethertype 0x8809/subtype 0x01 arrives.
 * The ingress port is the low nibble of rtl_tag.pmask on RX.
 */
void lacp_in(void) __banked
{
	uip_len = 0;

	if (LACP_I->subtype != SLOW_PROTO_SUBTYPE_LACP)
		return;

	/* Per rtl837x_common.h, pmask carries a 4-bit port number on RX
	 * (hardware-verified on RTL8373: per-port rx counters track the actual
	 * ingress port through full LACP convergence). */
	uint8_t port = ((uint8_t)HTONS(LACP_I->rtl_tag.pmask)) & 0x0f;
	if (port < machine.min_port || port > machine.max_port)
		return;

	/* Only ports assigned to a LACP-mode LAG participate in the protocol. */
	uint8_t lag = lacp_port_lag[port];
	if (lag == LACP_LAG_NONE)
		return;


#ifdef DEBUG
	print_string("LACP in, port "); print_byte(port);
	print_string(" partner_state "); print_byte(LACP_I->actor.state); write_char('\n');
#endif

	lacp_rx_count[port]++;

	/* Record partner = the remote's Actor block (802.3ad 43.4.9 recordPDU) */
	memcpy(lacp_partner_sys[port], LACP_I->actor.sys, 6);
	lacp_partner_key[port]       = HTONS(LACP_I->actor.key);
	lacp_partner_port[port]      = HTONS(LACP_I->actor.port);
	lacp_partner_sys_prio[port]  = HTONS(LACP_I->actor.sys_prio);
	lacp_partner_port_prio[port] = HTONS(LACP_I->actor.port_prio);
	lacp_partner_state[port]     = LACP_I->actor.state;

	/* Receive machine -> CURRENT, (re)arm partner timeout (43.4.12) */
	lacp_rx_state[port] = LACP_RX_CURRENT;
	lacp_timeout[port] = (LACP_I->actor.state & LACP_STATE_TIMEOUT)
	                   ? LACP_SHORT_TIMEOUT : LACP_LONG_TIMEOUT;

	/* Elect this LAG's aggregator partner system on first contact (43.4.14) */
	if (!lacp_agg_valid[lag]) {
		memcpy(lacp_agg_sys[lag], LACP_I->actor.sys, 6);
		lacp_agg_valid[lag] = 1;
	}

	/* Selection + Mux for this port; state change => Need-To-Transmit */
	if (lacp_mux_machine(port))
		lacp_ntt[port] = 1;

	/* update_NTT (43.4.12): if the partner's view of us is stale (their
	 * Partner block does not match our actor state/port), tell them again. */
	if (LACP_I->partner.state != lacp_actor_state[port]
	    || HTONS(LACP_I->partner.port) != (uint16_t)port + 1)
		lacp_ntt[port] = 1;

	lacp_mux_update();
}


/*
 * Steer the Slow-Protocols group (01:80:C2:00:00:02) to the CPU port only.
 * How and why, see doc/link_aggregation.md.
 */

/* Write the static Slow-Protocols L2 MC entry for VID `lacp_fdb_vid` with a
 * CPU-only member mask. */
static void lacp_fdb_set(void)
{
	lacp_fdb_guard = 0;
	do {	/* wait out any previous table op (bounded, cf. the IGMP guards) */
		reg_read_m(RTL837X_TBL_CTRL);
	} while ((sfr_data[3] & TBL_EXECUTE) && ++lacp_fdb_guard);

	REG_WRITE(RTL837x_TBL_DATA_IN_A, 0xc2, 0x00, 0x00, 0x02);
	REG_WRITE(RTL837x_TBL_DATA_IN_B, 0x20 | (lacp_fdb_vid >> 8), lacp_fdb_vid, 0x01, 0x80);
	REG_WRITE(RTL837x_TBL_DATA_IN_C, 0, 0, 0, PMASK_CPU >> 2);
	REG_WRITE(RTL837X_TBL_CTRL, 0, 0, TBL_L2_UNICAST, TBL_WRITE | TBL_EXECUTE);
	lacp_fdb_guard = 0;
	do {
		reg_read_m(RTL837X_TBL_CTRL);
	} while ((sfr_data[3] & TBL_EXECUTE) && ++lacp_fdb_guard);
}

/* Refresh the CPU-steering entries after a LACP topology change: one entry
 * per distinct PVID over all LACP candidate ports. Config-time only. */
static void lacp_fdb_update(void)
{
	for (lacp_fdb_i = machine.min_port; lacp_fdb_i <= machine.max_port; lacp_fdb_i++) {
		if (lacp_port_lag[lacp_fdb_i] == LACP_LAG_NONE)
			continue;
		lacp_fdb_vid = port_pvid_get(lacp_fdb_i);
		for (lacp_fdb_j = machine.min_port; lacp_fdb_j < lacp_fdb_i; lacp_fdb_j++) {
			if (lacp_port_lag[lacp_fdb_j] != LACP_LAG_NONE
			    && port_pvid_get(lacp_fdb_j) == lacp_fdb_vid)
				goto next_port;		/* this PVID is already written */
		}
		lacp_fdb_set();
next_port:	;
	}
}


/* Recompute each LACP LAG's hardware trunk membership: a port joins its LAG's
 * trunk once it is fully in sync (SYNC+COLLECTING+DISTRIBUTING and the partner
 * in SYNC). Each LACP-mode LAG is programmed independently (simplified 43.4.15). */
void lacp_mux_update(void) __banked
{
	for (uint8_t lag = 0; lag < LACP_NUM_LAGS; lag++) {
		if (!lacp_lag_ports[lag])	/* LAG not under LACP management */
			continue;
		lacp_scratch_mask = 0;
		for (uint8_t i = machine.min_port; i <= machine.max_port; i++) {
			if (lacp_port_lag[i] != lag)
				continue;
			if ((lacp_actor_state[i] & LACP_STATE_FULL) == LACP_STATE_FULL
			    && (lacp_partner_state[i] & LACP_STATE_SYNC))
				lacp_scratch_mask |= port_bit(i);
		}
		/* Program the trunk only when this LAG's membership actually changed
		 * - this runs on every timer tick. */
		if (lacp_scratch_mask != lacp_members_last[lag]) {
			lacp_members_last[lag] = lacp_scratch_mask;
			port_lag_members_set(lag, lacp_scratch_mask);
		}
	}
}


void lacp_timers(void) __banked
{
	if (lacp_clock) {			/* only act every LACP_TICK_DIVIDER ticks */
		lacp_clock--;
		return;
	}
	lacp_clock = LACP_TICK_DIVIDER;

	for (uint8_t i = machine.min_port; i <= machine.max_port; i++) {
		if (lacp_port_lag[i] == LACP_LAG_NONE)	/* not in a LACP LAG */
			continue;

		/* Periodic transmit machine (802.3ad 43.4.13) */
		if (lacp_periodic[i]) {
			lacp_periodic[i]--;
		} else {
			lacp_periodic[i] = (lacp_partner_state[i] & LACP_STATE_TIMEOUT)
			                 ? LACP_FAST_PERIODIC : LACP_SLOW_PERIODIC;
			lacp_ntt[i] = 1;
		}

		/* Partner timeout -> DEFAULTED (802.3ad 43.4.12): the mux machine
		 * then drops SYNC/COLLECTING/DISTRIBUTING and the port leaves the
		 * trunk. TODO(43.4.12): churn detection is diagnostics-only, skipped. */
		if (lacp_timeout[i]) {
			if (!--lacp_timeout[i]) {
				lacp_rx_state[i] = LACP_RX_DEFAULTED;
				lacp_partner_state[i] = LACP_STATE_DEFAULTED;
				if (lacp_mux_machine(i))
					lacp_ntt[i] = 1;
			}
		}

		if (lacp_ntt[i])
			lacp_send(i);
	}

	/* Release each LAG's aggregator identity once none of its ports has fresh
	 * partner info, so a re-cabled setup can elect a new partner (43.4.14). */
	for (uint8_t lag = 0; lag < LACP_NUM_LAGS; lag++) {
		if (!lacp_agg_valid[lag])
			continue;
		uint8_t any_current = 0;
		for (uint8_t i = machine.min_port; i <= machine.max_port; i++) {
			if (lacp_port_lag[i] == lag && lacp_rx_state[i] == LACP_RX_CURRENT) {
				any_current = 1;
				break;
			}
		}
		if (!any_current)
			lacp_agg_valid[lag] = 0;
	}

	lacp_mux_update();
}


/* Bring one port up as an active-fast LACP participant (43.4.12 init). */
static void lacp_port_init(uint8_t port)
{
	lacp_actor_state[port] = LACP_STATE_ACTIVITY | LACP_STATE_AGGREGATION
	                       | LACP_STATE_TIMEOUT;	/* active + fast */
	lacp_rx_state[port] = LACP_RX_INITIALIZE;
	lacp_partner_state[port] = LACP_STATE_DEFAULTED;
	memset(lacp_partner_sys[port], 0, 6);
	lacp_partner_key[port] = 0;
	lacp_partner_port[port] = 0;
	lacp_periodic[port] = LACP_FAST_PERIODIC;
	lacp_timeout[port] = 0;
	lacp_ntt[port] = 1;	/* announce ourselves immediately */
	lacp_rx_count[port] = 0;
}

/* Return a port to non-LACP state. (Port isolation is no longer touched -
 * the CPU-steering FDB entry replaced the sibling-isolation workaround.) */
static void lacp_port_release(uint8_t port)
{
	lacp_actor_state[port] = 0;
	lacp_ntt[port] = 0;
}

/* True while any LAG is under LACP management (drives the global RMA trap). */
static uint8_t lacp_any_lag(void)
{
	for (uint8_t l = 0; l < LACP_NUM_LAGS; l++)
		if (lacp_lag_ports[l])
			return 1;
	return 0;
}

/*
 * Turn the LACP engine on: deliver the Slow-Protocols group to the CPU.
 */
static void lacp_engine_on(void)
{
	lacp_clock = LACP_TICK_DIVIDER;
	lacpEnabled = 1;
	REG_SET(RTL837X_RMA2_CONF, RTL837X_RMA_ACT_FORWARD);
}

/* Turn the LACP engine off: stop delivering slow-protocols to the CPU. DROP
 * acts before the L2 lookup, so the static FDB entries can stay behind - they
 * are inert under DROP and the volatile LUT is cleared on reboot anyway. */
static void lacp_engine_off(void)
{
	REG_SET(RTL837X_RMA2_CONF, RTL837X_RMA_ACT_DROP);
	lacpEnabled = 0;
}

/* One-time boot init: no LAG runs LACP yet, so every port maps to LACP_LAG_NONE.
 * MUST run before any "lag ... lacp" config replay - xdata is not zeroed to the
 * 0xff sentinel, and a stray 0 would make a port look like it belongs to LAG 0. */
void lacp_init(void) __banked
{
	for (uint8_t i = 0; i < 10; i++) {
		lacp_port_lag[i] = LACP_LAG_NONE;
		lacp_actor_state[i] = 0;
		lacp_ntt[i] = 0;
	}
	for (uint8_t l = 0; l < LACP_NUM_LAGS; l++) {
		lacp_lag_ports[l] = 0;
		lacp_agg_valid[l] = 0;
		lacp_members_last[l] = 0;
	}
	lacpEnabled = 0;
}

/*
 * Assign a candidate-port mask to LACP-mode LAG `lag` (`lag <n> lacp <ports>`).
 * ports == 0 removes the LAG from LACP management. Ports leaving the LAG are
 * returned to normal switching; ports joining start the protocol immediately.
 * The engine's RMA trap is enabled on the first LACP LAG and torn down with the
 * last. Never wipes a static "lag" the user configured on a different group.
 */
void lacp_lag_set(uint8_t lag, uint16_t ports) __banked
{
	if (lag >= LACP_NUM_LAGS)
		return;
	lacp_scratch_flag = lacp_any_lag();

	/* Drop ports that were in this LAG but are not in the new mask. */
	for (uint8_t i = machine.min_port; i <= machine.max_port; i++) {
		if (lacp_port_lag[i] == lag && !(ports & port_bit(i))) {
			lacp_port_lag[i] = LACP_LAG_NONE;
			lacp_port_release(i);
		}
	}
	/* Release this LAG's trunk if LACP had programmed it (leave static LAGs). */
	if (lacp_members_last[lag]) {
		lacp_members_last[lag] = 0;
		port_lag_members_set(lag, 0);
	}
	lacp_agg_valid[lag] = 0;
	lacp_lag_ports[lag] = ports;

	if (ports && !lacp_scratch_flag)	/* first LACP LAG: bring the engine up first */
		lacp_engine_on();

	/* Adopt the new member ports (with the engine already up). */
	for (uint8_t i = machine.min_port; i <= machine.max_port; i++) {
		if (ports & port_bit(i)) {
			lacp_port_lag[i] = lag;
			lacp_port_init(i);
		}
	}

	if (!lacp_any_lag() && lacp_scratch_flag)	/* removed the last LACP LAG */
		lacp_engine_off();

	lacp_fdb_update();	/* (re)write the CPU-steering entries for the new topology */
}


/* Legacy "lacp on": one aggregator on LAG 0 spanning every port (the pre-
 * per-LAG behaviour). Explicit "lag <n> lacp <ports>" is the per-LAG path. */
void lacp_setup(void) __banked
{
	lacp_lag_set(0, LACP_PMASK_PORTS);
}


/* "lacp off": remove every LACP LAG (releases their trunks, stops the engine). */
void lacp_off(void) __banked
{
	for (uint8_t l = 0; l < LACP_NUM_LAGS; l++)
		if (lacp_lag_ports[l])
			lacp_lag_set(l, 0);
}


/* "lacp show": per-port protocol state - the primary bring-up diagnostic.
 * A stuck rx=0 counter means slow-protocol frames never reach the CPU port
 * (see the RMA note in lacp_engine_on()). */
void lacp_show(void) __banked
{
	print_string("LACP "); print_string(lacpEnabled ? "on" : "off"); write_char('\n');
	for (uint8_t l = 0; l < LACP_NUM_LAGS; l++) {
		if (!lacp_lag_ports[l])
			continue;
		print_string("lag "); print_byte(l + 1);
		print_string(" ports "); print_short(lacp_lag_ports[l]);
		print_string(" aggregator ");
		if (lacp_agg_valid[l]) {
			for (uint8_t j = 0; j < 6; j++)
				print_byte(lacp_agg_sys[l][j]);
		} else {
			print_string("(none)");
		}
		print_string(" members "); print_short(lacp_members_last[l]); write_char('\n');
	}

	for (uint8_t i = machine.min_port; i <= machine.max_port; i++) {
		print_string("port "); print_byte(machine.log_to_phys_port[i]);
		print_string(" actor "); print_byte(lacp_actor_state[i]);
		print_string(" partner "); print_byte(lacp_partner_state[i]);
		print_string(" rxst "); print_byte(lacp_rx_state[i]);
		print_string(" rx "); print_short(lacp_rx_count[i]);
		print_string(" psys ");
		for (uint8_t j = 0; j < 6; j++)
			print_byte(lacp_partner_sys[i][j]);
		write_char('\n');
	}
}


/* "lacp on|off" command handler (kept in the module with the rest of LACP). */
void lacp_cmd(uint8_t on) __banked
{
	/* lacpEnabled is owned by lacp_engine_on()/off(), driven from lacp_lag_set() */
	if (on) {
		print_string("LACP enabled\n");
		lacp_setup();
	} else {
		print_string("LACP disabled\n");
		lacp_off();
	}
}
