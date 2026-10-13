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
#include "rtl837x_trap.h"
#include "page_impl.h"
#include "rtl837x_regs.h"
#include "rtl837x_port.h"

extern __xdata bool lldp_enabled;
extern __code const struct machine machine;
extern __xdata uint8_t outbuf[];
extern __xdata uint16_t slen;

extern volatile __xdata uint32_t ticks;

__xdata uint8_t lldp_tick_last;
__xdata uint8_t lldp_seconds;
__xdata uint16_t lldp_logical_port_status = 0xffff;
__xdata struct lldp_neighbor lldp_nb[LLDP_NB_MAX];
static __xdata struct lldp_neighbor lldp_tmp;
static __code const uint8_t lldp_oui_8021[3] = {0x00, 0x80, 0xc2};
static __code const uint8_t lldp_oui_8023[3] = {0x00, 0x12, 0x0f};

static void lldp_put_dec(uint16_t v, bool html) __reentrant;
static bool lldp_bytes_eq(__xdata uint8_t *a, __xdata uint8_t *b, uint8_t n) __reentrant;

static bool lldp_oui_is(__xdata uint8_t *p, __code const uint8_t *oui) __reentrant
{
    return p[0] == oui[0] && p[1] == oui[1] && p[2] == oui[2];
}

static uint16_t lldp_port_mtu(uint8_t port) __reentrant
{
    reg_read_m(RTL8373_REG_MAC_L2_PORT_MAX_LEN + ((uint16_t)port << 8));
    return SFR_DATA_U16 & PORT_MAX_LEN_MASK;
}

static uint8_t lldp_warnings(__xdata struct lldp_neighbor *nb) __reentrant
{
    uint8_t i;
    uint8_t lag;
    uint8_t w = 0;

    if (nb->mtu && nb->mtu != lldp_port_mtu(nb->port))
        w |= LLDP_WARN_MTU;
    lag = port_lag_of(nb->port);
    if ((nb->agg & LLDP_AGG_ANNOUNCED) && ((nb->agg & LLDP_AGG_ACTIVE) != 0) != (lag != PORT_LAG_NONE))
        w |= LLDP_WARN_AGG;
    for (i = 0; i < LLDP_NB_MAX; i++) {
        if (&lldp_nb[i] == nb || !lldp_nb[i].ttl || lldp_nb[i].port == nb->port)
            continue;
        if (lldp_nb[i].chassis_subtype != nb->chassis_subtype || lldp_nb[i].chassis_len != nb->chassis_len
            || !lldp_bytes_eq(lldp_nb[i].chassis, nb->chassis, nb->chassis_len))
            continue;
        if (lag == PORT_LAG_NONE || port_lag_of(lldp_nb[i].port) != lag)
            w |= LLDP_WARN_DUPLICATE;
    }
    return w;
}

static void lldp_log_warnings(__xdata struct lldp_neighbor *nb) __reentrant
{
    print_string("LLDP: port ");
    lldp_put_dec(machine.log_to_phys_port[nb->port], false);
    print_string(" mismatch:");
    if (nb->warn & LLDP_WARN_MTU)
        print_string(" MTU");
    if (nb->warn & LLDP_WARN_AGG)
        print_string(" LAG");
    if (nb->warn & LLDP_WARN_DUPLICATE)
        print_string(" same-neighbour-twice");
    write_char('\n');
}

void lldp_init(void) __banked
{
    lldp_tick_last = (uint8_t)ticks;
    lldp_seconds = 0;
    memset(lldp_nb, 0, sizeof(lldp_nb));
}

void lldp_tick(void) __banked
{
    uint8_t i;

    if (!TICKS_DUE(lldp_tick_last, SYS_TICK_HZ))
        return;
    lldp_tick_last += SYS_TICK_HZ;

    for (i = 0; i < LLDP_NB_MAX; i++) {
        if (lldp_nb[i].ttl)
            lldp_nb[i].ttl--;
    }

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
    port_position = (p-LLDP_O->payload);
	*p++ = '0';       // filled in per port below

    //TTL
    *p++ = LLDP_TYPE(LLDP_TTL_TLV_TYPE);
    *p++ = LLDP_TTL_TLV_LENGTH;
    *p++ = 0x00; //padding
    *p++ = LLDP_TTL_TTL_SECONDS;

    p += lldp_sysname(p);
    p += lldp_sysdesc(p);

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

    for (uint8_t port = machine.min_port; port <= machine.max_port; port++) {

        if ((1 << port) & lldp_logical_port_status) {
            LLDP_O->payload[port_position] = '0' + machine.log_to_phys_port[port];
            LLDP_O->rtl_tag.pmask = HTONS((uint16_t)1 << port);

            tcpip_output();
        }
    }
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
    uint16_t machine_name_length = strlen(machine.machine_name);

    *p++ = LLDP_TYPE(LLDP_SYSDESC_TLV_TYPE);
    *p++ = machine_name_length;
    memcpyc(p, machine.machine_name, machine_name_length);
    return machine_name_length + 2; 
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

static bool lldp_bytes_eq(__xdata uint8_t *a, __xdata uint8_t *b, uint8_t n) __reentrant
{
    uint8_t i;

    for (i = 0; i < n; i++) {
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
            && lldp_nb[i].chassis_subtype == lldp_tmp.chassis_subtype
            && lldp_nb[i].chassis_len == lldp_tmp.chassis_len
            && lldp_bytes_eq(lldp_nb[i].chassis, lldp_tmp.chassis, lldp_tmp.chassis_len))
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
    uint16_t len;
    uint8_t body = TRAP_RX_BODY();

    memcpyc(lldp_tmp.chassis, LLDP_MULTICAST_ADDR, MAC_ADDR_LEN);
    if (!lldp_bytes_eq(uip_buf, lldp_tmp.chassis, MAC_ADDR_LEN))
        return false;
    if (uip_buf[body] != (LLDP_ETHERTYPE >> 8) || uip_buf[body + 1] != (LLDP_ETHERTYPE & 0xff))
        return false;

    memset(&lldp_tmp, 0, sizeof(lldp_tmp));
    lldp_tmp.port = ((uint8_t)HTONS(LLDP_RX_TAG->pmask)) & 0x0f;
    if (lldp_tmp.port < machine.min_port || lldp_tmp.port > machine.max_port)
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
            if (len > 1) {
                lldp_tmp.chassis_subtype = p[0];
                lldp_tmp.chassis_len = len - 1 > LLDP_NB_CHASSIS_LENGTH ? LLDP_NB_CHASSIS_LENGTH : len - 1;
                memcpy(lldp_tmp.chassis, p + 1, lldp_tmp.chassis_len);
            }
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
        case LLDP_PORT_DESC_TLV_TYPE:
            lldp_copy_string(lldp_tmp.port_desc, p, len, LLDP_NB_PORT_DESC_LENGTH);
            break;
        case LLDP_CAPABILITIES_TLV_TYPE:
            if (len == LLDP_CAPABILITIES_TLV_LENGTH)
                lldp_tmp.caps = ((uint16_t)p[2] << 8) | p[3];
            break;
        case LLDP_MGMT_ADDR_TLV_TYPE:
            if (len > LLDP_MGMT_ADDR_IPV4_LENGTH && p[0] == LLDP_MGMT_ADDR_IPV4_LENGTH
                && p[1] == LLDP_MGMT_ADDR_SUBTYPE_IPV4)
                memcpy(lldp_tmp.mgmt_addr, p + 2, sizeof(lldp_tmp.mgmt_addr));
            break;
        case LLDP_ORG_TLV_TYPE:
            if (len < LLDP_ORG_TLV_HEADER_LENGTH)
                break;
            if (lldp_oui_is(p, lldp_oui_8021) && p[3] == LLDP_8021_SUBTYPE_PVID && len == LLDP_ORG_TLV_HEADER_LENGTH + 2)
                lldp_tmp.pvid = ((uint16_t)p[4] << 8) | p[5];
            else if (lldp_oui_is(p, lldp_oui_8023) && p[3] == LLDP_8023_SUBTYPE_MAX_FRAME && len == LLDP_ORG_TLV_HEADER_LENGTH + 2)
                lldp_tmp.mtu = ((uint16_t)p[4] << 8) | p[5];
            else if (lldp_oui_is(p, lldp_oui_8023) && p[3] == LLDP_8023_SUBTYPE_LINK_AGG && len > LLDP_ORG_TLV_HEADER_LENGTH)
                lldp_tmp.agg = (p[4] & (LLDP_AGG_CAPABLE | LLDP_AGG_ACTIVE)) | LLDP_AGG_ANNOUNCED;
            break;
        }
        p += len;
    }

    if (!lldp_tmp.chassis_len)
        return true;

    nb = lldp_find();
    if (!lldp_tmp.ttl) {
        if (nb)
            nb->ttl = 0;
        return true;
    }
    lldp_tmp.warn = nb ? nb->warn : 0;
    if (!nb)
        nb = lldp_slot();
    memcpy(nb, &lldp_tmp, sizeof(lldp_tmp));
    type = lldp_warnings(nb);
    if (type != nb->warn) {
        nb->warn = type;
        if (type)
            lldp_log_warnings(nb);
    }
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

static void lldp_put_addr(__xdata uint8_t *addr, bool html) __reentrant
{
    uint8_t i;

    if (!(addr[0] | addr[1] | addr[2] | addr[3]))
        return;
    for (i = 0; i < sizeof(lldp_tmp.mgmt_addr); i++) {
        if (i)
            lldp_put('.', html);
        lldp_put_dec(addr[i], html);
    }
}

static void lldp_put_chassis(__xdata struct lldp_neighbor *nb, bool html) __reentrant
{
    uint8_t i;

    if (nb->chassis_subtype == LLDP_CHASSIS_ID_TLV_SUBTYPE && nb->chassis_len == MAC_ADDR_LEN) {
        for (i = 0; i < MAC_ADDR_LEN; i++) {
            if (i)
                lldp_put(':', html);
            lldp_put(itohex(nb->chassis[i] >> 4), html);
            lldp_put(itohex(nb->chassis[i]), html);
        }
        return;
    }
    if (nb->chassis_subtype == LLDP_CHASSIS_ID_SUBTYPE_NETWORK_ADDR
        && nb->chassis[0] == LLDP_ADDR_FAMILY_IPV4 && nb->chassis_len == LLDP_NETWORK_ADDR_IPV4_LENGTH) {
        for (i = 1; i < LLDP_NETWORK_ADDR_IPV4_LENGTH; i++) {
            if (i > 1)
                lldp_put('.', html);
            lldp_put_dec(nb->chassis[i], html);
        }
        return;
    }
    for (i = 0; i < nb->chassis_len; i++)
        lldp_put(lldp_printable(nb->chassis[i]), html);
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
        lldp_put_chassis(&lldp_nb[i], true);
        slen += strtox(outbuf + slen, "\",\"portId\":\"");
        lldp_put_string(lldp_nb[i].port_id, true);
        slen += strtox(outbuf + slen, "\",\"sysName\":\"");
        lldp_put_string(lldp_nb[i].sys_name, true);
        slen += strtox(outbuf + slen, "\",\"portDesc\":\"");
        lldp_put_string(lldp_nb[i].port_desc, true);
        slen += strtox(outbuf + slen, "\",\"mgmtAddr\":\"");
        lldp_put_addr(lldp_nb[i].mgmt_addr, true);
        slen += strtox(outbuf + slen, "\",\"caps\":");
        lldp_put_dec(lldp_nb[i].caps, true);
        slen += strtox(outbuf + slen, ",\"pvid\":");
        lldp_put_dec(lldp_nb[i].pvid, true);
        slen += strtox(outbuf + slen, ",\"mtu\":");
        lldp_put_dec(lldp_nb[i].mtu, true);
        slen += strtox(outbuf + slen, ",\"agg\":");
        lldp_put_dec((lldp_nb[i].agg & LLDP_AGG_ANNOUNCED) ? ((lldp_nb[i].agg & LLDP_AGG_ACTIVE) ? 2 : 1) : 0, true);
        slen += strtox(outbuf + slen, ",\"warn\":");
        lldp_put_dec(lldp_warnings(&lldp_nb[i]), true);
        slen += strtox(outbuf + slen, ",\"ttl\":");
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
        lldp_put_chassis(&lldp_nb[i], false);
        print_string("  ");
        lldp_put_string(lldp_nb[i].port_id, false);
        print_string("  ");
        lldp_put_string(lldp_nb[i].sys_name, false);
        print_string("  ");
        lldp_put_string(lldp_nb[i].port_desc, false);
        print_string("  ");
        lldp_put_addr(lldp_nb[i].mgmt_addr, false);
        print_string("  pvid ");
        lldp_put_dec(lldp_nb[i].pvid, false);
        print_string(" mtu ");
        lldp_put_dec(lldp_nb[i].mtu, false);
        print_string((lldp_nb[i].agg & LLDP_AGG_ACTIVE) ? " lag" : "");
        print_string("  ");
        lldp_put_dec(lldp_nb[i].ttl, false);
        print_string("s");
        print_string(lldp_warnings(&lldp_nb[i]) ? "  MISMATCH\n" : "\n");
    }
}
