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
#include "rtl837x_port.h"	/* port_pvid_get(), port_l2mc_set() */
#include "uip.h"
#include "machine.h"

extern __code struct machine machine;
extern __xdata uint8_t sfr_data[4];
extern __xdata struct machine_runtime machine_detected;	/* owned by rtl837x_port.c */

/* Scratch for stp_fdb_update(), in xdata: plain locals would overflow the
 * near-full internal-RAM overlay (OSEG), cf. rtl837x_lacp.c. */
__xdata uint16_t stp_fdb_vid;
__xdata uint8_t  stp_fdb_i;

extern __xdata struct uip_eth_addr uip_ethaddr;

extern __xdata uint8_t uip_buf[UIP_CONF_BUFFER_SIZE + 2];

struct bridge {
	uint8_t prio;
	uint8_t ext;
	uint8_t mac[6];
};

__xdata struct bridge root_bridge;
__xdata uint32_t root_bridge_cost;

__xdata uint8_t port_types[10];
__xdata uint16_t port_timers[10];
__xdata uint16_t port_hello[10];


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
};

#define STP_O ((__xdata struct stp_pkt *)&uip_buf[RTL_FRAME_DESC_SIZE])
#define STP_I ((__xdata struct stp_pkt_in *)&uip_buf[0])

#define FLAG_PROPOSAL 0x02
#define P_DESIGNATED ((STP_I->flags & 0x0c) == 0x0c)
#define P_PROPOSAL (STP_I->flags & FLAG_PROPOSAL)

signed char cmpMAC(__xdata uint8_t *m1, __xdata uint8_t *m2)
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


void stp_in(void) __banked
{
	// By default we do not send anything out
	uip_len = 0;
	// MSTPSTP_I_STATES 0x5310
	// reg_read_m(RTL837X_MSTP_STATES);

	print_string("Check BPDU... \n");
	for (uint8_t i = 0; i < 80; i++) {
		print_byte(uip_buf[i]);
		write_char(' ');
	}
	write_char('\n');
	print_byte(STP_I->dsap);
	print_byte(STP_I->ssap);
	print_byte(STP_I->ctrl);
	
	write_char('\n');
	// Make sure this is the type of RSTP packet we are interested in:
	if (!(STP_I->dsap == 0x42 && STP_I->ssap == 0x42 && STP_I->ctrl == 0x03))
		return;
	print_string("Checking RSTP\n");
	if (STP_I->proto)
		return;
//	write_char('A'); print_byte(STP_I->version); write_char('\n');
	if (STP_I->version != 2)
		return;
//	write_char('B'); print_byte(STP_I->bpdu_type); write_char('\n');
	if (STP_I->bpdu_type != 2)
		return;
//	write_char('\n');
//	print_string("Flags: "); print_byte(STP_I->flags); write_char('\n');
	print_string("Check new Root\n");
	if (STP_I->root.prio < root_bridge.prio
		|| ((STP_I->root.prio == root_bridge.prio) && cmpMAC(STP_I->root.mac, root_bridge.mac) < 0)) {
		print_string("Updating Root bridge\n");
			root_bridge.prio = STP_I->root.prio;
			memcpy(root_bridge.mac, STP_I->root.mac, 6);
	}
}


void stp_cnf_send(uint8_t port)
{
	STP_O->stp_addr[0] = 0x01; STP_O->stp_addr[1] = 0x80; STP_O->stp_addr[2] = 0xc2;
	STP_O->stp_addr[3] = STP_O->stp_addr[4] = STP_O->stp_addr[5] = 0x00;

	STP_O->rtl_tag.tag = HTONS(RTL_FRAME_TAG_ID);
	STP_O->rtl_tag.version = RTL_FRAME_TAG_VERSION;
	STP_O->rtl_tag.reason = 0x00;
	STP_O->rtl_tag.flags = 0x0020; // Disable L2 learning
	STP_O->rtl_tag.pmask = HTONS(((uint16_t)1) << port);

	STP_O->msg_len = HTONS(0x27);
	STP_O->dsap = 0x42;
	STP_O->ssap = 0x42;
	STP_O->ctrl = 0x03;
	STP_O->proto = 0x0000;
	STP_O->version = 0x02;		// RSTP
	STP_O->bpdu_type = 0x00;	// Config
	STP_O->flags = 0x81;

	memcpy(STP_O->src_addr, uip_ethaddr.addr, 6);
	memcpy(STP_O->root.mac, root_bridge.mac, 6);
	memcpy(STP_O->bridge.mac, uip_ethaddr.addr, 6);

	STP_O->root.prio = root_bridge.prio;
	STP_O->root.ext = 0x00;
	STP_O->root_path_cost = 0x00000000;

	STP_O->bridge.prio = 0x80;
	STP_O->bridge.ext = 0x00;

	STP_O->port_prio = 0x80;
	STP_O->port_id = port;
	STP_O->age = 0x00;  // FIXME: This only works because we do not use HTONS and the values are in 1/256 seconds
	STP_O->age_max = 20;
	STP_O->hello = 2;
	STP_O->fwd_delay = 0x0f;

//	uip_len = 0x27 + sizeof(struct rtl_tag);
	uip_len = sizeof(struct stp_pkt);
	tcpip_output();
}


void stp_timers(void) __banked
{
	for (uint8_t i = machine.min_port; i <= machine.max_port; i++) {
		port_hello[i]--;
		if (!port_hello[i]) {
			port_hello[i] = TIME_HELLO;
			print_string("STP_HELLO port ");
			print_byte(i); write_char('\n');
			stp_cnf_send(i);
		}
	}
}


/*
 * Steer BPDUs (01:80:C2:00:00:00) while STP runs: one static CPU-only L2
 * multicast entry per PVID in use, so BPDUs reach the CPU without being
 * flooded to other ports (a bridge running STP must consume BPDUs, not
 * relay them - relaying poisons the neighbours' view of the topology).
 *
 * With STP off the same entries are retargeted to all ports + CPU, which
 * restores the previous flood behaviour ("BPDU transparency"): the
 * surrounding spanning tree can keep spanning *through* this switch, which
 * unmanaged setups rely on. Same per-PVID/IVL rules as the LACP steering -
 * see port_l2mc_set() and rtl837x_lacp.c. NOTE: changing a port's PVID
 * while STP runs needs `stp off`/`on` to refresh the entries.
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
	sfr_data[0] = sfr_data[1] = sfr_data[2] = sfr_data[3] = 0;
	for (uint8_t i = machine.min_port; i <= machine.max_port; i++) {
		// Set STP port state to blocking
		// States are: 00 disable, 01 blocking, 10 learning, 11 forwarding
		uint8_t bit_mask = 0b01 << ( (i << 1) & 0x7);
		sfr_data[3 - (i >> 2)] |= bit_mask;
		port_hello[i] = TIME_HELLO;
		port_timers[i] = 0xa00;	// 10 sec in blocking state
	}
	sfr_data[1] |= 0x0f; // Do not block CPU-Port
	reg_write_m(RTL837X_MSTP_STATES); // R5310-000d555f 

	print_reg(RTL837X_MSTP_STATES); write_char('\n');

	root_bridge.prio = 0x80; // This corresponds to 32768
	root_bridge.ext	= 0x00;
	memcpy(root_bridge.mac, uip_ethaddr.addr, 6);

	/* Take BPDUs to the CPU only - we are a participating bridge now. */
	stp_fdb_update(PMASK_CPU);
}


void stp_off(void) __banked
{
	sfr_data[0] = sfr_data[1] = sfr_data[2] = sfr_data[3] = 0;
	for (uint8_t i = machine.min_port; i <= machine.max_port; i++) {
		// Set STP port state to forwarding
		// States are: 00 disable, 01 blocking, 10 learning, 11 forwarding
		uint8_t bit_mask = 0b11 << ( (i << 1) & 0x7);
		sfr_data[3 - (i >> 2)] |= bit_mask;
	}
	sfr_data[1] |= 0x0f; // Do not block CPU-Port
	reg_write_m(RTL837X_MSTP_STATES);

	/* Restore BPDU transparency: flood them again like an unmanaged switch. */
	stp_fdb_update(PMASK_CPU | (machine_detected.isRTL8373 ? PMASK_9 : PMASK_6));
}
