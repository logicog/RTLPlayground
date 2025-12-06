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
#include "uip.h"
#include "machine.h"

extern __code struct machine machine;
extern __xdata uint8_t sfr_data[4];

extern __code struct uip_eth_addr uip_ethaddr;

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
	uint8_t vtag[4];
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

#define STP_O ((__xdata struct stp_pkt *)&uip_buf[RTL_TAG_SIZE + VLAN_TAG_SIZE])
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
		|| ((STP_I->root.prio == root_bridge.prio) && cmpMAC(STP_I->root.mac, STP_I->root.mac) < 0)) {
		print_string("Updating Root bridge\n");
			root_bridge.prio = STP_I->root.prio;
			memcpy(root_bridge.mac, STP_I->root.mac, 6);
	}
}


void stp_cnf_send(uint8_t port)
{
	STP_O->stp_addr[0] = 0x01; STP_O->stp_addr[1] = 0x80; STP_O->stp_addr[2] = 0xc2;
	STP_O->stp_addr[3] = STP_O->stp_addr[4] = STP_O->stp_addr[5] = 0x00;

	STP_O->rtl_tag.tag = HTONS(0x8899);
	STP_O->rtl_tag.version = 0x04;
	STP_O->rtl_tag.dummy = 0x0000;
	STP_O->rtl_tag.flag = 0x20; // WHY ???
	STP_O->rtl_tag.pmask = HTONS(((uint16_t)1) << port);

	STP_O->msg_len = HTONS(0x27);
	STP_O->dsap = 0x42;
	STP_O->ssap = 0x42;
	STP_O->ctrl = 0x03;
	STP_O->proto = 0x0000;
	STP_O->version = 0x02;		// RSTP
	STP_O->bpdu_type = 0x00;	// Config
	STP_O->flags = 0x81;

	memcpyc(STP_O->src_addr, uip_ethaddr.addr, 6);
	memcpy(STP_O->root.mac, root_bridge.mac, 6);
	memcpyc(STP_O->bridge.mac, uip_ethaddr.addr, 6);

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
	memcpyc(root_bridge.mac, uip_ethaddr.addr, 6);
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
}
