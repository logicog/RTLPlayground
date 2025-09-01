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
#include "rtl837x_igmp.h"
#include "uip.h"

extern __xdata uint8_t minPort;
extern __xdata uint8_t maxPort;
extern __xdata uint8_t nSFPPorts;
extern __xdata uint8_t cpuPort;
extern __xdata uint8_t isRTL8373;

extern __code struct uip_eth_addr uip_ethaddr;

extern __xdata uint8_t uip_buf[UIP_CONF_BUFFER_SIZE+2];

// 8899 04 0000 20 0004
struct rtl_tag {
	uint16_t tag;
	uint8_t version;
	uint16_t dummy;
	uint8_t flag;
	uint16_t pmask;
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
	uint8_t root_prio;
	uint8_t root_ext;
	uint8_t root_mac[6];
	uint32_t root_path_cost;
	uint8_t bridge_prio;
	uint8_t bridge_ext;
	uint8_t bridge_mac[6];
	uint8_t port_prio;
	uint8_t port_id;
	uint16_t age;
	uint16_t age_max;
	uint16_t hello;
	uint16_t fwd_delay;
};


#define STP_O ((__xdata struct stp_pkt *)&uip_buf[RTL_TAG_SIZE + VLAN_TAG_SIZE])

void stp_cnf_send(uint8_t port) __banked
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
	memcpyc(STP_O->root_mac, uip_ethaddr.addr, 6); // For now we are the root-bridge
	memcpyc(STP_O->bridge_mac, uip_ethaddr.addr, 6);

	STP_O->root_prio = 0x80;
	STP_O->root_ext = 0x00;
	STP_O->root_path_cost = 0x00000000;

	STP_O->bridge_prio = 0x80;
	STP_O->bridge_ext = 0x00;

	STP_O->port_prio = 0x80;
	STP_O->port_id = port;
	STP_O->age = 0x00;  // FIXME: This only works because we do not use HTONS and the values are in 1/256 seconds
	STP_O->age_max = 20;
	STP_O->hello = 2;
	STP_O->fwd_delay = 0x0f;

//	uip_len = 0x27 + sizeof(struct rtl_tag);
	uip_len = sizeof(struct stp_pkt);
}
