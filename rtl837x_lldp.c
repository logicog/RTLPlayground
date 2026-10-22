#pragma codeseg BANK3
#pragma constseg BANK3

#include "rtl837x_common.h"
#include "rtl837x_lldp.h"
#include "rtl837x_sfr.h"
#include "rtl837x_regs.h"
#include "rtl837x_common.h"
#include "dhcp.h"
#include "uip.h"
#include "uip/uip.h"
#include "machine.h"
#include "version.h"
#include "rtl837x_trap.h"
#include "page_impl.h"

extern __xdata bool lldp_enabled;
extern __code const struct machine machine;
extern __xdata uint8_t outbuf[];
extern __xdata uint16_t slen;
extern __xdata uint8_t sfr_data[4];

extern volatile __xdata uint32_t ticks;

__xdata uint8_t lldp_tick_last;
__xdata uint8_t lldp_seconds;
__xdata uint16_t lldp_logical_port_status = 0xffff;
__xdata struct lldp_neighbor lldp_nb[LLDP_NB_MAX];
static __xdata struct lldp_neighbor lldp_tmp;

void lldp_init(void) __banked
{
    uint8_t i;

    lldp_tick_last = (uint8_t)ticks;
    lldp_seconds = 0;
    for (i = 0; i < LLDP_NB_MAX; i++)
        lldp_nb[i].ttl = 0;
}

void lldp_tick(void) __banked
{
    uint8_t i;

    if (!TICKS_DUE(lldp_tick_last, SYS_TICK_HZ))
        return;
    lldp_tick_last += SYS_TICK_HZ;
    if (!lldp_enabled)
        return;

    reg_read_m(RTL837X_REG_LINKS_STS);
    for (i = 0; i < LLDP_NB_MAX; i++) {
        if (!lldp_nb[i].ttl)
            continue;
        if ((sfr_data[(lldp_nb[i].port >> 3) + 1] >> (lldp_nb[i].port & 7)) & 1)
            lldp_nb[i].ttl--;
        else
            lldp_nb[i].ttl = 0;
    }

    if (++lldp_seconds < LLDP_TX_INTERVAL_S)
        return;
    lldp_seconds = 0;
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
    memcpyc(LLDP_O->dst.addr, LLDP_MULTICAST_ADDR, MAC_ADDR_LEN);
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

#define LLDP_RX_TAG ((__xdata struct rtl_tag *)&uip_buf[2 * MAC_ADDR_LEN])

static uint8_t lldp_printable(uint8_t c) __reentrant
{
    if (c < ' ' || c > '~' || c == '"' || c == '\\')
        return '.';
    return c;
}

static void lldp_copy_string(__xdata char *dst, __xdata uint8_t *src, uint16_t len, uint8_t max) __reentrant
{
    uint8_t i;

    if (len > max)
        len = max;
    for (i = 0; i < len; i++)
        dst[i] = lldp_printable(src[i]);
    dst[i] = 0;
}

static bool lldp_mac_eq(__xdata uint8_t *a, __xdata uint8_t *b) __reentrant
{
    uint8_t i;

    for (i = 0; i < MAC_ADDR_LEN; i++) {
        if (a[i] != b[i])
            return false;
    }
    return true;
}

static __xdata struct lldp_neighbor *lldp_find(void) __reentrant
{
    uint8_t i;

    for (i = 0; i < LLDP_NB_MAX; i++) {
        if (lldp_nb[i].ttl && lldp_nb[i].port == lldp_tmp.port
            && lldp_mac_eq(lldp_nb[i].chassis, lldp_tmp.chassis))
            return &lldp_nb[i];
    }
    return 0;
}

static __xdata struct lldp_neighbor *lldp_slot(void) __reentrant
{
    uint8_t i;
    __xdata struct lldp_neighbor *oldest = &lldp_nb[0];

    for (i = 0; i < LLDP_NB_MAX; i++) {
        if (!lldp_nb[i].ttl)
            return &lldp_nb[i];
        if (lldp_nb[i].ttl < oldest->ttl)
            oldest = &lldp_nb[i];
    }
    return oldest;
}

bool lldp_in(void) __banked __reentrant
{
    __xdata uint8_t *p;
    __xdata uint8_t *end;
    __xdata struct lldp_neighbor *nb;
    uint8_t type;
    uint8_t any;
    uint16_t len;
    uint8_t body = TRAP_RX_BODY();

    memcpyc(lldp_tmp.chassis, LLDP_MULTICAST_ADDR, MAC_ADDR_LEN);
    if (!lldp_mac_eq(uip_buf, lldp_tmp.chassis))
        return false;
    if (uip_buf[body] != (LLDP_ETHERTYPE >> 8) || uip_buf[body + 1] != (LLDP_ETHERTYPE & 0xff))
        return false;

    memset(&lldp_tmp, 0, sizeof(lldp_tmp));
    lldp_tmp.port = ((uint8_t)HTONS(LLDP_RX_TAG->pmask)) & 0x0f;
    if (!MACHINE_PORT_VALID(lldp_tmp.port))
        return true;
    lldp_tmp.ttl = LLDP_TTL_TTL_SECONDS;

    p = &uip_buf[body + LLDP_ETHERTYPE_LENGTH];
    end = &uip_buf[uip_len];
    while (p + 2 <= end) {
        type = p[0] >> 1;
        len = ((uint16_t)(p[0] & 1) << 8) | p[1];
        p += 2;
        if (type == LLDP_END_TLV_TYPE || p + len > end)
            break;
        switch (type) {
        case LLDP_CHASSIS_ID_TLV_TYPE:
            if (len == LLDP_CHASSIS_ID_TLV_LENGTH && p[0] == LLDP_CHASSIS_ID_TLV_SUBTYPE)
                memcpy(lldp_tmp.chassis, p + 1, MAC_ADDR_LEN);
            break;
        case LLDP_PORT_ID_TLV_TYPE:
            if (len > 1)
                lldp_copy_string(lldp_tmp.port_id, p + 1, len - 1, LLDP_NB_PORT_ID_LENGTH);
            break;
        case LLDP_TTL_TLV_TYPE:
            if (len == LLDP_TTL_TLV_LENGTH)
                lldp_tmp.ttl = ((uint16_t)p[0] << 8) | p[1];
            break;
        case LLDP_SYSNAME_TLV_TYPE:
            lldp_copy_string(lldp_tmp.sys_name, p, len, LLDP_NB_SYS_NAME_LENGTH);
            break;
        }
        p += len;
    }

    any = 0;
    for (type = 0; type < MAC_ADDR_LEN; type++)
        any |= lldp_tmp.chassis[type];
    if (!any)
        return true;

    nb = lldp_find();
    if (!lldp_tmp.ttl) {
        if (nb)
            nb->ttl = 0;
        return true;
    }
    if (!nb)
        nb = lldp_slot();
    memcpy(nb, &lldp_tmp, sizeof(lldp_tmp));
    return true;
}

static void lldp_put(uint8_t c, bool html) __reentrant
{
    if (html)
        outbuf[slen++] = c;
    else
        write_char(c);
}

static void lldp_put_dec(uint16_t v, bool html) __reentrant
{
    uint8_t digits[5];
    uint8_t n = 0;

    do {
        digits[n++] = '0' + v % 10;
        v /= 10;
    } while (v);
    while (n)
        lldp_put(digits[--n], html);
}

static void lldp_put_string(__xdata char *s, bool html) __reentrant
{
    while (*s)
        lldp_put(*s++, html);
}

static void lldp_put_mac(__xdata uint8_t *mac, bool html) __reentrant
{
    uint8_t i;

    for (i = 0; i < MAC_ADDR_LEN; i++) {
        if (i)
            lldp_put(':', html);
        lldp_put(itohex(mac[i] >> 4), html);
        lldp_put(itohex(mac[i]), html);
    }
}

void lldp_neighbors_to_html(void) __banked __reentrant
{
    uint8_t i;
    bool first = true;

    slen += strtox(outbuf + slen, ",\"neighbors\":[");
    for (i = 0; i < LLDP_NB_MAX; i++) {
        if (!lldp_nb[i].ttl)
            continue;
        if (slen + LLDP_NB_JSON_LENGTH + 2 > TCP_OUTBUF_SIZE)
            break;
        if (!first)
            lldp_put(',', true);
        first = false;
        slen += strtox(outbuf + slen, "{\"port\":");
        lldp_put_dec(machine.log_to_phys_port[lldp_nb[i].port], true);
        slen += strtox(outbuf + slen, ",\"chassis\":\"");
        lldp_put_mac(lldp_nb[i].chassis, true);
        slen += strtox(outbuf + slen, "\",\"portId\":\"");
        lldp_put_string(lldp_nb[i].port_id, true);
        slen += strtox(outbuf + slen, "\",\"sysName\":\"");
        lldp_put_string(lldp_nb[i].sys_name, true);
        slen += strtox(outbuf + slen, "\",\"ttl\":");
        lldp_put_dec(lldp_nb[i].ttl, true);
        lldp_put('}', true);
    }
    lldp_put(']', true);
}

void lldp_show(void) __banked __reentrant
{
    uint8_t i;

    for (i = 0; i < LLDP_NB_MAX; i++) {
        if (!lldp_nb[i].ttl)
            continue;
        print_string("Port ");
        lldp_put_dec(machine.log_to_phys_port[lldp_nb[i].port], false);
        print_string("  ");
        lldp_put_mac(lldp_nb[i].chassis, false);
        print_string("  ");
        lldp_put_string(lldp_nb[i].port_id, false);
        print_string("  ");
        lldp_put_string(lldp_nb[i].sys_name, false);
        print_string("  ");
        lldp_put_dec(lldp_nb[i].ttl, false);
        print_string("s\n");
    }
}
