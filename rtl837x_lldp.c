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
#include "version.h"
#include "rtl837x_trap.h"

extern __xdata bool lldp_enabled;
extern __code const struct machine machine;

extern volatile __xdata uint32_t ticks;

__xdata uint8_t lldp_tick_last;
__xdata uint8_t lldp_seconds;
__xdata uint16_t lldp_logical_port_status = 0xffff;
__xdata struct lldp_neighbor lldp_neighbors[LLDP_NB_PORTS];

void lldp_clear(void) __banked
{
    memset(lldp_neighbors, 0, sizeof(lldp_neighbors));
}

void lldp_init(void) __banked
{
    lldp_tick_last = (uint8_t)ticks;
    lldp_seconds = 0;
    lldp_clear();
}

// Age the neighbour table once a second
static void lldp_age(void)
{
    for (uint8_t i = 0; i < LLDP_NB_PORTS; i++) {
        if (lldp_neighbors[i].ttl)
            lldp_neighbors[i].ttl--;
    }
}

void lldp_tick(void) __banked
{
    if (!TICKS_DUE(lldp_tick_last, SYS_TICK_HZ))
        return;
    lldp_tick_last += SYS_TICK_HZ;

    lldp_age();

    if (++lldp_seconds < LLDP_TX_INTERVAL_S)
        return;
    lldp_seconds = 0;

    if (lldp_enabled)
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
    __xdata uint8_t *tail;
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

    tail = p;

    for (uint8_t port = MACHINE_MIN_PORT; port <= MACHINE_MAX_PORT; port++) {

        if ((1 << port) & ports) {
            LLDP_O->payload[port_position] = '0' + machine.log_to_phys_port[port];
            LLDP_O->rtl_tag.pmask = HTONS((uint16_t)1 << port);

            p = tail;
            p += lldp_portdesc(p, port);

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
    uint8_t machine_name_length = strlen(machine.machine_name);
    uint8_t version_length = strlen(VERSION_SW);

    *p++ = LLDP_TYPE(LLDP_SYSDESC_TLV_TYPE);
    *p++ = machine_name_length + 1 + version_length;
    memcpyc(p, machine.machine_name, machine_name_length);
    p += machine_name_length;
    *p++ = ' ';
    memcpyc(p, VERSION_SW, version_length);
    return machine_name_length + 1 + version_length + 2;
}

uint8_t lldp_portdesc(__xdata uint8_t *p, uint8_t port) __banked
{
    uint8_t name_length = strlen_x(port_names[port]);

    if (!name_length)
        return 0;
    *p++ = LLDP_TYPE(LLDP_PORT_DESC_TLV_TYPE);
    *p++ = name_length;
    memcpy(p, port_names[port], name_length);
    return name_length + 2;
}

// Incoming LLDP frame, trapped to the CPU with an RTL tag in front of the EtherType

static const char hexdigits[] = "0123456789abcdef";

// Copy a TLV value as text: a MAC address if mac is set and the length fits,
// printable ASCII otherwise
static void lldp_copy_str(__xdata char *dst, __xdata uint8_t *src, uint16_t len, bool mac)
{
    uint8_t n = 0;

    if (mac && len == MAC_ADDR_LEN) {
        for (uint8_t i = 0; i < MAC_ADDR_LEN; i++) {
            if (i)
                dst[n++] = ':';
            dst[n++] = hexdigits[src[i] >> 4];
            dst[n++] = hexdigits[src[i] & 0xf];
        }
    } else {
        while (len-- && n < LLDP_NB_STR - 1) {
            uint8_t c = *src++;
            dst[n++] = (c < 0x20 || c > 0x7e) ? '.' : c;
        }
    }
    dst[n] = 0;
}

void lldp_in(void) __banked
{
    __xdata uint8_t *p, *end;
    __xdata struct lldp_neighbor *nb;
    uint16_t len, ttl = 0;
    uint8_t type, port, body;

    body = TRAP_RX_BODY();
    if (uip_len > UIP_BUFSIZE)
        uip_len = UIP_BUFSIZE;
    if (uip_len < body + 2)
        goto out;
    if (uip_buf[body] != (LLDP_ETHERTYPE >> 8) || uip_buf[body + 1] != (LLDP_ETHERTYPE & 0xff))
        goto out;
    // Ignore our own frames
    if (!memcmp(&uip_buf[MAC_ADDR_LEN], uip_ethaddr.addr, MAC_ADDR_LEN))
        goto out;

    port = ((uint8_t)HTONS(((__xdata struct rtl_tag *)&uip_buf[2 * MAC_ADDR_LEN])->pmask)) & 0x0f;
    if (!MACHINE_PORT_VALID(port) || port >= LLDP_NB_PORTS)
        goto out;

    nb = &lldp_neighbors[port];
    nb->chassis[0] = nb->port[0] = nb->name[0] = 0;

    p = &uip_buf[body + 2];
    end = &uip_buf[uip_len];
    while (p + 2 <= end) {
        type = p[0] >> 1;
        len = ((uint16_t)(p[0] & 1) << 8) | p[1];
        p += 2;
        if (!type || p + len > end)
            break;
        switch (type) {
        case LLDP_CHASSIS_ID_TLV_TYPE:
            if (len)
                lldp_copy_str(nb->chassis, p + 1, len - 1, p[0] == LLDP_CHASSIS_SUBTYPE_MAC);
            break;
        case LLDP_PORT_ID_TLV_TYPE:
            if (len)
                lldp_copy_str(nb->port, p + 1, len - 1, p[0] == LLDP_PORT_SUBTYPE_MAC);
            break;
        case LLDP_TTL_TLV_TYPE:
            if (len >= 2)
                ttl = ((uint16_t)p[0] << 8) | p[1];
            break;
        case LLDP_SYSNAME_TLV_TYPE:
            lldp_copy_str(nb->name, p, len, 0);
            break;
        }
        p += len;
    }
    // A TTL of 0 (shutdown frame) removes the neighbour
    nb->ttl = ttl;
out:
    uip_len = 0;
}
