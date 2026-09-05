#include "rtl837x_common.h"
#include "lldp.h"
#include "rtl837x_sfr.h"
#include "rtl837x_common.h"
#include "dhcp.h"
#include "uip.h"
#include "uip/uip.h"
#include "machine.h"

extern __xdata uint8_t lldp_enabled;
extern __code const struct machine machine;

__xdata static uint8_t lldp_frame[LLDP_MAX_FRAME];

__xdata static uint8_t lldp_mac[LLDP_MAC_ADDR_LEN];
__xdata static char lldp_system_name[32];

uint8_t enable_lldp;

void lldp_init(void)
{
}

void lldp_tick(void)
{
    static uint8_t seconds;

    seconds++;

    if (seconds < LLDP_TX_INTERVAL_SEC)
        return;

    seconds = 0;

    if(lldp_enabled == 1)
        lldp_send();
}

//Outgoing LLDP packet.

struct lldp_pkt {
    struct uip_eth_addr dst;
    struct uip_eth_addr src;
    struct rtl_tag rtl_tag;
    uint16_t ether_type;

    uint8_t payload[64];
};

#define LLDP_O ((__xdata struct lldp_pkt *)&uip_buf[RTL_FRAME_DESC_SIZE])

void lldp_send(void) __reentrant
{
    uint8_t port;
    uint8_t *p;
    uint16_t len;
	uint8_t port_position;

    /*
     * LLDP destination multicast addr: 01:80:c2:00:00:0e
     */
    LLDP_O->dst.addr[0] = 0x01;
    LLDP_O->dst.addr[1] = 0x80;
    LLDP_O->dst.addr[2] = 0xc2;
    LLDP_O->dst.addr[3] = 0x00;
    LLDP_O->dst.addr[4] = 0x00;
    LLDP_O->dst.addr[5] = 0x0e;

    for (uint8_t i = 0; i < LLDP_MAC_ADDR_LEN; i++)
    	LLDP_O->src.addr[i] = uip_ethaddr.addr[i];

    /*
     * This is the RTL CPU tag, not the Ethernet EtherType.
     *     port 0 -> 0x0001
     *     port 1 -> 0x0002
     *     port 2 -> 0x0004
     */
    LLDP_O->rtl_tag.tag = HTONS(RTL_FRAME_TAG_ID);
    LLDP_O->rtl_tag.version = RTL_FRAME_TAG_VERSION;
    LLDP_O->rtl_tag.reason = 0x00;
    LLDP_O->rtl_tag.flags = HTONS(RTL_TAG_LEARN_DIS);

    LLDP_O->ether_type = HTONS(LLDP_ETHERTYPE);

    p = LLDP_O->payload;
    len = 0;

    /*
     * Chassis ID TLV:
     *
     * Type    = 1
     * Length  = 7
     * Subtype = 4, MAC address
     */
    p[len++] = 0x02;
    p[len++] = 0x07;
    p[len++] = 0x04;

    for (uint8_t i = 0; i < LLDP_MAC_ADDR_LEN; i++)
    	p[len + i] = uip_ethaddr.addr[i];

	len += 6;

    /*
     * Port ID TLV:
     *
     * Type    = 2
     * Length  = 2
     * Subtype = 7, locally assigned
     * Value   = logical port number
     */
    p[len++] = 0x04;
    p[len++] = 0x02;
    p[len++] = 0x07;
    port_position = len;
	p[len++] = '0';       /* filled in per port below */

    /*
     * Time To Live TLV:
     *
     * Type   = 3
     * Length = 2
     * TTL    = 120 seconds
     */
    p[len++] = 0x06;
    p[len++] = 0x02;
    p[len++] = 0x00;
    p[len++] = 120;

    /*
     * End of LLDPDU TLV.
     */
    p[len++] = 0x00;
    p[len++] = 0x00;

    //Ethernet payload must be at least 46 bytes,  so pad
    while (len < 46)
        p[len++] = 0x00;

    /*
     * uip_len is the Ethernet frame length excluding FCS.
     *
     * The frame consists of:
     *
     *     dst mac
     *     src mac
     *     rtl_tag  sizeof(struct rtl_tag)
     *     EtherType 2
     *     payload   len
     */
    uip_len = LLDP_MAC_ADDR_LEN + LLDP_MAC_ADDR_LEN + sizeof(struct rtl_tag) + 2 + len;

    for (port = machine.min_port; port <= machine.max_port; port++) {

        LLDP_O->payload[port_position] = '0' + port;

        //Restrict this packet to exactly one egress port
		// if it starts from 1 instead of 0:
		// LLDP_O->rtl_tag.pmask = HTONS((uint16_t)1 << (port - 1));
        LLDP_O->rtl_tag.pmask = HTONS((uint16_t)1 << port);

        tcpip_output();
    }
}