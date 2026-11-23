/*
 * This is a driver implementation for the IGMP features for the RTL827x platform
 * This code is in the Public Domain
 */

// #define REGDBG
// #define DEBUG

#define IPMC_USES_L3MC

#include <stdint.h>
#include <stddef.h>
#include "rtl837x_common.h"
#include "rtl837x_sfr.h"
#include "rtl837x_regs.h"
#include "rtl837x_igmp.h"
#include "rtl837x_port.h"
#include "rtl837x_stp.h"
#include "rtl837x_trap.h"
#include "machine.h"

extern __code const struct machine machine;
extern __xdata uint8_t igmpEnabled;

#include "uip.h"
#include "uip_arp.h"

#pragma codeseg BANK3
#pragma constseg BANK3

extern __xdata uint8_t cpuPort;
extern __xdata uint8_t sfr_data[4];
extern __xdata struct machine_runtime machine_detected;
extern __xdata uint8_t uip_buf[UIP_CONF_BUFFER_ALLOC_SIZE];

__xdata uint16_t idx;
#define IGMP_IP_HDR		offsetof(struct igmp_pkt, hlen)
#define IGMP_IP_IHL		0x0f
#define IGMP_GROUP_OFS		4
#define IGMP_V3_RECORD_OFS	8
#define IGMP_V3_RECORD_LEN	8

static __xdata uint16_t igmp_grp;
static __xdata uint8_t igmp_join;
static __xdata uint8_t igmp_vofs;
static __xdata uint8_t igmp_query;
static __xdata uint8_t igmp_report;
static __xdata uint16_t igmp_rports;
static __xdata uint16_t igmp_vid;
static __xdata uint16_t igmp_vmask;

#ifdef IPMC_USES_L3MC
struct ipmc_table_entry {
	uint8_t sip[4];
	uint8_t dip[4];
	uint16_t pmask;
	uint8_t igmp_index;
	uint8_t igmp_asic;
};
static __xdata struct ipmc_table_entry entry;
#else
struct l2mc_table_entry {
	uint8_t mac[6];
	uint16_t vlan;
	uint16_t pmask;
	uint8_t is_svl;
	uint8_t igmp_index;
	uint8_t igmp_asic;
};
static __xdata struct l2mc_table_entry entry;
#endif

struct igmp_pkt {
	uint8_t ipv4mc_addr[6];
	uint8_t src_addr[6];
	struct rtl_tag rtl_tag;
	uint16_t ipv4_tag;
	uint8_t hlen;
	uint8_t dscp;
	uint16_t len;
	uint16_t id;
	uint16_t flags;
	uint8_t ttl;
	uint8_t protocol;
	uint16_t checksum;
	uint8_t src_ip[4];
	uint8_t dst_ip[4];
	uint8_t ip_opt;
	uint8_t ip_len;
	uint16_t ra;
	uint8_t igmp_type;
	uint8_t igmp_res1;
	uint16_t igmp_checksum;
	uint16_t igmp_res2;
	uint16_t igmp_records;
	uint8_t igmp_rtype;
	uint8_t igmp_auxlen;
	uint16_t igmp_nsrc;
	uint8_t mc_ip[4];
};
#define IGMP_I ((__xdata struct igmp_pkt *)&uip_buf[0])
#define IGMP_V ((__xdata struct igmp_pkt *)&uip_buf[igmp_vofs])
#define IGMP_ETH_HDR offsetof(struct igmp_pkt, ipv4_tag)
#define IGMP_STP_FORWARDING 3

struct igmp_out {
	uint8_t dst[6];
	uint8_t src[6];
	struct rtl_tag rtl_tag;
	struct vlan_tag vlan_tag;
};

#define IGMP_O ((__xdata struct igmp_out *)&uip_buf[RTL_FRAME_DESC_SIZE])


static void igmp_filter(void)
{
	__xdata uint32_t act = LOOKUP_MISS_FLOOD;

	if (igmp_rports) {
		act = machine_detected.isRTL8373 ? LOOKUP_MISS_DROP_9 : LOOKUP_MISS_DROP_6;
		reg_bit_set(RTL837X_L2_CTRL, L2_CTRL_LUT_IPMC_HASH);
	} else {
		reg_bit_clear(RTL837X_L2_CTRL, L2_CTRL_LUT_IPMC_HASH);
	}
	REG_SET(RTL837X_IPV4_PORT_MC_LM_ACT, act);
}


void igmp_setup(void) __banked
{
	uint8_t i;
	print_string("igmp_setup called\n");
	igmpEnabled = 0;
	igmp_rports = 0;
	// For now, forward all unkown IP-MC pkts (2 bits per port. 00: flood via floodmask, 01: drop, 10: trap, 11: to rport)
	REG_SET(RTL837X_IPV4_PORT_MC_LM_ACT, LOOKUP_MISS_FLOOD);
	REG_SET(RTL837X_IPV6_PORT_MC_LM_ACT, LOOKUP_MISS_FLOOD);

	// Define ports where unknown MC addresses are flooded to:
	uint16_t mask = PMASK_6;
	if (machine_detected.isRTL8373)
		mask = PMASK_9;
	REG_SET(RTL837X_IPV4_UNKN_MC_FLD_PMSK, mask);
	REG_SET(RTL837X_IPV6_UNKN_MC_FLD_PMSK, mask);

	reg_bit_clear(RTL837X_L2_CTRL, L2_CTRL_LUT_IPMC_HASH);

	/* Configure per-port IGMP operations when protocol messages are received
	 * bits 0-9 enable MC protocol snooping
	 * bit 10: Enable dynamic router port learning
	 * bit 11: Enable MRP (Multicast Routing Protocol)
	 * bit 12: Allow fast leave
	 * bit 13: Allow IGMP reporting
	 * bit 14: Allow queries
	 * bits 16-24 configure max MC group used by that port.
	 * Operations for IGMP packets are:
	 * 00: handle in HW by ASIC
	 * 01: flood
	 * 10: trap
	 * 11: drop
	 * Bits 0-1: IGMPv1, 2-3: IGMPv2, 4-5: IGMPv3, 6-7: MLDv1 (for IPv6), 8-9: MLDv2
	 * For now the IGMP protocols are flooded (01), MLD which MAC-based is handled by ASIC
	 * All messages are allowed and maximum MC group is 0xff
	 */
	for (i = MACHINE_MIN_PORT; i <= MACHINE_MAX_PORT; i++)
		REG_SET(RTL837X_IGMP_PORT_CFG + (i << 2), IGMP_MAX_GROUP | IGMP_PROTOCOL_ENABLE | IGMP_FLOOD);

/*	// Allow all physical ports to be dynamic router ports
	reg_read_m(RTL837X_IGMP_ROUTER_PORT);
	if (machine_detected.isRTL8373) {
		REG_WRITE(RTL837X_IGMP_ROUTER_PORT, PMASK_9 >> 8, PMASK_9 & 0xff, sfr_data[1], sfr_data[0]);
	} else {
		REG_WRITE(RTL837X_IGMP_ROUTER_PORT, PMASK_6 >> 8, PMASK_6 & 0xff, sfr_data[1], sfr_data[0]);
	}
*/
}


void igmp_enable(void) __banked
{
	print_string("igmp_enable called\n");
	igmpEnabled = 1;
	igmp_rports = 0;
	// Configure trapping of unhandled IGMP protocol packets to CPU
	REG_SET(RTL837X_IGMP_TRAP_CFG, IGMP_CPU_PORT | IGMP_TRAP_PRIORITY);

	igmp_filter();

	// Configure per-port IGMP configuration, bits 0-10 enable MC protocol snooping,
	// bits 16-24 configure max MC group used by that port. Trap to CPU (10)
	for (uint8_t i = MACHINE_MIN_PORT; i <= MACHINE_MAX_PORT; i++) {
		REG_SET(RTL837X_IGMP_PORT_CFG + (i << 2), IGMP_MAX_GROUP | IGMP_PROTOCOL_ENABLE | IGMP_TRAP);
	}
}


/*
 * Configures the IGMP static router port(s) that will receive all IGMP
 * Report and Leave messages
 */
void igmp_router_port_set(uint16_t pmask) __banked
{
	print_string("igmp_router_port_set: "); print_short(pmask); print_string(", currently set to:\n");
	reg_read_m(RTL837X_IGMP_ROUTER_PORT);
	print_sfr_data(); write_char('\n');
	REG_WRITE(RTL837X_IGMP_ROUTER_PORT, sfr_data[0], sfr_data[1], pmask >> 8, pmask & 0xff);
}


/*
 * IGMP show the current entries and state
 */
void igmp_show(void) __banked
{
	print_string("igmp_show called\n");
	for (uint8_t i = MACHINE_MIN_PORT; i <= MACHINE_MAX_PORT; i++) {
		write_char('0' + i); write_char(':');
		reg_read_m(RTL837X_IGMP_PORT_CFG + (i << 2));
		print_sfr_data();
		write_char('\n');
	}
	// TODO: print all L3MC entries in the table
}


#ifdef IPMC_USES_L3MC
void entry_to_l3mc(void)
{
	REG_WRITE(RTL837x_TBL_DATA_IN_A, entry.sip[0], entry.sip[1], entry.sip[2], entry.sip[3]);
	REG_WRITE(RTL837x_TBL_DATA_IN_B, ((entry.pmask & 0x3) << 6) | (entry.dip[0] & 0xf) | 0x10, entry.dip[1], entry.dip[2], entry.dip[3]);
	REG_WRITE(RTL837x_TBL_DATA_IN_C, 0x00, entry.igmp_asic & 1, entry.igmp_index, entry.pmask >> 2);
}
#else
void entry_to_l2mc(void)
{
	// R5cb8-5e004201 R5cbc-20010100 R5cc0-00000020 R5cac-00000403
	REG_WRITE(RTL837x_TBL_DATA_IN_A, entry.mac[2], entry.mac[3], entry.mac[4], entry.mac[5]);
	REG_WRITE(RTL837x_TBL_DATA_IN_B, 0x20 | ((entry.pmask & 0x3) << 6) | (entry.vlan >> 8), entry.vlan & 0xff, entry.mac[0], entry.mac[1]);
	REG_WRITE(RTL837x_TBL_DATA_IN_C, 0x00, entry.igmp_asic & 1, entry.igmp_index, entry.pmask >> 2);
}
#endif


static void igmp_learn(void)
{
#ifdef DEBUG
	print_string("IGMP join "); print_byte(igmp_join); write_char('\n');
#endif
	if ((uip_buf[igmp_grp] & 0xf0) != 0xe0
	    || (uip_buf[igmp_grp] == 0xe0 && !uip_buf[igmp_grp + 1] && !uip_buf[igmp_grp + 2]))
		return;

#ifdef IPMC_USES_L3MC
	memset((__xdata uint8_t *)&entry, 0, sizeof(struct ipmc_table_entry));
	// For IPv4 MC, the Source-IP is 0.0.0.0
	// For IPv4 MC, the Destination-IP is the IPv4 MC address
	memcpy(entry.dip, &uip_buf[igmp_grp], sizeof(entry.dip));
	entry_to_l3mc();
#else
	/* The L2 Multicast MAC for IP-Multicast is 01:00:5e:xx:yy:zz, where
	 * xx = MC_IP[1] & 0x7f
	 * yy = MC_IP[2]
	 * zz = MC_IP[3]
	 */
	memset((__xdata uint8_t *)&entry, 0, sizeof(struct l2mc_table_entry));
	entry.mac[0] = 0x01; entry.mac[1] = 0x00; entry.mac[2] = 0x5e;
	entry.mac[3] = uip_buf[igmp_grp + 1] & 0x7f; entry.mac[4] = uip_buf[igmp_grp + 2]; entry.mac[5] = uip_buf[igmp_grp + 3];
	entry.vlan = 1; //TODO: Get this out of the packet and compare with VLAN table!
	entry_to_ipmc();
#endif
	// Wait for any pending Table operations to end
	do {
		reg_read_m(RTL837X_TBL_CTRL);
	} while (sfr_data[3] & 1);

	reg_read_m(RTL837x_TBL_DATA_0);
#ifdef DEBUG
	print_sfr_data();
#endif
	sfr_data[2] &= 0x3f;  // Sets the Read-method to 0 (why MAC-lookup?) and clear the CLEAR-Entry bit
	sfr_data[1] &= 0xf8;
	reg_write_m(RTL837x_TBL_DATA_0);
#ifdef DEBUG
	print_string(" l2 ctrl now: ");
	print_sfr_data();
#endif
	// First try to find entry to see whether it needs to be updated
	REG_WRITE(RTL837X_TBL_CTRL, 0x00, 0x00, TBL_L2_UNICAST, TBL_EXECUTE);
	do {
		reg_read_m(RTL837X_TBL_CTRL);
	} while (sfr_data[3] & 0x1);
#ifdef DEBUG
	print_string("\nsearch done\n");
	print_string("Table data searched:\n");
	reg_read_m(RTL837x_TBL_DATA_IN_A);
	print_sfr_data(); write_char(' ');
	reg_read_m(RTL837x_TBL_DATA_IN_B);
	print_sfr_data(); write_char(' ');
	reg_read_m(RTL837x_TBL_DATA_IN_C);
	print_sfr_data(); write_char('\n');
	print_string("Table data gotten:\n");
	reg_read_m(RTL837x_L2_DATA_OUT_A); write_char(' ');
	print_sfr_data();
	reg_read_m(RTL837x_L2_DATA_OUT_B);
	print_sfr_data(); write_char(' ');
	reg_read_m(RTL837x_L2_DATA_OUT_C);
	print_sfr_data(); write_char('\n');
	print_string("Result: ");
#endif
	reg_read_m(RTL837x_TBL_DATA_0);
#ifdef DEBUG
	print_sfr_data();
#endif
	idx = ((sfr_data[2] & 0xf) << 8) | sfr_data[3];
	if (igmp_join) {// Join group
		if (sfr_data[2] & 0x10) {
			reg_read_m(RTL837x_L2_DATA_OUT_B);
			entry.pmask = sfr_data[0] >> 6;
			reg_read_m(RTL837x_L2_DATA_OUT_C);
			entry.pmask |= ((uint16_t)sfr_data[3]) << 2;
		}
		// Update (found) entry with portmask from trapped Packet
		entry.pmask |= ((uint16_t)1) << ((IGMP_I->rtl_tag.pmask >> 8) & 0x0f);  // Swap bytes from network order, only 4 LSB count
//		print_string("\nPort-Mask: "); print_short(entry.pmask); write_char('\n');
	} else {  // Leave group
		if (sfr_data[2] & 0x10) {
			reg_read_m(RTL837x_L2_DATA_OUT_B);
			entry.pmask = sfr_data[0] >> 6;
			reg_read_m(RTL837x_L2_DATA_OUT_C);
			entry.pmask |= ((uint16_t)sfr_data[3]) << 2;
#ifdef DEBUG
			print_string("Portmask: ");
			print_short(entry.pmask);
			print_string("Index: ");
			print_short(idx);
			write_char('\n');
#endif
			// Remove portmask of IGMP packet from entry
			entry.pmask &= ~(((uint16_t)1) << ((IGMP_I->rtl_tag.pmask >> 8) & 0x0f));  // Swap bytes from network order, only 4 LSB count
//			print_string("\nPort-Mask: "); print_short(entry.pmask); write_char('\n');
		} else {
			return;
		}
		if (!entry.pmask && idx) { // No more ports in that group and an actual entry?
			// Delete Entry
			reg_read_m(RTL837x_TBL_DATA_0);
			sfr_data[1] |= 0x04;	// Clear entry
			reg_write_m(RTL837x_TBL_DATA_0);
			REG_WRITE(RTL837X_TBL_CTRL, idx >> 8, idx & 0xff, TBL_L2_UNICAST, TBL_WRITE | TBL_EXECUTE);
			do {
				reg_read_m(RTL837X_TBL_CTRL);
			} while (sfr_data[3] & 0x1);
			return;
		}
	}

	if (!entry.pmask)
		return;
	// Write the updated entry
#ifdef IPMC_USES_L3MC
	entry_to_l3mc();
#else
	entry_to_ipmc();
#endif
	reg_read_m(RTL837x_TBL_DATA_0);
#ifdef DEBUG
	print_sfr_data();
#endif
	sfr_data[2] &= 0x3f;  // Set the Read-method to 0 and clear the CLEAR-Entry bit
	sfr_data[1] &= 0xf8;
	reg_write_m(RTL837x_TBL_DATA_0);
#ifdef DEBUG
	print_string(" l2 ctrl now: ");
	print_sfr_data();
#endif
	reg_read_m(RTL837X_TBL_CTRL);
	REG_WRITE(RTL837X_TBL_CTRL, sfr_data[0], sfr_data[1], TBL_L2_UNICAST, TBL_WRITE | TBL_EXECUTE);
	do {
		reg_read_m(RTL837X_TBL_CTRL);
	} while (sfr_data[3] & 0x1);
#ifdef DEBUG
	print_string("\nupdate done\n");
	print_string("Table data written:\n");
	reg_read_m(RTL837x_TBL_DATA_IN_A);
	print_sfr_data(); write_char(' ');
	reg_read_m(RTL837x_TBL_DATA_IN_B);
	print_sfr_data(); write_char(' ');
	reg_read_m(RTL837x_TBL_DATA_IN_C);
	print_sfr_data(); write_char('\n');
	print_string("Result: ");
	reg_read_m(RTL837x_TBL_DATA_0);
	print_sfr_data();
#endif
	return;
}

static void igmp_snoop(void)
{
	__xdata uint16_t rec, n;

#ifdef DEBUG
	print_string("\nIPv4 MC packet:\n");
	for (uint8_t i = 0; i < 80; i++) {
		print_byte(uip_buf[i]);
		write_char(' ');
	}
	write_char('\n');
	print_string("Found IGMP, type: "); print_byte(uip_buf[igmp_grp]); write_char('\n');
#endif
	switch (uip_buf[igmp_grp]) {
	case IGMP_V1_REPORT:
	case IGMP_V2_REPORT:
		igmp_join = 1;
		break;
	case IGMP_V2_LEAVE:
		igmp_join = 0;
		break;
	case IGMP_V3_REPORT:
		rec = igmp_grp + IGMP_V3_RECORD_OFS;
		if (uip_len < rec)
			return;
		n = ((uint16_t)uip_buf[igmp_grp + 6] << 8) | uip_buf[igmp_grp + 7];
		for (; n-- && uip_len >= rec + IGMP_V3_RECORD_LEN && uip_buf[rec + 2] < 2;
		     rec += IGMP_V3_RECORD_LEN + ((((uint16_t)uip_buf[rec + 2] << 8) + uip_buf[rec + 3] + uip_buf[rec + 1]) << 2)) {
			switch (uip_buf[rec]) {
			case IGMP_V3_IS_INCLUDE:
			case IGMP_V3_TO_INCLUDE:
				igmp_join = uip_buf[rec + 2] || uip_buf[rec + 3];
				break;
			case IGMP_V3_IS_EXCLUDE:
			case IGMP_V3_TO_EXCLUDE:
			case IGMP_V3_ALLOW_NEW:
				igmp_join = 1;
				break;
			default:
				continue;
			}
			igmp_grp = rec + IGMP_GROUP_OFS;
			igmp_learn();
		}
		return;
	default:
		return;
	}
	igmp_grp += IGMP_GROUP_OFS;
	if (uip_len < igmp_grp + 4)
		return;
	igmp_learn();
}


static void igmp_forward(void)
{
	__xdata uint16_t i;
	__xdata uint8_t port = ((uint8_t)HTONS(IGMP_I->rtl_tag.pmask)) & 0x0f;
	__xdata uint16_t pmask = machine_detected.isRTL8373 ? PMASK_9 : PMASK_6;
	__xdata uint16_t m, link;
	__xdata uint8_t lag;

	if (uip_len < IGMP_ETH_HDR + igmp_vofs
	    || uip_len + RTL_FRAME_DESC_SIZE + VLAN_TAG_SIZE - igmp_vofs > UIP_CONF_BUFFER_ALLOC_SIZE) {
		uip_len = 0;
		return;
	}
	lag = port_lag_of(port);
	pmask &= igmp_vmask & port_isolation_get(port) & ~(lag == PORT_LAG_NONE ? PORT_BIT(port) : port_lag_members_get(lag));
	link = port_link_mask();
	for (i = 0; stp_enabled && i <= MACHINE_MAX_PORT; i++)
		if (stp_port_state(i) != IGMP_STP_FORWARDING)
			link &= ~PORT_BIT(i);
	pmask &= link;
	if (igmp_rports & ~link) {
		igmp_rports &= link;
		igmp_filter();
	}
	if (igmp_report && (igmp_rports & igmp_vmask))
		pmask &= igmp_rports;
	for (lag = 0; lag < 4; lag++) {
		m = port_lag_members_get(lag);
		i = m & pmask;
		pmask &= ~m | (i & -i);
	}
	if (!pmask) {
		uip_len = 0;
		return;
	}
	if (igmp_vofs) {
		for (i = uip_len; i--;)
			uip_buf[i + RTL_FRAME_DESC_SIZE] = uip_buf[i];
	} else {
		for (i = uip_len; i-- > IGMP_ETH_HDR;)
			uip_buf[i + RTL_FRAME_DESC_SIZE + VLAN_TAG_SIZE] = uip_buf[i];
		for (i = IGMP_ETH_HDR; i--;)
			uip_buf[i + RTL_FRAME_DESC_SIZE] = uip_buf[i];
	}

	IGMP_O->rtl_tag.tag = HTONS(RTL_FRAME_TAG_ID);
	IGMP_O->rtl_tag.version = RTL_FRAME_TAG_VERSION;
	IGMP_O->rtl_tag.reason = 0x00;
	IGMP_O->rtl_tag.flags = HTONS(RTL_TAG_LEARN_DIS);
	IGMP_O->rtl_tag.pmask = HTONS(pmask);
	if (!igmp_vofs) {
		IGMP_O->vlan_tag.svlan = HTONS(0x8100);
		IGMP_O->vlan_tag.vlan = HTONS(igmp_vid);
		uip_len += VLAN_TAG_SIZE;
	}
}


uint8_t igmp_packet_handler(void) __banked
{
	__xdata uint8_t port = ((uint8_t)HTONS(IGMP_I->rtl_tag.pmask)) & 0x0f;
	__xdata uint16_t vid, m;
	__xdata uint8_t lag;

	igmp_vofs = TRAP_RX_BODY() - TRAP_RX_HDR_SIZE;
	if (IGMP_V->ipv4_tag != HTONS(UIP_ETHTYPE_IP) || IGMP_V->protocol != IGMP_PROTO)
		return 0;
	if (!MACHINE_PORT_VALID(port) || (stp_enabled && stp_port_state(port) != IGMP_STP_FORWARDING)) {
		uip_len = 0;
		return 1;
	}
	igmp_grp = IGMP_IP_HDR + igmp_vofs + ((IGMP_V->hlen & IGMP_IP_IHL) << 2);
	igmp_query = igmp_report = 0;
	if (uip_len > igmp_grp) {
		igmp_query = uip_buf[igmp_grp] == IGMP_QUERY;
		igmp_report = uip_buf[igmp_grp] == IGMP_V1_REPORT || uip_buf[igmp_grp] == IGMP_V2_REPORT
			      || uip_buf[igmp_grp] == IGMP_V2_LEAVE || uip_buf[igmp_grp] == IGMP_V3_REPORT;
	}
	igmp_vid = port_pvid_get(port);
	if (igmp_vofs) {
		vid = ((uint16_t)(uip_buf[TRAP_RX_HDR_SIZE + 2] & 0x0f) << 8) | uip_buf[TRAP_RX_HDR_SIZE + 3];
		if (vid)
			igmp_vid = vid;
		uip_buf[TRAP_RX_HDR_SIZE + 2] = (uip_buf[TRAP_RX_HDR_SIZE + 2] & 0xf0) | (igmp_vid >> 8);
		uip_buf[TRAP_RX_HDR_SIZE + 3] = igmp_vid;
	}
	if (vlan_get(igmp_vid) || !(sfr_data[0] & 0x02)
	    || !((igmp_vmask = ((uint16_t)(sfr_data[2] & 0x03) << 8) | sfr_data[3]) & PORT_BIT(port))) {
		uip_len = 0;
		return 1;
	}
	if (igmp_query) {
		lag = port_lag_of(port);
		m = (lag == PORT_LAG_NONE ? PORT_BIT(port) : port_lag_members_get(lag)) & port_link_mask();
		if (m & ~igmp_rports) {
			igmp_rports |= m;
			igmp_filter();
		}
	}
	igmp_snoop();
	igmp_forward();
	return 1;
}
