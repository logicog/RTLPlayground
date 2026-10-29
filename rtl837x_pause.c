/*
 * 802.3x flow control of the copper ports: which pause the PHY advertises,
 * what the link partner offers and what the MAC negotiated.
 * This code is in the Public Domain
 */

#include <stdint.h>
#include "rtl837x_common.h"
#include "rtl837x_sfr.h"
#include "rtl837x_regs.h"
#include "rtl837x_phy.h"
#include "rtl837x_pause.h"
#include "phy.h"
#include "machine.h"

#pragma codeseg BANK3
#pragma constseg BANK3

extern __code const struct machine machine;
extern __xdata uint8_t sfr_data[4];

/*
 * Advertise symmetric pause on a copper port, or not. phy_pause_off keeps
 * the choice so that phy_set_speed() writes it again.
 */
void port_pause_set(__xdata uint8_t port, __xdata bool on) __banked
{
	if (machine.is_sfp[port]) {
		print_string("Flow control can't be set for SFP port "); print_phys_port(port); write_char('\n');
		return;
	}
	print_string("Flow control "); print_string(on ? "on" : "off");
	print_string(" for "); print_phys_port(port); write_char('\n');
	if (on)
		phy_pause_off &= ~(1 << port);
	else
		phy_pause_off |= 1 << port;
	// Changing the advertisement restarts the link, so leave a matching one alone
	phy_read(port, PHY_MMD_AN, PHY_ANEG_ADV);
	if ((SFR_DATA_U16 & (PHY_ANEG_PAUSE | PHY_ANEG_ASYM_PAUSE)) == (on ? PHY_ANEG_PAUSE : 0))
		return;
	phy_modify(port, PHY_MMD_AN, PHY_ANEG_ADV, PHY_ANEG_PAUSE | PHY_ANEG_ASYM_PAUSE, on ? PHY_ANEG_PAUSE : 0);
	phy_reset(port);
}



// PAUSE_* bits of a port. SFP ports have no autonegotiated pause flags.
uint8_t port_pause_state(uint8_t port) __banked
{
	uint8_t s = 0;

	if (!machine.is_sfp[port]) {
		phy_read(port, PHY_MMD_AN, PHY_ANEG_ADV);
		s = (SFR_DATA_U16 >> 10) & 0x03;
		phy_read(port, PHY_MMD_AN, PHY_ANEG_LP_ABILITY);
		s |= ((SFR_DATA_U16 >> 10) & 0x03) << 2;
	}
	// Bit N of both registers is chip port N
	reg_read_m(RTL837X_MAC_TX_PAUSE_STS);
	if (sfr_data[3 - (port >> 3)] & (1 << (port & 7)))
		s |= PAUSE_NEG_TX;
	reg_read_m(RTL837X_MAC_RX_PAUSE_STS);
	if (sfr_data[3 - (port >> 3)] & (1 << (port & 7)))
		s |= PAUSE_NEG_RX;
	return s;
}
