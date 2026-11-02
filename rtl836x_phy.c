/*
 * RTL836x PHY driver: what the fabric reports about the internal port PHYs.
 *
 * This die's port PHYs do not answer the clause-45 (MMD) reads the RTL8372/3
 * driver is built on: PHYSR reads 0 and the decoder turns that into a bogus
 * "10M half duplex" whatever the port is actually doing. PORTn_STATUS in the
 * fabric is the authoritative view here, so phy_show() decodes that instead.
 *
 * The RTL8224/RTL8261 SerDes PHYs, and the SDS/clause-45 helpers that exist to
 * configure them, are not present on this die and are deliberately not
 * implemented here: rtl837x_phy.c owns those. rtlos_phy.c compiles exactly
 * one of the two files, selected by SOC_FAMILY.
 *
 * Only what this die's code paths actually call is defined here. phy_config()
 * and phy_reset() exist but are stubs: the clause-22 port that will make them
 * work on this die is not written yet.
 *
 * This code is in the Public Domain
 */

#include <stdint.h>
#include "rtl837x_common.h"
#include "rtl837x_sfr.h"
#include "rtlos_regs.h"
#include "rtlos_phy.h"
#include "phy.h"
#include "machine.h"

#pragma codeseg BANK2
#pragma constseg BANK2

extern __code const struct machine machine;
extern __xdata uint8_t sfr_data[4];

__xdata struct phy_settings phy_settings;


/*
 * Per-PHY start-up tweaks. On the RTL8372/3 this is a long clause-45 sequence
 * (SerDes option, EEE advertisement, PHYCR2, LED and duplex bits); what this
 * die needs in its place has not been worked out yet, so the entry point
 * exists and does nothing but say so.
 *
 * Nothing calls it on an RTL8370 today: the RTL8372/3 init that does is not
 * compiled for this die.
 */
void phy_config(uint8_t phy) __banked
{
	print_string("phy_config: not ported to the RTL836x, phy ");
	print_byte(phy);
	write_char('\n');
}


/*
 * Set Speed of a PHY
 * See e.g. RTL8221B datasheet
 * duplex: 0: half, 1: full, 2: both
 */
void phy_set_speed(void) __banked
{
	uint16_t v;

	print_string("Setting port "); print_phys_port(phy_settings.port);
	if (machine.n_10g && phy_settings.port == 3)
		phy_settings.is10g_port = 1;
	if (machine.n_10g == 2 && phy_settings.port == 8)
		phy_settings.is10g_port = 1;
	if (phy_settings.speed == PHY_OFF) {
		print_string(" to disabled");
	} else {
		print_string(" to speed ");
		switch(phy_settings.speed) {
		case PHY_SPEED_AUTO:
			print_string("auto");
			if (phy_settings.is10g_port)
				print_string (" (10g)");
			break;
		case PHY_SPEED_10M:
			print_string("10M");
			if (phy_settings.duplex)
				print_string(" full duplex");
			else
				print_string(" half duplex");
			break;
		case PHY_SPEED_100M:
			print_string("100M");
			if (phy_settings.duplex)
				print_string(" full duplex");
			else
				print_string(" half duplex");
			break;
		case PHY_SPEED_1G:
			print_string("1G");
			break;
		case PHY_SPEED_2G5:
			print_string("2G5");
			break;
		default:
			print_string("UNKNOWN");
			break;
		}
	}
	write_char('\n');

	phy_read(phy_settings.port, PHY_MMD31, 0xa610);
	v = SFR_DATA_U16;
	if (phy_settings.speed == PHY_OFF) {
		phy_write(phy_settings.port, PHY_MMD31, 0xa610, v | 0x0800);
		return;
	}
	// Port is on, make sure of it:
	if (v & 0x0800)
		phy_write(phy_settings.port, PHY_MMD31, 0xa610, v & 0xf7ff);

	if (phy_settings.speed == PHY_SPEED_AUTO) {
			// AN Advertisement Register (MMD 7.0x0010)
			// bits 0-4: 0x1 (802.3 supported), Extended Next Page format used
			phy_write(phy_settings.port, PHY_MMD_AN, PHY_ANEG_ADV, 0x15e1);
			// Multi-GBASE-TBASE-T AN Control 1 Register (MMD 7.0x0020)
			// bit 14: SLAVE, bit 13: Multi-Port device, bit 8: 2.5GBit available, 1: LD
			phy_write(phy_settings.port, PHY_MMD_AN, PHY_ANEG_MGBASE_CTRL, 0x6081);
			// GBCR (1000Base-T Control Register, MMD 31.0xA412)
			if (phy_settings.is10g_port)
				phy_modify(phy_settings.port, PHY_MMD31, PHY_MMD31_GBCR, 0x0000, 0x0e00);
			else
				phy_modify(phy_settings.port, PHY_MMD31, PHY_MMD31_GBCR, 0x0000, 0x0200);
			phy_write(phy_settings.port, PHY_MMD_AN, PHY_ANEG_CTRL, 0x3200);	// Restart AN
	} else {
		// AN Control Register (MMD 7.0x0000)
		phy_write(phy_settings.port, PHY_MMD_AN, PHY_ANEG_CTRL, 0x2000);	// Clear bit 12: No Autoneg, Set Extended Pages (bit 13)
		if (phy_settings.speed == PHY_SPEED_10M) {
			phy_write(phy_settings.port, PHY_MMD_AN, PHY_ANEG_MGBASE_CTRL, 0x6001);
			uint16_t adv10;
			if (phy_settings.duplex == PHY_DUPLEX_HALF)
				adv10 = 0x1421;
			else if (phy_settings.duplex == PHY_DUPLEX_FULL)
				adv10 = 0x1441;
			else
				adv10 = 0x1461;
			phy_write(phy_settings.port, PHY_MMD_AN, PHY_ANEG_ADV, adv10);
			phy_modify(phy_settings.port, PHY_MMD31, PHY_MMD31_GBCR, 0x0200, 0x0000);
		} else if (phy_settings.speed == PHY_SPEED_100M) {
			phy_write(phy_settings.port, PHY_MMD_AN, PHY_ANEG_MGBASE_CTRL, 0x6001);
			uint16_t adv100;
			if (phy_settings.duplex == PHY_DUPLEX_HALF)
				adv100 = 0x1481;
			else if (phy_settings.duplex == PHY_DUPLEX_FULL)
				adv100 = 0x1501;
			else
				adv100 = 0x1581;
			phy_write(phy_settings.port, PHY_MMD_AN, PHY_ANEG_ADV, adv100);
			phy_modify(phy_settings.port, PHY_MMD31, PHY_MMD31_GBCR, 0x0200, 0x0000);
		} else {
			// AN Advertisement Register (MMD 7.0x0010)
			// bits 0-4: 0x1 (802.3 supported), bit 10: PAUSE,
			// Extended Next Page format used
			phy_write(phy_settings.port, PHY_MMD_AN, PHY_ANEG_ADV, 0x1401);
			if (phy_settings.speed == PHY_SPEED_1G) {
				// Multi-GBASE-TBASE-T AN Control 1 Register (MMD 7.0x0020)
				// bit 14: SLAVE, bit 13: Multi-Port device, 1: LD Loop timin enableed
				phy_write(phy_settings.port, PHY_MMD_AN, PHY_ANEG_MGBASE_CTRL, 0x6001);
				// GBCR (1000Base-T Control Register, MMD 31.0xA412)
				phy_modify(phy_settings.port, PHY_MMD31, PHY_MMD31_GBCR, 0x0000, 0x0200);
			} else if (phy_settings.speed == PHY_SPEED_2G5) {
				// Multi-GBASE-TBASE-T AN Control 1 Register (MMD 7.0x0020)
				// bit 14: SLAVE, bit 13: Multi-Port device, bit 8: 2.5GBit available, 1: LD Loop timin enableed
				phy_write(phy_settings.port, PHY_MMD_AN, PHY_ANEG_MGBASE_CTRL, 0x6081);
				// GBCR (1000Base-T Control Register, MMD 31.0xA412)
				phy_modify(phy_settings.port, PHY_MMD31, PHY_MMD31_GBCR, 0x0200, 0x0000);
			} else if (phy_settings.speed == PHY_SPEED_5G) {
				phy_write(phy_settings.port, PHY_MMD_AN, PHY_ANEG_MGBASE_CTRL, 0x6081);
				phy_modify(phy_settings.port, PHY_MMD31, PHY_MMD31_GBCR, 0x0400, 0x0000);
			} else if (phy_settings.speed == PHY_SPEED_10G) {
				phy_write(phy_settings.port, PHY_MMD_AN, PHY_ANEG_MGBASE_CTRL, 0x6081);
				phy_modify(phy_settings.port, PHY_MMD31, PHY_MMD31_GBCR, 0x0800, 0x0000);
			}
		}
		phy_write(phy_settings.port, PHY_MMD_AN, PHY_ANEG_CTRL, 0x3000);	// Enable AN
	}
}


void phy_set_duplex(void) __banked
{
	uint16_t v;

	print_string("Setting port "); print_phys_port(phy_settings.port);
	if (phy_settings.duplex)
		print_string(" to full duplex");
	else
		print_string(" to half duplex");
	write_char('\n');

	phy_read(phy_settings.port, PHY_MMD_AN, PHY_ANEG_CTRL);
	v = SFR_DATA_U16;	
	if (!(v & 0x1000)) { // AN disabled, we are in forced mode
		phy_read(phy_settings.port, PHY_MMD31, PHY_MMD31_FEDCR);
		v = SFR_DATA_U16;
		if (phy_settings.duplex)
			v |= 0x0100;
		else
			v &= 0xfeff;
		phy_write(phy_settings.port, PHY_MMD31, PHY_MMD31_FEDCR, v);
		return;
	}
	// Disable AN
	phy_write(phy_settings.port, PHY_MMD_AN, PHY_ANEG_CTRL, 0x2000);
	phy_read(phy_settings.port, PHY_MMD_AN, PHY_ANEG_ADV);
	v = SFR_DATA_U16;
	if (v & 0x0060) {
		if (phy_settings.duplex)
			phy_modify(phy_settings.port, PHY_MMD_AN, PHY_ANEG_ADV, 0xffbf, 0x0040);
		else
			phy_modify(phy_settings.port, PHY_MMD_AN, PHY_ANEG_ADV, 0xffdf, 0x0020);
	}
	if (v & 0x0180) {
		if (phy_settings.duplex)
			phy_modify(phy_settings.port, PHY_MMD_AN, PHY_ANEG_ADV, 0xfeff, 0x0100);
		else
			phy_modify(phy_settings.port, PHY_MMD_AN, PHY_ANEG_ADV, 0xff7f, 0x0080);
	}
	// Restart AN
	phy_write(phy_settings.port, PHY_MMD_AN, PHY_ANEG_CTRL, 0x3000);
}


void phy_show(uint8_t port) __banked
{
	uint16_t st;

	reg_read_m(RTL836x_PORT_STATUS_REG(port));
	st = ((uint16_t)sfr_data[2] << 8) | sfr_data[3];

	print_string("Link: ");
	if (!(st & RTL836x_PORT_STATUS_LINK)) {
		print_string("down (PORT");
		print_phys_port(port);
		print_string("_STATUS = ");
		print_short(st);
		print_string(")\n");
		return;
	}

	switch (st & RTL836x_PORT_STATUS_SPEED_MASK) {
	case RTL836x_PORT_STATUS_SPEED_10:
		print_string("up 10M");
		break;
	case RTL836x_PORT_STATUS_SPEED_100:
		print_string("up 100M");
		break;
	default:
		print_string("up 1000M");
		break;
	}
	if (st & RTL836x_PORT_STATUS_DUPLEX)
		print_string(" full duplex");
	else
		print_string(" half duplex");
	if (st & RTL836x_PORT_STATUS_TXPAUSE)
		print_string(" tx-pause");
	if (st & RTL836x_PORT_STATUS_RXPAUSE)
		print_string(" rx-pause");
	if (!(st & RTL836x_PORT_STATUS_NWAY))
		print_string(" (forced)");
	print_string("\nPORT");
	print_phys_port(port);
	print_string("_STATUS = ");
	print_short(st);
	write_char('\n');
}


/*
 * Reset one port PHY. The RTL8372/3 implementation toggles the PHY's power-down
 * bit over clause-45; that access does not work on this die, so this is a stub
 * until the clause-22 helper lands.
 */
void phy_reset(uint8_t port) __banked
{
	print_string("phy_reset: not ported to the RTL836x, port ");
	print_phys_port(port);
	write_char('\n');
}
