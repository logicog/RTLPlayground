#pragma codeseg BANK3
#pragma constseg BANK3

#include "rtl837x_common.h"
#include "rtl837x_lldp.h"
#include "rtl837x_sfr.h"
#include "rtl837x_common.h"
#include "dhcp.h"
#include "uip.h"
#include "uip/uip.h"
#include "machine.h"

extern __xdata bool lldp_enabled;
extern __code const struct machine machine;

__xdata uint16_t lldp_ticks;
__xdata uint16_t lldp_logical_port_status = 0xffff;

void lldp_init(void) __banked
{
    lldp_ticks = 0;
}

void lldp_tick(void) __banked
{
    lldp_ticks++;

    if (lldp_ticks < LLDP_TX_INTERVAL_TICKS)
        return;

    lldp_ticks = 0;

    if(lldp_enabled == 1)
        lldp_send();
}

//Outgoing LLDP packet.

struct lldp_pkt {
    struct uip_eth_addr dst;
    struct uip_eth_addr src;
    struct rtl_tag rtl_tag;
    uint16_t ether_type;

    uint8_t payload[LLDP_MAX_PAYLOAD];
};

#define LLDP_O ((__xdata struct lldp_pkt *)&uip_buf[RTL_FRAME_DESC_SIZE])

// The type is 7 bits and the length is 9 bits
// we can represent this as the 7 most significant
// bits of the type bit and leave the least significant
// bit as 0 always as we're not going to need 2^9 for
// the length at any point
#define LLDP_TYPE(value) (value << 1)

static void lldp_send_ports(uint16_t ttl, uint16_t ports) __reentrant
{
    __xdata uint8_t *p;
    uint8_t port_position;

    lldp_set_addresses();
    lldp_set_rtl_wrapper();

    p = LLDP_O->payload;

    //Chassis ID
    *p++ = LLDP_TYPE(LLDP_CHASSIS_ID_TLV_TYPE);
    *p++ = LLDP_CHASSIS_ID_TLV_LENGTH;
    *p++ = LLDP_CHASSIS_ID_TLV_SUBTYPE;

    memcpy(p, uip_ethaddr.addr, MAC_ADDR_LEN);
    p += MAC_ADDR_LEN;

    //Port ID
    *p++ = LLDP_TYPE(LLDP_PORT_ID_TLV_TYPE);
    *p++ = LLDP_PORT_ID_TLV_LENGTH;
    *p++ = LLDP_PORT_ID_TLV_SUBTYPE;
    memcpyc(p, LLDP_PORT_ID_PREFIX, LLDP_PORT_ID_PREFIX_LENGTH);
    p += LLDP_PORT_ID_PREFIX_LENGTH;
    port_position = (p-LLDP_O->payload);
	*p++ = '0';       // filled in per port below

    //TTL
    *p++ = LLDP_TYPE(LLDP_TTL_TLV_TYPE);
    *p++ = LLDP_TTL_TLV_LENGTH;
    *p++ = ttl >> 8;
    *p++ = ttl & 0xff;

    p += lldp_sysname(p);
    p += lldp_sysdesc(p);

    //System Capabilities: supported, enabled
    *p++ = LLDP_TYPE(LLDP_CAPABILITIES_TLV_TYPE);
    *p++ = LLDP_CAPABILITIES_TLV_LENGTH;
    *p++ = LLDP_CAPABILITY_BRIDGE >> 8;
    *p++ = LLDP_CAPABILITY_BRIDGE & 0xff;
    *p++ = LLDP_CAPABILITY_BRIDGE >> 8;
    *p++ = LLDP_CAPABILITY_BRIDGE & 0xff;

    //Management Address: IPv4
    if (uip_hostaddr[0] | uip_hostaddr[1]) {
        *p++ = LLDP_TYPE(LLDP_MGMT_ADDR_TLV_TYPE);
        *p++ = LLDP_MGMT_ADDR_TLV_LENGTH;
        *p++ = LLDP_MGMT_ADDR_STR_LENGTH;
        *p++ = LLDP_MGMT_ADDR_SUBTYPE_IPV4;
        memcpy(p, (__xdata uint8_t *)uip_hostaddr, sizeof(uip_ipaddr_t));
        p += sizeof(uip_ipaddr_t);
        *p++ = LLDP_MGMT_IF_NUMBERING_UNKNOWN;
        *p++ = 0; //interface number
        *p++ = 0;
        *p++ = 0;
        *p++ = 0;
        *p++ = 0; //OID length
    }

    // End of LLDPDU TLV
    *p++ = 0x00; //padding
    *p++ = 0x00; //padding

    while (p < (LLDP_O->payload + LLDP_MIN_ETHERNET_PAYLOAD_LENGTH))
        *p++ = 0x00;

    /*
     * uip_len is the Ethernet frame length excluding FCS.
     *
     * The frame consists of:
     *
     *     dst mac
     *     src mac
     *     rtl_tag  sizeof(struct rtl_tag)
     *     EtherType (2 bytes)
     *     payload (how far we've moved p pointer)
     */
    uip_len = MAC_ADDR_LEN + MAC_ADDR_LEN + sizeof(struct rtl_tag) + LLDP_ETHERTYPE_LENGTH + (p - LLDP_O->payload);

    for (uint8_t port = MACHINE_MIN_PORT; port <= MACHINE_MAX_PORT; port++) {

        if ((1 << port) & ports) {
            LLDP_O->payload[port_position] = '0' + machine.log_to_phys_port[port];
            LLDP_O->rtl_tag.pmask = HTONS((uint16_t)1 << port);

            tcpip_output();
        }
    }
}

void lldp_send(void) __banked __reentrant
{
    lldp_send_ports(LLDP_TTL_TTL_SECONDS, lldp_logical_port_status);
}

void lldp_shutdown(uint16_t ports) __banked __reentrant
{
    lldp_send_ports(0, ports);
}

void lldp_set_addresses(void) __banked {
    // LLDP destination multicast addr: 01:80:c2:00:00:0e
    memcpyc(LLDP_O->dst.addr, "\x01\x80\xc2\x00\x00\x0e", MAC_ADDR_LEN);
    memcpy(LLDP_O->src.addr, uip_ethaddr.addr, MAC_ADDR_LEN);
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

uint8_t lldp_sysname(__xdata uint8_t *p) __banked
{
    uint8_t hostname_length = strlen_x(hostname);

    *p++ = LLDP_TYPE(LLDP_SYSNAME_TLV_TYPE);
    *p++ = hostname_length;
    memcpy(p, hostname, hostname_length);
    return hostname_length + 2;
}

uint8_t lldp_sysdesc(__xdata uint8_t *p) __banked
{
    uint16_t machine_name_length = strlen(machine.machine_name);

    *p++ = LLDP_TYPE(LLDP_SYSDESC_TLV_TYPE);
    *p++ = machine_name_length;
    memcpyc(p, machine.machine_name, machine_name_length);
    return machine_name_length + 2; 
}
