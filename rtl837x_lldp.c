#pragma codeseg BANK2
#pragma constseg BANK2

#include "rtl837x_common.h"
#include "rtl837x_lldp.h"
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

uint8_t lldp_seconds;

void lldp_init(void) __banked
{
    lldp_seconds = 0;
}

void lldp_tick(void) __banked
{
    lldp_seconds++;

    if (lldp_seconds < LLDP_TX_INTERVAL_SEC)
        return;

    lldp_seconds = 0;

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

// The type is 7 bits and the length is 9 bits
// we can represent this as the 7 most significant
// bits of the type bit and leave the least significant
// bit as 0 always as we're not going to need 2^9 for
// the length at any point
#define LLDP_TYPE(value) (value << 1)

void lldp_send(void) __banked __reentrant
{
    uint8_t port;
    uint8_t *p;
    uint16_t len;
	uint8_t port_position;

    uint16_t hostname_length = 0;
    uint16_t machine_name_length = 0;
    __xdata char *hp = hostname;
    const char *mp = machine.machine_name;

    lldp_set_addresses();
    lldp_set_rtl_wrapper();

    p = LLDP_O->payload;
    len = 0;

    //Chassis ID
    p[len++] = LLDP_TYPE(LLDP_CHASSIS_ID_TLV_TYPE);
    p[len++] = LLDP_CHASSIS_ID_TLV_LENGTH;
    p[len++] = LLDP_CHASSIS_ID_TLV_SUBTYPE;

    for (uint8_t i = 0; i < LLDP_MAC_ADDR_LEN; i++)
    	p[len + i] = uip_ethaddr.addr[i];

	len += LLDP_MAC_ADDR_LEN;

    //Port ID
    p[len++] = LLDP_TYPE(LLDP_PORT_ID_TLV_TYPE);
    p[len++] = LLDP_PORT_ID_TLV_LENGTH;
    p[len++] = LLDP_PORT_ID_TLV_SUBTYPE;
    port_position = len;
	p[len++] = '0';       // filled in per port below

    //TTL
    p[len++] = LLDP_TYPE(LLDP_TTL_TLV_TYPE);
    p[len++] = LLDP_TTL_TLV_LENGTH;
    p[len++] = 0x00; //padding
    p[len++] = LLDP_TTL_TTL_SECONDS;

    //SysName
    p[len++] = LLDP_TYPE(LLDP_SYSNAME_TLV_TYPE);
    p[len++] = 0;

    while (*hp)
    {
        hostname_length++;
        p[len++] = *hp++;
    }
    p[len-hostname_length-1] = hostname_length;

    //SysDesc
    p[len++] = LLDP_TYPE(LLDP_SYSDESC_TLV_TYPE);
    p[len++] = 0;

    while (*mp)
    {
        machine_name_length++;
        p[len++] = *mp++;
    }
    p[len-machine_name_length-1] = machine_name_length;

    // End of LLDPDU TLV
    p[len++] = 0x00; //padding
    p[len++] = 0x00; //padding

    //Ethernet payload must be at least 46 bytes, so pad
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

        LLDP_O->payload[port_position] = '1' + port;
        LLDP_O->rtl_tag.pmask = HTONS((uint16_t)1 << port);

        tcpip_output();
    }
}

void lldp_set_addresses(void) __banked {
    // LLDP destination multicast addr: 01:80:c2:00:00:0e
    LLDP_O->dst.addr[0] = 0x01;
    LLDP_O->dst.addr[1] = 0x80;
    LLDP_O->dst.addr[2] = 0xc2;
    LLDP_O->dst.addr[3] = 0x00;
    LLDP_O->dst.addr[4] = 0x00;
    LLDP_O->dst.addr[5] = 0x0e;

    for (uint8_t i = 0; i < LLDP_MAC_ADDR_LEN; i++)
        LLDP_O->src.addr[i] = uip_ethaddr.addr[i];
}

void lldp_set_rtl_wrapper(void) __banked {
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
}