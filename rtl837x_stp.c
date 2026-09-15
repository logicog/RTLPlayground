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
#include "rtl837x_mstp.h"

#undef stp_root_port
#undef stp_prio
#undef stp_backup
#include "rtl837x_port.h"
#include "uip.h"
#include "machine.h"

extern __xdata uint8_t err_status;

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
__xdata uint8_t  stp_bprio[STP_TREES];	/* bridge priority high byte (0x80 = 32768) */
#define BRIDGE_PRIO	(stp_bprio[stp_t])
__xdata uint8_t  stp_hello_s;		/* 1-10 s */
__xdata uint8_t  stp_maxage_s;		/* 6-40 s */
__xdata uint8_t  stp_fwddelay_s;	/* 4-30 s, also our listen period */
__xdata uint8_t  stp_rstp;		/* 0 = STP, 1 = RSTP, STP_VER_MSTP = MSTP */
__xdata uint8_t  stp_txhold;		/* BPDUs per port per second */


__xdata uint8_t  stp_pflags[STP_ENTITIES];
__xdata uint32_t stp_pcost[STP_TREES * STP_ENTITIES];		/* 0 = auto */
__xdata uint8_t  stp_pprio[STP_TREES * STP_ENTITIES];
__xdata uint8_t  stp_pspeed[STP_ENTITIES];	/* speed nibble last read from the ASIC */
__xdata uint8_t  stp_pp2p[STP_ENTITIES];		/* admin point-to-point: 0 auto, 1 on, 2 off */

__xdata struct stp_vec stp_pv[STP_TREES * STP_ENTITIES];	/* priority vector last heard on the port, current while stp_info_while runs */
__xdata uint8_t  stp_pvhops[STP_TREES * STP_ENTITIES];	/* remaining hops heard with it */
__xdata struct stp_vec stp_msg;		/* vector of the BPDU being received */
__xdata struct stp_vec stp_cand;
__xdata struct stp_vec stp_bestv;
__xdata struct stp_vec stp_desv;
__xdata uint8_t  stp_msg_hops;
__xdata uint8_t  stp_vofs;		/* first byte of a vector that counts: 0 for the CIST, STP_VEC_MSTI for an MSTI */
__xdata uint16_t stp_internal;		/* bit per port: the neighbour is in our MST region */
__xdata uint8_t  stp_mst_rx;		/* the BPDU being received is a valid MST BPDU */
__xdata uint16_t stp_v3len;
__xdata uint8_t  stp_maxhops;
__xdata uint8_t  stp_rhops[STP_TREES];	/* remaining hops of the root information held */
__xdata uint16_t stp_alts[STP_TREES];		/* bit per port: blocked, a better bridge owns the segment */
#define ALT	(stp_alts[stp_t])
__xdata struct bridge stp_self;
__xdata uint8_t  stp_j;
__xdata uint8_t  stp_best;
__xdata int8_t   stp_cmp;
__xdata uint8_t  stp_rxage[STP_ENTITIES];		/* message age heard on the port, seconds */
__xdata uint8_t  stp_rxmaxage[STP_ENTITIES];
__xdata uint8_t  stp_rxhello[STP_ENTITIES];
__xdata uint8_t  stp_rxfwd[STP_ENTITIES];
__xdata uint16_t stp_info_while[STP_TREES * STP_ENTITIES];	/* ticks the heard information stays valid, 0 = none */
__xdata uint8_t  stp_root_maxage;	/* max age and forward delay of the root */
__xdata uint8_t  stp_root_fwd;
__xdata uint16_t stp_reselect_due;	/* bit per tree: work out the roles again at the end of the tick */
__xdata uint16_t stp_rsel;
__xdata uint16_t stp_trees;		/* bit per tree that runs: the CIST, and in MSTP every instance with VLANs */
__xdata uint8_t  stp_tt;
__xdata uint8_t  stp_ss_t;
__xdata uint8_t  stp_st;
__xdata uint8_t  stp_msg_flags;		/* flags of the message being received: the CIST's, or an instance's */
__xdata uint8_t  stp_msg_rst;		/* the message carries a port role */
__xdata uint8_t  stp_cist_same;		/* the BPDU repeats the CIST information held for the port */
__xdata uint8_t  stp_mi;
__xdata uint8_t  stp_mn;
__xdata uint8_t  stp_tx_t;
__xdata uint8_t * __xdata stp_rec;	/* MSTI configuration message being read or written */
__xdata uint8_t  stp_hw_msti;
__xdata uint8_t  stp_resync;		/* the CIST regional root moved: the instances sync their internal ports */
__xdata uint8_t  stp_sm_t;		/* write the instance of each VLAN to the VLAN table */

/* ---- Status / runtime ---- */
__xdata struct stp_vec stp_rv[STP_TREES];	/* root priority vector of each tree, rpid = root port */
#define RV	(stp_rv[stp_t])
__xdata uint8_t  stp_rport[STP_TREES];		/* 0xff = we are the root */
#define ROOT_PORT	(stp_rport[stp_t])
__xdata uint16_t stp_tc_count;
__xdata uint16_t stp_tc_seen;
__xdata uint32_t stp_tc_secs;		/* seconds since the topology change counter last moved */
__xdata uint8_t  stp_pcost_short;	/* 1: automatic port costs from the 802.1D-1998 table */
__xdata uint8_t  stp_bpdu_filter;	/* 1: keep BPDUs on the CPU while STP is off instead of flooding */
__xdata uint16_t stp_scratch16;	/* scratch for status printing only */

__xdata uint16_t port_timers[STP_TREES * STP_ENTITIES];	/* listen-period countdown (0 = not listening) */
__xdata uint16_t port_hello[STP_ENTITIES];	/* hello TX countdown */
__xdata uint16_t stp_bpdu_age[STP_ENTITIES];	/* ticks since last BPDU seen on port (saturating) */
__xdata uint8_t  stp_loop_held[STP_TREES * STP_ENTITIES];	/* port is out of forwarding because a loop was seen on it */
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
__xdata uint16_t stp_tcwhile[STP_TREES * STP_ENTITIES];	/* ticks left to send TC on the port, 0 = none */
__xdata uint16_t stp_newinfo;		/* bit per port: send a BPDU at the next tick */
__xdata uint8_t  stp_k;
__xdata uint8_t  stp_tcn;
__xdata uint8_t  stp_armed;
__xdata uint16_t stp_agrees[STP_TREES];		/* bit per port: answer with an agreement in the next BPDU */
#define AGREE	(stp_agrees[stp_t])
__xdata uint16_t stp_rrwhile[STP_TREES * STP_ENTITIES];	/* ticks the port still counts as a recent root port */
__xdata uint16_t stp_rbwhile[STP_TREES * STP_ENTITIES];	/* ticks the port still counts as a recent backup port */
__xdata uint16_t stp_reroots[STP_TREES];		/* bit per port: recent root port held out of forwarding */
#define REROOT	(stp_reroots[stp_t])
__xdata uint8_t  stp_synced[STP_TREES];	/* root port the last sync was done for, 0xff = none */
#define SYNCED_ROOT	(stp_synced[stp_t])
__xdata uint16_t stp_backups[STP_TREES];		/* bit per port: blocked because another port of ours owns the segment */
#define BACKUP	(stp_backups[stp_t])
__xdata uint16_t stp_alt_agreeds[STP_TREES];	/* bit per port: alternate or backup port that already agreed to the information it holds */
#define ALT_AGREED	(stp_alt_agreeds[stp_t])
__xdata uint16_t stp_legacy;		/* bit per port: the neighbour speaks 802.1D, send Config BPDUs and TCN */
__xdata uint16_t stp_seen_stp;		/* bit per port: an 802.1D BPDU arrived during the migrate time */
__xdata uint16_t stp_seen_rstp;		/* bit per port: an RST BPDU arrived during the migrate time */
__xdata uint8_t  stp_mdelay[STP_ENTITIES];		/* ticks before the port may change protocol again */
__xdata uint8_t  stp_i;
__xdata uint32_t stp_cost_scratch;
__xdata uint8_t  stp_t;			/* tree being worked on, 0 = CIST */
__xdata uint8_t  stp_tsave;		/* tree to come back to after a detour through the CIST */
__xdata uint8_t  stp_tb;			/* stp_t * STP_ENTITIES: first index of the tree in per-port arrays */
__xdata uint8_t  stp_loop_peer;		/* the other own port seen on a looped segment */
__xdata uint8_t  stp_ent_of[STP_PORTS];		/* entity a port answers to: itself, or STP_LAG_BASE + lag */
__xdata uint16_t stp_lag_mask[STP_LAG_COUNT];	/* member ports of each lag, 0 = no such lag */
__xdata uint16_t stp_map_changed;	/* bit per entity: its membership changed, start it over */
__xdata uint16_t stp_link_phys;		/* carrier bitmap of the physical ports */
__xdata uint16_t stp_ent_bit;
__xdata uint16_t stp_ss_mask;
__xdata uint8_t  stp_ss_i;
__xdata uint8_t  stp_lag;

#define PT(e)		((uint8_t)(stp_tb + (e)))

#define STP_EDGE_DELAY	(3 * STP_HZ)	/* auto-edge: forward after 3 s without BPDU */

#define MAXAGE_S	(stp_rport[0] == 0xff ? stp_maxage_s : stp_root_maxage)
#define FWD_S		(stp_rport[0] == 0xff ? stp_fwddelay_s : stp_root_fwd)
#define FWD_TICKS	((uint16_t)FWD_S * STP_HZ)
#define P2P(i)		(stp_pp2p[i] != 2)
#define SEND_RSTP(i)	(stp_rstp && !((stp_legacy >> (i)) & 1))
#define STP_MIGRATE	(3 * STP_HZ)

#define AUTO_COST	20000UL		/* path cost of a link whose speed we cannot read */
#define PCOST(i)	(stp_pcost[PT(i)] ? stp_pcost[PT(i)] : stp_speed_cost[(stp_pcost_short << 3) | (stp_pspeed[i] & 0x7)])

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
#define MST_I ((__xdata uint8_t *)&STP_I->version1_length + 1)
#define MST_O ((__xdata uint8_t *)&STP_O->version1_length + 1)

#define BPDU_VER_STP		0x00
#define BPDU_VER_RSTP		0x02
#define BPDU_VER_MSTP		0x03

#define BPDU_TYPE_CONFIG	0x00
#define BPDU_TYPE_RST		0x02
#define BPDU_TYPE_TCN		0x80

#define BPDU_LEN_CONFIG		0x26	// LLC and a 35 byte body
#define BPDU_LEN_RST		0x27	// LLC and a 36 byte body
#define BPDU_LEN_TCN		0x07	// LLC and a 4 byte body
#define BPDU_LEN_MIN_HEADER	33	// addresses through bpdu_type
#define BPDU_LEN_MST		105	// LLC and a 102 byte body without MSTI messages
#define MST_V3_FIXED		64	// version 3 length without MSTI messages

#define BPDU_FLAG_TC		0x01
#define BPDU_FLAG_PROPOSAL	0x02
#define BPDU_FLAG_AGREEMENT	0x40
#define BPDU_FLAG_LEARNING	0x10
#define BPDU_FLAG_FORWARDING	0x20
#define BPDU_FLAG_TCACK		0x80
#define BPDU_FLAG_MASTER	0x80

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
static __code const char stp_role_txt[]  = "dis rootdesgaltnbackmast";
static __code const char stp_edge_txt[]  = "no  yes ";

static uint8_t stp_state_get(uint8_t port) __reentrant;
static uint8_t stp_ent_active(uint8_t e) __reentrant;
static void stp_msti_tx(uint8_t port) __reentrant;
static void stp_internal_update(uint8_t port, uint8_t now) __reentrant;

static __code const uint8_t stp_tree_base[STP_TREES] = {
	0 * STP_ENTITIES, 1 * STP_ENTITIES, 2 * STP_ENTITIES, 3 * STP_ENTITIES,
	4 * STP_ENTITIES, 5 * STP_ENTITIES, 6 * STP_ENTITIES, 7 * STP_ENTITIES,
	8 * STP_ENTITIES, 9 * STP_ENTITIES, 10 * STP_ENTITIES, 11 * STP_ENTITIES,
	12 * STP_ENTITIES, 13 * STP_ENTITIES, 14 * STP_ENTITIES, 15 * STP_ENTITIES
};

static void stp_tree(uint8_t t) __reentrant
{
	stp_t = t;
	stp_tb = stp_tree_base[t];
}

static void print_field(__code const char *txt, uint8_t idx, uint8_t width) __reentrant
{
	txt += idx * width;
	while (width--)
		write_char(*txt++);
}


void stp_tree_status(uint8_t t) __banked
{
	stp_tree(t);
	if (!stp_enabled) {
		print_string("STP off\n");
		stp_tree(0);
		return;
	}
	if (stp_t) {
		print_string("MSTI ");
		itoa(stp_t);
		print_string(((stp_trees >> stp_t) & 1) ? "\n" : ", not running\n");
	} else {
		print_string(stp_rstp == STP_VER_MSTP ? "STP on, MSTP\n" : stp_rstp ? "STP on, RSTP\n" : "STP on, STP\n");
	}
	print_string("bridge  ");
	print_bridge_id(BRIDGE_PRIO, stp_t, uip_ethaddr.addr);
	write_char('\n');
	if (!stp_t) {
		print_string("root    ");
		print_bridge_id(RV.root.prio, RV.root.ext, RV.root.mac);
		if (ROOT_PORT == 0xff) {
			print_string(" (this switch)\n");
		} else {
			print_string(" port ");
			print_ent(ROOT_PORT);
			print_string(" cost ");
			for (stp_i = 0; stp_i < 4; stp_i++)
				print_byte(RV.ext[stp_i]);
			write_char('\n');
		}
	}
	if (stp_t || stp_rstp == STP_VER_MSTP) {
		print_string("region  ");
		print_bridge_id(RV.rroot.prio, RV.rroot.ext, RV.rroot.mac);
		if (stp_t && ROOT_PORT != 0xff) {
			print_string(" port ");
			print_ent(ROOT_PORT);
		}
		print_string(" cost ");
		for (stp_i = 0; stp_i < 4; stp_i++)
			print_byte(RV.icost[stp_i]);
		print_string(" hops ");
		itoa(stp_rhops[stp_t]);
		write_char('\n');
	}
	if (!stp_t) {
		print_string("changes ");
		print_short(stp_tc_count);
		write_char('\n');
	}
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
	stp_tree(0);
}


void stp_status(void) __banked
{
	stp_tree_status(0);
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


static int8_t stp_vcmp(__xdata struct stp_vec *a, __xdata struct stp_vec *b, uint8_t len) __reentrant
{
	return cmpBytes((__xdata uint8_t *)a + stp_vofs, (__xdata uint8_t *)b + stp_vofs, len - stp_vofs);
}


static void stp_vcopy(__xdata struct stp_vec *to, __xdata struct stp_vec *from, uint8_t len) __reentrant
{
	while (len--)
		((__xdata uint8_t *)to)[len] = ((__xdata uint8_t *)from)[len];
}


static void stp_add_cost(__xdata uint8_t *c, uint32_t v) __reentrant
{
	stp_cost_scratch = ((uint32_t)c[0] << 24) | ((uint32_t)c[1] << 16) | ((uint32_t)c[2] << 8) | c[3];
	stp_cost_scratch += v;
	if (stp_cost_scratch < v)
		stp_cost_scratch = 0xffffffffUL;
	c[0] = stp_cost_scratch >> 24;
	c[1] = stp_cost_scratch >> 16;
	c[2] = stp_cost_scratch >> 8;
	c[3] = stp_cost_scratch;
}


static void stp_self_id(void) __reentrant
{
	stp_self.prio = BRIDGE_PRIO;
	stp_self.ext = stp_t;
	memcpy(stp_self.mac, uip_ethaddr.addr, 6);
}


static void stp_own_vec(__xdata struct stp_vec *v) __reentrant
{
	stp_self_id();
	for (stp_k = 0; stp_k < STP_VEC_LEN; stp_k++)
		((__xdata uint8_t *)v)[stp_k] = 0;
	stp_vcopy((__xdata struct stp_vec *)&v->root, (__xdata struct stp_vec *)&stp_self, 8);
	stp_vcopy((__xdata struct stp_vec *)&v->rroot, (__xdata struct stp_vec *)&stp_self, 8);
	stp_vcopy((__xdata struct stp_vec *)&v->dbr, (__xdata struct stp_vec *)&stp_self, 8);
}


static void stp_msg_build(void) __reentrant
{
	stp_vcopy((__xdata struct stp_vec *)&stp_msg.root, (__xdata struct stp_vec *)&STP_I->root, 8);
	stp_vcopy((__xdata struct stp_vec *)stp_msg.ext, (__xdata struct stp_vec *)&STP_I->root_path_cost, 4);
	stp_vcopy((__xdata struct stp_vec *)&stp_msg.rroot, (__xdata struct stp_vec *)&STP_I->bridge, 8);
	if (stp_mst_rx) {
		stp_vcopy((__xdata struct stp_vec *)stp_msg.icost, (__xdata struct stp_vec *)(MST_I + 53), 4);
		stp_vcopy((__xdata struct stp_vec *)&stp_msg.dbr, (__xdata struct stp_vec *)(MST_I + 57), 8);
		stp_msg_hops = MST_I[65];
	} else {
		for (stp_k = 0; stp_k < 4; stp_k++)
			stp_msg.icost[stp_k] = 0;
		stp_vcopy((__xdata struct stp_vec *)&stp_msg.dbr, (__xdata struct stp_vec *)&STP_I->bridge, 8);
		stp_msg_hops = stp_maxhops;
	}
	stp_msg.dpid[0] = STP_I->port_prio;
	stp_msg.dpid[1] = STP_I->port_id;
}


static uint8_t stp_region_match(void) __reentrant
{
	if (MST_I[2] || MST_I[35] != (uint8_t)(mstp_revision >> 8) || MST_I[36] != (uint8_t)mstp_revision)
		return 0;
	stp_scratch = 1;
	for (stp_k = 0; stp_k < MSTP_NAME_LEN; stp_k++) {
		if (stp_scratch && !mstp_region[stp_k])
			stp_scratch = 0;
		if (MST_I[3 + stp_k] != (stp_scratch ? (uint8_t)mstp_region[stp_k] : 0))
			return 0;
	}
	for (stp_k = 0; stp_k < 16; stp_k++)
		if (MST_I[37 + stp_k] != mstp_digest[stp_k])
			return 0;
	return 1;
}


static uint8_t stp_clamp(uint8_t v, uint8_t lo, uint8_t hi, uint8_t dflt) __reentrant
{
	return (v < lo || v > hi) ? dflt : v;
}


static void stp_rcv_info(uint8_t port) __reentrant
{
	if (stp_msg_rst
	    && (stp_msg_flags & BPDU_ROLE_MASK) != BPDU_ROLE_DESIGNATED)
		return;

	if (stp_info_while[PT(port)]
	    && (cmpBytes(stp_msg.dbr.mac, stp_pv[PT(port)].dbr.mac, 6)
		|| stp_msg.dpid[1] != stp_pv[PT(port)].dpid[1])
	    && stp_vcmp(&stp_msg, &stp_pv[PT(port)], STP_VEC_HEARD) > 0)
		return;

	if (!stp_info_while[PT(port)] || stp_vcmp(&stp_msg, &stp_pv[PT(port)], STP_VEC_HEARD))
		ALT_AGREED &= ~((uint16_t)1 << port);

	stp_vcopy(&stp_pv[PT(port)], &stp_msg, STP_VEC_HEARD);
	stp_pvhops[PT(port)] = stp_msg_hops;

	if (!stp_t) {
		stp_rxage[port] = (uint8_t)STP_I->age;
		stp_rxmaxage[port] = stp_clamp((uint8_t)STP_I->age_max, 6, 40, 20);
		stp_rxhello[port] = stp_clamp((uint8_t)STP_I->hello, 1, 10, 2);
		stp_rxfwd[port] = stp_clamp((uint8_t)STP_I->fwd_delay, 4, 30, 15);
	}
	if (stp_t || ((stp_internal >> port) & 1))
		stp_info_while[PT(port)] = stp_msg_hops > 1
			? (uint16_t)3 * stp_rxhello[port] * STP_HZ : 0;
	else
		stp_info_while[PT(port)] = (uint16_t)stp_rxage[port] + 1 <= stp_rxmaxage[port]
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


static void stp_state_reg(uint8_t t, uint8_t port, uint8_t state) __reentrant
{
	stp_ss_mask = stp_members(port);
	reg_read_m(RTL837X_MSTP_STATES + (t << 2));
	for (stp_ss_i = 0; stp_ss_i < STP_PORTS; stp_ss_i++) {
		if (!((stp_ss_mask >> stp_ss_i) & 1))
			continue;
		stp_scratch = 3 - (stp_ss_i >> 2);
		sfr_data[stp_scratch] &= ~(uint8_t)(0b11 << ((stp_ss_i << 1) & 0x7));
		sfr_data[stp_scratch] |= (uint8_t)(state << ((stp_ss_i << 1) & 0x7));
	}
	reg_write_m(RTL837X_MSTP_STATES + (t << 2));
}


static void stp_state_set(uint8_t port, uint8_t state) __reentrant
{
	stp_state_reg(stp_t, port, state);
	if (stp_t || ((stp_internal >> port) & 1))
		return;
	for (stp_ss_t = 1; stp_ss_t < STP_TREES; stp_ss_t++)
		if ((stp_trees >> stp_ss_t) & 1)
			stp_state_reg(stp_ss_t, port, state);
}


static uint8_t stp_state_get(uint8_t port) __reentrant
{
	if (stp_first(stp_members(port)) == 0xff)
		return 0;
	reg_read_m(RTL837X_MSTP_STATES + (stp_t << 2));
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
	if (stp_tcwhile[PT(port)])
		return;
	stp_tcwhile[PT(port)] = SEND_RSTP(port) ? ((uint16_t)stp_hello_s + 1) * STP_HZ
				     : (uint16_t)MAXAGE_S * STP_HZ + FWD_TICKS;
	stp_newinfo |= (uint16_t)1 << port;
}


static uint8_t stp_tc_prop(uint8_t from) __reentrant
{
	stp_armed = 0;
	for (stp_k = 0; stp_k < STP_ENTITIES; stp_k++) {
		if (stp_k == from || !stp_ent_active(stp_k) || stp_state_get(stp_k) != 0b11)
			continue;
		if (!(stp_pflags[stp_k] & STP_PF_ENABLED) || (stp_pflags[stp_k] & STP_PF_OPEREDGE))
			continue;
		if (stp_t && !((stp_internal >> stp_k) & 1)) {
			/* A boundary port carries the instance's VLANs in its CIST
			 * state, so it is flushed like any other port of the tree,
			 * and the change leaves the region in the CIST (13.27). */
			if (stp_tcwhile[stp_k])
				continue;
			stp_forget(stp_k);
			stp_tsave = stp_t;
			stp_tree(0);
			stp_new_tc_while(stp_k);
			stp_tree(stp_tsave);
			stp_armed = 1;
			continue;
		}
		if (stp_tcwhile[PT(stp_k)])
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
	port_timers[PT(port)] = 0;
	stp_loop_held[PT(port)] = 0;
	BACKUP &= ~((uint16_t)1 << port);
	stp_state_set(port, 0b11);
	print_string("STP: rapid transition, port forwarding ");
	print_port_nl(port);
	stp_tc_detected(port);
}


static void stp_sync(uint8_t from) __reentrant
{
	for (stp_k = 0; stp_k < STP_ENTITIES; stp_k++) {
		if (!stp_ent_active(stp_k) || (stp_t && !((stp_internal >> stp_k) & 1)))
			continue;
		if (stp_k == from || stp_k == ROOT_PORT || !(stp_pflags[stp_k] & STP_PF_ENABLED)
		    || (stp_pflags[stp_k] & STP_PF_OPEREDGE) || ((ALT >> stp_k) & 1)
		    || !((stp_link_prev >> stp_k) & 1))
			continue;
		if (stp_state_get(stp_k) != 0b01) {
			stp_state_set(stp_k, 0b01);
			port_timers[PT(stp_k)] = FWD_TICKS;
		}
		stp_newinfo |= (uint16_t)1 << stp_k;
	}
	if (from != ROOT_PORT)
		return;
	SYNCED_ROOT = from;
	if (port_timers[PT(from)])
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
	stp_internal_update(port, 0);
	if (!port_timers[PT(port)]) {
		print_string("STP: loop detected, blocking port ");
		print_port_nl(port);
		stp_state_set(port, 0b01);
		stp_pflags[port] &= ~STP_PF_OPEREDGE;
		stp_forget(port);
		stp_tcwhile[PT(port)] = 0;
	}
	stp_loop_held[PT(port)] = 1;
	BACKUP |= (uint16_t)1 << port;
	port_timers[PT(port)] = FWD_TICKS;
}


/* Take the bridge back as root of its own tree (initial state / root aged out) */
static void stp_claim_root(void)
{
	stp_own_vec(&RV);
	ROOT_PORT = 0xff;
	if (!stp_t)
		stp_msg_age = 0;
	stp_rhops[stp_t] = stp_maxhops;
}


static uint8_t stp_info_fresh(uint8_t port) __reentrant
{
	return stp_info_while[PT(port)] != 0;
}


static void stp_des_vec(uint8_t port) __reentrant
{
	stp_vcopy(&stp_desv, &RV, STP_VEC_LEN);
	stp_vcopy((__xdata struct stp_vec *)&stp_desv.dbr, (__xdata struct stp_vec *)&stp_self, 8);
	stp_desv.dpid[0] = stp_pprio[PT(port)];
	stp_desv.dpid[1] = port + 1;
}


static int8_t stp_cmp_designated(uint8_t port) __reentrant
{
	stp_des_vec(port);
	return stp_vcmp(&stp_pv[PT(port)], &stp_desv, STP_VEC_HEARD);
}


static void stp_reroot_tree(void) __reentrant
{
	for (stp_k = 0; stp_k < STP_ENTITIES; stp_k++) {
		if (!stp_ent_active(stp_k) || (stp_t && !((stp_internal >> stp_k) & 1)))
			continue;
		if (stp_k == ROOT_PORT || !stp_rrwhile[PT(stp_k)]
		    || !(stp_pflags[stp_k] & STP_PF_ENABLED) || (stp_pflags[stp_k] & STP_PF_OPEREDGE))
			continue;
		REROOT |= (uint16_t)1 << stp_k;
		if (stp_state_get(stp_k) != 0b01) {
			stp_state_set(stp_k, 0b01);
			port_timers[PT(stp_k)] = FWD_TICKS;
			stp_newinfo |= (uint16_t)1 << stp_k;
		}
	}
}


static uint8_t stp_cost_set(__xdata uint8_t *c) __reentrant
{
	return c[0] | c[1] | c[2] | c[3];
}


static void stp_sync_master(void) __reentrant
{
	for (stp_sm_t = 1; stp_sm_t < STP_TREES; stp_sm_t++) {
		if (!((stp_trees >> stp_sm_t) & 1))
			continue;
		stp_tree(stp_sm_t);
		AGREE = 0;
		ALT_AGREED = 0;
		SYNCED_ROOT = 0xff;
		for (stp_k = 0; stp_k < STP_ENTITIES; stp_k++) {
			if (!stp_ent_active(stp_k) || !((stp_internal >> stp_k) & 1) || stp_k == ROOT_PORT
			    || ((ALT >> stp_k) & 1) || !(stp_pflags[stp_k] & STP_PF_ENABLED)
			    || (stp_pflags[stp_k] & STP_PF_OPEREDGE) || !((stp_link_prev >> stp_k) & 1))
				continue;
			if (stp_state_get(stp_k) != 0b01) {
				stp_state_set(stp_k, 0b01);
				port_timers[PT(stp_k)] = FWD_TICKS;
			}
			stp_newinfo |= (uint16_t)1 << stp_k;
		}
	}
	stp_tree(0);
}


static void stp_reselect(void)
{
	stp_vofs = stp_t ? STP_VEC_MSTI : 0;
	stp_own_vec(&stp_bestv);

	stp_best = 0xff;
	for (stp_j = 0; stp_j < STP_ENTITIES; stp_j++) {
		if (!stp_ent_active(stp_j))
			continue;
		if (!(stp_pflags[stp_j] & STP_PF_ENABLED) || (stp_pflags[stp_j] & STP_PF_ROOTGUARD))
			continue;
		if (!stp_info_fresh(stp_j))
			continue;
		if (!cmpBytes(stp_pv[PT(stp_j)].dbr.mac, uip_ethaddr.addr, 6))
			continue;
		if (stp_t && !((stp_internal >> stp_j) & 1))
			continue;
		stp_vcopy(&stp_cand, &stp_pv[PT(stp_j)], STP_VEC_HEARD);
		stp_cand.rpid[0] = stp_pprio[PT(stp_j)];
		stp_cand.rpid[1] = stp_j + 1;
		if ((stp_internal >> stp_j) & 1) {
			stp_add_cost(stp_cand.icost, PCOST(stp_j));
		} else {
			stp_add_cost(stp_cand.ext, PCOST(stp_j));
			stp_vcopy((__xdata struct stp_vec *)&stp_cand.rroot, (__xdata struct stp_vec *)&stp_self, 8);
			for (stp_k = 0; stp_k < 4; stp_k++)
				stp_cand.icost[stp_k] = 0;
		}
		if (stp_vcmp(&stp_cand, &stp_bestv, STP_VEC_LEN) < 0) {
			stp_vcopy(&stp_bestv, &stp_cand, STP_VEC_LEN);
			stp_best = stp_j;
		}
	}

	if (stp_best != 0xff) {
		stp_cmp = cmpBytes((__xdata uint8_t *)&RV.rroot, (__xdata uint8_t *)&stp_bestv.rroot, 8);
		if (!stp_t && !stp_cmp)
			stp_cmp = cmpBytes((__xdata uint8_t *)&RV.root, (__xdata uint8_t *)&stp_bestv.root, 8);
		if (ROOT_PORT != stp_best || stp_cmp) {
			if (stp_cmp)
				print_string("Updating Root bridge\n");
			SYNCED_ROOT = 0xff;
			ROOT_PORT = stp_best;
			stp_tc_count++;
		}
		if (!stp_t && cmpBytes((__xdata uint8_t *)&RV.rroot, (__xdata uint8_t *)&stp_bestv.rroot, 8)
		    && (stp_cost_set(RV.ext) || stp_cost_set(stp_bestv.ext)))
			stp_resync = 1;
		stp_vcopy(&RV, &stp_bestv, STP_VEC_LEN);
		if ((stp_internal >> stp_best) & 1)
			stp_rhops[stp_t] = stp_pvhops[PT(stp_best)] ? stp_pvhops[PT(stp_best)] - 1 : 0;
		else
			stp_rhops[stp_t] = stp_maxhops;
		if (!stp_t) {
			stp_msg_age = stp_rxage[stp_best] + (((stp_internal >> stp_best) & 1) ? 0 : 1);
			stp_root_maxage = stp_rxmaxage[stp_best];
			stp_root_fwd = stp_rxfwd[stp_best];
		}
	} else if (ROOT_PORT != 0xff) {
		print_string("STP: root aged out, claiming root\n");
		SYNCED_ROOT = 0xff;
		if (!stp_t && stp_cost_set(RV.ext))
			stp_resync = 1;
		stp_claim_root();
		stp_tc_count++;
	}

	for (stp_j = 0; stp_j < STP_ENTITIES; stp_j++) {
		if (!stp_ent_active(stp_j) || (stp_t && !((stp_internal >> stp_j) & 1)))
			continue;
		if (!(stp_pflags[stp_j] & STP_PF_ENABLED))
			continue;
		if (stp_j != ROOT_PORT && stp_info_fresh(stp_j)
		    && stp_cmp_designated(stp_j) < 0) {
			stp_pflags[stp_j] &= ~STP_PF_OPEREDGE;
			port_timers[PT(stp_j)] = 0;
			stp_state_set(stp_j, 0b01);
			if (!((ALT >> stp_j) & 1)) {
				ALT |= (uint16_t)1 << stp_j;
				stp_forget(stp_j);
				stp_tcwhile[PT(stp_j)] = 0;
				print_string("STP: better bridge on the segment, blocking port ");
				print_port_nl(stp_j);
			}
			continue;
		}
		if (!((ALT >> stp_j) & 1))
			continue;
		ALT &= ~((uint16_t)1 << stp_j);
		ALT_AGREED &= ~((uint16_t)1 << stp_j);
		port_timers[PT(stp_j)] = FWD_TICKS;
		print_string("STP: port released, listening ");
		print_port_nl(stp_j);
	}

	if (stp_rstp && ROOT_PORT != 0xff && port_timers[PT(ROOT_PORT)]
	    && !((ALT >> ROOT_PORT) & 1) && !stp_rbwhile[PT(ROOT_PORT)]) {
		stp_reroot_tree();
		stp_forward_now(ROOT_PORT);
	}
	if (!stp_t && stp_resync) {
		stp_resync = 0;
		stp_sync_master();
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
	stp_tcn = !SEND_RSTP(port) && port == ROOT_PORT;

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
		STP_O->flags = port == ROOT_PORT ? BPDU_ROLE_ROOT
			     : (((ALT | BACKUP) >> port) & 1) ? BPDU_ROLE_ALTBACK : BPDU_ROLE_DESIGNATED;
		stp_scratch = stp_state_get(port);
		if (stp_scratch == 0b11)
			STP_O->flags |= BPDU_FLAG_LEARNING | BPDU_FLAG_FORWARDING;
		else if (stp_scratch == 0b10)
			STP_O->flags |= BPDU_FLAG_LEARNING;
		if (port != ROOT_PORT && port_timers[PT(port)] && P2P(port)
		    && !(stp_pflags[port] & STP_PF_OPEREDGE) && !(((ALT | BACKUP) >> port) & 1))
			STP_O->flags |= BPDU_FLAG_PROPOSAL;
		if ((AGREE >> port) & 1) {
			STP_O->flags |= BPDU_FLAG_AGREEMENT;
			AGREE &= ~((uint16_t)1 << port);
		}
	} else {
		STP_O->msg_len = HTONS(BPDU_LEN_CONFIG);
		STP_O->version = BPDU_VER_STP;
		STP_O->bpdu_type = BPDU_TYPE_CONFIG;
		STP_O->flags = 0x00;
	}
	if (stp_tcwhile[PT(port)])
		STP_O->flags |= BPDU_FLAG_TC;
	STP_O->flags |= stp_tx_flags_extra;
	stp_tx_flags_extra = 0;
	if (stp_tcn || (STP_O->flags & BPDU_FLAG_TC))
		stp_cnt[STP_CNT_TCTX][port]++;

	memcpy(STP_O->src_addr, uip_ethaddr.addr, 6);
	STP_O->src_addr[0] |= 0x02;
	STP_O->src_addr[5] = (uip_ethaddr.addr[5] & 0xf0) | port;
	stp_vcopy((__xdata struct stp_vec *)&STP_O->root, (__xdata struct stp_vec *)&stp_rv[0].root, 8);
	stp_vcopy((__xdata struct stp_vec *)&STP_O->root_path_cost, (__xdata struct stp_vec *)stp_rv[0].ext, 4);
	if (SEND_RSTP(port)) {
		stp_vcopy((__xdata struct stp_vec *)&STP_O->bridge, (__xdata struct stp_vec *)&stp_rv[0].rroot, 8);
	} else {
		STP_O->bridge.prio = stp_bprio[0];
		STP_O->bridge.ext = 0x00;
		memcpy(STP_O->bridge.mac, uip_ethaddr.addr, 6);
	}

	STP_O->port_prio = stp_pprio[PT(port)];
	STP_O->port_id = port + 1;
	/* Message age, incremented by one second per bridge we relay through.
	 * The timer fields are in 1/256 s on the wire, and sdcc stores uint16
	 * little-endian, so assigning the plain second count lands the value in
	 * the high (seconds) octet - see age_max/hello/fwd_delay below. */
	STP_O->age = (stp_rport[0] == 0xff) ? 0 : (uint16_t)stp_msg_age;
	STP_O->age_max = MAXAGE_S;
	STP_O->hello = stp_hello_s;
	STP_O->fwd_delay = FWD_S;
	STP_O->version1_length = 0;	/* RST BPDU: no version-1 information */

	uip_len = stp_tcn ? sizeof(struct stp_pkt) - 32
			  : (SEND_RSTP(port) ? sizeof(struct stp_pkt) : sizeof(struct stp_pkt) - 1);
	if (!stp_tcn && SEND_RSTP(port) && stp_rstp == STP_VER_MSTP) {
		STP_O->version = BPDU_VER_MSTP;
		STP_O->msg_len = HTONS(BPDU_LEN_MST);
		MST_O[0] = 0;
		MST_O[1] = MST_V3_FIXED;
		MST_O[2] = 0;
		stp_scratch = 1;
		for (stp_k = 0; stp_k < MSTP_NAME_LEN; stp_k++) {
			if (stp_scratch && !mstp_region[stp_k])
				stp_scratch = 0;
			MST_O[3 + stp_k] = stp_scratch ? mstp_region[stp_k] : 0;
		}
		MST_O[35] = mstp_revision >> 8;
		MST_O[36] = mstp_revision;
		for (stp_k = 0; stp_k < 16; stp_k++)
			MST_O[37 + stp_k] = mstp_digest[stp_k];
		for (stp_k = 0; stp_k < 4; stp_k++)
			MST_O[53 + stp_k] = stp_rv[0].icost[stp_k];
		MST_O[57] = stp_bprio[0];
		MST_O[58] = 0;
		for (stp_k = 0; stp_k < 6; stp_k++)
			MST_O[59 + stp_k] = uip_ethaddr.addr[stp_k];
		MST_O[65] = stp_rhops[0];
		stp_mn = 0;
		for (stp_tx_t = 1; stp_tx_t < STP_TREES; stp_tx_t++) {
			if (!((stp_trees >> stp_tx_t) & 1))
				continue;
			stp_rec = MST_O + 66 + ((uint16_t)stp_mn << 4);
			stp_msti_tx(port);
			stp_mn++;
		}
		stp_tree(0);
		MST_O[0] = (MST_V3_FIXED + ((uint16_t)stp_mn << 4)) >> 8;
		MST_O[1] = MST_V3_FIXED + (stp_mn << 4);
		STP_O->msg_len = HTONS(BPDU_LEN_MST + ((uint16_t)stp_mn << 4));
		uip_len = (uint16_t)(MST_O - (__xdata uint8_t *)STP_O) + 66 + ((uint16_t)stp_mn << 4);
	}
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
	stp_self_id();
	stp_des_vec(port);
	return stp_vcmp(&stp_msg, &stp_desv, STP_VEC_HEARD);
}


static void stp_dispute_rx(uint8_t port) __reentrant
{
	if (port == ROOT_PORT || ((ALT >> port) & 1))
		return;
	if (stp_msg_rst
	    && (stp_msg_flags & BPDU_ROLE_MASK) != BPDU_ROLE_DESIGNATED)
		return;
	if (stp_msg_vs_ours(port) <= 0)
		return;
	stp_newinfo |= (uint16_t)1 << port;
	if (!stp_rstp || !stp_msg_rst
	    || !(stp_msg_flags & BPDU_FLAG_LEARNING))
		return;
	if (stp_state_get(port) != 0b01) {
		stp_state_set(port, 0b01);
		print_string("STP: dispute, port discarding ");
		print_port_nl(port);
	}
	port_timers[PT(port)] = FWD_TICKS;
}


static void stp_rapid_rx(uint8_t port) __reentrant
{
	if (stp_rstp && stp_msg_rst && P2P(port)) {
		if (port == ROOT_PORT
		    && (stp_msg_flags & BPDU_ROLE_MASK) == BPDU_ROLE_DESIGNATED
		    && (stp_msg_flags & BPDU_FLAG_PROPOSAL)) {
			if (SYNCED_ROOT != port)
				stp_sync(port);
			AGREE |= (uint16_t)1 << port;
			stp_newinfo |= (uint16_t)1 << port;
		} else if ((((ALT | BACKUP) >> port) & 1)
			   && (stp_msg_flags & BPDU_ROLE_MASK) == BPDU_ROLE_DESIGNATED
			   && (stp_msg_flags & BPDU_FLAG_PROPOSAL)) {
			if (!((ALT_AGREED >> port) & 1))
				stp_sync(port);
			ALT_AGREED |= (uint16_t)1 << port;
			AGREE |= (uint16_t)1 << port;
			stp_newinfo |= (uint16_t)1 << port;
		} else if (port != ROOT_PORT && port_timers[PT(port)]
			   && !((ALT >> port) & 1)
			   && !(((REROOT >> port) & 1) && stp_rrwhile[PT(port)])
			   && ((stp_msg_flags & BPDU_ROLE_MASK) == BPDU_ROLE_ROOT
			       || (stp_msg_flags & BPDU_ROLE_MASK) == BPDU_ROLE_ALTBACK)
			   && (stp_msg_flags & BPDU_FLAG_AGREEMENT)
			   && !cmpBytes((__xdata uint8_t *)&stp_msg + stp_vofs, (__xdata uint8_t *)&RV + stp_vofs, 8)
			   && (!stp_t || stp_cist_same)) {
			stp_forward_now(port);
		}
	}
}


static uint8_t stp_mst_valid(void) __reentrant
{
	if (STP_I->version < BPDU_VER_MSTP || STP_I->bpdu_type != BPDU_TYPE_RST
	    || STP_I->version1_length || HTONS(STP_I->msg_len) < BPDU_LEN_MST
	    || stp_rxlen < (uint16_t)(MST_I - (__xdata uint8_t *)uip_buf) + 66)
		return 0;
	stp_v3len = ((uint16_t)MST_I[0] << 8) | MST_I[1];
	return stp_v3len >= MST_V3_FIXED && !((stp_v3len - MST_V3_FIXED) & 0x0f)
	       && HTONS(STP_I->msg_len) >= stp_v3len + 41
	       && stp_rxlen >= (uint16_t)(MST_I - (__xdata uint8_t *)uip_buf) + 2 + stp_v3len;
}


static void stp_tc_instances(uint8_t port) __reentrant
{
	for (stp_tt = 1; stp_tt < STP_TREES; stp_tt++) {
		if (!((stp_trees >> stp_tt) & 1))
			continue;
		stp_tree(stp_tt);
		stp_tc_prop(port);
	}
	stp_tree(0);
}


static void stp_internal_update(uint8_t port, uint8_t now) __reentrant
{
	if (stp_loop_held[port])
		now = 0;
	stp_ent_bit = (uint16_t)1 << port;
	if (((stp_internal >> port) & 1) == now)
		return;
	if (now)
		stp_internal |= stp_ent_bit;
	else
		stp_internal &= ~stp_ent_bit;
	if (stp_trees == 1)
		return;
	stp_st = stp_state_get(port);
	for (stp_tt = 1; stp_tt < STP_TREES; stp_tt++) {
		if (!((stp_trees >> stp_tt) & 1))
			continue;
		stp_tree(stp_tt);
		stp_info_while[PT(port)] = 0;
		stp_rrwhile[PT(port)] = 0;
		stp_tcwhile[PT(port)] = 0;
		REROOT &= ~stp_ent_bit;
		BACKUP &= ~stp_ent_bit;
		ALT_AGREED &= ~stp_ent_bit;
		ALT &= ~stp_ent_bit;
		AGREE &= ~stp_ent_bit;
		if (now) {
			stp_state_reg(stp_tt, port, 0b01);
			port_timers[PT(port)] = FWD_TICKS;
		} else {
			port_timers[PT(port)] = 0;
			stp_state_reg(stp_tt, port, stp_st);
		}
		stp_reselect_due |= (uint16_t)1 << stp_tt;
	}
	stp_tree(0);
	if (now)
		stp_newinfo |= stp_ent_bit;
}


static void stp_msti_rx(uint8_t port) __reentrant
{
	if (!stp_mst_rx || !((stp_internal >> port) & 1))
		return;
	if (port == ROOT_PORT || (((ALT | BACKUP) >> port) & 1))
		stp_cist_same = !cmpBytes((__xdata uint8_t *)&stp_msg, (__xdata uint8_t *)&stp_pv[port], 20);
	else
		stp_cist_same = !cmpBytes((__xdata uint8_t *)&stp_msg, (__xdata uint8_t *)&RV, 20);
	stp_mn = (stp_v3len - MST_V3_FIXED) >> 4;
	for (stp_mi = 0; stp_mi < stp_mn; stp_mi++) {
		stp_rec = MST_I + 66 + ((uint16_t)stp_mi << 4);
		if ((stp_rec[1] & 0x0f) || !stp_rec[2] || stp_rec[2] >= STP_TREES
		    || !((stp_trees >> stp_rec[2]) & 1))
			continue;
		stp_tree(stp_rec[2]);
		stp_vofs = STP_VEC_MSTI;
		stp_vcopy((__xdata struct stp_vec *)&stp_msg.rroot, (__xdata struct stp_vec *)(stp_rec + 1), 8);
		stp_vcopy((__xdata struct stp_vec *)stp_msg.icost, (__xdata struct stp_vec *)(stp_rec + 9), 4);
		stp_vcopy((__xdata struct stp_vec *)&stp_msg.dbr, (__xdata struct stp_vec *)(MST_I + 57), 8);
		stp_msg.dbr.prio = stp_rec[13] & 0xf0;
		stp_msg.dbr.ext = stp_t;
		stp_msg.dpid[0] = (stp_rec[14] & 0xf0) | (STP_I->port_prio & 0x0f);
		stp_msg.dpid[1] = STP_I->port_id;
		stp_msg_hops = stp_rec[15];
		stp_msg_flags = stp_rec[0];
		stp_rcv_info(port);
		stp_reselect();
		stp_rapid_rx(port);
		stp_dispute_rx(port);
		if ((stp_msg_flags & BPDU_FLAG_TC) && !((ALT >> port) & 1))
			stp_tc_prop(port);
	}
	stp_tree(0);
	stp_vofs = 0;
}


static void stp_msti_tx(uint8_t port) __reentrant
{
	stp_tree(stp_tx_t);
	if ((stp_internal >> port) & 1) {
		stp_rec[0] = port == ROOT_PORT ? BPDU_ROLE_ROOT
			   : (((ALT | BACKUP) >> port) & 1) ? BPDU_ROLE_ALTBACK : BPDU_ROLE_DESIGNATED;
		if (port != ROOT_PORT && port_timers[PT(port)] && P2P(port)
		    && !(stp_pflags[port] & STP_PF_OPEREDGE) && !(((ALT | BACKUP) >> port) & 1))
			stp_rec[0] |= BPDU_FLAG_PROPOSAL;
		if ((AGREE >> port) & 1) {
			stp_rec[0] |= BPDU_FLAG_AGREEMENT;
			AGREE &= ~((uint16_t)1 << port);
		}
	} else {
		stp_rec[0] = STP_O->flags & BPDU_ROLE_MASK;
		if (port == stp_rport[0])
			stp_rec[0] |= BPDU_FLAG_MASTER;
	}
	stp_st = stp_state_get(port);
	if (stp_st == 0b11)
		stp_rec[0] |= BPDU_FLAG_LEARNING | BPDU_FLAG_FORWARDING;
	else if (stp_st == 0b10)
		stp_rec[0] |= BPDU_FLAG_LEARNING;
	if (stp_tcwhile[PT(port)])
		stp_rec[0] |= BPDU_FLAG_TC;
	stp_vcopy((__xdata struct stp_vec *)(stp_rec + 1), (__xdata struct stp_vec *)&RV.rroot, 8);
	stp_vcopy((__xdata struct stp_vec *)(stp_rec + 9), (__xdata struct stp_vec *)RV.icost, 4);
	stp_rec[13] = BRIDGE_PRIO;
	stp_rec[14] = stp_pprio[PT(port)];
	stp_rec[15] = stp_rhops[stp_t];
}


void stp_in(void) __banked
{
	__xdata uint8_t port;

	stp_tree(0);
	stp_vofs = 0;
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

	/* BPDU guard: an edge-facing port must never see a BPDU - shut it down.
	 * The port leaves the region's trees first: an internal port only takes
	 * the state of the tree being worked on, and this has to reach all. */
	if (stp_pflags[port] & STP_PF_BPDUGUARD) {
		print_string("STP: BPDU guard tripped, disabling port ");
		print_port_nl(port);
		stp_pflags[port] |= STP_PF_TRIPPED;
		stp_internal_update(port, 0);
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
		if (stp_trees != 1)
			stp_tc_instances(port);
		return;
	}

	/* Everything below reads the full Config/RST body. */
	if (stp_rxlen < 64)
		return;

	stp_mst_rx = stp_mst_valid();
	stp_msg_build();
	if (stp_mst_rx && stp_rstp == STP_VER_MSTP && cmpBytes(stp_msg.dbr.mac, uip_ethaddr.addr, 6)
	    && stp_region_match())
		stp_internal_update(port, 1);
	else
		stp_internal_update(port, 0);
	stp_msg_flags = STP_I->flags;
	stp_msg_rst = 0;
	if (STP_I->bpdu_type == BPDU_TYPE_RST)
		stp_msg_rst = 1;

	/* Our own BPDU coming back: two of our ports sit on one segment. Only
	 * the one with the worse Port ID stops forwarding, and only the other
	 * one writes that state, so the two never race each other.
	 */
	if (cmpBytes(stp_msg.dbr.mac, uip_ethaddr.addr, 6) == 0) {
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
		if (STP_I->port_prio != stp_pprio[PT(port)]) {
			if (STP_I->port_prio < stp_pprio[PT(port)])
				return;			/* peer is better: it decides */
		} else if (stp_loop_peer < port) {
			return;
		}
		stp_loop_hold_peer(stp_loop_peer);
		return;
	}

	if ((STP_I->flags & BPDU_FLAG_TCACK) && port == ROOT_PORT)
		stp_tcwhile[PT(port)] = 0;
	if ((STP_I->flags & BPDU_FLAG_TC) && !((ALT >> port) & 1) && stp_tc_prop(port))
		stp_tc_count++;
	if ((STP_I->flags & BPDU_FLAG_TC) && stp_trees != 1 && !((stp_internal >> port) & 1)
	    && !((ALT >> port) & 1))
		stp_tc_instances(port);

	if ((stp_pflags[port] & STP_PF_ROOTGUARD)
	    && cmpBytes((__xdata uint8_t *)&stp_msg.root, (__xdata uint8_t *)&RV.root, 8) < 0) {
		print_string("STP: root guard blocking port ");
		print_port_nl(port);
		stp_internal_update(port, 0);
		stp_state_set(port, 0b01);
		port_timers[PT(port)] = FWD_TICKS;
		stp_pflags[port] &= ~STP_PF_OPEREDGE;
		return;
	}

	stp_rcv_info(port);
	stp_reselect();

	stp_rapid_rx(port);
	stp_dispute_rx(port);
	if (stp_trees != 1)
		stp_msti_rx(port);
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
	stp_legacy &= ~stp_ent_bit;
	stp_seen_stp &= ~stp_ent_bit;
	stp_seen_rstp &= ~stp_ent_bit;
	stp_heard &= ~stp_ent_bit;
	stp_internal &= ~stp_ent_bit;
	stp_newinfo &= ~stp_ent_bit;
	stp_link_prev = (stp_link_prev & ~stp_ent_bit) | (stp_link_now & stp_ent_bit);
	stp_pflags[e] &= ~(STP_PF_OPEREDGE | STP_PF_TRIPPED);
	stp_bpdu_age[e] = 0;
	stp_rxage[e] = 0;
	stp_rxmaxage[e] = stp_maxage_s;
	stp_rxhello[e] = stp_hello_s;
	stp_rxfwd[e] = stp_fwddelay_s;
	stp_tx_budget[e] = stp_txhold;
	stp_mdelay[e] = STP_MIGRATE;
	port_hello[e] = (uint16_t)stp_hello_s * STP_HZ;
	for (stp_tt = STP_TREES; stp_tt--; ) {
		stp_tree(stp_tt);
		ALT &= ~stp_ent_bit;
		BACKUP &= ~stp_ent_bit;
		ALT_AGREED &= ~stp_ent_bit;
		AGREE &= ~stp_ent_bit;
		REROOT &= ~stp_ent_bit;
		stp_loop_held[PT(e)] = 0;
		stp_info_while[PT(e)] = 0;
		stp_tcwhile[PT(e)] = 0;
		stp_rrwhile[PT(e)] = 0;
		stp_rbwhile[PT(e)] = 0;
		port_timers[PT(e)] = 0;
	}
	if (!stp_ent_active(e))
		return;
	if (!(stp_pflags[e] & STP_PF_ENABLED) || (stp_pflags[e] & STP_PF_ADMEDGE)) {
		if (stp_pflags[e] & STP_PF_ADMEDGE)
			stp_pflags[e] |= STP_PF_OPEREDGE;
		stp_state_set(e, 0b11);
	} else {
		stp_state_set(e, 0b01);
		port_timers[PT(e)] = FWD_TICKS;
	}
}


static uint8_t stp_tree_here(void) __reentrant
{
	if (!((stp_trees >> stp_tt) & 1) || (stp_tt && !((stp_internal >> stp_i) & 1)))
		return 0;
	stp_tree(stp_tt);
	return 1;
}


static void stp_tick_timers(void) __reentrant
{
	if (stp_info_while[PT(stp_i)] && !--stp_info_while[PT(stp_i)])
		stp_reselect_due |= (uint16_t)1 << stp_t;
	if (stp_tcwhile[PT(stp_i)])
		stp_tcwhile[PT(stp_i)]--;
	if (stp_i == ROOT_PORT)
		stp_rrwhile[PT(stp_i)] = FWD_TICKS;
	else if (stp_rrwhile[PT(stp_i)] && !--stp_rrwhile[PT(stp_i)])
		REROOT &= ~((uint16_t)1 << stp_i);
	if ((BACKUP >> stp_i) & 1)
		stp_rbwhile[PT(stp_i)] = (uint16_t)2 * stp_hello_s * STP_HZ;
	else if (stp_rbwhile[PT(stp_i)])
		stp_rbwhile[PT(stp_i)]--;
}


static uint8_t stp_tx_wanted(void) __reentrant
{
	for (stp_tt = 0; stp_tt < STP_TREES; stp_tt++) {
		if (!stp_tree_here())
			continue;
		if (((AGREE >> stp_i) & 1)
		    || (!((ALT >> stp_i) & 1) && (stp_i != ROOT_PORT || stp_tcwhile[PT(stp_i)]))) {
			stp_tree(0);
			return 1;
		}
	}
	stp_tree(0);
	return 0;
}


static void stp_tick_state(void) __reentrant
{
	if (port_timers[PT(stp_i)] && ((ALT >> stp_i) & 1))
		port_timers[PT(stp_i)] = 0;

	if (!port_timers[PT(stp_i)])
		return;
	if (!--port_timers[PT(stp_i)]) {
		if (((REROOT >> stp_i) & 1) && stp_rrwhile[PT(stp_i)]) {
			port_timers[PT(stp_i)] = stp_rrwhile[PT(stp_i)] + 1;
		} else if (stp_state_get(stp_i) == 0b01) {
			stp_state_set(stp_i, 0b10);
			port_timers[PT(stp_i)] = FWD_TICKS;
			print_string("STP: port learning ");
			print_port_nl(stp_i);
		} else {
			stp_loop_held[PT(stp_i)] = 0;
			BACKUP &= ~((uint16_t)1 << stp_i);
			stp_state_set(stp_i, 0b11);
			print_string("STP: port forwarding ");
			print_port_nl(stp_i);
			stp_tc_detected(stp_i);
		}
	} else if (!stp_t && (stp_pflags[stp_i] & STP_PF_AUTOEDGE)
		   && !stp_loop_held[PT(stp_i)]
		   && !((stp_heard >> stp_i) & 1)
		   && stp_bpdu_age[stp_i] > STP_EDGE_DELAY) {
		port_timers[PT(stp_i)] = 0;
		stp_pflags[stp_i] |= STP_PF_OPEREDGE;
		stp_state_set(stp_i, 0b11);
		print_string("STP: edge port forwarding ");
		print_port_nl(stp_i);
	}
}


static void stp_trees_update(void) __reentrant
{
	stp_trees = 1;
	if (stp_rstp == STP_VER_MSTP)
		stp_trees |= mstp_used;
}


void stp_timers(void) __banked
{
	mstp_digest_step();
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
				stp_ent_bit = (uint16_t)1 << stp_i;
				stp_internal &= ~stp_ent_bit;
				stp_state_set(stp_i, 0b01);
				for (stp_tt = STP_TREES; stp_tt--; ) {
					stp_tree(stp_tt);
					ALT &= ~stp_ent_bit;
					if ((stp_link_now >> stp_i) & 1) {
						port_timers[PT(stp_i)] = FWD_TICKS;
					} else {
						port_timers[PT(stp_i)] = 0;
						stp_info_while[PT(stp_i)] = 0;
						stp_rrwhile[PT(stp_i)] = 0;
						stp_tcwhile[PT(stp_i)] = 0;
						REROOT &= ~stp_ent_bit;
						BACKUP &= ~stp_ent_bit;
						ALT_AGREED &= ~stp_ent_bit;
					}
				}
				if ((stp_link_now >> stp_i) & 1) {
					stp_pflags[stp_i] &= ~STP_PF_OPEREDGE;
					stp_bpdu_age[stp_i] = 0;
					stp_newinfo |= stp_ent_bit;
					stp_legacy &= ~stp_ent_bit;
					stp_seen_stp &= ~stp_ent_bit;
					stp_seen_rstp &= ~stp_ent_bit;
					stp_mdelay[stp_i] = STP_MIGRATE;
				} else {
					stp_heard &= ~stp_ent_bit;
					print_string("STP: link down, port blocking ");
					print_port_nl(stp_i);
					stp_forget(stp_i);
				}
			}
			stp_link_prev = stp_link_now;
		}
		for (stp_tt = 0; stp_tt < STP_TREES; stp_tt++) {
			if (!((stp_trees >> stp_tt) & 1))
				continue;
			stp_tree(stp_tt);
			stp_reselect();
		}
		stp_tree(0);
	}

	for (stp_i = 0; stp_i < STP_ENTITIES; stp_i++) {
		if (!stp_ent_active(stp_i) || !(stp_pflags[stp_i] & STP_PF_ENABLED))
			continue;

		if (stp_bpdu_age[stp_i] < 0xffff)
			stp_bpdu_age[stp_i]++;
		for (stp_tt = 0; stp_tt < STP_TREES; stp_tt++)
			if (stp_tree_here())
				stp_tick_timers();
		stp_tree(0);
		stp_migrate_check(stp_i);

		if (port_hello[stp_i])
			port_hello[stp_i]--;
		if (!port_hello[stp_i]) {
			port_hello[stp_i] = (uint16_t)stp_hello_s * STP_HZ;
			if (stp_tx_wanted())
				stp_cnf_send(stp_i);
		}
		if ((stp_newinfo >> stp_i) & 1) {
			stp_newinfo &= ~((uint16_t)1 << stp_i);
			if (stp_tx_wanted())
				stp_cnf_send(stp_i);
		}

		for (stp_tt = 0; stp_tt < STP_TREES; stp_tt++)
			if (stp_tree_here())
				stp_tick_state();
		stp_tree(0);
	}

	stp_rsel = stp_reselect_due;
	stp_reselect_due = 0;
	for (stp_tt = 0; stp_tt < STP_TREES; stp_tt++) {
		if (!((stp_rsel >> stp_tt) & 1))
			continue;
		stp_tree(stp_tt);
		stp_reselect();
	}
	stp_tree(0);
}


/* Reset all configuration to the 802.1D/802.1w defaults. Called once at boot
 * (before the startup config replays "stp ..." commands over it). */
void stp_defaults(void) __banked
{
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
		stp_pflags[stp_i] = STP_PF_ENABLED | STP_PF_AUTOEDGE;
		stp_bpdu_age[stp_i] = 0;
		port_hello[stp_i] = 0;
		stp_tx_budget[stp_i] = 6;
		stp_mdelay[stp_i] = 0;
	}
	stp_maxhops = 20;
	stp_internal = 0;
	for (stp_j = STP_TREES; stp_j--; ) {
		stp_tree(stp_j);
		BRIDGE_PRIO = 0x80;
		for (stp_i = 0; stp_i < STP_ENTITIES; stp_i++) {
			stp_pcost[PT(stp_i)] = 0;
			stp_pprio[PT(stp_i)] = 0x80;
			port_timers[PT(stp_i)] = 0;
			stp_info_while[PT(stp_i)] = 0;
			stp_tcwhile[PT(stp_i)] = 0;
			stp_rrwhile[PT(stp_i)] = 0;
			stp_rbwhile[PT(stp_i)] = 0;
			stp_loop_held[PT(stp_i)] = 0;
		}
		REROOT = 0;
		AGREE = 0;
		SYNCED_ROOT = 0xff;
		BACKUP = 0;
		ALT_AGREED = 0;
		ALT = 0;
		stp_claim_root();
	}
	stp_newinfo = 0;
	stp_legacy = 0;
	stp_seen_stp = 0;
	stp_seen_rstp = 0;
	stp_heard = 0;
	stp_tc_count = 0;
	mstp_defaults();
}


/*
 * Steer BPDUs while STP runs, and restore flooding when it stops.
 * Changing a port's PVID while STP runs needs "stp off" then "stp on".
 */
static void stp_vlan_msti_write(uint16_t vid) __reentrant
{
	sfr_data[1] = (sfr_data[1] & 0x0f) | (stp_st << 4);
	reg_write_m(RTL837x_TBL_DATA_IN_A);
	sfr_data[0] = vid >> 8;
	sfr_data[1] = vid;
	sfr_data[2] = TBL_VLAN;
	sfr_data[3] = TBL_WRITE | TBL_EXECUTE;
	reg_write_m(RTL837X_TBL_CTRL);
	do
		reg_read_m(RTL837X_TBL_CTRL);
	while (sfr_data[3] & TBL_EXECUTE);
}


static void stp_fdb_update(__xdata uint16_t pmask)
{
	__xdata uint16_t stp_fdb_vid;
	__xdata uint8_t  stp_fdb_i;

	/* Unlike LACPDUs (always untagged, so per-PVID entries suffice), BPDUs
	 * can arrive VLAN-tagged and then classify into the tag's VID - cover
	 * every VLAN that exists in the VLAN table, plus every port's PVID for
	 * the untagged case. A duplicate VID just overwrites the same slot. */
	for (stp_fdb_vid = 1; stp_fdb_vid < 4095; stp_fdb_vid++) {
		if (vlan_get(stp_fdb_vid) < 0)
			continue;
		if (!(sfr_data[0] & 0x02))	/* bit 1: VLAN table entry valid */
			continue;
		stp_st = stp_hw_msti ? mstp_vid_msti(stp_fdb_vid) : 0;
		if (((sfr_data[1] >> 4) & 0x0f) != stp_st)
			stp_vlan_msti_write(stp_fdb_vid);
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
	stp_internal = 0;
	stp_newinfo = 0;
	stp_legacy = 0;
	stp_seen_stp = 0;
	stp_seen_rstp = 0;
	stp_reselect_due = 0;
	stp_trees_update();
	for (stp_tt = STP_TREES; stp_tt--; ) {
		stp_tree(stp_tt);
		stp_claim_root();
		REROOT = 0;
		AGREE = 0;
		SYNCED_ROOT = 0xff;
		BACKUP = 0;
		ALT_AGREED = 0;
		ALT = 0;
		sfr_data[0] = sfr_data[1] = sfr_data[2] = sfr_data[3] = 0;
		sfr_data[1] |= 0x0c; // Do not block the CPU port (bits 3:2 of byte 1 = port 9)
		reg_write_m(RTL837X_MSTP_STATES + (stp_tt << 2));
	}
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
	stp_hw_msti = 0;
	if (stp_rstp == STP_VER_MSTP)
		stp_hw_msti = 1;
	stp_fdb_update(PMASK_CPU);
}


void stp_off(void) __banked
{
	for (stp_i = 0; stp_i < STP_ENTITIES; stp_i++)
		stp_pflags[stp_i] &= ~(STP_PF_OPEREDGE | STP_PF_TRIPPED);
	for (stp_tt = STP_TREES; stp_tt--; ) {
		stp_tree(stp_tt);
		sfr_data[0] = sfr_data[1] = sfr_data[2] = sfr_data[3] = 0;
		for (stp_i = machine.min_port; stp_i <= machine.max_port; stp_i++)
			sfr_data[3 - (stp_i >> 2)] |= (uint8_t)(0b11 << ((stp_i << 1) & 0x7));
		sfr_data[1] |= 0x0c; // Do not block the CPU port (bits 3:2 of byte 1 = port 9)
		reg_write_m(RTL837X_MSTP_STATES + (stp_tt << 2));
		for (stp_i = 0; stp_i < STP_ENTITIES; stp_i++) {
			stp_loop_held[PT(stp_i)] = 0;
			port_timers[PT(stp_i)] = 0;
		}
	}
	stp_trees = 1;
	stp_hw_msti = 0;

	if (stp_bpdu_filter)
		stp_fdb_update(PMASK_CPU);
	else
		stp_fdb_update(PMASK_CPU | (machine_detected.isRTL8373 ? PMASK_9 : PMASK_6));
}


void stp_port_admin(uint8_t port, uint8_t on) __banked
{
	/* Either way the port starts over outside the region: a port with STP
	 * off must not keep an internal bit that a later instance would turn
	 * into a listen period nobody ends. */
	stp_internal_update(port, 0);
	for (stp_tt = 0; stp_tt < STP_TREES; stp_tt++) {
		if (!((stp_trees >> stp_tt) & 1))
			continue;
		stp_tree(stp_tt);
		if (on) {
			stp_state_reg(stp_tt, port, 0b01);
			port_timers[PT(port)] = FWD_TICKS;
		} else {
			stp_state_reg(stp_tt, port, 0b11);
		}
	}
	stp_tree(0);
}


void stp_mstp_changed(void) __banked
{
	if (!stp_enabled || stp_rstp != STP_VER_MSTP)
		return;
	stp_rsel = stp_trees;
	stp_trees_update();
	stp_st = 0;
	for (stp_tt = 1; stp_tt < STP_TREES; stp_tt++) {
		if (!((stp_trees & ~stp_rsel) >> stp_tt & 1))
			continue;
		stp_tree(stp_tt);
		stp_claim_root();
		REROOT = 0;
		AGREE = 0;
		SYNCED_ROOT = 0xff;
		BACKUP = 0;
		ALT_AGREED = 0;
		ALT = 0;
		for (stp_i = 0; stp_i < STP_ENTITIES; stp_i++) {
			stp_info_while[PT(stp_i)] = 0;
			stp_tcwhile[PT(stp_i)] = 0;
			stp_rrwhile[PT(stp_i)] = 0;
			stp_rbwhile[PT(stp_i)] = 0;
			stp_loop_held[PT(stp_i)] = 0;
			port_timers[PT(stp_i)] = 0;
			if (!stp_ent_active(stp_i))
				continue;
			if ((stp_internal >> stp_i) & 1) {
				stp_state_reg(stp_tt, stp_i, 0b01);
				port_timers[PT(stp_i)] = FWD_TICKS;
			} else {
				stp_tree(0);
				stp_st = stp_state_get(stp_i);
				stp_tree(stp_tt);
				stp_state_reg(stp_tt, stp_i, stp_st);
			}
		}
	}
	stp_tree(0);
	stp_hw_msti = 1;
	stp_fdb_update(PMASK_CPU);
}


uint8_t stp_port_role(uint8_t port) __banked
{
	if (!(stp_pflags[port] & STP_PF_ENABLED) || (stp_pflags[port] & STP_PF_TRIPPED)
	    || !((stp_link_prev >> port) & 1))
		return 0;
	if (stp_t && !((stp_internal >> port) & 1)) {
		if (port == stp_rport[0])
			return 5;
		if ((stp_backups[0] >> port) & 1)
			return 4;
		if ((stp_alts[0] >> port) & 1)
			return 3;
		return 2;
	}
	if (port == ROOT_PORT)
		return 1;
	if ((BACKUP >> port) & 1)
		return 4;
	if ((ALT >> port) & 1)
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


void stp_tree_prio(uint8_t t) __banked
{
	stp_tree(t);
	if (ROOT_PORT == 0xff)
		stp_claim_root();
	stp_tree(0);
}


void stp_prio_apply(void) __banked
{
	stp_tree_prio(0);
}


void stp_vlan_new(void) __banked
{
	if (!stp_hw_msti || vlan_get(vlan_settings.vlan) < 0 || !(sfr_data[0] & 0x02))
		return;
	stp_st = mstp_vid_msti(vlan_settings.vlan);
	if (((sfr_data[1] >> 4) & 0x0f) != stp_st)
		stp_vlan_msti_write(vlan_settings.vlan);
}
