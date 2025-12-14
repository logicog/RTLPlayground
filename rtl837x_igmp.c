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

void igmp_setup(void) __banked
{
	uint8_t i;

	// For now, forward all unkown MC pkts (2 bits per port. 00: flood via floodmask, 01: drop, 10: trap, 11: to rport)
	REG_SET(RTL837X_MC_LOOKUPMISS_ACTIONS, 0x00000000); //0x4f78

	// Define ports where unknown MC addresses are flooded to:
	REG_SET(RTL837X_MC_FLOODMASK, machine.isRTL8373 ? PMASK_9 : PMASK_6);

	// Enable lookup of IPv4 MC addresses in table
	reg_bit_set(RTL837X_L2_CTRL, 3); // 0x5350

	// Configure per-port IGMP configuration, bits 0-10 enable MC protocol snooping,
	// bits 16-24 configure max MC group used by that port. For now all protocols are flooded (01)
	for (i = machine.min_port; i <= machine.max_port; i++)
		REG_SET(RTL837X_IGMP_PORT_CFG + (i << 2), 0x00ff7c15); 
}


void igmp_enable(void) __banked
{
	uint8_t i;

	REG_SET(0x50bc, 00010007); // Trap control?

	// Drop unknown MC messages
	REG_SET(RTL837X_MC_LOOKUPMISS_ACTIONS, 0x00015540); //0x4f78

	// Configure per-port IGMP configuration, bits 0-10 enable MC protocol snooping,
	// bits 16-24 configure max MC group used by that port. Trap to CPU (10)
	for (i = machine.min_port; i <= machine.max_port; i++)
		REG_SET(RTL837X_IGMP_PORT_CFG + (i << 2), 0x00ff7c2a); // 0x00ff7000: Handling by ASIC (00)
}
