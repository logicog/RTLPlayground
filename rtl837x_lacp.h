#ifndef _RTL837X_LACP_H_
#define _RTL837X_LACP_H_

/*
 * LACP (IEEE 802.3ad Clause 43) for the RTL837x platform.
 * This code is in the Public Domain.
 */

#include <stdint.h>

void lacp_init(void) __banked;		/* boot init: clear per-LAG state */
void lacp_in(void) __banked;
void lacp_setup(void) __banked;
void lacp_timers(void) __banked;
void lacp_off(void) __banked;
void lacp_cmd(uint8_t on) __banked;	/* "lacp on|off" master engine handler */
void lacp_show(void) __banked;		/* "lacp show" - per-port state + RX counters */
/* Assign a candidate-port mask to a LACP-mode LAG (`lag <n> lacp <ports>`).
 * ports == 0 removes the LAG from LACP management. Enables the engine on first
 * LACP LAG and tears it down when the last one goes away. */
void lacp_lag_set(uint8_t lag, uint16_t ports) __banked;

/*
 * Per-LAG LACP: each of the 4 hardware trunk groups (0-3) can independently run
 * LACP on an admin-assigned set of candidate ports, forming its own aggregator
 * (its own elected partner System). A port participates in LACP only if it has
 * been assigned to a LACP-mode LAG (lacp_port_lag[port] != LACP_LAG_NONE).
 * Ports of the same LAG that converge with a consistent partner join that LAG's
 * hardware trunk. This is the port-channel model: `lag <n> lacp <ports>`.
 */
#define LACP_NUM_LAGS		4
#define LACP_LAG_NONE		0xff	/* port not assigned to any LACP LAG */

/* Protocol state, exposed read-only for the web UI (page_impl.c send_lacp())
 * and the serial console. Owned by rtl837x_lacp.c / rtlplayground.c. */
extern __xdata uint8_t  lacpEnabled;
extern __xdata uint8_t  lacp_actor_state[10];
extern __xdata uint8_t  lacp_partner_state[10];
extern __xdata uint8_t  lacp_rx_state[10];
extern __xdata uint8_t  lacp_partner_sys[10][6];
extern __xdata uint16_t lacp_rx_count[10];
extern __xdata uint8_t  lacp_port_lag[10];		/* LAG (0-3) a port runs LACP on, or LACP_LAG_NONE */
extern __xdata uint16_t lacp_lag_ports[LACP_NUM_LAGS];	/* admin candidate-port mask per LACP LAG (0 = LAG not LACP) */
extern __xdata uint8_t  lacp_agg_sys[LACP_NUM_LAGS][6];	/* elected partner System per LAG */
extern __xdata uint8_t  lacp_agg_valid[LACP_NUM_LAGS];	/* aggregator elected for this LAG */
extern __xdata uint16_t lacp_members_last[LACP_NUM_LAGS];/* trunk members we last programmed per LAG */

/* Slow-Protocols / LACPDU identifiers (802.3ad 43.4) */
#define SLOW_PROTO_ETHERTYPE	0x8809
#define SLOW_PROTO_SUBTYPE_LACP	0x01
#define LACP_VERSION		0x01
#define LACP_DST5		0x02	/* 01:80:C2:00:00:02 last octet */

/* LACPDU TLV framing (802.3ad 43.4.2): type bytes + fixed TLV lengths. */
#define LACP_TLV_ACTOR		0x01
#define LACP_TLV_PARTNER	0x02
#define LACP_TLV_COLLECTOR	0x03
#define LACP_TLV_TERMINATOR	0x00
#define LACP_TLV_LEN_INFO	0x14	/* Actor/Partner Information TLV length */
#define LACP_TLV_LEN_COLLECTOR	0x10	/* Collector TLV length */

/* Our aggregation identity: System priority is a constant (Linux 802.3ad
 * defaults to the same 0xffff); the default Actor port priority likewise. */
#define LACP_SYS_PRIO		0xffff
#define LACP_DEF_PORT_PRIO	0x00ff

/* Actor_State / Partner_State flag bits (802.3ad 43.4.2) */
#define LACP_STATE_ACTIVITY	0x01	/* 1 = Active LACP (send periodically)  */
#define LACP_STATE_TIMEOUT	0x02	/* 1 = Short (fast) timeout             */
#define LACP_STATE_AGGREGATION	0x04	/* 1 = link is aggregatable             */
#define LACP_STATE_SYNC		0x08	/* 1 = in sync with partner             */
#define LACP_STATE_COLLECTING	0x10
#define LACP_STATE_DISTRIBUTING	0x20
#define LACP_STATE_DEFAULTED	0x40	/* partner info is administrative default*/
#define LACP_STATE_EXPIRED	0x80	/* wire-format doc; unused by the simplified RX machine */

/* Fully-participating actor: in sync AND collecting AND distributing. */
#define LACP_STATE_FULL		(LACP_STATE_SYNC | LACP_STATE_COLLECTING | LACP_STATE_DISTRIBUTING)

/*
 * Timer units: one decrement per active lacp_timers() call, measured at about
 * 50 Hz on a SWTGW218AS. The rate jitters with how often interrupts wake the
 * main loop; the longest gap observed was 1.6x the median, so a period is a
 * range and the worst case is what matters.
 *
 * The transmit periods are not free to choose. Our own timeouts only decide
 * how patient we are, but our TX period decides when the PARTNER gives up on
 * us: 802.3ad pairs fast periodic 1 s with a 3 s short timeout, and slow
 * periodic 30 s with a 90 s long timeout. A period whose worst case reaches
 * the partner's threshold makes it expire us intermittently, which reads as
 * an aggregate that works but never stops churning. */
#define LACP_FAST_PERIODIC	0x0032	/* fast TX ~1 s, worst ~1.6 s; partner expires at 3 s  */
#define LACP_SLOW_PERIODIC	0x05dc	/* slow TX ~30 s, worst ~48 s; partner expires at 90 s */
#define LACP_SHORT_TIMEOUT	0x0300	/* we drop a silent partner after 6-9 s  */
#define LACP_LONG_TIMEOUT	0x5a00	/* long-timeout variant, after 3-5 min   */

/* Per-port LACP receive-machine state (802.3ad 43.4.12) */
#define LACP_RX_INITIALIZE	0
#define LACP_RX_PORT_DISABLED	1	/* numbering reserved (exported via /lacp.json "rs") */
#define LACP_RX_EXPIRED		2	/* numbering reserved (exported via /lacp.json "rs") */
#define LACP_RX_DEFAULTED	3
#define LACP_RX_CURRENT		4

#endif
