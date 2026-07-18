/*
 * LACP (IEEE 802.3ad Clause 43) driver for the RTL837x platform.
 * This code is in the Public Domain.
 *
 * Structure mirrors rtl837x_stp.c: a slow-protocol control frame is trapped to
 * the CPU port via the Reserved-Multicast mechanism, parsed via a struct overlaid
 * on uip_buf, and periodic LACPDUs are emitted per port using the RTL frame tag
 * (rtl_tag.pmask selects the egress port). Once the actor/partner reach
 * SYNC+COLLECTING+DISTRIBUTING, the participating ports are programmed into a
 * hardware trunk group via port_lag_members_set().
 *
 * STATUS: skeleton. Frame RX/TX, periodic timers and trunk programming are wired;
 * the four 802.3ad state machines (Receive / Periodic / Mux / Selection) are
 * stubbed where marked TODO(43.4.x).
 */

// #define DEBUG

/* Place this module's code and constants in code bank 2 (cf. rtl837x_igmp.c) */
#pragma codeseg BANK2
#pragma constseg BANK2

#include <stdint.h>
#include "rtl837x_common.h"
#include "rtl837x_sfr.h"
#include "rtl837x_regs.h"
#include "rtl837x_lacp.h"
#include "rtl837x_port.h"
#include "uip.h"
#include "machine.h"

extern __code struct machine machine;
extern __xdata uint8_t sfr_data[4];
extern __xdata struct uip_eth_addr uip_ethaddr;
extern __xdata uint8_t uip_buf[UIP_CONF_BUFFER_SIZE + 2];
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
__xdata uint16_t lacp_periodic[10];		/* down-counter to next TX       */
__xdata uint16_t lacp_timeout[10];		/* down-counter to partner expiry*/
__xdata uint8_t  lacp_ntt[10];			/* Need-To-Transmit flag         */

/* Our aggregation identity (shared across ports of the same LAG) */
__xdata uint16_t lacp_sys_prio;
__xdata uint16_t lacp_actor_key;

/* Tick divider so lacp_timers() may be called on every main-loop tick */
#define LACP_TICK_DIVIDER 3
__xdata uint8_t lacp_clock;

/* Last member mask written to the trunk group - avoids hammering the trunk
 * registers on every timer tick when membership has not changed. */
__xdata uint16_t lacp_members_last;

/* Aggregator identity: partner system elected for the (single) active LAG.
 * Ports whose partner differs stay unselected - the classic LACP protection
 * against aggregating links that go to different switches (43.4.14). */
__xdata uint8_t lacp_agg_sys[6];
__xdata uint8_t lacp_agg_valid;

/* Per-port RX LACPDU count - primarily a hardware bring-up diagnostic to
 * verify slow-protocol frames actually reach the CPU port (see "lacp show") */
__xdata uint16_t lacp_rx_count[10];


void lacp_mux_update(void) __banked;	/* defined below, used from lacp_in */

/* 6-byte system-ID compare (local, to avoid coupling to stp.c's cmpMAC) */
static uint8_t lacp_sys_eq(__xdata uint8_t *a, __xdata uint8_t *b)
{
	for (uint8_t i = 0; i < 6; i++) {
		if (a[i] != b[i])
			return 0;
	}
	return 1;
}


/*
 * Selection logic (simplified 43.4.14): a port may join the aggregator when
 * we have fresh partner info, the partner considers the link aggregatable,
 * and the partner is the same system the aggregator was elected for.
 */
static uint8_t lacp_port_selected(uint8_t port)
{
	if (lacp_rx_state[port] != LACP_RX_CURRENT)
		return 0;
	if (!(lacp_partner_state[port] & LACP_STATE_AGGREGATION))
		return 0;
	if (!lacp_agg_valid)
		return 0;
	return lacp_sys_eq(lacp_partner_sys[port], lacp_agg_sys);
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


/* Build and emit one LACPDU out physical port `port` (802.3ad 43.4.2). */
void lacp_send(uint8_t port) __banked
{
	LACP_O->dst[0] = 0x01; LACP_O->dst[1] = 0x80; LACP_O->dst[2] = 0xc2;
	LACP_O->dst[3] = 0x00; LACP_O->dst[4] = 0x00; LACP_O->dst[5] = LACP_DST5;
	memcpy(LACP_O->src, uip_ethaddr.addr, 6);

	LACP_O->rtl_tag.tag = HTONS(RTL_FRAME_TAG_ID);
	LACP_O->rtl_tag.version = RTL_FRAME_TAG_VERSION;
	LACP_O->rtl_tag.reason = 0x00;
	LACP_O->rtl_tag.flags = 0x0020;			/* disable L2 learning (as STP) */
	LACP_O->rtl_tag.pmask = HTONS(port_bit(port));	/* egress this port only         */

	LACP_O->ethertype = HTONS(SLOW_PROTO_ETHERTYPE);
	LACP_O->subtype = SLOW_PROTO_SUBTYPE_LACP;
	LACP_O->version = LACP_VERSION;

	/* Actor TLV */
	LACP_O->tlv_actor = 0x01;
	LACP_O->actor_len = 0x14;
	LACP_O->actor.sys_prio = HTONS(lacp_sys_prio);
	memcpy(LACP_O->actor.sys, uip_ethaddr.addr, 6);
	LACP_O->actor.key = HTONS(lacp_actor_key);
	LACP_O->actor.port_prio = HTONS(0x00ff);
	LACP_O->actor.port = HTONS((uint16_t)port + 1);	/* 1-based port id */
	LACP_O->actor.state = lacp_actor_state[port];

	/* Partner TLV: echo the last partner info we recorded */
	LACP_O->tlv_partner = 0x02;
	LACP_O->partner_len = 0x14;
	LACP_O->partner.sys_prio = 0;
	memcpy(LACP_O->partner.sys, lacp_partner_sys[port], 6);
	LACP_O->partner.key = HTONS(lacp_partner_key[port]);
	LACP_O->partner.port_prio = HTONS(0x00ff);
	LACP_O->partner.port = HTONS(lacp_partner_port[port]);
	LACP_O->partner.state = lacp_partner_state[port];

	/* Collector TLV + Terminator */
	LACP_O->tlv_collector = 0x03;
	LACP_O->collector_len = 0x10;
	LACP_O->collector_max_delay = 0;
	LACP_O->tlv_terminator = 0x00;
	LACP_O->terminator_len = 0x00;

	lacp_ntt[port] = 0;
	uip_len = sizeof(struct lacpdu);
	tcpip_output();
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

	/* Per rtl837x_common.h, pmask carries a 4-bit port number on RX. NOTE:
	 * unverified on hardware (STP never reads it; IGMP uses another path) -
	 * confirm the nibble position with a real capture before relying on it. */
	uint8_t port = ((uint8_t)HTONS(LACP_I->rtl_tag.pmask)) & 0x0f;
	if (port < machine.min_port || port > machine.max_port)
		return;

#ifdef DEBUG
	print_string("LACP in, port "); print_byte(port);
	print_string(" partner_state "); print_byte(LACP_I->actor.state); write_char('\n');
#endif

	lacp_rx_count[port]++;

	/* Record partner = the remote's Actor block (802.3ad 43.4.9 recordPDU) */
	memcpy(lacp_partner_sys[port], LACP_I->actor.sys, 6);
	lacp_partner_key[port]   = HTONS(LACP_I->actor.key);
	lacp_partner_port[port]  = HTONS(LACP_I->actor.port);
	lacp_partner_state[port] = LACP_I->actor.state;

	/* Receive machine -> CURRENT, (re)arm partner timeout (43.4.12) */
	lacp_rx_state[port] = LACP_RX_CURRENT;
	lacp_timeout[port] = (LACP_I->actor.state & LACP_STATE_TIMEOUT)
	                   ? LACP_SHORT_TIMEOUT : LACP_LONG_TIMEOUT;

	/* Elect the aggregator's partner system on first contact (43.4.14) */
	if (!lacp_agg_valid) {
		memcpy(lacp_agg_sys, LACP_I->actor.sys, 6);
		lacp_agg_valid = 1;
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


/* Add/remove this port from the hardware trunk once fully in sync. TODO(43.4.15) */
void lacp_mux_update(void) __banked
{
	uint16_t members = 0;
	for (uint8_t i = machine.min_port; i <= machine.max_port; i++) {
		uint8_t need = LACP_STATE_SYNC | LACP_STATE_COLLECTING | LACP_STATE_DISTRIBUTING;
		if ((lacp_actor_state[i] & need) == need
		    && (lacp_partner_state[i] & LACP_STATE_SYNC))
			members |= port_bit(i);
	}
	/* Program the LACP-managed hardware trunk group only when membership
	 * actually changed - this runs on every timer tick. */
	if (members != lacp_members_last) {
		lacp_members_last = members;
		port_lag_members_set(LACP_TRUNK_ID, members);
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
		if (!(lacp_actor_state[i] & LACP_STATE_AGGREGATION))
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

	/* Release the aggregator identity once no port has fresh partner info,
	 * so a re-cabled setup can elect a new partner system (43.4.14). */
	if (lacp_agg_valid) {
		uint8_t any_current = 0;
		for (uint8_t i = machine.min_port; i <= machine.max_port; i++) {
			if (lacp_rx_state[i] == LACP_RX_CURRENT)
				any_current = 1;
		}
		if (!any_current)
			lacp_agg_valid = 0;
	}

	lacp_mux_update();
}


void lacp_setup(void) __banked
{
	print_string("Enabling LACP\n");

	lacp_sys_prio = 0xffff;
	lacp_actor_key = 0x0001;	/* one LAG; refine per speed/duplex later */

	for (uint8_t i = machine.min_port; i <= machine.max_port; i++) {
		lacp_actor_state[i] = LACP_STATE_ACTIVITY | LACP_STATE_AGGREGATION
		                    | LACP_STATE_TIMEOUT;	/* active + fast */
		lacp_rx_state[i] = LACP_RX_INITIALIZE;
		lacp_partner_state[i] = LACP_STATE_DEFAULTED;
		memset(lacp_partner_sys[i], 0, 6);
		lacp_partner_key[i] = 0;
		lacp_partner_port[i] = 0;
		lacp_periodic[i] = LACP_FAST_PERIODIC;
		lacp_timeout[i] = 0;
		lacp_ntt[i] = 1;	/* announce ourselves immediately */
		lacp_rx_count[i] = 0;
	}

	lacp_agg_valid = 0;
	lacp_clock = LACP_TICK_DIVIDER;
	/* Do NOT touch the trunk group here. A static "lag 0 ..." from the saved
	 * configuration may already cover these ports, and clearing a group on
	 * live member ports was observed to leave them in a broken ingress state
	 * (dead LAN) until the next config replay. LACP only writes the trunk
	 * once a partner actually negotiates (see lacp_mux_update). */
	lacp_members_last = 0;

	/*
	 * Trap the Slow-Protocols group (01:80:C2:00:00:02) to the CPU. The ASIC
	 * default action for this address is "forward" (flood), not "trap", so
	 * without this the CPU never sees LACPDUs and negotiation cannot start.
	 * (Confirmed on hardware: RX counters stayed 0 until this was set.)
	 */
	REG_SET(RTL837X_RMA2_CONF, RTL837X_RMA_ACT_TRAP_CPU);
}


void lacp_off(void) __banked
{
	/* Stop trapping slow-protocols back to the ASIC default (forward). */
	REG_SET(RTL837X_RMA2_CONF, 0x00000000);

	/* Release the trunk ONLY if LACP actually programmed it - never wipe a
	 * user-configured static "lag 0 ..." that we did not create. */
	if (lacp_members_last) {
		lacp_members_last = 0;
		port_lag_members_set(LACP_TRUNK_ID, 0);
	}
	lacp_agg_valid = 0;
	for (uint8_t i = machine.min_port; i <= machine.max_port; i++) {
		lacp_actor_state[i] = 0;
		lacp_ntt[i] = 0;
	}
}


/* "lacp show": per-port protocol state - the primary bring-up diagnostic.
 * A stuck rx=0 counter means slow-protocol frames never reach the CPU port
 * (see the RMA note in lacp_setup()). */
void lacp_show(void) __banked
{
	print_string("LACP "); print_string(lacpEnabled ? "on" : "off");
	print_string(", aggregator ");
	if (lacp_agg_valid) {
		for (uint8_t j = 0; j < 6; j++)
			print_byte(lacp_agg_sys[j]);
	} else {
		print_string("(none)");
	}
	print_string(", members "); print_short(lacp_members_last); write_char('\n');

	for (uint8_t i = machine.min_port; i <= machine.max_port; i++) {
		print_string("port "); print_byte(i);
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
	if (on) {
		print_string("LACP enabled\n");
		lacpEnabled = 1;
		lacp_setup();
	} else {
		print_string("LACP disabled\n");
		lacp_off();
		lacpEnabled = 0;
	}
}
