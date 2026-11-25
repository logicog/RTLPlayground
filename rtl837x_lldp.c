#pragma codeseg BANK3
#pragma constseg BANK3

#include "rtl837x_common.h"
#include "rtl837x_lldp.h"
#include "rtl837x_sfr.h"
#include "rtl837x_regs.h"
#include "rtl837x_trap.h"
#include "dhcp.h"
#include "uip.h"
#include "machine.h"
#include "version.h"
#include "timer.h"
#include "rtl837x_port.h"
#include "phy.h"

extern __xdata bool lldp_enabled;
extern __xdata uint8_t sfr_data[4];
extern __code const struct machine machine;
extern __code const uint8_t *hex;
static __code const uint8_t lldp_oui_8021[3] = {0x00, 0x80, 0xc2};
static __code const uint8_t lldp_oui_8023[3] = {0x00, 0x12, 0x0f};

static __xdata struct timer lldp_timer;
__xdata uint8_t lldp_seconds;
__xdata uint8_t lldp_fast;
__xdata bool lldp_was_enabled;
__xdata uint16_t lldp_links_last;
__xdata uint16_t lldp_logical_port_status = 0xffff;

__xdata struct lldp_neighbor lldp_neighbors[LLDP_MAX_PORTS];
__xdata uint16_t lldp_tx_frames;
__xdata uint16_t lldp_rx_frames;
__xdata uint16_t lldp_rx_discarded;
__xdata uint16_t lldp_rx_ageouts;

// Incoming LLDP header (Ethernet + RTL CPU tag)
struct lldp_in_hdr {
    uint8_t dst[MAC_ADDR_LEN];
    uint8_t src[MAC_ADDR_LEN];
    struct rtl_tag rtl_tag;
};

#define LLDP_H ((__xdata struct lldp_in_hdr *)&uip_buf[0])

// Scratch buffers in XRAM (keeps internal DSEG/stack usage near zero)
static __xdata struct lldp_neighbor rx_tmp;
static __xdata uint8_t  lldp_rxbody;
static __xdata uint16_t lldp_rxlen;

void lldp_clear_neighbors(void) __banked
{
    memset(lldp_neighbors, 0, sizeof(lldp_neighbors));
}

void lldp_init(void) __banked
{
    timer_set(&lldp_timer, SYS_TICK_HZ);
    lldp_seconds = 0;
    lldp_fast = 0;
    lldp_was_enabled = false;
    lldp_links_last = 0;
    lldp_tx_frames = 0;
    lldp_rx_frames = 0;
    lldp_rx_discarded = 0;
    lldp_rx_ageouts = 0;
    memset(lldp_neighbors, 0, sizeof(lldp_neighbors));
}

static uint16_t lldp_links(void) __reentrant
{
    reg_read_m(RTL837X_REG_LINKS_STS);
    return ((uint16_t)sfr_data[2] << 8) | sfr_data[1];
}

static void lldp_age_neighbors(uint16_t links)
{
    static __xdata uint8_t i;

    for (i = MACHINE_MIN_PORT; i <= MACHINE_MAX_PORT; i++) {
        if (i >= LLDP_MAX_PORTS)
            break;
        if (!lldp_neighbors[i].ttl)
            continue;
        if (!(links & ((uint16_t)1 << i))) {
            lldp_neighbors[i].ttl = 0;
            lldp_rx_ageouts++;
        } else if (--lldp_neighbors[i].ttl == 0) {
            lldp_rx_ageouts++;
        }
    }
}

void lldp_tick(void) __banked
{
    static __xdata uint16_t links;

    if (!timer_expired(&lldp_timer))
        return;

    timer_reset(&lldp_timer);

    if (!lldp_enabled) {
        lldp_was_enabled = false;
        return;
    }

    links = lldp_links();
    lldp_age_neighbors(links);

    if (!lldp_was_enabled || (links & ~lldp_links_last & lldp_logical_port_status))
        lldp_fast = LLDP_FAST_TX_COUNT;
    lldp_links_last = links;
    lldp_was_enabled = true;

    if (lldp_fast) {
        lldp_fast--;
        lldp_seconds = 0;
        lldp_send();
        return;
    }

    if (++lldp_seconds < LLDP_TX_INTERVAL_S)
        return;
    lldp_seconds = 0;
    lldp_send();
}

// ============================================================================
// Incoming LLDP (RX) parser & neighbor table
// ============================================================================

static void lldp_fmt_mac(__xdata char *dst, __xdata const uint8_t *mac)
{
    static __xdata uint8_t i;
    static __xdata uint8_t b;

    for (i = 0; i < 6; i++) {
        b = mac[i];
        *dst++ = hex[b >> 4];
        *dst++ = hex[b & 0x0f];
        if (i < 5)
            *dst++ = ':';
    }
    *dst = 0;
}

static void lldp_fmt_ipv4(__xdata char *dst, __xdata const uint8_t *ip)
{
    static __xdata uint8_t i, v, d;

    for (i = 0; i < 4; i++) {
        v = ip[i];
        if (v >= 100) {
            d = v / 100;
            *dst++ = '0' + d;
            v -= d * 100;
            d = v / 10;
            *dst++ = '0' + d;
            v -= d * 10;
        } else if (v >= 10) {
            d = v / 10;
            *dst++ = '0' + d;
            v -= d * 10;
        }
        *dst++ = '0' + v;
        if (i < 3)
            *dst++ = '.';
    }
    *dst = 0;
}

static void lldp_copy_str(__xdata char *dst, uint8_t max_len, __xdata const uint8_t *src, uint16_t len)
{
    static __xdata uint8_t out;
    static __xdata uint16_t i;
    static __xdata uint8_t c;

    out = 0;
    for (i = 0; i < len && out < (uint8_t)(max_len - 1); i++) {
        c = src[i];
        if (c >= 0x20 && c <= 0x7e)
            dst[out++] = c;
    }
    dst[out] = 0;
}

void lldp_in(void) __banked
{
    static __xdata uint8_t  port;
    static __xdata uint16_t pos;
    static __xdata uint16_t tlv_hdr;
    static __xdata uint8_t  tlv_type;
    static __xdata uint16_t tlv_len;
    static __xdata uint8_t  seen_mandatory;

    lldp_rxbody = TRAP_RX_BODY();
    lldp_rxlen = uip_len;
    uip_len = 0;

    if (lldp_rxlen < (uint16_t)lldp_rxbody + 2) {
        lldp_rx_discarded++;
        return;
    }

    if (uip_buf[lldp_rxbody] != (uint8_t)(LLDP_ETHERTYPE >> 8)
        || uip_buf[lldp_rxbody + 1] != (uint8_t)(LLDP_ETHERTYPE & 0xff)) {
        return;
    }

    port = ((uint8_t)HTONS(LLDP_H->rtl_tag.pmask)) & 0x0f;
    if (!MACHINE_PORT_VALID(port) || port >= LLDP_MAX_PORTS) {
        lldp_rx_discarded++;
        return;
    }

    memset(&rx_tmp, 0, sizeof(rx_tmp));
    seen_mandatory = 0;
    pos = (uint16_t)lldp_rxbody + 2;

    while (pos + 2 <= lldp_rxlen) {
        tlv_hdr = ((uint16_t)uip_buf[pos] << 8) | uip_buf[pos + 1];
        tlv_type = (uint8_t)(tlv_hdr >> 9);
        tlv_len = tlv_hdr & 0x01ff;
        pos += 2;

        if (pos + tlv_len > lldp_rxlen) {
            lldp_rx_discarded++;
            return;
        }

        if (tlv_type == 0) {
            // End of LLDPDU
            break;
        } else if (tlv_type == LLDP_CHASSIS_ID_TLV_TYPE) {
            if (tlv_len < 2) {
                lldp_rx_discarded++;
                return;
            }
            if (uip_buf[pos] == 4 && tlv_len == 7) {
                lldp_fmt_mac(rx_tmp.chassis, &uip_buf[pos + 1]);
            } else if (uip_buf[pos] == 5 && tlv_len == 6 && uip_buf[pos + 1] == 1) {
                lldp_fmt_ipv4(rx_tmp.chassis, &uip_buf[pos + 2]);
            } else {
                lldp_copy_str(rx_tmp.chassis, LLDP_CHASSIS_MAX, &uip_buf[pos + 1], tlv_len - 1);
            }
            seen_mandatory |= 0x01;
        } else if (tlv_type == LLDP_PORT_ID_TLV_TYPE) {
            if (tlv_len < 2) {
                lldp_rx_discarded++;
                return;
            }
            if (uip_buf[pos] == 3 && tlv_len == 7) {
                lldp_fmt_mac(rx_tmp.port_id, &uip_buf[pos + 1]);
            } else if (uip_buf[pos] == 4 && tlv_len == 6 && uip_buf[pos + 1] == 1) {
                lldp_fmt_ipv4(rx_tmp.port_id, &uip_buf[pos + 2]);
            } else {
                lldp_copy_str(rx_tmp.port_id, LLDP_PORTID_MAX, &uip_buf[pos + 1], tlv_len - 1);
            }
            seen_mandatory |= 0x02;
        } else if (tlv_type == LLDP_TTL_TLV_TYPE) {
            if (tlv_len != 2) {
                lldp_rx_discarded++;
                return;
            }
            rx_tmp.ttl = ((uint16_t)uip_buf[pos] << 8) | uip_buf[pos + 1];
            seen_mandatory |= 0x04;
        } else if (tlv_type == LLDP_PORT_DESC_TLV_TYPE) {
            lldp_copy_str(rx_tmp.port_desc, LLDP_PORTDESC_MAX, &uip_buf[pos], tlv_len);
        } else if (tlv_type == LLDP_SYSNAME_TLV_TYPE) {
            lldp_copy_str(rx_tmp.sys_name, LLDP_SYSNAME_MAX, &uip_buf[pos], tlv_len);
        } else if (tlv_type == LLDP_CAPABILITIES_TLV_TYPE && tlv_len >= 4) {
            rx_tmp.caps_enabled = ((uint16_t)uip_buf[pos + 2] << 8) | uip_buf[pos + 3];
        } else if (tlv_type == LLDP_MGMT_ADDR_TLV_TYPE && tlv_len >= 6) {
            // addr_str_len == 5 (1 subtype + 4 IPv4 bytes), subtype == 1 (IPv4)
            if (uip_buf[pos] == 5 && uip_buf[pos + 1] == LLDP_MGMT_ADDR_SUBTYPE_IPV4) {
                memcpy(rx_tmp.mgmt_ip, &uip_buf[pos + 2], 4);
            }
        }

        pos += tlv_len;
    }

    if (seen_mandatory != 0x07) {
        lldp_rx_discarded++;
        return;
    }

    lldp_rx_frames++;
    if (rx_tmp.ttl == 0) {
        // Shutdown LLDPDU: expire immediately
        lldp_neighbors[port].ttl = 0;
        return;
    }

    memcpy(&lldp_neighbors[port], &rx_tmp, sizeof(struct lldp_neighbor));
}

void lldp_show(void) __banked
{
    static __xdata uint8_t i;
    static __xdata uint8_t found;
    static __xdata char ipbuf[16];

    print_string("LLDP: ");
    print_string(lldp_enabled ? "on" : "off");
    print_string("\nTX: ");
    itoa_short(lldp_tx_frames);
    print_string("  RX: ");
    itoa_short(lldp_rx_frames);
    print_string("  Discarded: ");
    itoa_short(lldp_rx_discarded);
    print_string("  Ageouts: ");
    itoa_short(lldp_rx_ageouts);
    write_char('\n');

    found = 0;
    for (i = MACHINE_MIN_PORT; i <= MACHINE_MAX_PORT; i++) {
        if (i >= LLDP_MAX_PORTS || !lldp_neighbors[i].ttl)
            continue;
        if (!found) {
            print_string("Port  SysName  Chassis  RemotePort  MgmtIP  TTL\n");
            found = 1;
        }
        print_phys_port(i);
        print_string("  ");
        if (lldp_neighbors[i].sys_name[0])
            print_string_x(lldp_neighbors[i].sys_name);
        else
            write_char('-');
        print_string("  ");
        print_string_x(lldp_neighbors[i].chassis);
        print_string("  ");
        print_string_x(lldp_neighbors[i].port_id);
        print_string("  ");
        if (lldp_neighbors[i].mgmt_ip[0] | lldp_neighbors[i].mgmt_ip[1]
            | lldp_neighbors[i].mgmt_ip[2] | lldp_neighbors[i].mgmt_ip[3]) {
            lldp_fmt_ipv4(ipbuf, lldp_neighbors[i].mgmt_ip);
            print_string_x(ipbuf);
        } else {
            write_char('-');
        }
        print_string("  ");
        itoa_short(lldp_neighbors[i].ttl);
        print_string("s\n");
    }
    if (!found)
        print_string("No LLDP neighbors\n");
}

// ============================================================================
// Outgoing LLDP packet (TX)
// ============================================================================

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

    if (!ports)
        return;

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
            p += lldp_port_org_tlvs(p, port);

            // End of LLDPDU TLV
            *p++ = 0x00; //padding
            *p++ = 0x00; //padding

            while (p < (LLDP_O->payload + LLDP_MIN_ETHERNET_PAYLOAD_LENGTH))
                *p++ = 0x00;

            uip_len = MAC_ADDR_LEN + MAC_ADDR_LEN + sizeof(struct rtl_tag) + LLDP_ETHERTYPE_LENGTH + (p - LLDP_O->payload);

            tcpip_output();
            lldp_tx_frames++;
        }
    }
}

void lldp_send(void) __banked __reentrant
{
    uint16_t active = lldp_logical_port_status & lldp_links();
    lldp_send_ports(LLDP_TTL_TTL_SECONDS, active);
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

static __xdata uint8_t *lldp_org_header(__xdata uint8_t *p, __code const uint8_t *oui, uint8_t subtype, uint8_t length) __reentrant
{
    *p++ = (uint8_t)LLDP_TYPE(LLDP_ORG_TLV_TYPE);
    *p++ = LLDP_ORG_TLV_HEADER_LENGTH + length;
    memcpyc(p, oui, 3);
    p += 3;
    *p++ = subtype;
    return p;
}

static uint8_t lldp_link_code(uint8_t port) __reentrant
{
    uint8_t b;

    reg_read_m(port < 8 ? RTL837X_REG_LINKS : RTL837X_REG_LINKS_89);
    b = sfr_data[3 - ((port & 7) >> 1)];
    return (port & 1) ? b >> 4 : b & 0xf;
}

static __xdata uint8_t *lldp_mac_phy(__xdata uint8_t *p, uint8_t port) __reentrant
{
    uint16_t cap = 0;
    uint16_t mau = LLDP_MAU_UNKNOWN;
    uint8_t autoneg = 0;
    uint8_t lc;
    uint16_t v;

    p = lldp_org_header(p, lldp_oui_8023, LLDP_8023_SUBTYPE_MAC_PHY, 5);
    lc = lldp_link_code(port);
    if (machine.is_sfp[port]) {
        if (lc == LINKS_CODE_1000M)
            mau = LLDP_MAU_1000BASE_X_FD;
        else if (lc == LINKS_CODE_10G)
            mau = LLDP_MAU_10GBASE_X;
    } else {
        autoneg = LLDP_AUTONEG_SUPPORTED;
        phy_read(port, PHY_MMD_AN, PHY_ANEG_CTRL);
        if (SFR_DATA_U16 & PHY_ANEG_CTRL_ENABLE)
            autoneg |= LLDP_AUTONEG_ENABLED;
        phy_read(port, PHY_MMD_AN, PHY_ANEG_ADV);
        v = SFR_DATA_U16;
        if (v & PHY_ANEG_ADV_10BASE_T_HD)
            cap |= LLDP_AUTONEG_CAP_10BASE_T;
        if (v & PHY_ANEG_ADV_10BASE_T_FD)
            cap |= LLDP_AUTONEG_CAP_10BASE_T_FD;
        if (v & PHY_ANEG_ADV_100BASE_TX_HD)
            cap |= LLDP_AUTONEG_CAP_100BASE_TX;
        if (v & PHY_ANEG_ADV_100BASE_TX_FD)
            cap |= LLDP_AUTONEG_CAP_100BASE_TX_FD;
        phy_read(port, PHY_MMD31, PHY_MMD31_GBCR);
        if (SFR_DATA_U16 & PHY_GBCR_1000BASE_T_FD)
            cap |= LLDP_AUTONEG_CAP_1000BASE_T_FD;
        if (lc == LINKS_CODE_2G5) {
            mau = LLDP_MAU_2500BASE_T;
        } else if (lc == LINKS_CODE_5G) {
            mau = LLDP_MAU_5GBASE_T;
        } else if (lc == LINKS_CODE_10G) {
            mau = LLDP_MAU_10GBASE_T;
        } else {
            phy_read(port, PHY_MMD31, PHY_MMD31_PHYSR);
            v = SFR_DATA_U16;
            switch (PHYSR_SPEED(v)) {
            case PHYSR_SPEED_10M:
                mau = (v & PHYSR_FULL_DUPLEX) ? LLDP_MAU_10BASE_T_FD : LLDP_MAU_10BASE_T_HD;
                break;
            case PHYSR_SPEED_100M:
                mau = (v & PHYSR_FULL_DUPLEX) ? LLDP_MAU_100BASE_TX_FD : LLDP_MAU_100BASE_TX_HD;
                break;
            case PHYSR_SPEED_1000M:
                mau = (v & PHYSR_FULL_DUPLEX) ? LLDP_MAU_1000BASE_T_FD : LLDP_MAU_1000BASE_T_HD;
                break;
            }
        }
    }
    *p++ = autoneg;
    *p++ = cap >> 8;
    *p++ = cap & 0xff;
    *p++ = mau >> 8;
    *p++ = mau & 0xff;
    return p;
}

uint8_t lldp_port_org_tlvs(__xdata uint8_t *p, uint8_t port) __banked __reentrant
{
    __xdata uint8_t *start = p;
    uint16_t v;
    uint8_t lag;

    p = lldp_mac_phy(p, port);

    v = port_pvid_get(port);
    p = lldp_org_header(p, lldp_oui_8021, LLDP_8021_SUBTYPE_PVID, 2);
    *p++ = v >> 8;
    *p++ = v & 0xff;

    lag = port_lag_of(port);
    p = lldp_org_header(p, lldp_oui_8023, LLDP_8023_SUBTYPE_LINK_AGG, 5);
    *p++ = LLDP_AGG_CAPABLE | (lag == PORT_LAG_NONE ? 0 : LLDP_AGG_ACTIVE);
    *p++ = 0;
    *p++ = 0;
    *p++ = 0;
    *p++ = lag == PORT_LAG_NONE ? 0 : lag + 1;

    reg_read_m(RTL8373_REG_MAC_L2_PORT_MAX_LEN + ((uint16_t)port << 8));
    v = SFR_DATA_U16 & PORT_MAX_LEN_MASK;
    p = lldp_org_header(p, lldp_oui_8023, LLDP_8023_SUBTYPE_MAX_FRAME, 2);
    *p++ = v >> 8;
    *p++ = v & 0xff;

    return p - start;
}
