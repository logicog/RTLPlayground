/*
 * This is a driver implementation for the IGMP features for the RTL827x platform
 * This code is in the Public Domain
 */

// #define REGDBG
// #define DEBUG

#define IPMC_USES_L3MC

#include <stdint.h>
#include "rtl837x_common.h"
#include "rtl837x_sfr.h"
#include "rtl837x_regs.h"
#include "rtl837x_igmp.h"
#include "machine.h"

extern __code struct machine machine;

#include "uip.h"

#pragma codeseg BANK1
#pragma constseg BANK1

extern __xdata uint8_t cpuPort;
extern __data uint8_t sfr_data[4];

extern __xdata uint8_t uip_buf[UIP_CONF_BUFFER_SIZE + 2];

__xdata uint16_t idx;

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


void igmp_setup(void) __banked
{
	uint8_t i;
	print_string("igmp_setup called\n");
	// For now, forward all unkown IP-MC pkts (2 bits per port. 00: flood via floodmask, 01: drop, 10: trap, 11: to rport)
	REG_SET(RTL837X_IPV4_PORT_MC_LM_ACT, LOOKUP_MISS_FLOOD);
	REG_SET(RTL837X_IPV6_PORT_MC_LM_ACT, LOOKUP_MISS_FLOOD);

	// Define ports where unknown MC addresses are flooded to:
	REG_SET(RTL837X_IPV4_UNKN_MC_FLD_PMSK, machine.isRTL8373? PMASK_9: PMASK_6);
	REG_SET(RTL837X_IPV6_UNKN_MC_FLD_PMSK, machine.isRTL8373? PMASK_9: PMASK_6);

	// Enable lookup of IPv4 MC addresses in table
	reg_bit_set(RTL837X_L2_CTRL, L2_CTRL_LUT_IPMC_HASH);

	// Configure per-port IGMP configuration, bits 0-10 enable MC protocol snooping,
	// bits 16-24 configure max MC group used by that port. For now all protocols are flooded (01)
	for (i = machine.min_port; i <= machine.max_port; i++)
		REG_SET(RTL837X_IGMP_PORT_CFG + (i << 2), 0x00ff7c15); 

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
	for (i = machine.min_port; i <= machine.max_port; i++) {
		print_byte(i); write_char(':');
		REG_SET(RTL837X_IGMP_PORT_CFG + (i << 2), IGMP_MAX_GROUP | IGMP_PROTOCOL_ENABLE | IGMP_FLOOD);
		write_char('\n');
	}

/*	// Allow all physical ports to be dynamic router ports
	reg_read_m(RTL837X_IGMP_ROUTER_PORT);
	if (isRTL8373) {
		REG_WRITE(RTL837X_IGMP_ROUTER_PORT, PMASK_9 >> 8, PMASK_9 & 0xff, sfr_data[1], sfr_data[0]);
	} else {
		REG_WRITE(RTL837X_IGMP_ROUTER_PORT, PMASK_6 >> 8, PMASK_6 & 0xff, sfr_data[1], sfr_data[0]);
	}
*/
}


void igmp_enable(void) __banked
{
	print_string("igmp_enable called\n");
	// Configure trapping of unhandled IGMP protocol packets to CPU
	REG_SET(RTL837X_IGMP_TRAP_CFG, IGMP_CPU_PORT | IGMP_TRAP_PRIORITY);

	// Drop unknown IP-MC packets
	REG_SET(RTL837X_IPV4_PORT_MC_LM_ACT, machine.isRTL8373? LOOKUP_MISS_DROP_9: LOOKUP_MISS_DROP_6);
//	REG_SET(RTL837X_IPV6_PORT_MC_LM_ACT, machine.isRTL8373? LOOKUP_MISS_DROP_9: LOOKUP_MISS_DROP_6);

	// Configure per-port IGMP configuration, bits 0-10 enable MC protocol snooping,
	// bits 16-24 configure max MC group used by that port. Trap to CPU (10)
	for (uint8_t i = machine.min_port; i <= machine.max_port; i++) {
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
	for (uint8_t i = machine.min_port; i <= machine.max_port; i++) {
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


void igmp_packet_handler(void) __banked
{
	// By default we do not send anything out
	uip_len = 0;

#ifdef DEBUG
	print_string("\nIPv4 MC packet:\n");
	for (uint8_t i = 0; i < 80; i++) {
		print_byte(uip_buf[i]);
		write_char(' ');
	}
	write_char('\n');
#endif
	if (IGMP_I->protocol != 2)
		return;
#ifdef DEBUG
	print_string("Found IGMP, type: "); print_byte(IGMP_I->igmp_type); write_char('\n');
#endif
	// We react to IGMPv1/v2 and v3 membership reports
	if (!(IGMP_I->igmp_type == 0x12 || IGMP_I->igmp_type == 0x16 || IGMP_I->igmp_type == 0x22))
		return;
#ifdef DEBUG
	print_string("IGMP membership report, type "); print_byte(IGMP_I->igmp_rtype); write_char('\n');
#endif

#ifdef IPMC_USES_L3MC
	memset(&entry, 0, sizeof(struct ipmc_table_entry));
	// For IPv4 MC, the Source-IP is 0.0.0.0
	entry.sip[0] = 0x00; entry.sip[1] = 0x00; entry.sip[2] = 0x00; entry.sip[3] = 0x00;
	// For IPv4 MC, the Destination-IP is the IPv4 MC address
	entry.dip[0] = IGMP_I->mc_ip[0]; entry.dip[1] = IGMP_I->mc_ip[1]; entry.dip[2] = IGMP_I->mc_ip[2]; entry.dip[3] = IGMP_I->mc_ip[3];
	entry_to_l3mc();
#else
	/* The L2 Multicast MAC for IP-Multicast is 01:00:5e:xx:yy:zz, where
	 * xx = MC_IP[1] & 0x7f
	 * yy = MC_IP[2]
	 * zz = MC_IP[3]
	 */
	memset(&entry, 0, sizeof(struct l2mc_table_entry));
	entry.mac[0] = 0x01; entry.mac[1] = 0x00; entry.mac[2] = 0x5e;
	entry.mac[3] = IGMP_I->mc_ip[1] & 0x7f; entry.mac[4] = IGMP_I->mc_ip[2]; entry.mac[5] = IGMP_I->mc_ip[3];
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
	if (IGMP_I->igmp_rtype == 0x4) {// Join group
		if (sfr_data[2] & 0x10) {
			print_string("\nIGMP-Entry FOUND\n");
			reg_read_m(RTL837x_L2_DATA_OUT_B);
			entry.pmask = sfr_data[0] >> 6;
			reg_read_m(RTL837x_L2_DATA_OUT_C);
			entry.pmask |= ((uint16_t)sfr_data[3]) << 2;
		}
		// Update (found) entry with portmask from trapped Packet
		entry.pmask |= (1L << (IGMP_I->rtl_tag.pmask >> 8));  // Swap bytes from network order, only 4 LSB count
//		print_string("\nPort-Mask: "); print_short(entry.pmask); write_char('\n');
	} else if (IGMP_I->igmp_rtype == 0x3){  // Leave group
		if (sfr_data[2] & 0x10) {
			print_string("\nIGMP_Entry FOUND\n");
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
			entry.pmask &= ~(1L << (IGMP_I->rtl_tag.pmask >> 8));  // Swap bytes from network order, only 4 LSB count
//			print_string("\nPort-Mask: "); print_short(entry.pmask); write_char('\n');
		} else {
			print_string("IGMP Entry already deleted\n");
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
			print_string("IGMP Entry deleted\n");
			return;
		}
	} else {  // Unknown message: ignore.
		return;
	}

	if (!entry.pmask)
		return;
	print_string("Updating IGMP entry\n");
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
}
