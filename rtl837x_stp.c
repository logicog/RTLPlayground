/*
 * This is a driver implementation for the Spanning Tree Protocol features for the RTL837x platform
 * This code is in the Public Domain
 */

// #define REGDBG
// #define DEBUG

#include <stdint.h>
#include "rtl837x_common.h"
#include "rtl837x_sfr.h"
#include "rtl837x_regs.h"
#include "rtl837x_stp.h"
#include "rtl837x_port.h"
#include "uip.h"
#include "machine.h"

// All entry points are __banked and nothing here runs from an interrupt,
// so the module does not need to stay in the resident bank
#pragma codeseg BANK3
#pragma constseg BANK3

extern __code struct machine machine;
extern __xdata uint8_t sfr_data[4];
extern __xdata struct machine_runtime machine_detected;


extern __xdata struct uip_eth_addr uip_ethaddr;

extern __xdata uint8_t uip_buf[UIP_CONF_BUFFER_SIZE + 2];


/* ---- Configuration ---- */
__xdata uint8_t  stp_prio;	/* bridge priority high byte (0x80 = 32768) */
__xdata uint8_t  stp_hello_s;		/* 1-10 s */
__xdata uint8_t  stp_maxage_s;		/* 6-40 s */
__xdata uint8_t  stp_fwddelay_s;	/* 4-30 s, also our listen period */
__xdata uint8_t  stp_rstp;		/* 1 = RST BPDUs, 0 = legacy Config BPDUs */
__xdata uint8_t  stp_txhold;		/* BPDUs per port per second */


__xdata uint8_t  stp_pflags[STP_ENTITIES];
__xdata uint32_t stp_pcost[STP_ENTITIES];		/* 0 = auto */
__xdata uint8_t  stp_pprio[STP_ENTITIES];
__xdata uint8_t  stp_pspeed[STP_ENTITIES];	/* speed nibble last read from the ASIC */
__xdata uint8_t  stp_pp2p[STP_ENTITIES];		/* admin point-to-point: 0 auto, 1 on, 2 off */

/* Designated bridge, port and cost last heard on the port; stp_info_while
 * tells whether they are still current.
 */
__xdata struct bridge stp_dbridge[STP_ENTITIES];
__xdata struct bridge stp_droot[STP_ENTITIES];
__xdata uint16_t stp_dpid[STP_ENTITIES];
__xdata uint32_t stp_dcost[STP_ENTITIES];
__xdata uint16_t stp_alt;		/* bit per port: blocked, a better bridge owns the segment */
__xdata struct bridge stp_self;
__xdata uint8_t  stp_j;
__xdata uint8_t  stp_best;
__xdata uint32_t stp_cost_cand;
__xdata uint32_t stp_cost_best;
__xdata int8_t   stp_cmp;
__xdata uint8_t  stp_rxage[STP_ENTITIES];		/* message age heard on the port, seconds */
__xdata uint8_t  stp_rxmaxage[STP_ENTITIES];
__xdata uint8_t  stp_rxhello[STP_ENTITIES];
__xdata uint8_t  stp_rxfwd[STP_ENTITIES];
__xdata uint16_t stp_info_while[STP_ENTITIES];	/* ticks the heard information stays valid, 0 = none */
__xdata uint8_t  stp_root_maxage;	/* max age and forward delay of the root */
__xdata uint8_t  stp_root_fwd;
__xdata uint8_t  stp_reselect_due;
__xdata uint32_t stp_rpcost[STP_ENTITIES];	/* root path cost through the port: heard cost plus port cost */

/* ---- Status / runtime ---- */
__xdata struct bridge root_bridge;
__xdata uint32_t root_bridge_cost;	/* our cost to the root (rx cost + root port cost) */
__xdata uint8_t  stp_root_port;		/* 0xff = we are the root */
__xdata uint16_t stp_tc_count;
__xdata uint16_t stp_tc_seen;
__xdata uint32_t stp_tc_secs;		/* seconds since the topology change counter last moved */
__xdata uint8_t  stp_pcost_short;	/* 1: automatic port costs from the 802.1D-1998 table */
__xdata uint8_t  stp_bpdu_filter;	/* 1: keep BPDUs on the CPU while STP is off instead of flooding */
__xdata uint16_t stp_scratch16;	/* scratch for status printing only */

__xdata uint16_t port_timers[STP_ENTITIES];	/* listen-period countdown (0 = not listening) */
__xdata uint16_t port_hello[STP_ENTITIES];	/* hello TX countdown */
__xdata uint16_t stp_bpdu_age[STP_ENTITIES];	/* ticks since last BPDU seen on port (saturating) */
__xdata uint8_t  stp_loop_held[STP_ENTITIES];	/* port is out of forwarding because a loop was seen on it */
__xdata uint16_t stp_heard;		/* bit per port: a BPDU arrived since the link last went down */
__xdata uint8_t  stp_tx_budget[STP_ENTITIES];	/* tx hold: BPDUs left in the current second */
__xdata uint32_t stp_cnt[STP_CNT_N][STP_ENTITIES];	/* per-port counters, first index STP_CNT_* */
__xdata uint16_t stp_sec_tick;		/* 1 s window for the tx budget */
__xdata uint16_t stp_link_prev;		/* carrier bitmap as of the last check */
__xdata uint16_t stp_link_now;

__xdata uint8_t  stp_scratch;
__xdata uint8_t  stp_tx_flags_extra;	/* one-shot flags OR-ed into the next BPDU (TCA) */
__xdata uint16_t stp_rxlen;		/* received frame length, saved before uip_len is consumed */
__xdata uint8_t  stp_msg_age;		/* message age of the root info we hold, seconds */
__xdata uint16_t stp_tcwhile[STP_ENTITIES];	/* ticks left to send TC on the port, 0 = none */
__xdata uint16_t stp_newinfo;		/* bit per port: send a BPDU at the next tick */
__xdata uint8_t  stp_k;
__xdata uint8_t  stp_tcn;
__xdata uint8_t  stp_armed;
__xdata uint16_t stp_agree;		/* bit per port: answer with an agreement in the next BPDU */
__xdata uint16_t stp_rrwhile[STP_ENTITIES];	/* ticks the port still counts as a recent root port */
__xdata uint16_t stp_rbwhile[STP_ENTITIES];	/* ticks the port still counts as a recent backup port */
__xdata uint16_t stp_reroot;		/* bit per port: recent root port held out of forwarding */
__xdata uint8_t  stp_synced_root;	/* root port the last sync was done for, 0xff = none */
__xdata uint16_t stp_backup;		/* bit per port: blocked because another port of ours owns the segment */
__xdata uint16_t stp_alt_agreed;	/* bit per port: alternate or backup port that already agreed to the information it holds */
__xdata uint16_t stp_legacy;		/* bit per port: the neighbour speaks 802.1D, send Config BPDUs and TCN */
__xdata uint16_t stp_seen_stp;		/* bit per port: an 802.1D BPDU arrived during the migrate time */
__xdata uint16_t stp_seen_rstp;		/* bit per port: an RST BPDU arrived during the migrate time */
__xdata uint8_t  stp_mdelay[STP_ENTITIES];		/* ticks before the port may change protocol again */
__xdata uint8_t  stp_i;
__xdata uint32_t stp_cost_scratch;
__xdata uint8_t  stp_loop_peer;		/* the other own port seen on a looped segment */
__xdata uint8_t  stp_ent_of[STP_PORTS];		/* entity a port answers to: itself, or STP_LAG_BASE + lag */
__xdata uint16_t stp_lag_mask[STP_LAG_COUNT];	/* member ports of each lag, 0 = no such lag */
__xdata uint16_t stp_map_changed;	/* bit per entity: its membership changed, start it over */
__xdata uint16_t stp_link_phys;		/* carrier bitmap of the physical ports */
__xdata uint16_t stp_ent_bit;
__xdata uint16_t stp_ss_mask;
__xdata uint8_t  stp_ss_i;
__xdata uint8_t  stp_lag;

#define STP_EDGE_DELAY	(3 * STP_HZ)	/* auto-edge: forward after 3 s without BPDU */

#define MAXAGE_S	(stp_root_port == 0xff ? stp_maxage_s : stp_root_maxage)
#define FWD_S		(stp_root_port == 0xff ? stp_fwddelay_s : stp_root_fwd)
#define FWD_TICKS	((uint16_t)FWD_S * STP_HZ)
#define P2P(i)		(stp_pp2p[i] != 2)
#define SEND_RSTP(i)	(stp_rstp && !((stp_legacy >> (i)) & 1))
#define STP_MIGRATE	(3 * STP_HZ)

#define AUTO_COST	20000UL		/* path cost of a link whose speed we cannot read */
#define PCOST(i)	(stp_pcost[i] ? stp_pcost[i] : stp_speed_cost[(stp_pcost_short << 3) | (stp_pspeed[i] & 0x7)])

/* Path costs indexed by the speed nibble the ASIC reports: 0 10M, 1 100M,
 * 2 1G, 4 10G, 5 2.5G, 6 5G; the rest are unknown to us. Entries 0-7 hold the
 * 802.1Q recommended values, 8-15 the 802.1D-1998 ones for the same speeds. */
static __code const uint32_t stp_speed_cost[16] = {
	2000000UL, 200000UL, 20000UL, AUTO_COST,
	2000UL, 8000UL, 4000UL, AUTO_COST,
	100UL, 19UL, 4UL, 4UL,
	2UL, 3UL, 3UL, 4UL
};

struct stp_pkt {
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
	uint8_t version1_length;	/* RST BPDU only: length of the (empty) v1 part */
};

struct stp_pkt_in {
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
	uint8_t version1_length;	/* RST BPDU only: length of the (empty) v1 part */
};

#define STP_O ((__xdata struct stp_pkt *)&uip_buf[RTL_FRAME_DESC_SIZE])
#define STP_I ((__xdata struct stp_pkt_in *)&uip_buf[0])

#define BPDU_VER_STP		0x00
#define BPDU_VER_RSTP		0x02

#define BPDU_TYPE_CONFIG	0x00
#define BPDU_TYPE_RST		0x02
#define BPDU_TYPE_TCN		0x80

#define BPDU_LEN_CONFIG		0x26	// LLC and a 35 byte body
#define BPDU_LEN_RST		0x27	// LLC and a 36 byte body
#define BPDU_LEN_TCN		0x07	// LLC and a 4 byte body
#define BPDU_LEN_MIN_HEADER	33	// addresses through bpdu_type

#define BPDU_FLAG_TC		0x01
#define BPDU_FLAG_PROPOSAL	0x02
#define BPDU_FLAG_AGREEMENT	0x40
#define BPDU_FLAG_LEARNING	0x10
#define BPDU_FLAG_FORWARDING	0x20
#define BPDU_FLAG_TCACK		0x80

#define BPDU_ROLE_ROOT		(0b10 << 2)
#define BPDU_ROLE_DESIGNATED	(0b11 << 2)
#define BPDU_ROLE_MASK		(0b11 << 2)
#define BPDU_ROLE_ALTBACK	(0b01 << 2)

/* Console messages name the port on the front panel, not the internal index. */
static void print_ent(uint8_t e) __reentrant
{
	if (e >= STP_LAG_BASE) {
		write_char('L');
		write_char('1' + e - STP_LAG_BASE);
	} else {
		print_byte(machine.log_to_phys_port[e]);
	}
}


static void print_port_nl(uint8_t port) __reentrant
{
	print_ent(port);
	write_char('\n');
}


static void print_bridge_id(uint8_t prio, uint8_t ext, __xdata uint8_t *mac) __reentrant
{
	print_byte(prio); print_byte(ext); write_char('/');
	for (stp_i = 0; stp_i < 6; stp_i++)
		print_byte(mac[stp_i]);
}


/* Fixed width columns so the rows line up under the header without a
 * formatter. The state indices are the ASIC's own two bits, in the order
 * stp_state_set() writes them. */
static __code const char stp_state_txt[] = "off  blocklearnfwd  ";
static __code const char stp_role_txt[]  = "dis rootdesgaltnback";
static __code const char stp_edge_txt[]  = "no  yes ";

static uint8_t stp_state_get(uint8_t port) __reentrant;
static uint8_t stp_ent_active(uint8_t e) __reentrant;

static void print_field(__code const char *txt, uint8_t idx, uint8_t width) __reentrant
{
	txt += idx * width;
	while (width--)
		write_char(*txt++);
}


void stp_status(void) __banked
{
	if (!stp_enabled) {
		print_string("STP off\n");
		return;
	}
	print_string(stp_rstp ? "STP on, RSTP\n" : "STP on, STP\n");
	print_string("bridge  ");
	print_bridge_id(stp_prio, 0, uip_ethaddr.addr);
	print_string("\nroot    ");
	print_bridge_id(root_bridge.prio, root_bridge.ext, root_bridge.mac);
	if (stp_root_port == 0xff) {
		print_string(" (this switch)\n");
	} else {
		print_string(" port ");
		print_ent(stp_root_port);
		print_string(" cost ");
		print_long(root_bridge_cost);
		write_char('\n');
	}
	print_string("changes ");
	print_short(stp_tc_count);
	write_char('\n');
	print_string("port state role edge tx bpdu\n");
	for (stp_i = 0; stp_i < STP_ENTITIES; stp_i++) {
		if (!stp_ent_active(stp_i))
			continue;
		write_char(' ');
		print_ent(stp_i);
		print_string("  ");
		print_field(stp_state_txt, stp_state_get(stp_i), 5);
		write_char(' ');
		print_field(stp_role_txt, stp_port_role(stp_i), 4);
		write_char(' ');
		print_field(stp_edge_txt, stp_pflags[stp_i] & STP_PF_OPEREDGE ? 1 : 0, 4);
		write_char(' ');
		print_byte((uint8_t)stp_cnt[STP_CNT_TX][stp_i]);
		write_char(' ');
		stp_scratch16 = stp_bpdu_age[stp_i] / STP_HZ;
		itoa(stp_scratch16 > 255 ? 255 : (uint8_t)stp_scratch16);
		write_char('\n');
	}
}


/* Lexicographic compare of n bytes. A MAC is 6 of them; a Bridge Identifier
 * is 8, the two priority octets ahead of the MAC, compared as one unsigned
 * number per 802.1D. */
int8_t cmpBytes(__xdata uint8_t *m1, __xdata uint8_t *m2, uint8_t n) __reentrant
{
	for (uint8_t i = 0; i < n; i++) {
		if (m1[i] == m2[i])
			continue;
		if (m1[i] < m2[i])
			return -1;
		return 1;
	}
	return 0;
}


static int8_t stp_msg_cmp(uint8_t port) __reentrant
{
	stp_cmp = cmpBytes((__xdata uint8_t *)&STP_I->root, (__xdata uint8_t *)&stp_droot[port], 8);
	if (stp_cmp)
		return stp_cmp;
	if (stp_cost_scratch != stp_dcost[port])
		return stp_cost_scratch < stp_dcost[port] ? -1 : 1;
	stp_cmp = cmpBytes((__xdata uint8_t *)&STP_I->bridge, (__xdata uint8_t *)&stp_dbridge[port], 8);
	if (stp_cmp)
		return stp_cmp;
	if (STP_I->port_prio != (uint8_t)(stp_dpid[port] >> 8))
		return STP_I->port_prio < (uint8_t)(stp_dpid[port] >> 8) ? -1 : 1;
	if (STP_I->port_id != (uint8_t)stp_dpid[port])
		return STP_I->port_id < (uint8_t)stp_dpid[port] ? -1 : 1;
	return 0;
}


static uint8_t stp_clamp(uint8_t v, uint8_t lo, uint8_t hi, uint8_t dflt) __reentrant
{
	return (v < lo || v > hi) ? dflt : v;
}


static void stp_rcv_info(uint8_t port) __reentrant
{
	if (STP_I->bpdu_type == BPDU_TYPE_RST
	    && (STP_I->flags & BPDU_ROLE_MASK) != BPDU_ROLE_DESIGNATED)
		return;

	((__xdata uint8_t *)&stp_cost_scratch)[0] = ((__xdata uint8_t *)&STP_I->root_path_cost)[3];
	((__xdata uint8_t *)&stp_cost_scratch)[1] = ((__xdata uint8_t *)&STP_I->root_path_cost)[2];
	((__xdata uint8_t *)&stp_cost_scratch)[2] = ((__xdata uint8_t *)&STP_I->root_path_cost)[1];
	((__xdata uint8_t *)&stp_cost_scratch)[3] = ((__xdata uint8_t *)&STP_I->root_path_cost)[0];

	if (stp_info_while[port]
	    && (cmpBytes(STP_I->bridge.mac, stp_dbridge[port].mac, 6)
		|| STP_I->port_id != (uint8_t)stp_dpid[port])
	    && stp_msg_cmp(port) > 0)
		return;

	if (!stp_info_while[port] || stp_msg_cmp(port))
		stp_alt_agreed &= ~((uint16_t)1 << port);

	stp_dbridge[port].prio = STP_I->bridge.prio;
	stp_dbridge[port].ext = STP_I->bridge.ext;
	memcpy(stp_dbridge[port].mac, STP_I->bridge.mac, 6);
	stp_droot[port].prio = STP_I->root.prio;
	stp_droot[port].ext = STP_I->root.ext;
	memcpy(stp_droot[port].mac, STP_I->root.mac, 6);
	stp_dpid[port] = ((uint16_t)STP_I->port_prio << 8) | STP_I->port_id;
	stp_dcost[port] = stp_cost_scratch;

	stp_rxage[port] = (uint8_t)STP_I->age;
	stp_rxmaxage[port] = stp_clamp((uint8_t)STP_I->age_max, 6, 40, 20);
	stp_rxhello[port] = stp_clamp((uint8_t)STP_I->hello, 1, 10, 2);
	stp_rxfwd[port] = stp_clamp((uint8_t)STP_I->fwd_delay, 4, 30, 15);
	stp_info_while[port] = (uint16_t)stp_rxage[port] + 1 <= stp_rxmaxage[port]
		? (uint16_t)3 * stp_rxhello[port] * STP_HZ : 0;
}




/* Write one port's 2-bit state into the ASIC's MSTP register.
 * 00 disable, 01 blocking, 10 learning, 11 forwarding. */
static uint8_t stp_ent_active(uint8_t e) __reentrant
{
	if (e >= STP_ENTITIES)
		return 0;
	if (e >= STP_LAG_BASE)
		return stp_lag_mask[e - STP_LAG_BASE] != 0;
	if (e < machine.min_port || e > machine.max_port)
		return 0;
	return stp_ent_of[e] == e;
}


uint8_t stp_ent_id(uint8_t e) __banked
{
	if (!stp_ent_active(e))
		return 0;
	if (e >= STP_LAG_BASE)
		return e + 101 - STP_LAG_BASE;
	return machine.log_to_phys_port[e];
}


static uint16_t stp_members(uint8_t e) __reentrant
{
	return e >= STP_LAG_BASE ? stp_lag_mask[e - STP_LAG_BASE] : (uint16_t)1 << e;
}


static uint8_t stp_first(uint16_t mask) __reentrant
{
	for (stp_ss_i = 0; stp_ss_i < STP_PORTS; stp_ss_i++)
		if ((mask >> stp_ss_i) & 1)
			return stp_ss_i;
	return 0xff;
}


static void stp_state_set(uint8_t port, uint8_t state) __reentrant
{
	stp_ss_mask = stp_members(port);
	reg_read_m(RTL837X_MSTP_STATES);
	for (stp_ss_i = 0; stp_ss_i < STP_PORTS; stp_ss_i++) {
		if (!((stp_ss_mask >> stp_ss_i) & 1))
			continue;
		stp_scratch = 3 - (stp_ss_i >> 2);
		sfr_data[stp_scratch] &= ~(uint8_t)(0b11 << ((stp_ss_i << 1) & 0x7));
		sfr_data[stp_scratch] |= (uint8_t)(state << ((stp_ss_i << 1) & 0x7));
	}
	reg_write_m(RTL837X_MSTP_STATES);
}


static uint8_t stp_state_get(uint8_t port) __reentrant
{
	if (stp_first(stp_members(port)) == 0xff)
		return 0;
	reg_read_m(RTL837X_MSTP_STATES);
	return (sfr_data[3 - (stp_ss_i >> 2)] >> ((stp_ss_i << 1) & 0x7)) & 0b11;
}


uint8_t stp_port_state(uint8_t port) __banked
{
	return stp_state_get(port);
}


static void stp_forget(uint8_t e) __reentrant
{
	stp_ss_mask = stp_members(e);
	for (stp_ss_i = 0; stp_ss_i < STP_PORTS; stp_ss_i++)
		if ((stp_ss_mask >> stp_ss_i) & 1)
			port_l2_forget_port(stp_ss_i);
}


static void stp_new_tc_while(uint8_t port) __reentrant
{
	if (stp_tcwhile[port])
		return;
	stp_tcwhile[port] = SEND_RSTP(port) ? ((uint16_t)stp_hello_s + 1) * STP_HZ
				     : (uint16_t)MAXAGE_S * STP_HZ + FWD_TICKS;
	stp_newinfo |= (uint16_t)1 << port;
}


static uint8_t stp_tc_prop(uint8_t from) __reentrant
{
	stp_armed = 0;
	for (stp_k = 0; stp_k < STP_ENTITIES; stp_k++) {
		if (stp_k == from || !stp_ent_active(stp_k) || stp_tcwhile[stp_k] || stp_state_get(stp_k) != 0b11)
			continue;
		if (!(stp_pflags[stp_k] & STP_PF_ENABLED) || (stp_pflags[stp_k] & STP_PF_OPEREDGE))
			continue;
		stp_forget(stp_k);
		stp_new_tc_while(stp_k);
		stp_armed = 1;
	}
	return stp_armed;
}


static void stp_tc_detected(uint8_t port) __reentrant
{
	if (stp_pflags[port] & STP_PF_OPEREDGE)
		return;
	stp_tc_count++;
	stp_new_tc_while(port);
	stp_tc_prop(port);
}


static void stp_forward_now(uint8_t port) __reentrant
{
	port_timers[port] = 0;
	stp_loop_held[port] = 0;
	stp_backup &= ~((uint16_t)1 << port);
	stp_state_set(port, 0b11);
	print_string("STP: rapid transition, port forwarding ");
	print_port_nl(port);
	stp_tc_detected(port);
}


static void stp_sync(uint8_t from) __reentrant
{
	for (stp_k = 0; stp_k < STP_ENTITIES; stp_k++) {
		if (!stp_ent_active(stp_k))
			continue;
		if (stp_k == from || stp_k == stp_root_port || !(stp_pflags[stp_k] & STP_PF_ENABLED)
		    || (stp_pflags[stp_k] & STP_PF_OPEREDGE) || ((stp_alt >> stp_k) & 1)
		    || !((stp_link_prev >> stp_k) & 1))
			continue;
		if (stp_state_get(stp_k) != 0b01) {
			stp_state_set(stp_k, 0b01);
			port_timers[stp_k] = FWD_TICKS;
		}
		stp_newinfo |= (uint16_t)1 << stp_k;
	}
	if (from != stp_root_port)
		return;
	stp_synced_root = from;
	if (port_timers[from])
		stp_forward_now(from);
}


/* Hold one port out of forwarding because a loop was seen on it, and keep
 * holding it for as long as the caller keeps saying so. The caller is the
 * port that won the Port ID compare (see stp_in) - a different port than
 * the one held, except when the frame came back on the port it left.
 */
static void stp_loop_hold_peer(uint8_t port) __reentrant
{
	if (!stp_ent_active(port))
		return;
	if (!(stp_pflags[port] & STP_PF_ENABLED))
		return;
	if (stp_pflags[port] & STP_PF_TRIPPED)
		return;
	if (!port_timers[port]) {
		print_string("STP: loop detected, blocking port ");
		print_port_nl(port);
		stp_state_set(port, 0b01);
		stp_pflags[port] &= ~STP_PF_OPEREDGE;
		stp_forget(port);
		stp_tcwhile[port] = 0;
	}
	stp_loop_held[port] = 1;
	stp_backup |= (uint16_t)1 << port;
	port_timers[port] = FWD_TICKS;
}


/* Take the bridge back as root of its own tree (initial state / root aged out) */
static void stp_claim_root(void)
{
	root_bridge.prio = stp_prio;
	root_bridge.ext = 0x00;
	memcpy(root_bridge.mac, uip_ethaddr.addr, 6);
	root_bridge_cost = 0;
	stp_root_port = 0xff;
	stp_msg_age = 0;
}


static uint8_t stp_info_fresh(uint8_t port) __reentrant
{
	return stp_info_while[port] != 0;
}


static int8_t stp_cmp_root_path(uint8_t a, uint8_t b) __reentrant
{
	stp_cmp = cmpBytes((__xdata uint8_t *)&stp_droot[a], (__xdata uint8_t *)&stp_droot[b], 8);
	if (stp_cmp)
		return stp_cmp;
	if (stp_rpcost[a] != stp_rpcost[b])
		return stp_rpcost[a] < stp_rpcost[b] ? -1 : 1;
	stp_cmp = cmpBytes((__xdata uint8_t *)&stp_dbridge[a], (__xdata uint8_t *)&stp_dbridge[b], 8);
	if (stp_cmp)
		return stp_cmp;
	if (stp_dpid[a] != stp_dpid[b])
		return stp_dpid[a] < stp_dpid[b] ? -1 : 1;
	if (stp_pprio[a] != stp_pprio[b])
		return stp_pprio[a] < stp_pprio[b] ? -1 : 1;
	return a < b ? -1 : 1;
}


static int8_t stp_cmp_designated(uint8_t port) __reentrant
{
	stp_cmp = cmpBytes((__xdata uint8_t *)&stp_droot[port], (__xdata uint8_t *)&root_bridge, 8);
	if (stp_cmp)
		return stp_cmp;
	if (stp_dcost[port] != root_bridge_cost)
		return stp_dcost[port] < root_bridge_cost ? -1 : 1;
	stp_self.prio = stp_prio;
	stp_self.ext = 0x00;
	memcpy(stp_self.mac, uip_ethaddr.addr, 6);
	stp_cmp = cmpBytes((__xdata uint8_t *)&stp_dbridge[port], (__xdata uint8_t *)&stp_self, 8);
	if (stp_cmp)
		return stp_cmp;
	if (stp_dpid[port] == (((uint16_t)stp_pprio[port] << 8) | (port + 1)))
		return 0;
	return stp_dpid[port] < (((uint16_t)stp_pprio[port] << 8) | (port + 1)) ? -1 : 1;
}


static void stp_reroot_tree(void) __reentrant
{
	for (stp_k = 0; stp_k < STP_ENTITIES; stp_k++) {
		if (!stp_ent_active(stp_k))
			continue;
		if (stp_k == stp_root_port || !stp_rrwhile[stp_k]
		    || !(stp_pflags[stp_k] & STP_PF_ENABLED) || (stp_pflags[stp_k] & STP_PF_OPEREDGE))
			continue;
		stp_reroot |= (uint16_t)1 << stp_k;
		if (stp_state_get(stp_k) != 0b01) {
			stp_state_set(stp_k, 0b01);
			port_timers[stp_k] = FWD_TICKS;
			stp_newinfo |= (uint16_t)1 << stp_k;
		}
	}
}


static void stp_reselect(void)
{
	stp_self.prio = stp_prio;
	stp_self.ext = 0x00;
	memcpy(stp_self.mac, uip_ethaddr.addr, 6);

	stp_best = 0xff;
	for (stp_j = 0; stp_j < STP_ENTITIES; stp_j++) {
		if (!stp_ent_active(stp_j))
			continue;
		if (!(stp_pflags[stp_j] & STP_PF_ENABLED) || (stp_pflags[stp_j] & STP_PF_ROOTGUARD))
			continue;
		if (!stp_info_fresh(stp_j))
			continue;
		if (!cmpBytes(stp_dbridge[stp_j].mac, uip_ethaddr.addr, 6))
			continue;
		if (cmpBytes((__xdata uint8_t *)&stp_droot[stp_j], (__xdata uint8_t *)&stp_self, 8) >= 0)
			continue;
		stp_rpcost[stp_j] = stp_dcost[stp_j] + PCOST(stp_j);
		if (stp_best == 0xff || stp_cmp_root_path(stp_j, stp_best) < 0)
			stp_best = stp_j;
	}

	if (stp_best != 0xff) {
		if (stp_root_port != stp_best
		    || cmpBytes((__xdata uint8_t *)&root_bridge, (__xdata uint8_t *)&stp_droot[stp_best], 8)) {
			if (cmpBytes((__xdata uint8_t *)&root_bridge, (__xdata uint8_t *)&stp_droot[stp_best], 8))
				print_string("Updating Root bridge\n");
			stp_synced_root = 0xff;
			stp_root_port = stp_best;
			root_bridge.prio = stp_droot[stp_best].prio;
			root_bridge.ext = stp_droot[stp_best].ext;
			memcpy(root_bridge.mac, stp_droot[stp_best].mac, 6);
			stp_tc_count++;
		}
		root_bridge_cost = stp_rpcost[stp_best];
		stp_msg_age = stp_rxage[stp_best];
		stp_root_maxage = stp_rxmaxage[stp_best];
		stp_root_fwd = stp_rxfwd[stp_best];
	} else if (stp_root_port != 0xff) {
		print_string("STP: root aged out, claiming root\n");
		stp_synced_root = 0xff;
		stp_claim_root();
		stp_tc_count++;
	}

	for (stp_j = 0; stp_j < STP_ENTITIES; stp_j++) {
		if (!stp_ent_active(stp_j))
			continue;
		if (!(stp_pflags[stp_j] & STP_PF_ENABLED))
			continue;
		if (stp_j != stp_root_port && stp_info_fresh(stp_j)
		    && stp_cmp_designated(stp_j) < 0) {
			stp_pflags[stp_j] &= ~STP_PF_OPEREDGE;
			port_timers[stp_j] = 0;
			stp_state_set(stp_j, 0b01);
			if (!((stp_alt >> stp_j) & 1)) {
				stp_alt |= (uint16_t)1 << stp_j;
				stp_forget(stp_j);
				stp_tcwhile[stp_j] = 0;
				print_string("STP: better bridge on the segment, blocking port ");
				print_port_nl(stp_j);
			}
			continue;
		}
		if (!((stp_alt >> stp_j) & 1))
			continue;
		stp_alt &= ~((uint16_t)1 << stp_j);
		stp_alt_agreed &= ~((uint16_t)1 << stp_j);
		port_timers[stp_j] = FWD_TICKS;
		print_string("STP: port released, listening ");
		print_port_nl(stp_j);
	}

	if (stp_rstp && stp_root_port != 0xff && port_timers[stp_root_port]
	    && !((stp_alt >> stp_root_port) & 1) && !stp_rbwhile[stp_root_port]) {
		stp_reroot_tree();
		stp_forward_now(stp_root_port);
	}
}


void stp_cnf_send(uint8_t port) __reentrant
{
	/* A one-shot flag (TCA) belongs to the BPDU we were asked to send: drop
	 * it with the frame, or it would surface on an unrelated port later. */
	if (!(stp_pflags[port] & STP_PF_ENABLED) || (stp_pflags[port] & (STP_PF_FILTER | STP_PF_TRIPPED))
	    || !((stp_link_prev >> port) & 1) || !(stp_members(port) & stp_link_phys)) {
		stp_tx_flags_extra = 0;
		return;
	}
	if (!stp_tx_budget[port]) {	/* tx hold count exhausted for this second */
		stp_tx_flags_extra = 0;
		return;
	}
	stp_tx_budget[port]--;
	stp_cnt[STP_CNT_TX][port]++;
	stp_tcn = !SEND_RSTP(port) && port == stp_root_port;

	STP_O->stp_addr[0] = 0x01; STP_O->stp_addr[1] = 0x80; STP_O->stp_addr[2] = 0xc2;
	STP_O->stp_addr[3] = STP_O->stp_addr[4] = STP_O->stp_addr[5] = 0x00;

	STP_O->rtl_tag.tag = HTONS(RTL_FRAME_TAG_ID);
	STP_O->rtl_tag.version = RTL_FRAME_TAG_VERSION;
	STP_O->rtl_tag.reason = 0x00;
	STP_O->rtl_tag.flags = HTONS(RTL_TAG_LEARN_DIS);
	STP_O->rtl_tag.pmask = HTONS(((uint16_t)1) << stp_first(stp_members(port) & stp_link_phys));

	STP_O->dsap = 0x42;
	STP_O->ssap = 0x42;
	STP_O->ctrl = 0x03;
	STP_O->proto = 0x0000;
	if (stp_tcn) {
		STP_O->msg_len = HTONS(BPDU_LEN_TCN);
		STP_O->version = BPDU_VER_STP;
		STP_O->bpdu_type = BPDU_TYPE_TCN;
		STP_O->flags = 0x00;
	} else if (SEND_RSTP(port)) {
		STP_O->msg_len = HTONS(BPDU_LEN_RST);
		STP_O->version = BPDU_VER_RSTP;
		STP_O->bpdu_type = BPDU_TYPE_RST;
		STP_O->flags = port == stp_root_port ? BPDU_ROLE_ROOT
			     : (((stp_alt | stp_backup) >> port) & 1) ? BPDU_ROLE_ALTBACK : BPDU_ROLE_DESIGNATED;
		stp_scratch = stp_state_get(port);
		if (stp_scratch == 0b11)
			STP_O->flags |= BPDU_FLAG_LEARNING | BPDU_FLAG_FORWARDING;
		else if (stp_scratch == 0b10)
			STP_O->flags |= BPDU_FLAG_LEARNING;
		if (port != stp_root_port && port_timers[port] && P2P(port)
		    && !(stp_pflags[port] & STP_PF_OPEREDGE) && !(((stp_alt | stp_backup) >> port) & 1))
			STP_O->flags |= BPDU_FLAG_PROPOSAL;
		if ((stp_agree >> port) & 1) {
			STP_O->flags |= BPDU_FLAG_AGREEMENT;
			stp_agree &= ~((uint16_t)1 << port);
		}
	} else {
		STP_O->msg_len = HTONS(BPDU_LEN_CONFIG);
		STP_O->version = BPDU_VER_STP;
		STP_O->bpdu_type = BPDU_TYPE_CONFIG;
		STP_O->flags = 0x00;
	}
	if (stp_tcwhile[port])
		STP_O->flags |= BPDU_FLAG_TC;
	STP_O->flags |= stp_tx_flags_extra;
	stp_tx_flags_extra = 0;
	if (stp_tcn || (STP_O->flags & BPDU_FLAG_TC))
		stp_cnt[STP_CNT_TCTX][port]++;

	memcpy(STP_O->src_addr, uip_ethaddr.addr, 6);
	STP_O->src_addr[0] |= 0x02;
	STP_O->src_addr[5] = (uip_ethaddr.addr[5] & 0xf0) | port;
	memcpy(STP_O->root.mac, root_bridge.mac, 6);
	memcpy(STP_O->bridge.mac, uip_ethaddr.addr, 6);

	STP_O->root.prio = root_bridge.prio;
	STP_O->root.ext = root_bridge.ext;
	/* Our root path cost, big-endian (0 while we are the root ourselves) */
	STP_O->root_path_cost = ((root_bridge_cost & 0xff) << 24)
	                      | ((root_bridge_cost & 0xff00) << 8)
	                      | ((root_bridge_cost >> 8) & 0xff00)
	                      | (root_bridge_cost >> 24);

	STP_O->bridge.prio = stp_prio;
	STP_O->bridge.ext = 0x00;

	STP_O->port_prio = stp_pprio[port];
	STP_O->port_id = port + 1;
	/* Message age, incremented by one second per bridge we relay through.
	 * The timer fields are in 1/256 s on the wire, and sdcc stores uint16
	 * little-endian, so assigning the plain second count lands the value in
	 * the high (seconds) octet - see age_max/hello/fwd_delay below. */
	STP_O->age = (stp_root_port == 0xff) ? 0 : (uint16_t)(stp_msg_age + 1);
	STP_O->age_max = MAXAGE_S;
	STP_O->hello = stp_hello_s;
	STP_O->fwd_delay = FWD_S;
	STP_O->version1_length = 0;	/* RST BPDU: no version-1 information */

	uip_len = stp_tcn ? sizeof(struct stp_pkt) - 32
			  : (SEND_RSTP(port) ? sizeof(struct stp_pkt) : sizeof(struct stp_pkt) - 1);
	tcpip_output();
}


static uint8_t stp_len_ok(void) __reentrant
{
	if (STP_I->bpdu_type == BPDU_TYPE_TCN)
		return HTONS(STP_I->msg_len) >= BPDU_LEN_TCN;
	if (STP_I->bpdu_type == BPDU_TYPE_RST)
		return HTONS(STP_I->msg_len) >= BPDU_LEN_RST;
	return HTONS(STP_I->msg_len) >= BPDU_LEN_CONFIG;
}


static void stp_rx_seen(uint8_t port) __reentrant
{
	stp_bpdu_age[port] = 0;
	stp_cnt[STP_CNT_RX][port]++;
	if (stp_len_ok() && (STP_I->bpdu_type == BPDU_TYPE_TCN || (STP_I->flags & BPDU_FLAG_TC)))
		stp_cnt[STP_CNT_TCRX][port]++;
	if (!stp_rstp)
		return;
	if (STP_I->version < BPDU_VER_RSTP)
		stp_seen_stp |= (uint16_t)1 << port;
	else
		stp_seen_rstp |= (uint16_t)1 << port;
}


static void stp_migrate_check(uint8_t port) __reentrant
{
	if (stp_mdelay[port]) {
		stp_mdelay[port]--;
		return;
	}
	if (!((stp_legacy >> port) & 1) && ((stp_seen_stp >> port) & 1)) {
		stp_legacy |= (uint16_t)1 << port;
		print_string("STP: 802.1D neighbour, sending STP on port ");
	} else if (((stp_legacy >> port) & 1) && ((stp_seen_rstp >> port) & 1)) {
		stp_legacy &= ~((uint16_t)1 << port);
		print_string("STP: RSTP neighbour, sending RSTP on port ");
	} else {
		return;
	}
	print_port_nl(port);
	stp_seen_stp &= ~((uint16_t)1 << port);
	stp_seen_rstp &= ~((uint16_t)1 << port);
	stp_mdelay[port] = STP_MIGRATE;
	stp_newinfo |= (uint16_t)1 << port;
}


static int8_t stp_msg_vs_ours(uint8_t port) __reentrant
{
	stp_cmp = cmpBytes((__xdata uint8_t *)&STP_I->root, (__xdata uint8_t *)&root_bridge, 8);
	if (stp_cmp)
		return stp_cmp;
	((__xdata uint8_t *)&stp_cost_scratch)[0] = ((__xdata uint8_t *)&STP_I->root_path_cost)[3];
	((__xdata uint8_t *)&stp_cost_scratch)[1] = ((__xdata uint8_t *)&STP_I->root_path_cost)[2];
	((__xdata uint8_t *)&stp_cost_scratch)[2] = ((__xdata uint8_t *)&STP_I->root_path_cost)[1];
	((__xdata uint8_t *)&stp_cost_scratch)[3] = ((__xdata uint8_t *)&STP_I->root_path_cost)[0];
	if (stp_cost_scratch != root_bridge_cost)
		return stp_cost_scratch < root_bridge_cost ? -1 : 1;
	stp_self.prio = stp_prio;
	stp_self.ext = 0x00;
	memcpy(stp_self.mac, uip_ethaddr.addr, 6);
	stp_cmp = cmpBytes((__xdata uint8_t *)&STP_I->bridge, (__xdata uint8_t *)&stp_self, 8);
	if (stp_cmp)
		return stp_cmp;
	if (STP_I->port_prio != stp_pprio[port])
		return STP_I->port_prio < stp_pprio[port] ? -1 : 1;
	if (STP_I->port_id != port + 1)
		return STP_I->port_id < port + 1 ? -1 : 1;
	return 0;
}


static void stp_dispute_rx(uint8_t port) __reentrant
{
	if (port == stp_root_port || ((stp_alt >> port) & 1))
		return;
	if (STP_I->bpdu_type == BPDU_TYPE_RST
	    && (STP_I->flags & BPDU_ROLE_MASK) != BPDU_ROLE_DESIGNATED)
		return;
	if (stp_msg_vs_ours(port) <= 0)
		return;
	stp_newinfo |= (uint16_t)1 << port;
	if (!stp_rstp || STP_I->bpdu_type != BPDU_TYPE_RST
	    || !(STP_I->flags & BPDU_FLAG_LEARNING))
		return;
	if (stp_state_get(port) != 0b01) {
		stp_state_set(port, 0b01);
		print_string("STP: dispute, port discarding ");
		print_port_nl(port);
	}
	port_timers[port] = FWD_TICKS;
}


static void stp_rapid_rx(uint8_t port) __reentrant
{
	if (stp_rstp && STP_I->bpdu_type == BPDU_TYPE_RST && P2P(port)) {
		if (port == stp_root_port
		    && (STP_I->flags & BPDU_ROLE_MASK) == BPDU_ROLE_DESIGNATED
		    && (STP_I->flags & BPDU_FLAG_PROPOSAL)) {
			if (stp_synced_root != port)
				stp_sync(port);
			stp_agree |= (uint16_t)1 << port;
			stp_newinfo |= (uint16_t)1 << port;
		} else if ((((stp_alt | stp_backup) >> port) & 1)
			   && (STP_I->flags & BPDU_ROLE_MASK) == BPDU_ROLE_DESIGNATED
			   && (STP_I->flags & BPDU_FLAG_PROPOSAL)) {
			if (!((stp_alt_agreed >> port) & 1))
				stp_sync(port);
			stp_alt_agreed |= (uint16_t)1 << port;
			stp_agree |= (uint16_t)1 << port;
			stp_newinfo |= (uint16_t)1 << port;
		} else if (port != stp_root_port && port_timers[port]
			   && !((stp_alt >> port) & 1)
			   && !(((stp_reroot >> port) & 1) && stp_rrwhile[port])
			   && ((STP_I->flags & BPDU_ROLE_MASK) == BPDU_ROLE_ROOT
			       || (STP_I->flags & BPDU_ROLE_MASK) == BPDU_ROLE_ALTBACK)
			   && (STP_I->flags & BPDU_FLAG_AGREEMENT)
			   && !cmpBytes((__xdata uint8_t *)&STP_I->root, (__xdata uint8_t *)&root_bridge, 8)) {
			stp_forward_now(port);
		}
	}
}


void stp_in(void) __banked
{
	uint8_t port;

	if (uip_len < BPDU_LEN_MIN_HEADER) {
		uip_len = 0;
		return;
	}
	stp_rxlen = uip_len;

	// By default we do not send anything out
	uip_len = 0;

	/* Ingress port: low nibble of the CPU tag's pmask on RX */
	stp_scratch = ((uint8_t)HTONS(STP_I->rtl_tag.pmask)) & 0x0f;
	if (stp_scratch < machine.min_port || stp_scratch > machine.max_port)
		return;
	port = stp_ent_of[stp_scratch];

	// Make sure this is the type of (R)STP packet we are interested in:
	if (!(STP_I->dsap == 0x42 && STP_I->ssap == 0x42 && STP_I->ctrl == 0x03))
		return;
	if (STP_I->proto)
		return;
	if (!((STP_I->version >= BPDU_VER_RSTP && STP_I->bpdu_type == BPDU_TYPE_RST)
	      || (STP_I->version == BPDU_VER_STP
	          && (STP_I->bpdu_type == BPDU_TYPE_CONFIG
	              || STP_I->bpdu_type == BPDU_TYPE_TCN))))
		return;

	if (!(stp_pflags[port] & STP_PF_ENABLED) || (stp_pflags[port] & STP_PF_FILTER))
		return;

	/* BPDU guard: an edge-facing port must never see a BPDU - shut it down. */
	if (stp_pflags[port] & STP_PF_BPDUGUARD) {
		print_string("STP: BPDU guard tripped, disabling port ");
		print_port_nl(port);
		stp_pflags[port] |= STP_PF_TRIPPED;
		stp_state_set(port, 0b00);
		stp_tc_count++;
		return;
	}

	stp_rx_seen(port);

	/* A port that hears a BPDU is not an edge port, whatever it decided
	 * during the silence after the link came up. Only the flag is dropped:
	 * the port keeps whatever forwarding state the rules below give it,
	 * rather than being pushed back through the listen period, which would
	 * black-hole a working link for a forward delay on the first BPDU. The
	 * flag matters beyond the status page, since topology changes skip
	 * edge ports and would go on skipping the counter and the L2 flush for
	 * a port that has a bridge behind it. */
	stp_pflags[port] &= ~STP_PF_OPEREDGE;
	stp_heard |= (uint16_t)1 << port;

	if (!stp_len_ok())
		return;

	if (STP_I->bpdu_type == BPDU_TYPE_TCN) {
		stp_tx_flags_extra = BPDU_FLAG_TCACK;
		stp_cnf_send(port);
		uip_len = 0;
		stp_tc_count++;
		stp_new_tc_while(port);
		stp_tc_prop(port);
		return;
	}

	/* Everything below reads the full Config/RST body. */
	if (stp_rxlen < 64)
		return;

	/* Our own BPDU coming back: two of our ports sit on one segment. Only
	 * the one with the worse Port ID stops forwarding, and only the other
	 * one writes that state, so the two never race each other.
	 */
	if (cmpBytes(STP_I->bridge.mac, uip_ethaddr.addr, 6) == 0) {
		/* Equal means the frame came back on the port it left: a loop
		 * further out, behind an unmanaged switch. There is no pair to
		 * pick from, so that port holds itself down - and since it can
		 * only re-arm while it is receiving, that case degrades to the
		 * forward-delay pulse we had before rather than a real latch.
		 * The peer's number is validated by the callee, not here. */
		stp_loop_peer = STP_I->port_id;		/* 1-based, as we send it */
		if (!stp_loop_peer)
			return;
		stp_loop_peer--;
		/* A Port ID is (priority, number) and priority is compared
		 * first - stp_cnf_send() puts stp_pprio[] on the wire next to
		 * the number, so "stp port N prio" has to be able to decide
		 * which end of a looped pair keeps forwarding. Comparing the
		 * number alone would quietly ignore it. */
		if (STP_I->port_prio != stp_pprio[port]) {
			if (STP_I->port_prio < stp_pprio[port])
				return;			/* peer is better: it decides */
		} else if (stp_loop_peer < port) {
			return;
		}
		stp_loop_hold_peer(stp_loop_peer);
		return;
	}

	if ((STP_I->flags & BPDU_FLAG_TCACK) && port == stp_root_port)
		stp_tcwhile[port] = 0;
	if ((STP_I->flags & BPDU_FLAG_TC) && !((stp_alt >> port) & 1) && stp_tc_prop(port))
		stp_tc_count++;

	if ((stp_pflags[port] & STP_PF_ROOTGUARD)
	    && cmpBytes((__xdata uint8_t *)&STP_I->root, (__xdata uint8_t *)&root_bridge, 8) < 0) {
		print_string("STP: root guard blocking port ");
		print_port_nl(port);
		stp_state_set(port, 0b01);
		port_timers[port] = FWD_TICKS;
		stp_pflags[port] &= ~STP_PF_OPEREDGE;
		return;
	}

	stp_rcv_info(port);
	stp_reselect();

	stp_rapid_rx(port);
	stp_dispute_rx(port);
}


static void stp_tc_clock(void) __reentrant
{
	if (stp_tc_seen != stp_tc_count) {
		stp_tc_seen = stp_tc_count;
		stp_tc_secs = 0;
	} else if (stp_tc_secs != 0xffffffffUL) {
		stp_tc_secs++;
	}
}


void stp_lag_map(void) __banked
{
	for (stp_ss_i = 0; stp_ss_i < STP_PORTS; stp_ss_i++)
		stp_ent_of[stp_ss_i] = stp_ss_i;
	for (stp_lag = 0; stp_lag < STP_LAG_COUNT; stp_lag++) {
		stp_ss_mask = port_lag_members_get(stp_lag);
		if (stp_ss_mask != stp_lag_mask[stp_lag]) {
			stp_map_changed |= stp_ss_mask | stp_lag_mask[stp_lag]
					 | (uint16_t)1 << (STP_LAG_BASE + stp_lag);
			stp_lag_mask[stp_lag] = stp_ss_mask;
		}
		for (stp_ss_i = 0; stp_ss_i < STP_PORTS; stp_ss_i++)
			if ((stp_ss_mask >> stp_ss_i) & 1)
				stp_ent_of[stp_ss_i] = STP_LAG_BASE + stp_lag;
	}
}


static void stp_links_read(void) __reentrant
{
	reg_read_m(RTL837X_REG_LINKS_STS);
	stp_link_phys = (uint16_t)sfr_data[1] | ((uint16_t)sfr_data[2] << 8);
	stp_link_now = 0;
	for (stp_k = 0; stp_k < STP_ENTITIES; stp_k++) {
		if (!stp_ent_active(stp_k) || stp_first(stp_members(stp_k) & stp_link_phys) == 0xff)
			continue;
		stp_link_now |= (uint16_t)1 << stp_k;
		if (stp_k >= STP_LAG_BASE)
			stp_pspeed[stp_k] = stp_pspeed[stp_ss_i];
	}
}


static void stp_ent_reset(uint8_t e) __reentrant
{
	stp_ent_bit = (uint16_t)1 << e;
	stp_alt &= ~stp_ent_bit;
	stp_backup &= ~stp_ent_bit;
	stp_alt_agreed &= ~stp_ent_bit;
	stp_legacy &= ~stp_ent_bit;
	stp_seen_stp &= ~stp_ent_bit;
	stp_seen_rstp &= ~stp_ent_bit;
	stp_agree &= ~stp_ent_bit;
	stp_reroot &= ~stp_ent_bit;
	stp_heard &= ~stp_ent_bit;
	stp_newinfo &= ~stp_ent_bit;
	stp_link_prev = (stp_link_prev & ~stp_ent_bit) | (stp_link_now & stp_ent_bit);
	stp_pflags[e] &= ~(STP_PF_OPEREDGE | STP_PF_TRIPPED);
	stp_loop_held[e] = 0;
	stp_bpdu_age[e] = 0;
	stp_tx_budget[e] = stp_txhold;
	stp_info_while[e] = 0;
	stp_tcwhile[e] = 0;
	stp_mdelay[e] = STP_MIGRATE;
	stp_rrwhile[e] = 0;
	stp_rbwhile[e] = 0;
	port_timers[e] = 0;
	port_hello[e] = (uint16_t)stp_hello_s * STP_HZ;
	if (!stp_ent_active(e))
		return;
	if (!(stp_pflags[e] & STP_PF_ENABLED) || (stp_pflags[e] & STP_PF_ADMEDGE)) {
		if (stp_pflags[e] & STP_PF_ADMEDGE)
			stp_pflags[e] |= STP_PF_OPEREDGE;
		stp_state_set(e, 0b11);
	} else {
		stp_state_set(e, 0b01);
		port_timers[e] = FWD_TICKS;
	}
}


void stp_timers(void) __banked
{
	/* Refill the per-port tx budgets once per second (tx hold count) */
	if (++stp_sec_tick >= STP_HZ) {
		stp_sec_tick = 0;
		for (stp_i = 0; stp_i < STP_ENTITIES; stp_i++)
			stp_tx_budget[stp_i] = stp_txhold;
		stp_tc_clock();

		/* Link supervision. Without this the state machine never learns
		 * that a port lost carrier: it keeps the port in forwarding, keeps
		 * announcing on it, and never flushes what was learned behind it -
		 * yet losing a link is the most ordinary topology change there is.
		 * Once per second is soon enough, and it keeps register reads out
		 * of the 50 Hz tick. */
		reg_read_m(RTL837X_REG_LINKS);
		for (stp_i = machine.min_port; stp_i <= machine.max_port; stp_i++) {
			if (stp_i == 8)
				reg_read_m(RTL837X_REG_LINKS_89);
			stp_pspeed[stp_i] = (stp_i & 1)
				? (sfr_data[3 - ((stp_i & 7) >> 1)] >> 4)
				: (sfr_data[3 - ((stp_i & 7) >> 1)] & 0xf);
		}
		stp_lag_map();
		stp_links_read();
		if (stp_map_changed) {
			for (stp_i = 0; stp_i < STP_ENTITIES; stp_i++)
				if ((stp_map_changed >> stp_i) & 1)
					stp_ent_reset(stp_i);
			stp_map_changed = 0;
		}

		if (stp_link_now != stp_link_prev) {
			for (stp_i = 0; stp_i < STP_ENTITIES; stp_i++) {
				if (!stp_ent_active(stp_i))
					continue;
				if (!(stp_pflags[stp_i] & STP_PF_ENABLED))
					continue;
				if (!((stp_link_now ^ stp_link_prev) >> stp_i & 1))
					continue;
				/* Either way the port must stop forwarding first. */
				stp_state_set(stp_i, 0b01);
				if ((stp_link_now >> stp_i) & 1) {
					/* Carrier back: re-run the listen period rather than
					 * forwarding straight away - the segment may have been
					 * rewired while we were down. Auto edge still applies. */
					port_timers[stp_i] = FWD_TICKS;
					stp_pflags[stp_i] &= ~STP_PF_OPEREDGE;
					stp_alt &= ~((uint16_t)1 << stp_i);
					stp_bpdu_age[stp_i] = 0;
					stp_newinfo |= (uint16_t)1 << stp_i;
					stp_legacy &= ~((uint16_t)1 << stp_i);
					stp_seen_stp &= ~((uint16_t)1 << stp_i);
					stp_seen_rstp &= ~((uint16_t)1 << stp_i);
					stp_mdelay[stp_i] = STP_MIGRATE;
				} else {
					port_timers[stp_i] = 0;
					stp_info_while[stp_i] = 0;
					stp_rrwhile[stp_i] = 0;
					stp_reroot &= ~((uint16_t)1 << stp_i);
					stp_backup &= ~((uint16_t)1 << stp_i);
					stp_alt_agreed &= ~((uint16_t)1 << stp_i);
					stp_heard &= ~((uint16_t)1 << stp_i);
					stp_alt &= ~((uint16_t)1 << stp_i);
					print_string("STP: link down, port blocking ");
					print_port_nl(stp_i);
					stp_forget(stp_i);
					stp_tcwhile[stp_i] = 0;
				}
			}
			stp_link_prev = stp_link_now;
		}
		stp_reselect();
	}

	for (stp_i = 0; stp_i < STP_ENTITIES; stp_i++) {
		if (!stp_ent_active(stp_i) || !(stp_pflags[stp_i] & STP_PF_ENABLED))
			continue;

		if (stp_bpdu_age[stp_i] < 0xffff)
			stp_bpdu_age[stp_i]++;
		if (stp_info_while[stp_i] && !--stp_info_while[stp_i])
			stp_reselect_due = 1;
		if (stp_tcwhile[stp_i])
			stp_tcwhile[stp_i]--;
		if (stp_i == stp_root_port)
			stp_rrwhile[stp_i] = FWD_TICKS;
		else if (stp_rrwhile[stp_i] && !--stp_rrwhile[stp_i])
			stp_reroot &= ~((uint16_t)1 << stp_i);
		if ((stp_backup >> stp_i) & 1)
			stp_rbwhile[stp_i] = (uint16_t)2 * stp_hello_s * STP_HZ;
		else if (stp_rbwhile[stp_i])
			stp_rbwhile[stp_i]--;
		stp_migrate_check(stp_i);

		/* Periodic hello */
		if (port_hello[stp_i])
			port_hello[stp_i]--;
		if (!port_hello[stp_i]) {
			port_hello[stp_i] = (uint16_t)stp_hello_s * STP_HZ;
			/* Only designated ports announce periodically: the root port is
			 * where our own root information comes FROM, and echoing it back
			 * there just feeds the upstream bridge its own data (and looks
			 * like a competing designated bridge on that segment). */
			if (((stp_agree >> stp_i) & 1) || (!((stp_alt >> stp_i) & 1) && (stp_i != stp_root_port || stp_tcwhile[stp_i])))
				stp_cnf_send(stp_i);
		}
		if ((stp_newinfo >> stp_i) & 1) {
			stp_newinfo &= ~((uint16_t)1 << stp_i);
			if (((stp_agree >> stp_i) & 1) || (!((stp_alt >> stp_i) & 1) && (stp_i != stp_root_port || stp_tcwhile[stp_i])))
				stp_cnf_send(stp_i);
		}

		/* Promote a port out of blocking once its listen period expires
		 * with no reason to stay blocked (no better root heard: we are
		 * the designated bridge on that port). */
		if (port_timers[stp_i] && ((stp_alt >> stp_i) & 1))
			port_timers[stp_i] = 0;

		if (port_timers[stp_i]) {
			if (!--port_timers[stp_i]) {
				if (((stp_reroot >> stp_i) & 1) && stp_rrwhile[stp_i]) {
					port_timers[stp_i] = stp_rrwhile[stp_i] + 1;
				} else if (stp_state_get(stp_i) == 0b01) {
					stp_state_set(stp_i, 0b10);
					port_timers[stp_i] = FWD_TICKS;
					print_string("STP: port learning ");
					print_port_nl(stp_i);
				} else {
					stp_loop_held[stp_i] = 0;
					stp_backup &= ~((uint16_t)1 << stp_i);
					stp_state_set(stp_i, 0b11);
					print_string("STP: port forwarding ");
					print_port_nl(stp_i);
					stp_tc_detected(stp_i);
				}
			} else if ((stp_pflags[stp_i] & STP_PF_AUTOEDGE)
			           && !stp_loop_held[stp_i]
			           && !((stp_heard >> stp_i) & 1)
			           && stp_bpdu_age[stp_i] > STP_EDGE_DELAY) {
				/* Auto edge: nothing talks (R)STP on this port - it is
				 * host-facing, go to forwarding without the full wait. */
				port_timers[stp_i] = 0;
				stp_pflags[stp_i] |= STP_PF_OPEREDGE;
				stp_state_set(stp_i, 0b11);
				print_string("STP: edge port forwarding ");
				print_port_nl(stp_i);
			}
		}
	}

	if (stp_reselect_due) {
		stp_reselect_due = 0;
		stp_reselect();
	}
}


/* Reset all configuration to the 802.1D/802.1w defaults. Called once at boot
 * (before the startup config replays "stp ..." commands over it). */
void stp_defaults(void) __banked
{
	stp_prio = 0x80;	/* high byte of the priority: 0x8000 is 32768 */
	stp_hello_s = 2;
	stp_maxage_s = 20;
	stp_fwddelay_s = 15;
	stp_rstp = 1;
	stp_txhold = 6;
	stp_pcost_short = 0;
	stp_bpdu_filter = 0;
	for (stp_i = 0; stp_i < STP_PORTS; stp_i++)
		stp_ent_of[stp_i] = stp_i;
	for (stp_i = 0; stp_i < STP_LAG_COUNT; stp_i++)
		stp_lag_mask[stp_i] = 0;
	stp_map_changed = 0;
	for (stp_i = 0; stp_i < STP_ENTITIES; stp_i++) {
		/* enabled, auto-edge on: host-facing ports go forwarding after
		 * 3 s of BPDU silence instead of the full forward delay */
		stp_pflags[stp_i] = STP_PF_ENABLED | STP_PF_AUTOEDGE;
		stp_pcost[stp_i] = 0;	/* auto */
		stp_pprio[stp_i] = 0x80;
		stp_bpdu_age[stp_i] = 0;
		port_timers[stp_i] = 0;
		port_hello[stp_i] = 0;
		stp_tx_budget[stp_i] = 6;
		stp_info_while[stp_i] = 0;
		stp_tcwhile[stp_i] = 0;
		stp_mdelay[stp_i] = 0;
		stp_rrwhile[stp_i] = 0;
		stp_rbwhile[stp_i] = 0;
	}
	stp_reroot = 0;
	stp_newinfo = 0;
	stp_legacy = 0;
	stp_seen_stp = 0;
	stp_seen_rstp = 0;
	stp_agree = 0;
	stp_synced_root = 0xff;
	stp_backup = 0;
	stp_alt_agreed = 0;
	stp_heard = 0;
	stp_alt = 0;
	stp_tc_count = 0;
	stp_claim_root();
}


/*
 * Steer BPDUs while STP runs, and restore flooding when it stops.
 * Changing a port's PVID while STP runs needs "stp off" then "stp on".
 */
static void stp_fdb_update(__xdata uint16_t pmask)
{
	uint16_t stp_fdb_vid;
	uint8_t  stp_fdb_i;

	/* Unlike LACPDUs (always untagged, so per-PVID entries suffice), BPDUs
	 * can arrive VLAN-tagged and then classify into the tag's VID - cover
	 * every VLAN that exists in the VLAN table, plus every port's PVID for
	 * the untagged case. A duplicate VID just overwrites the same slot. */
	for (stp_fdb_vid = 1; stp_fdb_vid < 4095; stp_fdb_vid++) {
		if (vlan_get(stp_fdb_vid) < 0)
			continue;
		if (!(sfr_data[0] & 0x02))	/* bit 1: VLAN table entry valid */
			continue;
		port_l2mc_set(0x00, stp_fdb_vid, pmask);
	}
	for (stp_fdb_i = machine.min_port; stp_fdb_i <= machine.max_port; stp_fdb_i++) {
		stp_fdb_vid = port_pvid_get(stp_fdb_i);
		port_l2mc_set(0x00, stp_fdb_vid, pmask);
	}
}


static void stp_times_check(void) __reentrant
{
	if ((uint16_t)2 * (stp_fwddelay_s - 1) >= stp_maxage_s
	    && stp_maxage_s >= (uint16_t)2 * (stp_hello_s + 1))
		return;
	print_string("STP: timers break 2*(fwd-1) >= maxage >= 2*(hello+1), using 2/20/15\n");
	stp_hello_s = 2;
	stp_maxage_s = 20;
	stp_fwddelay_s = 15;
}


void stp_counters_clear(void) __banked __reentrant
{
	for (uint8_t i = 0; i < STP_CNT_N * STP_ENTITIES; i++)
		((__xdata uint32_t *)stp_cnt)[i] = 0;
}


void stp_setup(void) __banked
{
	stp_times_check();
	stp_counters_clear();
	stp_tc_seen = stp_tc_count;
	stp_tc_secs = 0;
	print_string("Enabling STP: ");
	stp_claim_root();
	stp_newinfo = 0;
	stp_reroot = 0;
	stp_legacy = 0;
	stp_seen_stp = 0;
	stp_seen_rstp = 0;
	stp_agree = 0;
	stp_synced_root = 0xff;
	stp_backup = 0;
	stp_alt_agreed = 0;
	sfr_data[0] = sfr_data[1] = sfr_data[2] = sfr_data[3] = 0;
	sfr_data[1] |= 0x0c; // Do not block the CPU port (bits 3:2 of byte 1 = port 9)
	reg_write_m(RTL837X_MSTP_STATES);
	stp_lag_map();
	stp_links_read();
	stp_link_prev = stp_link_now;
	stp_map_changed = 0;
	for (stp_i = 0; stp_i < STP_ENTITIES; stp_i++)
		stp_ent_reset(stp_i);

	print_reg(RTL837X_MSTP_STATES); write_char('\n');

	for (stp_i = machine.min_port; stp_i <= machine.max_port; stp_i++) {
		if (!(stp_pflags[stp_ent_of[stp_i]] & STP_PF_ENABLED))
			continue;
		if (port_ingress_filter_get(stp_i) != VLAN_TAGGED)
			continue;
		print_string("STP: port ");
		write_char('0' + machine.log_to_phys_port[stp_i]);
		print_string(" admits tagged frames only - BPDUs are untagged and will not arrive\n");
	}

	/* Take BPDUs to the CPU only - we are a participating bridge now. */
	stp_fdb_update(PMASK_CPU);
}


void stp_off(void) __banked
{
	sfr_data[0] = sfr_data[1] = sfr_data[2] = sfr_data[3] = 0;
	for (stp_i = machine.min_port; stp_i <= machine.max_port; stp_i++)
		sfr_data[3 - (stp_i >> 2)] |= (uint8_t)(0b11 << ((stp_i << 1) & 0x7));
	for (stp_i = 0; stp_i < STP_ENTITIES; stp_i++) {
		stp_pflags[stp_i] &= ~(STP_PF_OPEREDGE | STP_PF_TRIPPED);
		stp_loop_held[stp_i] = 0;
		port_timers[stp_i] = 0;
	}
	sfr_data[1] |= 0x0c; // Do not block the CPU port (bits 3:2 of byte 1 = port 9)
	reg_write_m(RTL837X_MSTP_STATES);

	if (stp_bpdu_filter)
		stp_fdb_update(PMASK_CPU);
	else
		stp_fdb_update(PMASK_CPU | (machine_detected.isRTL8373 ? PMASK_9 : PMASK_6));
}


void stp_port_admin(uint8_t port, uint8_t on) __banked
{
	if (on) {
		stp_state_set(port, 0b01);
		port_timers[port] = FWD_TICKS;
	} else {
		stp_state_set(port, 0b11);
	}
}


uint8_t stp_port_role(uint8_t port) __banked
{
	if (!(stp_pflags[port] & STP_PF_ENABLED) || (stp_pflags[port] & STP_PF_TRIPPED)
	    || !((stp_link_prev >> port) & 1))
		return 0;
	if (port == stp_root_port)
		return 1;
	if ((stp_backup >> port) & 1)
		return 4;
	if ((stp_alt >> port) & 1)
		return 3;
	return 2;
}


void stp_port_mcheck(uint8_t port) __banked
{
	stp_legacy &= ~((uint16_t)1 << port);
	stp_seen_stp &= ~((uint16_t)1 << port);
	stp_seen_rstp &= ~((uint16_t)1 << port);
	stp_mdelay[port] = STP_MIGRATE;
	stp_newinfo |= (uint16_t)1 << port;
}


void stp_prio_apply(void) __banked
{
	if (stp_root_port == 0xff)
		stp_claim_root();
}
