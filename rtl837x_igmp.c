/*
 * This is a driver implementation for the IGMP features for the RTL827x platform
 * This code is in the Public Domain
 */

// #define REGDBG
// #define DEBUG

#include <stdint.h>
#include "rtl837x_common.h"
#include "rtl837x_sfr.h"
#include "rtl837x_regs.h"
#include "rtl837x_igmp.h"
#include "machine.h"

extern __code struct machine machine;
extern __xdata uint8_t cpuPort;
extern __xdata uint8_t sfr_data[4];

void igmp_setup(void) __banked
{
	uint8_t i;
	print_string("igmp_setup called\n");
	// For now, forward all unkown IP-MC pkts (2 bits per port. 00: flood via floodmask, 01: drop, 10: trap, 11: to rport)
	REG_SET(RTL837X_IPV4_PORT_MC_LM_ACT, LOOKUP_MISS_FLOOD);
	REG_SET(RTL837X_IPV6_PORT_MC_LM_ACT, LOOKUP_MISS_FLOOD);

	// For now, forward all unkown MC pkts (2 bits per port. 00: flood via floodmask, 01: drop, 10: trap, 11: to rport)
	REG_SET(RTL837X_MC_LOOKUPMISS_ACTIONS, 0x00000000); //0x4f78

	// Define ports where unknown MC addresses are flooded to:
	REG_SET(RTL837X_MC_FLOODMASK, machine.isRTL8373 ? PMASK_9 : PMASK_6);

	// Define ports where unknown MC addresses are flooded to:
	if (isRTL8373) {
		REG_SET(RTL837X_IPV4_UNKN_MC_FLD_PMSK, PMASK_9);
		REG_SET(RTL837X_IPV6_UNKN_MC_FLD_PMSK, PMASK_9);
	} else {
		REG_SET(RTL837X_IPV4_UNKN_MC_FLD_PMSK, PMASK_6);
		REG_SET(RTL837X_IPV6_UNKN_MC_FLD_PMSK, PMASK_6);
	}

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
	 * 10: drop
	 * Bits 0-1: IGMPv1, 2-3: IGMPv2, 4-5: IGMPv3, 6-7: MLDv1 (for IPv6), 8-9: MLDv2
	 * For now the IGMP protocols are flooded (01), MLD which MAC-based is handled by ASIC
	 * All messages are allowed and maximum MC group is 0xff
	 */
	for (i = minPort; i <= maxPort; i++)
		REG_SET(RTL837X_IGMP_PORT_CFG + (i << 2), IGMP_MAX_GROUP | IGMP_PROTOCOL_ENABLE | IGMP_FLOOD);

	// Allow all physical ports to be dynamic router ports
	reg_read_m(RTL837X_IGMP_ROUTER_PORT);
	if (isRTL8373) {
		REG_WRITE(RTL837X_IGMP_ROUTER_PORT, PMASK_9 >> 8, PMASK_9 & 0xff, sfr_data[1], sfr_data[0]);
	} else {
		REG_WRITE(RTL837X_IGMP_ROUTER_PORT, PMASK_6 >> 8, PMASK_6 & 0xff, sfr_data[1], sfr_data[0]);
	}

}


void igmp_enable(void) __banked
{
	uint8_t i;

	print_string("igmp_enable called\n");
	// Configure trapping of unhandled IGMP protocol packets to CPU
	REG_SET(RTL837X_IGMP_TRAP_CFG, IGMP_CPU_PORT | IGMP_TRAP_PRIORITY);

	// Drop unknown IP-MC packets
	if (isRTL8373) {
		REG_SET(RTL837X_IPV4_PORT_MC_LM_ACT, LOOKUP_MISS_DROP_9);
		REG_SET(RTL837X_IPV6_PORT_MC_LM_ACT, LOOKUP_MISS_DROP_9);
	} else {
		REG_SET(RTL837X_IPV4_PORT_MC_LM_ACT, LOOKUP_MISS_DROP_6);
		REG_SET(RTL837X_IPV6_PORT_MC_LM_ACT, LOOKUP_MISS_DROP_6);
	}

	// Configure per-port IGMP configuration, bits 0-10 enable MC protocol snooping,
	// bits 16-24 configure max MC group used by that port. Trap to CPU (10)
	for (i = minPort; i <= maxPort; i++)
		REG_SET(RTL837X_IGMP_PORT_CFG + (i << 2), IGMP_MAX_GROUP | IGMP_PROTOCOL_ENABLE | IGMP_ASIC);
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
