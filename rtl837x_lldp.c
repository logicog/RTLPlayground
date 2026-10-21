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
#include "rtl837x_regs.h"
#include "rtl837x_port.h"
#include "phy.h"

extern __xdata bool lldp_enabled;
extern __code const struct machine machine;
extern __xdata uint8_t sfr_data[4];
static __code const uint8_t lldp_oui_8021[3] = {0x00, 0x80, 0xc2};
static __code const uint8_t lldp_oui_8023[3] = {0x00, 0x12, 0x0f};

__xdata uint16_t lldp_ticks;
__xdata uint16_t lldp_logical_port_status = 0xffff;
__xdata uint8_t lldp_tlv_mask = LLDP_TX_ALL;

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
    if (lldp_tlv_mask & LLDP_TX_SYSCAP) {
        *p++ = LLDP_TYPE(LLDP_CAPABILITIES_TLV_TYPE);
        *p++ = LLDP_CAPABILITIES_TLV_LENGTH;
        *p++ = LLDP_CAPABILITY_BRIDGE >> 8;
        *p++ = LLDP_CAPABILITY_BRIDGE & 0xff;
        *p++ = LLDP_CAPABILITY_BRIDGE >> 8;
        *p++ = LLDP_CAPABILITY_BRIDGE & 0xff;
    }

    //Management Address: IPv4
    if ((lldp_tlv_mask & LLDP_TX_MGMT) && (uip_hostaddr[0] | uip_hostaddr[1])) {
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

    for (uint8_t port = machine.min_port; port <= machine.max_port; port++) {

        if ((1 << port) & ports) {
            LLDP_O->payload[port_position] = '0' + machine.log_to_phys_port[port];
            LLDP_O->rtl_tag.pmask = HTONS((uint16_t)1 << port);

            p = tail;
            if (lldp_tlv_mask & LLDP_TX_PORTDESC)
                p += lldp_portdesc(p, port);
            p += lldp_port_org_tlvs(p, port);

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
    uint16_t machine_name_length = strlen(machine.machine_name);

    *p++ = LLDP_TYPE(LLDP_SYSDESC_TLV_TYPE);
    *p++ = machine_name_length;
    memcpyc(p, machine.machine_name, machine_name_length);
    return machine_name_length + 2; 
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
    uint16_t v;

    p = lldp_org_header(p, lldp_oui_8023, LLDP_8023_SUBTYPE_MAC_PHY, 5);
    if (machine.is_sfp[port]) {
        if (lldp_link_code(port) == LINKS_CODE_1000M)
            mau = LLDP_MAU_1000BASE_X_FD;
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

    if (lldp_tlv_mask & LLDP_TX_MACPHY)
        p = lldp_mac_phy(p, port);

    if (lldp_tlv_mask & LLDP_TX_PVID) {
        v = port_pvid_get(port);
        p = lldp_org_header(p, lldp_oui_8021, LLDP_8021_SUBTYPE_PVID, 2);
        *p++ = v >> 8;
        *p++ = v & 0xff;
    }

    if (lldp_tlv_mask & LLDP_TX_LAG) {
        lag = port_lag_of(port);
        p = lldp_org_header(p, lldp_oui_8023, LLDP_8023_SUBTYPE_LINK_AGG, 5);
        *p++ = LLDP_AGG_CAPABLE | (lag == PORT_LAG_NONE ? 0 : LLDP_AGG_ACTIVE);
        *p++ = 0;
        *p++ = 0;
        *p++ = 0;
        *p++ = lag == PORT_LAG_NONE ? 0 : lag + 1;
    }

    if (lldp_tlv_mask & LLDP_TX_MFS) {
        reg_read_m(RTL8373_REG_MAC_L2_PORT_MAX_LEN + ((uint16_t)port << 8));
        v = SFR_DATA_U16 & PORT_MAX_LEN_MASK;
        p = lldp_org_header(p, lldp_oui_8023, LLDP_8023_SUBTYPE_MAX_FRAME, 2);
        *p++ = v >> 8;
        *p++ = v & 0xff;
    }

    return p - start;
}
