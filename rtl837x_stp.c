/*
 * This is a driver implementation for the Spanning Tree Protocol features for the RTL837x platform
 * This code is in the Public Domain
 */

// #define REGDBG
// #define DEBUG

#pragma codeseg BANK2
#pragma constseg BANK2

#include <stdint.h>
#include "rtl837x_common.h"
#include "rtl837x_sfr.h"
#include "rtl837x_regs.h"
#include "rtl837x_stp.h"
#include "rtl837x_port.h"	/* port_pvid_get(), port_l2mc_set() */
#include "uip.h"
#include "machine.h"

extern __code struct machine machine;
extern __xdata uint8_t sfr_data[4];
extern __xdata struct machine_runtime machine_detected;	/* owned by rtl837x_port.c */

__xdata uint16_t stp_fdb_vid;
__xdata uint8_t  stp_fdb_i;

extern __xdata struct uip_eth_addr uip_ethaddr;

extern __xdata uint8_t uip_buf[UIP_CONF_BUFFER_SIZE + 2];
extern __xdata uint16_t management_vlan;	/* owned by rtlplayground.c; suppressed per-frame for BPDUs */

/* CLI tokenizer state + helpers (owned by cmd_parser.c, HOME bank) */
extern __xdata uint8_t cmd_buffer[CMD_BUF_SIZE];
extern __xdata uint8_t cmd_words_len;
extern __xdata uint8_t cmd_words_b[15];
extern __xdata char save_cmd;		/* 0 while execute_config() replays the saved config */
uint8_t cmd_compare(uint8_t start, __code uint8_t * cmd);
uint8_t atoi_byte(__xdata uint8_t *out, uint8_t idx);

/* ---- Configuration ---- */
__xdata uint8_t  stp_prio;	/* bridge priority high byte (0x80 = 32768) */
__xdata uint8_t  stp_hello_s;
__xdata uint8_t  stp_maxage_s;
__xdata uint8_t  stp_fwddelay_s;
__xdata uint8_t  stp_rstp;
__xdata uint8_t  stp_txhold;


__xdata uint8_t  stp_ent_of[10];
__xdata uint16_t stp_lag_mask[STP_LAG_COUNT];
__xdata uint8_t  stp_ss_i;
__xdata uint8_t  stp_map_dirty;
__xdata uint8_t  stp_pflags[STP_ENTITIES];
__xdata uint32_t stp_pcost[STP_ENTITIES];
__xdata uint8_t  stp_pprio[STP_ENTITIES];
__xdata uint8_t  stp_pp2p[STP_ENTITIES];

__xdata struct bridge stp_dbridge[STP_ENTITIES];
__xdata uint16_t stp_dpid[STP_ENTITIES];
__xdata uint32_t stp_dcost[STP_ENTITIES];

/* ---- Status / runtime ---- */
__xdata struct bridge root_bridge;
__xdata uint32_t root_bridge_cost;	/* our cost to the root (rx cost + root port cost) */
__xdata uint8_t  stp_root_port;		/* 0xff = we are the root */
__xdata uint16_t stp_tc_count;
__xdata uint16_t stp_scratch16;	/* scratch for status printing only */
__xdata uint8_t  stp_st_of;	/* which port's state a status row shows */

__xdata uint16_t port_timers[STP_ENTITIES];	/* listen-period countdown (0 = not listening) */
__xdata uint16_t port_hello[STP_ENTITIES];	/* hello TX countdown */
__xdata uint16_t stp_bpdu_age[STP_ENTITIES];	/* ticks since last BPDU seen on port (saturating) */
__xdata uint8_t  stp_tx_budget[STP_ENTITIES];	/* tx hold: BPDUs left in the current second */
__xdata uint8_t  stp_tx_count[STP_ENTITIES];	/* BPDUs actually put on the wire, wraps at 256 */
__xdata uint16_t stp_sec_tick;		/* 1 s window for the tx budget */
__xdata uint16_t stp_link_prev;		/* carrier bitmap as of the last check */
__xdata uint16_t stp_link_now;

__xdata uint8_t  stp_scratch;
__xdata uint8_t  stp_tx_flags_extra;	/* one-shot flags OR-ed into the next BPDU (TCA) */
__xdata uint16_t stp_rxlen;		/* received frame length, saved before uip_len is consumed */
__xdata uint8_t  stp_msg_age;		/* message age of the root info we hold, seconds */
__xdata uint16_t stp_tc_while;		/* ticks left to set the TC flag in our BPDUs */
__xdata uint8_t  stp_i;
__xdata uint32_t stp_cost_scratch;
__xdata uint8_t  stp_loop_peer;		/* the other own port seen on a looped segment */

#define STP_EDGE_DELAY	(3 * STP_HZ)	/* auto-edge: forward after 3 s without BPDU */

#define AUTO_COST	20000UL		/* path cost used when stp_pcost == 0 (1G default) */
#define PCOST(i)	(stp_pcost[i] ? stp_pcost[i] : AUTO_COST)

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

/* Console messages name the port on the front panel, not the internal index. */
static void print_port_nl(uint8_t port) __reentrant
{
	print_byte(machine.log_to_phys_port[port]);
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
static __code const char stp_role_txt[]  = "desgroot";
static __code const char stp_edge_txt[]  = "no  yes ";

static uint8_t stp_ent_active(uint8_t e) __reentrant;

static void print_field(__code const char *txt, uint8_t idx, uint8_t width) __reentrant
{
	txt += idx * width;
	while (width--)
		write_char(*txt++);
}


/* Where you look when the tree is not what you expected. */
static void stp_status(void)
{
	if (!stpEnabled) {
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
		print_byte(machine.log_to_phys_port[stp_root_port]);
		print_string(" cost ");
		print_long(root_bridge_cost);
		write_char('\n');
	}
	print_string("changes ");
	print_short(stp_tc_count);
	write_char('\n');
	print_string("port state role edge tx bpdu\n");
	reg_read_m(RTL837X_MSTP_STATES);
	for (stp_i = 0; stp_i < STP_ENTITIES; stp_i++) {
		if (!stp_ent_active(stp_i))
			continue;	/* a member is spoken for by its LAG */
		write_char(' ');
		if (stp_i >= STP_LAG_BASE) {
			write_char('L');
			write_char('1' + stp_i - STP_LAG_BASE);
		} else {
			print_byte(machine.log_to_phys_port[stp_i]);
		}
		print_string("  ");
		/* The ASIC holds no state for a LAG, so show a member's, the
		 * same one the JSON reports. They are kept in step by
		 * stp_state_set(), which writes every member together. */
		stp_st_of = stp_i;
		if (stp_i >= STP_LAG_BASE) {
			stp_st_of = 0;
			while (stp_st_of < 10 && !((stp_lag_mask[stp_i - STP_LAG_BASE] >> stp_st_of) & 1))
				stp_st_of++;
			if (stp_st_of >= 10)
				stp_st_of = 0;
		}
		print_field(stp_state_txt, (sfr_data[3 - (stp_st_of >> 2)] >> ((stp_st_of << 1) & 0x7)) & 0x3, 5);
		write_char(' ');
		/* Only the root port is named. Everything else reads as designated
		 * because that is all the state machine tracks today; an alternate
		 * port is a designated one that happens to sit in blocking. */
		print_field(stp_role_txt, stp_i == stp_root_port ? 1 : 0, 4);
		write_char(' ');
		print_field(stp_edge_txt, stp_pflags[stp_i] & STP_PF_OPEREDGE ? 1 : 0, 4);
		/* BPDUs we put on the wire here. A designated port must show this
		 * climbing once per hello; the root port never does, because we do
		 * not announce back towards the root. Without it the only way to
		 * tell "we are silent" from "the neighbour is not listening" is a
		 * capture on the far side. */
		write_char(' ');
		print_byte(stp_tx_count[stp_i]);
		/* Seconds since the last BPDU on this port, capped at 255. Without
		 * it nothing in the output separates "nobody is speaking (R)STP
		 * out there" from "we are dropping what arrives", and stp_in()
		 * leaves on eight different conditions without saying so. */
		write_char(' ');
		stp_scratch16 = stp_bpdu_age[stp_i] / STP_HZ;
		itoa(stp_scratch16 > 255 ? 255 : (uint8_t)stp_scratch16);
		write_char('\n');
	}
}


/* __reentrant so the temporaries land on the stack: stp_in() is __banked and
 * its locals get exclusive internal RAM, which is what runs out first here. */
static void stp_record_designated(uint8_t port) __reentrant
{
	stp_dbridge[port].prio = STP_I->bridge.prio;
	stp_dbridge[port].ext = STP_I->bridge.ext;
	memcpy(stp_dbridge[port].mac, STP_I->bridge.mac, 6);
	stp_dpid[port] = ((uint16_t)STP_I->port_prio << 8) | STP_I->port_id;
	stp_cost_scratch = STP_I->root_path_cost;
	stp_dcost[port] = ((stp_cost_scratch & 0xff) << 24)
			| ((stp_cost_scratch & 0xff00) << 8)
			| ((stp_cost_scratch >> 8) & 0xff00)
			| (stp_cost_scratch >> 24);
}


/* Lexicographic compare of n bytes. A MAC is 6 of them; a Bridge Identifier
 * is 8, the two priority octets ahead of the MAC, compared as one unsigned
 * number per 802.1D. */
signed char cmpBytes(__xdata uint8_t *m1, __xdata uint8_t *m2, uint8_t n) __reentrant
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


/* Write one port's 2-bit state into the ASIC's MSTP register.
 * 00 disable, 01 blocking, 10 learning, 11 forwarding. */
static void stp_lag_map(void)
{
	for (stp_ss_i = 0; stp_ss_i < 10; stp_ss_i++)
		stp_ent_of[stp_ss_i] = stp_ss_i;
	for (stp_scratch = 0; stp_scratch < STP_LAG_COUNT; stp_scratch++) {
		reg_read_m(RTL837X_TRK_MBR_CTRL_BASE + (stp_scratch << 2));
		if (stp_lag_mask[stp_scratch] != (((uint16_t)sfr_data[2] << 8) | sfr_data[3]))
			stp_map_dirty = 1;
		stp_lag_mask[stp_scratch] = ((uint16_t)sfr_data[2] << 8) | sfr_data[3];
		for (stp_ss_i = 0; stp_ss_i < 10; stp_ss_i++)
			if ((stp_lag_mask[stp_scratch] >> stp_ss_i) & 1)
				stp_ent_of[stp_ss_i] = STP_LAG_BASE + stp_scratch;
	}
}


static uint8_t stp_ent_active(uint8_t e) __reentrant
{
	if (e >= STP_LAG_BASE)
		return stp_lag_mask[e - STP_LAG_BASE] != 0;
	return e >= machine.min_port && e <= machine.max_port && stp_ent_of[e] == e;
}


static uint8_t stp_ent_has(uint8_t ent, uint8_t p) __reentrant
{
	if (ent < STP_LAG_BASE)
		return ent == p;
	return (stp_lag_mask[ent - STP_LAG_BASE] >> p) & 1;
}


static uint8_t stp_state_port(uint8_t e) __reentrant
{
	if (e < STP_LAG_BASE)
		return e;
	for (stp_ss_i = 0; stp_ss_i < 10; stp_ss_i++)
		if ((stp_lag_mask[e - STP_LAG_BASE] >> stp_ss_i) & 1)
			return stp_ss_i;
	return 0;
}


static void stp_state_bits(uint8_t port, uint8_t state) __reentrant
{
	for (stp_ss_i = 0; stp_ss_i < 10; stp_ss_i++)
		if (stp_ent_has(port, stp_ss_i))
			sfr_data[3 - (stp_ss_i >> 2)] |= (uint8_t)(state << ((stp_ss_i << 1) & 0x7));
}


static void stp_state_set(uint8_t port, uint8_t state) __reentrant
{
	reg_read_m(RTL837X_MSTP_STATES);
	for (stp_ss_i = 0; stp_ss_i < 10; stp_ss_i++) {
		if (!stp_ent_has(port, stp_ss_i))
			continue;
		stp_scratch = 3 - (stp_ss_i >> 2);
		sfr_data[stp_scratch] &= ~(uint8_t)(0b11 << ((stp_ss_i << 1) & 0x7));
		sfr_data[stp_scratch] |= (uint8_t)(state << ((stp_ss_i << 1) & 0x7));
	}
	reg_write_m(RTL837X_MSTP_STATES);
}


/* Signal a topology change. Edge ports are exempt. */
static void stp_topology_change(uint8_t port) __reentrant
{
	if (stp_pflags[port] & STP_PF_OPEREDGE)
		return;
	stp_tc_count++;
	stp_tc_while = ((uint16_t)stp_maxage_s + stp_fwddelay_s) * STP_HZ;
	port_l2_forget_port(port);
}


/* Hold one port out of forwarding because a loop was seen on it, and keep
 * holding it for as long as the caller keeps saying so. The caller is the
 * port that won the Port ID compare (see stp_in) - a different port than
 * the one held, except when the frame came back on the port it left.
 */
static void stp_loop_hold_peer(uint8_t port) __reentrant
{
	/* The port number arrives in a BPDU, so it is somebody else's data,
	 * and our own bridge MAC is public in every BPDU we send - a forged
	 * frame can name any port it likes. Bound it to the ports this module
	 * actually manages, like every other loop here does. Out of that
	 * range nothing would ever release the block either: stp_timers()
	 * walks min_port..max_port and skips ports that are not STP-enabled,
	 * so their port_timers[] never counts down. Naming the CPU port would
	 * otherwise cost us our own management path. */
	if (port < machine.min_port || port > machine.max_port)
		return;
	if (!(stp_pflags[port] & STP_PF_ENABLED))
		return;
	if (stp_pflags[port] & STP_PF_TRIPPED)
		return;
	if (!port_timers[port]) {		/* not held down yet */
		print_string("STP: loop detected, blocking port ");
		print_port_nl(port);
		stp_state_set(port, 0b01);
		stp_pflags[port] &= ~STP_PF_OPEREDGE;
		stp_topology_change(port);
	}
	port_timers[port] = (uint16_t)stp_fwddelay_s * STP_HZ;
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


void stp_cnf_send(uint8_t port) __reentrant
{
	/* A one-shot flag (TCA) belongs to the BPDU we were asked to send: drop
	 * it with the frame, or it would surface on an unrelated port later. */
	if (!(stp_pflags[port] & STP_PF_ENABLED) || (stp_pflags[port] & (STP_PF_FILTER | STP_PF_TRIPPED))) {
		stp_tx_flags_extra = 0;
		return;
	}
	if (!stp_tx_budget[port]) {	/* tx hold count exhausted for this second */
		stp_tx_flags_extra = 0;
		return;
	}
	stp_tx_budget[port]--;
	stp_tx_count[port]++;

	STP_O->stp_addr[0] = 0x01; STP_O->stp_addr[1] = 0x80; STP_O->stp_addr[2] = 0xc2;
	STP_O->stp_addr[3] = STP_O->stp_addr[4] = STP_O->stp_addr[5] = 0x00;

	STP_O->rtl_tag.tag = HTONS(RTL_FRAME_TAG_ID);
	STP_O->rtl_tag.version = RTL_FRAME_TAG_VERSION;
	STP_O->rtl_tag.reason = 0x00;
	/* Through HTONS like every tag field: raw 0x0020 lands on the wire as
	 * 0x2000 (EFID), the ASIC fails to parse the tag and floods the frame
	 * with the 0x8899 header still on it (same bug class as LACP had).
	 * NOTE: no RTL_TAG_KEEP here - hardware-verified that KEEP on an
	 * LLC/802.3 (length-field) frame makes the ASIC drop it entirely,
	 * while the same flag works fine on ethertype frames (LACP). */
	STP_O->rtl_tag.flags = HTONS(RTL_TAG_LEARN_DIS);
	if (port >= STP_LAG_BASE) {
		stp_scratch = 0;
		while (stp_scratch < 10 && !stp_ent_has(port, stp_scratch))
			stp_scratch++;
		if (stp_scratch >= 10) {
			stp_tx_flags_extra = 0;
			return;
		}
		STP_O->rtl_tag.pmask = HTONS(((uint16_t)1) << stp_scratch);
	} else {
		STP_O->rtl_tag.pmask = HTONS(((uint16_t)1) << port);
	}

	STP_O->dsap = 0x42;
	STP_O->ssap = 0x42;
	STP_O->ctrl = 0x03;
	STP_O->proto = 0x0000;
	if (stp_rstp) {
		/* 802.3 length = LLC (3) + RST BPDU body (36, incl. version1_length) */
		STP_O->msg_len = HTONS(0x27);
		STP_O->version = 0x02;		/* RSTP */
		STP_O->bpdu_type = 0x02;	/* Rapid Spanning Tree BPDU */
		/* Flags describe this port, so derive them instead of announcing
		 * designated+learning+forwarding unconditionally: a blocked port
		 * claiming to forward, or the root port claiming designated, is a
		 * lie on the wire even when nothing downstream acts on it (yet).
		 * Role is root on the root port and designated everywhere else -
		 * there is no alternate/backup role computation, so a port blocked
		 * by loop detection still transmits as designated, just with the
		 * learning and forwarding bits clear. Those two mirror the ASIC
		 * state (0b11 = forwarding); a listening or blocked port sends
		 * neither. */
		reg_read_m(RTL837X_MSTP_STATES);
		stp_scratch = stp_state_port(port);
		STP_O->flags = (uint8_t)((port == stp_root_port ? 0b10 : 0b11) << 2);
		if (((sfr_data[3 - (stp_scratch >> 2)] >> ((stp_scratch << 1) & 0x7)) & 0b11) == 0b11)
			STP_O->flags |= 0x30;	/* learning + forwarding */
	} else {
		/* 802.3 length = LLC (3) + Config BPDU body (35) */
		STP_O->msg_len = HTONS(0x26);
		STP_O->version = 0x00;		/* legacy STP */
		STP_O->bpdu_type = 0x00;	/* Config BPDU */
		STP_O->flags = 0x00;
	}
	if (stp_tc_while)
		STP_O->flags |= 0x01;		/* Topology Change */
	STP_O->flags |= stp_tx_flags_extra;	/* e.g. TCA in reply to a TCN */
	stp_tx_flags_extra = 0;

	memcpy(STP_O->src_addr, uip_ethaddr.addr, 6);
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
	STP_O->age_max = stp_maxage_s;
	STP_O->hello = stp_hello_s;
	STP_O->fwd_delay = stp_fwddelay_s;
	STP_O->version1_length = 0;	/* RST BPDU: no version-1 information */

	/* BPDUs are link-local and must egress untagged: with a management VLAN
	 * set, tcpip_output() splices an 802.1Q tag after the SA, shifting the
	 * in-frame rtl_tag out of the position the ASIC parses - the CPU tag then
	 * leaks onto the wire as 0x8899 and the BPDU is flooded, not sent.
	 * Hardware-verified fix, same as lacp_send(). */
	{
	uint16_t saved_mgmt_vlan = management_vlan;
	management_vlan = 0;
	/* A legacy Config BPDU body is 35 bytes - without the trailing
	 * version-1 length byte that only the RST BPDU (36 bytes) carries. */
	uip_len = stp_rstp ? sizeof(struct stp_pkt) : sizeof(struct stp_pkt) - 1;
	tcpip_output();
	management_vlan = saved_mgmt_vlan;
	}
}


void stp_in(void) __banked
{
	/* Robustness: never read fields past the received frame. 33 covers the
	 * header through bpdu_type; the full Config/RST body is re-checked below.
	 * (uip_len is consumed and zeroed at the end - keep a local view.) */
	if (uip_len < 33) {
		uip_len = 0;
		return;
	}
	stp_rxlen = uip_len;

	// By default we do not send anything out (handle_rx would TX otherwise)
	uip_len = 0;

	/* Ingress port: low nibble of the CPU tag's pmask on RX */
	stp_scratch = ((uint8_t)HTONS(STP_I->rtl_tag.pmask)) & 0x0f;
	if (stp_scratch < machine.min_port || stp_scratch > machine.max_port)
		return;
	{
	__xdata static uint8_t port_l;	/* NOT stp_scratch: stp_state_set() clobbers it */
	uint8_t port = (port_l = stp_ent_of[stp_scratch]);
	(void)port_l;

	// Make sure this is the type of (R)STP packet we are interested in:
	if (!(STP_I->dsap == 0x42 && STP_I->ssap == 0x42 && STP_I->ctrl == 0x03))
		return;
	if (STP_I->proto)
		return;
	/* Accept RSTP BPDUs (v2 type 2), legacy Config BPDUs (v0 type 0) and
	 * legacy TCN BPDUs (v0 type 0x80, 4-byte body).
	 * Version 2 *or greater*: 802.1D-2004 14.4 requires an RSTP bridge to
	 * accept a higher Protocol Version and treat it as RST, ignoring what
	 * it does not understand. MSTP (802.1s) sends version 3 type 2 with a
	 * prefix deliberately identical to an RST BPDU for exactly this reason;
	 * insisting on == 2 makes us blind to every MST bridge on the segment. */
	if (!((STP_I->version >= 2 && STP_I->bpdu_type == 2)
	      || (STP_I->version == 0
	          && (STP_I->bpdu_type == 0 || STP_I->bpdu_type == 0x80))))
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

	stp_bpdu_age[port] = 0;

	/* A port that hears a BPDU is not an edge port, whatever it decided
	 * during the silence after the link came up. Only the flag is dropped:
	 * the port keeps whatever forwarding state the rules below give it,
	 * rather than being pushed back through the listen period, which would
	 * black-hole a working link for a forward delay on the first BPDU. The
	 * flag matters beyond the status page, since stp_topology_change()
	 * exempts edge ports and so would go on skipping the counter and the
	 * L2 flush for a port that has a bridge behind it. */
	stp_pflags[port] &= ~STP_PF_OPEREDGE;

	if (STP_I->bpdu_type == 0x80) {
		/* TCN: a downstream bridge reports a topology change. Acknowledge it
		 * on this port so the sender stops repeating, then treat it like a
		 * change of our own: flush the port and carry the TC flag on the
		 * designated ports for the full window. No TCN goes towards the
		 * root, so a legacy root further up keeps its normal aging. */
		stp_tx_flags_extra = 0x80;	/* Topology Change Acknowledgment */
		stp_cnf_send(port);		/* transmits internally */
		uip_len = 0;			/* ...so handle_rx must not TX again */
		stp_topology_change(port);
		return;
	}

	/* Everything below reads the full Config/RST body. */
	if (stp_rxlen < 64)
		return;

	/* Our own BPDU coming back at us: two of our ports sit on one segment.
	 * Only the one with the worse Port ID has to stop forwarding - 802.1D
	 * calls it a backup port. Blocking both, as we used to, kills a segment
	 * that can still carry traffic, and worse, leaves nobody forwarding to
	 * hear the loop: both then time out of blocking together and the pair
	 * oscillates for as long as the cable is in (measured: a topology change
	 * every ~4 s).
	 *
	 * The port with the better Port ID decides for both and is the only one
	 * that touches state - the other just drops the frame. One writer is
	 * what makes this safe: while both were still deciding for themselves,
	 * the winner's re-arm landed in the loser's port_timers[] first, the
	 * loser then read it as "already blocked" and skipped its own
	 * stp_state_set(), and the loop stayed open. Whether that happened came
	 * down to which frame the switch handed us first.
	 *
	 * The winner is forwarding by construction (nothing here ever blocks
	 * it), so it goes on hearing the loop and re-arms the loser's timer on
	 * every BPDU - that is what makes the block a latch rather than a
	 * forward-delay pulse, and it needs no assumption about a blocked port
	 * still receiving. Pull the cable and the re-arming stops, so the loser
	 * comes back on its own after a forward delay - and the link
	 * supervision above gets there first anyway. */
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

	/* Topology Change in transit. The flag arms a short window that our
	 * own BPDUs copy downstream (the TX side already sends TC while
	 * stp_tc_while runs) and that each further flagged BPDU refreshes, so
	 * it expires one hello after the neighbour stops - without shortening
	 * the long window a local change may have armed. The flush runs once,
	 * on the arming edge: everything learned on the other non-edge ports
	 * may sit behind the moved link and must be relearned. */
	if (STP_I->flags & 0x01) {
		if (!stp_tc_while) {
			uint8_t i;
			stp_tc_count++;
			for (i = machine.min_port; i <= machine.max_port; i++)
				if (i != port && (stp_pflags[i] & STP_PF_ENABLED)
				    && !(stp_pflags[i] & STP_PF_OPEREDGE))
					port_l2_forget_port(i);
		}
		if (stp_tc_while < ((uint16_t)stp_hello_s + 1) * STP_HZ)
			stp_tc_while = ((uint16_t)stp_hello_s + 1) * STP_HZ;
	}

	stp_record_designated(port);

	/* Better root than the one we know? The identifier is priority, system
	 * ID extension and MAC in that order: comparing the priority byte and
	 * then jumping to the MAC skipped the twelve bits in between, so two
	 * bridges differing only in the extension were ranked by MAC. */
	if (cmpBytes((__xdata uint8_t *)&STP_I->root, (__xdata uint8_t *)&root_bridge, 8) < 0) {
		/* Root guard: this port must never become our path to the root. */
		if (stp_pflags[port] & STP_PF_ROOTGUARD) {
			print_string("STP: root guard blocking port ");
			print_port_nl(port);
			stp_state_set(port, 0b01);
			port_timers[port] = (uint16_t)stp_fwddelay_s * STP_HZ;
			stp_pflags[port] &= ~STP_PF_OPEREDGE;
			return;
		}
		print_string("Updating Root bridge\n");
		root_bridge.prio = STP_I->root.prio;
		root_bridge.ext = STP_I->root.ext;
		memcpy(root_bridge.mac, STP_I->root.mac, 6);
		stp_root_port = port;
		stp_tc_count++;
	}

	/* Refresh our cost to the root when the update comes in on the root port */
	if (port == stp_root_port) {
		/* Age of the information we now hold (see the TX note on the wire
		 * format); saturate rather than wrap on absurd input. */
		stp_msg_age = (STP_I->age > 254) ? 254 : (uint8_t)STP_I->age;
		root_bridge_cost = stp_dcost[port] + PCOST(port);
	}
	}
}


void stp_timers(void) __banked
{
	/* Refill the per-port tx budgets once per second (tx hold count) */
	if (++stp_sec_tick >= STP_HZ) {
		stp_sec_tick = 0;
		for (stp_i = 0; stp_i < STP_ENTITIES; stp_i++)
			stp_tx_budget[stp_i] = stp_txhold;

		stp_lag_map();
		if (stp_map_dirty) {
			stp_map_dirty = 0;
			for (stp_i = STP_LAG_BASE; stp_i < STP_ENTITIES; stp_i++)
				if (stp_lag_mask[stp_i - STP_LAG_BASE])
					stp_state_set(stp_i, port_timers[stp_i] ? 0b01 : 0b11);
		}

		/* Link supervision. Without this the state machine never learns
		 * that a port lost carrier: it keeps the port in forwarding, keeps
		 * announcing on it, and never flushes what was learned behind it -
		 * yet losing a link is the most ordinary topology change there is.
		 * Once per second is soon enough, and it keeps register reads out
		 * of the 50 Hz tick. */
		reg_read_m(RTL837X_REG_LINKS_STS);
		stp_link_now = (uint16_t)sfr_data[1] | ((uint16_t)sfr_data[2] << 8);
		{
			__xdata static uint16_t eff;
			eff = 0;
			for (stp_i = machine.min_port; stp_i <= machine.max_port; stp_i++)
				if ((stp_link_now >> stp_i) & 1)
					eff |= (uint16_t)1 << stp_ent_of[stp_i];
			stp_link_now = eff;
		}
		if (stp_link_now != stp_link_prev) {
			for (stp_i = 0; stp_i < STP_ENTITIES; stp_i++) {
				if (stp_i < 10 && (stp_i < machine.min_port || stp_i > machine.max_port))
					continue;
				if (stp_i < 10 && stp_ent_of[stp_i] != stp_i)
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
					port_timers[stp_i] = (uint16_t)stp_fwddelay_s * STP_HZ;
					stp_pflags[stp_i] &= ~STP_PF_OPEREDGE;
					stp_bpdu_age[stp_i] = 0;
				} else {
					port_timers[stp_i] = 0;
					print_string("STP: link down, port blocking ");
					print_port_nl(stp_i);
					stp_topology_change(stp_i);
				}
			}
			stp_link_prev = stp_link_now;
		}
	}

	for (stp_i = 0; stp_i < STP_ENTITIES; stp_i++) {
		if (!stp_ent_active(stp_i) || !(stp_pflags[stp_i] & STP_PF_ENABLED))
			continue;

		if (stp_bpdu_age[stp_i] < 0xffff)
			stp_bpdu_age[stp_i]++;

		/* Periodic hello */
		if (port_hello[stp_i])
			port_hello[stp_i]--;
		if (!port_hello[stp_i]) {
			port_hello[stp_i] = (uint16_t)stp_hello_s * STP_HZ;
			/* Only designated ports announce periodically: the root port is
			 * where our own root information comes FROM, and echoing it back
			 * there just feeds the upstream bridge its own data (and looks
			 * like a competing designated bridge on that segment). */
			if (stp_i != stp_root_port)
				stp_cnf_send(stp_i);
		}

		/* Promote a port out of blocking once its listen period expires
		 * with no reason to stay blocked (no better root heard: we are
		 * the designated bridge on that port). */
		if (port_timers[stp_i]) {
			if (!--port_timers[stp_i]) {
				stp_state_set(stp_i, 0b11);
				print_string("STP: port forwarding ");
				print_port_nl(stp_i);
				stp_topology_change(stp_i);
			} else if ((stp_pflags[stp_i] & STP_PF_AUTOEDGE)
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

	if (stp_tc_while)
		stp_tc_while--;

	/* Age out a root that went silent: reclaim the tree. */
	if (stp_root_port != 0xff
	    && stp_bpdu_age[stp_root_port] > (uint16_t)stp_maxage_s * STP_HZ) {
		print_string("STP: root aged out, claiming root\n");
		stp_claim_root();
		stp_tc_count++;
	}
}


/* Reset all configuration to the 802.1D/802.1w defaults. Called once at boot
 * (before the startup config replays "stp ..." commands over it). */
void stp_defaults(void) __banked
{
	stp_prio = 0x80;	/* 32768 */
	stp_hello_s = 2;
	stp_maxage_s = 20;
	stp_fwddelay_s = 15;
	stp_rstp = 1;
	stp_txhold = 6;
	for (stp_i = 0; stp_i < 10; stp_i++)
		stp_ent_of[stp_i] = stp_i;
	for (stp_i = 0; stp_i < STP_LAG_COUNT; stp_i++)
		stp_lag_mask[stp_i] = 0;
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
	}
	stp_tc_count = 0;
	stp_tc_while = 0;
	stp_claim_root();
}


/*
 * Steer BPDUs while STP runs, and restore flooding when it stops.
 * Changing a port's PVID while STP runs needs "stp off" then "stp on".
 */
static void stp_fdb_update(__xdata uint16_t pmask)
{
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


void stp_setup(void) __banked
{
	print_string("Enabling STP: ");
	stp_lag_map();
	sfr_data[0] = sfr_data[1] = sfr_data[2] = sfr_data[3] = 0;
	for (stp_i = 0; stp_i < STP_ENTITIES; stp_i++) {
		if (stp_i < 10 && (stp_i < machine.min_port || stp_i > machine.max_port))
			continue;
		if (stp_i >= STP_LAG_BASE && !stp_lag_mask[stp_i - STP_LAG_BASE])
			continue;
		stp_pflags[stp_i] &= ~(STP_PF_OPEREDGE | STP_PF_TRIPPED);
		stp_bpdu_age[stp_i] = 0;
		stp_tx_budget[stp_i] = stp_txhold;
		stp_tx_count[stp_i] = 0;
		port_hello[stp_i] = (uint16_t)stp_hello_s * STP_HZ;
		if (stp_i < 10 && stp_ent_of[stp_i] != stp_i)
			continue;	/* member: the LAG entity decides */
		if (!(stp_pflags[stp_i] & STP_PF_ENABLED)
		    || (stp_pflags[stp_i] & STP_PF_ADMEDGE)) {
			/* not participating, or admin edge: forwarding immediately */
			if (stp_pflags[stp_i] & STP_PF_ADMEDGE)
				stp_pflags[stp_i] |= STP_PF_OPEREDGE;
			stp_state_bits(stp_i, 0b11);
			port_timers[stp_i] = 0;
		} else {
			/* listen first: blocking until the forward-delay expires */
			stp_state_bits(stp_i, 0b01);
			port_timers[stp_i] = (uint16_t)stp_fwddelay_s * STP_HZ;
		}
	}
	sfr_data[1] |= 0x0c; // Do not block the CPU port (bits 3:2 of byte 1 = port 9)
	reg_write_m(RTL837X_MSTP_STATES);

	print_reg(RTL837X_MSTP_STATES); write_char('\n');

	for (stp_i = machine.min_port; stp_i <= machine.max_port; stp_i++) {
		if (!(stp_pflags[stp_i] & STP_PF_ENABLED))
			continue;
		if (port_ingress_filter_get(stp_i) != VLAN_TAGGED)
			continue;
		print_string("STP: port ");
		write_char('0' + machine.log_to_phys_port[stp_i]);
		print_string(" admits tagged frames only - BPDUs are untagged and will not arrive\n");
	}

	/* Seed the carrier bitmap, so turning STP on does not report every
	 * port that was already down as a fresh topology change. */
	reg_read_m(RTL837X_REG_LINKS_STS);
	stp_link_prev = (uint16_t)sfr_data[1] | ((uint16_t)sfr_data[2] << 8);

	stp_claim_root();

	/* Take BPDUs to the CPU only - we are a participating bridge now. */
	stp_fdb_update(PMASK_CPU);
}


void stp_off(void) __banked
{
	sfr_data[0] = sfr_data[1] = sfr_data[2] = sfr_data[3] = 0;
	for (stp_i = machine.min_port; stp_i <= machine.max_port; stp_i++) {
		// Set STP port state to forwarding
		// States are: 00 disable, 01 blocking, 10 learning, 11 forwarding
		sfr_data[3 - (stp_i >> 2)] |= (uint8_t)(0b11 << ((stp_i << 1) & 0x7));
		stp_pflags[stp_i] &= ~(STP_PF_OPEREDGE | STP_PF_TRIPPED);
		port_timers[stp_i] = 0;
	}
	sfr_data[1] |= 0x0c; // Do not block the CPU port (bits 3:2 of byte 1 = port 9)
	reg_write_m(RTL837X_MSTP_STATES);

	/* Restore BPDU transparency: flood them again like an unmanaged switch. */
	stp_fdb_update(PMASK_CPU | (machine_detected.isRTL8373 ? PMASK_9 : PMASK_6));
}


void stp_parse(void) __banked __reentrant
{
	if (cmd_compare(1, "on")) {
		print_string("STP enabled\n");
		stpEnabled = 1;
		stp_setup();
		return;
	}
	if (cmd_compare(1, "off")) {
		print_string("STP disabled\n");
		stp_off();
		stpEnabled = 0;
		return;
	}
	if (cmd_compare(1, "status")) {
		stp_status();
		return;
	}
	if (cmd_words_len < 3)
		goto err;

	if (cmd_compare(1, "port") || cmd_compare(1, "lag")) {
		if (cmd_words_len < 4)
			goto err;
		if (atoi_byte(&stp_scratch, cmd_words_b[2]) || !stp_scratch)
			goto err;
		{
		uint8_t port;
		if (cmd_compare(1, "lag")) {
			if (stp_scratch > STP_LAG_COUNT)
				goto err;
			port = STP_LAG_BASE + stp_scratch - 1;
		} else {
			if (stp_scratch > 9)
				goto err;
			port = machine.phys_to_log_port[stp_scratch - 1];
			if (stp_ent_of[port] != port) {
				print_string("Port belongs to a LAG, configure it as lag ");
				print_byte(stp_ent_of[port] - STP_LAG_BASE + 1);
				write_char('\n');
				return;
			}
		}
		/* every sub-command except on/off carries one more argument; without
		 * this check cmd_compare(4,..) would read a stale word from the
		 * PREVIOUS command line (cmd_words_b is not cleared between commands) */
		if (cmd_words_len < 5 && !cmd_compare(3, "on") && !cmd_compare(3, "off"))
			goto err;
		if (cmd_compare(3, "on")) {
			stp_pflags[port] |= STP_PF_ENABLED;
			stp_pflags[port] &= ~STP_PF_TRIPPED;
			if (stpEnabled) {	/* (re)join: listen first */
				stp_state_set(port, 0b01);
				port_timers[port] = (uint16_t)stp_fwddelay_s * STP_HZ;
			}
		} else if (cmd_compare(3, "off")) {
			stp_pflags[port] &= ~STP_PF_ENABLED;
			if (stpEnabled)
				stp_state_set(port, 0b11);	/* plain forwarding */
		} else if (cmd_compare(3, "edge")) {
			/* Also drop the *operational* edge flag: it is what exempts the
			 * port from topology changes and lets it skip the listen period,
			 * so leaving it set would keep the old behaviour until the next
			 * "stp off"/"stp on". An admin edge is operational immediately. */
			stp_pflags[port] &= ~(STP_PF_ADMEDGE | STP_PF_AUTOEDGE | STP_PF_OPEREDGE);
			if (cmd_compare(4, "on"))
				stp_pflags[port] |= STP_PF_ADMEDGE | STP_PF_OPEREDGE;
			else if (cmd_compare(4, "auto"))
				stp_pflags[port] |= STP_PF_AUTOEDGE;
			else if (!cmd_compare(4, "off"))
				goto err;
		} else if (cmd_compare(3, "cost")) {
			/* raw 802.1D value, 0..200000000; 0 = auto (speed-based) */
			stp_cost_scratch = 0;
			{
			__xdata uint8_t *cp = &cmd_buffer[cmd_words_b[4]];
			if (*cp < '0' || *cp > '9')
				goto err;
			while (*cp >= '0' && *cp <= '9') {
				stp_cost_scratch = stp_cost_scratch * 10 + (*cp - '0');
				cp++;
			}
			}
			if (stp_cost_scratch > 200000000UL)
				goto err;
			stp_pcost[port] = stp_cost_scratch;
		} else if (cmd_compare(3, "p2p")) {
			if (cmd_compare(4, "auto"))
				stp_pp2p[port] = 0;
			else if (cmd_compare(4, "on"))
				stp_pp2p[port] = 1;
			else if (cmd_compare(4, "off"))
				stp_pp2p[port] = 2;
			else
				goto err;
		} else if (cmd_compare(3, "prio")) {
			if (atoi_byte(&stp_scratch, cmd_words_b[4]))
				goto err;
			stp_pprio[port] = stp_scratch & 0xf0;
		} else if (cmd_compare(3, "guard")) {
			stp_pflags[port] &= ~(STP_PF_BPDUGUARD | STP_PF_ROOTGUARD);
			if (cmd_compare(4, "bpdu"))
				stp_pflags[port] |= STP_PF_BPDUGUARD;
			else if (cmd_compare(4, "root"))
				stp_pflags[port] |= STP_PF_ROOTGUARD;
			else if (!cmd_compare(4, "none"))
				goto err;
		} else if (cmd_compare(3, "filter")) {
			if (cmd_compare(4, "on"))
				stp_pflags[port] |= STP_PF_FILTER;
			else if (cmd_compare(4, "off"))
				stp_pflags[port] &= ~STP_PF_FILTER;
			else
				goto err;
		} else {
			goto err;
		}
		}
		return;
	}

	if (atoi_byte(&stp_scratch, cmd_words_b[2])) {
		if (cmd_compare(1, "version")) {
			if (cmd_compare(2, "rstp"))
				stp_rstp = 1;
			else if (cmd_compare(2, "stp"))
				stp_rstp = 0;
			else
				goto err;
			return;
		}
		goto err;
	}
	if (cmd_compare(1, "prio")) {
		if (stp_scratch > 15)
			goto err;
		stp_prio = stp_scratch << 4;	/* n * 4096, as the BPDU's high byte */
		if (stp_root_port == 0xff)
			stp_claim_root();	/* re-announce with the new priority */
	} else if (cmd_compare(1, "hello")) {
		if (stp_scratch < 1 || stp_scratch > 10)
			goto err;
		stp_hello_s = stp_scratch;
	} else if (cmd_compare(1, "maxage")) {
		if (stp_scratch < 6 || stp_scratch > 40)
			goto err;
		stp_maxage_s = stp_scratch;
	} else if (cmd_compare(1, "fwd")) {
		if (stp_scratch < 4 || stp_scratch > 30)
			goto err;
		stp_fwddelay_s = stp_scratch;
	} else if (cmd_compare(1, "txhold")) {
		if (stp_scratch < 1 || stp_scratch > 10)
			goto err;
		stp_txhold = stp_scratch;
	} else {
		goto err;
	}
	return;
err:
	print_string("Error: stp on|off|status | prio <0-15> | hello <1-10> | maxage <6-40> | fwd <4-30> | txhold <1-10> | version rstp|stp | port <1-9>|lag <1-4> on|off|edge|cost|prio|guard|filter ...\n");
}
