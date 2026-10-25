#pragma codeseg BANK3
#pragma constseg BANK3

#include "rtl837x_common.h"
#include "rtl837x_lldp.h"
#include "rtl837x_sfr.h"
#include "rtl837x_regs.h"
#include "rtl837x_trap.h"
#include "dhcp.h"
#include "uip.h"
#include "uip/uip.h"
#include "machine.h"

extern __xdata bool lldp_enabled;
extern __code const struct machine machine;
extern __xdata uint8_t sfr_data[4];
extern __xdata uint8_t outbuf[TCP_OUTBUF_SIZE];
extern __xdata uint16_t slen;

__xdata uint16_t lldp_ticks;
__xdata uint8_t lldp_sec_ticks;
/* Set in lldp_init(): initializers of xdata globals are not applied on this
 * chip, the startup copy (__mcs51_genXINIT) writes through MPAGE and misses */
__xdata uint16_t lldp_logical_port_status;
__xdata struct lldp_neighbor lldp_neighbors[LLDP_NB_PORTS];

static void lldp_age(void);

void lldp_init(void) __banked
{
    lldp_ticks = 0;
    lldp_sec_ticks = 0;
    lldp_logical_port_status = 0xffff;	/* send on all ports until "port <n> lldp block" */
    lldp_neighbors_clear();
}

void lldp_neighbors_clear(void) __banked
{
    // memset() takes a byte count, so one entry at a time
    for (__xdata uint8_t i = 0; i < LLDP_NB_PORTS; i++)
        memset((__xdata uint8_t *)&lldp_neighbors[i], 0, sizeof(struct lldp_neighbor));
}

void lldp_tick(void) __banked
{
    lldp_ticks++;

    if (++lldp_sec_ticks >= SYS_TICK_HZ) {
        lldp_sec_ticks = 0;
        lldp_age();
    }

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

    for (uint8_t port = MACHINE_MIN_PORT; port <= MACHINE_MAX_PORT; port++) {

        if ((1 << port) & lldp_logical_port_status) {
            LLDP_O->payload[port_position] = '0' + machine.log_to_phys_port[port];
            LLDP_O->rtl_tag.pmask = HTONS((uint16_t)1 << port);

            tcpip_output();
        }
    }
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


/*
 * Neighbor discovery: parse the LLDPDUs trapped to the CPU, see "lldp on",
 * and keep what the partner on each port announced until its TTL runs out
 * or the link goes down.
 */

struct lldp_in_hdr {
    struct uip_eth_addr dst;
    struct uip_eth_addr src;
    struct rtl_tag rtl_tag;
};

#define LLDP_H ((__xdata struct lldp_in_hdr *)&uip_buf[0])

/* State and scratch of the neighbor code live in xdata: every byte of DSEG is one of stack */
__xdata uint16_t lldp_link;
__xdata struct lldp_neighbor * __xdata lldp_nb;

static void lldp_age(void)
{
    if (!lldp_enabled)
        return;

    reg_read_m(RTL837X_REG_LINKS_STS);
    lldp_link = (uint16_t)sfr_data[1] | ((uint16_t)sfr_data[2] << 8);

    for (uint8_t port = MACHINE_MIN_PORT; port <= MACHINE_MAX_PORT; port++) {
        lldp_nb = &lldp_neighbors[port];
        if (!lldp_nb->ttl)
            continue;
        if (!(lldp_link & PORT_BIT(port)))
            lldp_nb->ttl = 0;
        else
            lldp_nb->ttl--;
    }
}

static bool lldp_has_ip(void)
{
    return lldp_nb->mgmt_ip[0] | lldp_nb->mgmt_ip[1] | lldp_nb->mgmt_ip[2] | lldp_nb->mgmt_ip[3];
}

static char lldp_hex_char(uint8_t n)
{
    n &= 0x0f;
    return n < 10 ? '0' + n : 'a' - 10 + n;
}

__xdata uint8_t * __xdata lldp_src;
__xdata uint16_t lldp_src_len;
__xdata bool lldp_binary;

// Copy lldp_src_len bytes at lldp_src as text into dst, or as aa:bb:.. if lldp_binary
static void lldp_copy_str(__xdata char *dst)
{
    __xdata char *end = dst + LLDP_NB_STR_SIZE - 1;

    while (lldp_src_len-- && dst < end) {
        uint8_t c = *lldp_src++;
        if (lldp_binary) {
            if (dst + 2 > end)
                break;
            *dst++ = lldp_hex_char(c >> 4);
            *dst++ = lldp_hex_char(c);
            if (lldp_src_len && dst < end)
                *dst++ = ':';
        } else {
            if (c < 0x20 || c > 0x7e || c == '"' || c == '\\')
                c = '.';
            *dst++ = c;
        }
    }
    *dst = NUL;
}

void lldp_in(void) __banked
{
    __xdata uint8_t port, type, subtype;
    __xdata uint16_t pos, len;

    pos = TRAP_RX_BODY();
    len = uip_len;
    uip_len = 0;

    if (len < pos + LLDP_ETHERTYPE_LENGTH
        || uip_buf[pos] != (LLDP_ETHERTYPE >> 8) || uip_buf[pos + 1] != (LLDP_ETHERTYPE & 0xff))
        return;
    pos += LLDP_ETHERTYPE_LENGTH;

    port = ((uint8_t)HTONS(LLDP_H->rtl_tag.pmask)) & 0x0f;
    if (!MACHINE_PORT_VALID(port))
        return;

    lldp_nb = &lldp_neighbors[port];
    memset((__xdata uint8_t *)lldp_nb, 0, sizeof(struct lldp_neighbor));

    // TLVs: 7 bits type, 9 bits length, value
    while (pos + 2 <= len) {
        type = uip_buf[pos] >> 1;
        lldp_src_len = ((uint16_t)(uip_buf[pos] & 1) << 8) | uip_buf[pos + 1];
        pos += 2;
        if (type == LLDP_END_TLV_TYPE || pos + lldp_src_len > len)
            break;
        lldp_src = &uip_buf[pos];
        pos += lldp_src_len;

        switch (type) {
        case LLDP_CHASSIS_ID_TLV_TYPE:
        case LLDP_PORT_ID_TLV_TYPE:
            if (!lldp_src_len)
                break;
            subtype = *lldp_src++;
            lldp_src_len--;
            lldp_binary = false;
            if (type == LLDP_CHASSIS_ID_TLV_TYPE) {
                if (subtype == LLDP_CHASSIS_ID_SUBTYPE_MAC || subtype == LLDP_CHASSIS_ID_SUBTYPE_ADDR)
                    lldp_binary = true;
                lldp_copy_str(lldp_nb->str[LLDP_NB_CHASSIS_ID]);
            } else {
                if (subtype == LLDP_PORT_ID_SUBTYPE_MAC || subtype == LLDP_PORT_ID_SUBTYPE_ADDR
                    || subtype == LLDP_PORT_ID_SUBTYPE_CIRCUIT)
                    lldp_binary = true;
                lldp_copy_str(lldp_nb->str[LLDP_NB_PORT_ID]);
            }
            break;
        case LLDP_TTL_TLV_TYPE:
            if (lldp_src_len >= 2)
                lldp_nb->ttl = ((uint16_t)lldp_src[0] << 8) | lldp_src[1];
            break;
        case LLDP_PORT_DESC_TLV_TYPE:
            lldp_binary = false;
            lldp_copy_str(lldp_nb->str[LLDP_NB_PORT_DESC]);
            break;
        case LLDP_SYSNAME_TLV_TYPE:
            lldp_binary = false;
            lldp_copy_str(lldp_nb->str[LLDP_NB_SYS_NAME]);
            break;
        case LLDP_SYSCAP_TLV_TYPE:
            if (lldp_src_len >= 4) {
                lldp_nb->sys_cap = ((uint16_t)lldp_src[0] << 8) | lldp_src[1];
                lldp_nb->en_cap = ((uint16_t)lldp_src[2] << 8) | lldp_src[3];
            }
            break;
        case LLDP_MGMT_ADDR_TLV_TYPE:
            // Address string length (subtype + address), subtype, address; keep the first IPv4
            if (lldp_src_len >= 6 && lldp_src[0] == 5 && lldp_src[1] == LLDP_MGMT_ADDR_FAMILY_IPV4
                && !lldp_has_ip())
                memcpy(lldp_nb->mgmt_ip, &lldp_src[2], 4);
            break;
        }
    }

    // Chassis ID, Port ID and TTL are mandatory, TTL 0 is a shutdown LLDPDU
    if (!lldp_nb->str[LLDP_NB_CHASSIS_ID][0] || !lldp_nb->str[LLDP_NB_PORT_ID][0])
        lldp_nb->ttl = 0;
}

void lldp_show(void) __banked
{
    print_string("LLDP "); print_string(lldp_enabled ? "on" : "off");
    print_string(", send mask "); print_short(lldp_logical_port_status); write_char('\n');
    for (uint8_t i = MACHINE_MIN_PORT; i <= MACHINE_MAX_PORT; i++) {
        print_string("port "); itoa(machine.log_to_phys_port[i]);
        print_string((lldp_logical_port_status & PORT_BIT(i)) ? " send on\n" : " send off\n");
    }
    for (uint8_t i = MACHINE_MIN_PORT; i <= MACHINE_MAX_PORT; i++) {
        lldp_nb = &lldp_neighbors[i];
        if (!lldp_nb->ttl)
            continue;
        print_string("port "); itoa(machine.log_to_phys_port[i]);
        print_string(" name \""); print_string_x(lldp_nb->str[LLDP_NB_SYS_NAME]);
        print_string("\" chassis "); print_string_x(lldp_nb->str[LLDP_NB_CHASSIS_ID]);
        print_string(" port-id "); print_string_x(lldp_nb->str[LLDP_NB_PORT_ID]);
        print_string(" desc \""); print_string_x(lldp_nb->str[LLDP_NB_PORT_DESC]);
        print_string("\" ip ");
        for (uint8_t j = 0; j < 4; j++) {
            itoa(lldp_nb->mgmt_ip[j]);
            if (j < 3)
                write_char('.');
        }
        print_string(" cap "); print_short(lldp_nb->sys_cap);
        write_char('/'); print_short(lldp_nb->en_cap);
        print_string(" ttl "); itoa_short(lldp_nb->ttl);
        write_char('\n');
    }
}

/*
 * /lldp_nb.json: the active neighbors. Written here rather than in
 * httpd/page_impl.c because BANK1 is nearly full. The strings were sanitized
 * on reception and go out as they are.
 */

/* JSON keys of lldp_neighbor.str, in index order */
static __code const char * __code const lldp_nb_keys[LLDP_NB_STRS] = {
    ",\"ch\":\"", ",\"pi\":\"", ",\"pd\":\"", ",\"sn\":\""
};

/* Worst case per neighbor is about 230 bytes, all 9 ports fit in outbuf */
#define LLDP_NB_JSON_MAX 240

static void lldp_out_str(__code const char *s)
{
    while (*s)
        outbuf[slen++] = *s++;
}

__xdata uint8_t lldp_digits[5];

__xdata uint8_t lldp_ndigits;

static void lldp_out_dec(uint16_t v)
{
    lldp_ndigits = 0;

    do {
        lldp_digits[lldp_ndigits++] = '0' + v % 10;
        v /= 10;
    } while (v);
    while (lldp_ndigits)
        outbuf[slen++] = lldp_digits[--lldp_ndigits];
}

void lldp_neighbors_json(void) __banked
{
    __xdata char *v;
    char sep = '[';

    slen = 0;
    lldp_out_str("HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Type: application/json\r\n\r\n");
    for (uint8_t i = MACHINE_MIN_PORT; i <= MACHINE_MAX_PORT; i++) {
        lldp_nb = &lldp_neighbors[i];
        if (!lldp_nb->ttl || slen > TCP_OUTBUF_SIZE - LLDP_NB_JSON_MAX)
            continue;
        outbuf[slen++] = sep;
        sep = ',';
        lldp_out_str("{\"port\":");
        lldp_out_dec(machine.log_to_phys_port[i]);
        lldp_out_str(",\"ttl\":");
        lldp_out_dec(lldp_nb->ttl);
        for (uint8_t k = 0; k < LLDP_NB_STRS; k++) {
            lldp_out_str(lldp_nb_keys[k]);
            v = lldp_nb->str[k];
            while (*v)
                outbuf[slen++] = *v++;
            outbuf[slen++] = '"';
        }
        lldp_out_str(",\"ip\":\"");
        if (lldp_has_ip()) {
            for (uint8_t j = 0; j < 4; j++) {
                if (j)
                    outbuf[slen++] = '.';
                lldp_out_dec(lldp_nb->mgmt_ip[j]);
            }
        }
        lldp_out_str("\",\"cap\":");
        lldp_out_dec(lldp_nb->sys_cap);
        lldp_out_str(",\"ecap\":");
        lldp_out_dec(lldp_nb->en_cap);
        outbuf[slen++] = '}';
    }
    if (sep == '[')
        outbuf[slen++] = sep;
    outbuf[slen++] = ']';
}
