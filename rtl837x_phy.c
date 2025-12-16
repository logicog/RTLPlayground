/*
 * This is a driver implementation for the Internal PHYs and RTL8221/RTL8224 PHYs
 * for the RTL827x platform
 * This code is in the Public Domain
 */

#define REGDBG

// Phy ID of the external RTL8224 PHY.
#define RTL8224_PHY_ID 0x00

#include <stdint.h>
#include "rtl837x_common.h"
#include "rtl837x_sfr.h"
#include "rtl837x_regs.h"
#include "rtl837x_phy.h"
#include "phy.h"

#pragma codeseg BANK1
#pragma constseg BANK1

extern __code uint16_t bit_mask[16];


__code uint16_t rtl8224_ca[42] = {
	0x4480, 0xc842,
	0x0400, 0xc9c2,
	0x6d02, 0xcc42,
	0x424e, 0xcdc2,
	0x0002, 0xcec2,
	0x1390, 0xce6c,
	0x003f, 0xca6c,
	0x0200, 0xc86c,
	0x0080, 0xc25c,
	0x0408, 0xc35c,
	0x020d, 0xc3dc,
	0x0601, 0xc4dc,
	0x222c, 0xc5dc,
	0xa217, 0xc65c,
	0xfe40, 0xc6dc,
	0xf5c1, 0xcadc,
	0x0443, 0xcb5c,
	0xabb0, 0xcedc,
	0x5078, 0xc90c,
	0xc45c, 0xc18c,
	0, 0
};

__code uint16_t rtl8224_cb[60] = {
	0xc45c, 0xc18c, 0x8040,
	0x0030, 0xc040, 0x8040,
	0x0010, 0xc040, 0x8040,
	0x0050, 0xc040, 0x8040,
	0x00d0, 0xc040, 0x8040,
	0x0cd0, 0xc040, 0x8040,
	0x04d0, 0xc040, 0x8040,
	0x04d0, 0xc040, 0x8040,
	0x0cd0, 0xc040, 0x8040,
	0x00d0, 0xc040, 0x8040,
	0x00d0, 0xc040, 0x8040,
	0x0050, 0xc040, 0x8040,
	0x0010, 0xc040, 0x8040,
	0x0010, 0xc040, 0x8040,
	0x0030, 0xc040, 0x8040,
	0x0000, 0xc040, 0x803e,
	0x000b, 0xc03e, 0x803e,
	0x0000, 0xc03e, 0x8042,
	0x4906, 0xc042, 0x82ec,
	0xffff,0,0
};

void rtl8224_phy_enable(void) __banked
{
	uint16_t pval;

	// p001e.0a90:00f3 R02f8-000000f3 R02f4-000000fc P000001.1e000a90:00fc
	print_string("\r\nrtl8224_phy_enable called\r\n");
	phy_read(RTL8224_PHY_ID, 0x1e, 0xa90);
	pval = SFR_DATA_U16;

	// PHY Initialization:
	REG_WRITE(0x2f8, 0, 0, pval >> 8, pval);

	pval &= 0xfff0;
	pval |= 0x0c;
	REG_WRITE(0x2f4, 0, 0, pval >> 8, pval);

	phy_write(RTL8224_PHY_ID, 0x1e, 0xa90, pval);
	delay(50);

	print_string("\r\nrtl8224_phy_enable done\r\n");
}


void phy_config(uint8_t phy) __banked
{
	print_string("\r\nphy_config: ");
	write_char('0' + phy);

	delay(20);
	// PHY configuration: External 8221B?
	//	p081e.75f3:ffff P000100.1e0075f3:fffe
	phy_modify(phy, 0x1e, 0x75f3, 0x0001, 0x0000);
	delay(20);

	//	p081e.697a:ffff P000100.1e00697a:ffc1 / p031e.697a:0003 P000008.1e00697a:0001
	// SERDES OPTION 1 Register (MMD 30.0x6) bits 0-5: 0x01: Set HiSGMII+SGMII
	phy_modify(phy, 0x1e, 0x697a, 0x003f, 0x0001);
	delay(20);

	//	p031f.a432:0811 P000008.1f00a432:0831
	// PHYCR2 PHY Specific Control Register 2, MMD 31. 0xA432), set bit 5: enable EEE
	phy_modify(phy, 0x1f, 0xa432, 0x0000, 0x0020);

	//	p0307.003e:0000 P000008.0700003e:0001
	// EEE avertisment 2 register MMMD 7.0x003e, set bit 0: 2.5G has EEE capability
	phy_modify(phy, 0x7, 0x3e, 0x0000, 0x0001);
	delay(20);

	//	p031f.a442:043c P000008.1f00a442:0430
	// Unknown, but clear bits 2/3
	phy_modify(phy, 0x1f, 0xa442, 0x000c, 0x0000);
	delay(20);

	// P000100.1e0075b5:e084
	phy_write(phy, 0x1e, 0x75b5, 0xe084);
	delay(20);

	//	p031e.75b2:0000 P000008.1e0075b2:0060
	// set bits 5/6
	phy_modify(phy, 0x1e, 0x75b2, 0x0000, 0x0060);
	delay(20);

	//	p081f.d040:ffff P000100.1f00d040:feff
	// LCR6 (LED Control Register 6, MMD 31.D040), set bits 8/9 to 0b10
	phy_modify(phy, 0x1e, 0xd040, 0x0300, 0x0200);
	delay(20);

	//	p081f.a400:ffff P000100.1f00a400:ffff, then: p081f.a400:ffff P000100.1f00a400:bfff
	//	p031f.a400:1040 P000008.1f00a400:5040, then: p031f.a400:5040 P000008.1f00a400:1040
	// FEDCR (Fast Ethernet Duplex Control Register, MMD 31.0xA400)
	// Set bit 14, sleep, then clear again, according to the datasheet these bits are reserved
	phy_modify(phy, 0x1f, 0xa400, 0x0000, 0x4000);
	delay(20);

	phy_modify(phy, 0x1f, 0xa400, 0x4000, 0x0000);
	delay(20);

	print_string("\r\n  phy config done\r\n");
}


void phy_config_8224(void) __banked
{
	uint16_t pval;
	print_string("\r\nphy_config_8224 called\r\n");

	// p001e.7b20:0bff R02f8-00000bff R02f4-00000bed P000001.1e007b20:0bed
	phy_read(RTL8224_PHY_ID, 0x1e, 0x7b20);
	pval = SFR_DATA_U16;

	REG_WRITE(0x2f8, 0, 0, pval >> 8, pval);
	pval &= 0x0fe0;
	pval |= 0x000d;
	REG_WRITE(0x2f4, 0, 0, pval >> 8, pval);

	phy_write(RTL8224_PHY_ID, 0x1e, 0x7b20, pval);

	uint8_t i = 0;
	while (rtl8224_ca[i]) {
		phy_write(RTL8224_PHY_ID, 0x1e, 0x400, rtl8224_ca[i]);
		i++;
		phy_write(RTL8224_PHY_ID, 0x1e, 0x3f8, rtl8224_ca[i]);
		i++;
		do {
			phy_read(RTL8224_PHY_ID, 0x1e, 0x3f8);
		} while (SFR_DATA_8 & 0x80);
	}

	print_string("\r\nphy_config_8224 done\r\n");
}


/*
 * Set Speed of a PHY
 * See e.g. RTL8221B datasheet
 */
void phy_set_speed(uint8_t port, uint8_t speed) __banked
{
	uint16_t v;
	phy_read(port, PHY_MMD_CTRL, 0xa610);
	v = SFR_DATA_U16;
	if (speed == PHY_OFF) {
		phy_write(port, PHY_MMD_CTRL, 0xa610, v | 0x0800);
		return;
	}
	// Port is on, make sure of it:
	if (v & 0x0800)
		phy_write(port, PHY_MMD_CTRL, 0xa610, v & 0xf7ff);

	if (speed == PHY_SPEED_AUTO) {
			// AN Advertisement Register (MMD 7.0x0010)
			// bits 0-4: 0x1 (802.3 supported), Extended Next Page format used
			phy_write(port, PHY_MMD_AN, 0x10, 0x15e1);
			// Multi-GBASE-TBASE-T AN Control 1 Register (MMD 7.0x0020)
			// bit 14: SLAVE, bit 13: Multi-Port device, bit 8: 2.5GBit available, 1: LD
			phy_write(port, PHY_MMD_AN, 0x20, 0x6081);
			// GBCR (1000Base-T Control Register, MMD 31.0xA412)
			phy_modify(port, PHY_MMD_CTRL, 0xa412, 0x0000, 0x0200); // Loop timing enabled
			phy_write(port, PHY_MMD_AN, 0x00, 0x3200);	// Restart AN
	} else {
		// AN Control Register (MMD 7.0x0000)
		phy_write(port, PHY_MMD_AN, 0x00, 0x2000);	// Clear bit 12: No Autoneg, Set Extended Pages (bit 13)
		if (speed == PHY_SPEED_10M) {
			phy_write(port, PHY_MMD_AN, 0x20, 0x6001);
			phy_write(port, PHY_MMD_AN, 0x10, 0x1461);
			phy_modify(port, PHY_MMD_CTRL, 0xa412, 0x0200, 0x0000);
		} else if (speed == PHY_SPEED_100M) {
			phy_write(port, PHY_MMD_AN, 0x20, 0x6001);
			phy_write(port, PHY_MMD_AN, 0x10, 0x1581);
			phy_modify(port, PHY_MMD_CTRL, 0xa412, 0x0200, 0x0000);
		} else {
			// AN Advertisement Register (MMD 7.0x0010)
			// bits 0-4: 0x1 (802.3 supported), Extended Next Page format used
			phy_write(port, PHY_MMD_AN, 0x10, 0x1001);
			if (speed == PHY_SPEED_1G) {
				// Multi-GBASE-TBASE-T AN Control 1 Register (MMD 7.0x0020)
				// bit 14: SLAVE, bit 13: Multi-Port device, 1: LD Loop timin enableed
				phy_write(port, PHY_MMD_AN, 0x20, 0x6001);
				// GBCR (1000Base-T Control Register, MMD 31.0xA412)
				phy_modify(port, PHY_MMD_CTRL, 0xa412, 0x0000, 0x0200);
			} else if (speed == PHY_SPEED_2G5) {
				// Multi-GBASE-TBASE-T AN Control 1 Register (MMD 7.0x0020)
				// bit 14: SLAVE, bit 13: Multi-Port device, bit 8: 2.5GBit available, 1: LD Loop timin enableed
				phy_write(port, PHY_MMD_AN, 0x20, 0x6081);
				// GBCR (1000Base-T Control Register, MMD 31.0xA412)
				phy_modify(port, PHY_MMD_CTRL, 0xa412, 0x0200, 0x0000);
			}
		}
		phy_write(port, PHY_MMD_AN, 0x00, 0x3000);	// Enable AN
	}
}


void phy_set_duplex(uint8_t port, uint8_t fullduplex) __banked
{
	uint16_t v;
	phy_read(port, PHY_MMD_AN, 0x00);
	v = SFR_DATA_U16;	
	if (!(v & 0x1000)) { // AN disabled, we are in forced mode
		phy_read(port, PHY_MMD_CTRL, 0xa400);
		v = SFR_DATA_U16;
		if (fullduplex)
			v |= 0x0100;
		else
			v &= 0xfeff;
		phy_write(port, PHY_MMD_CTRL, 0xa400, v);
		return;
	}
	// Disable AN
	phy_write(port, PHY_MMD_AN, 0x00, 0x2000);
	phy_read(port, PHY_MMD_AN, 0x10);
	v = SFR_DATA_U16;
	if (v & 0x0060) {
		if (fullduplex)
			phy_modify(port, PHY_MMD_AN, 0x10, 0xffbf, 0x0040);
		else
			phy_modify(port, PHY_MMD_AN, 0x10, 0xffdf, 0x0020);
	}
	if (v & 0x0180) {
		if (fullduplex)
			phy_modify(port, PHY_MMD_AN, 0x10, 0xfeff, 0x0100);
		else
			phy_modify(port, PHY_MMD_AN, 0x10, 0xff7f, 0x0080);
	}
	// Restart AN
	phy_write(port, PHY_MMD_AN, 0x00, 0x3000);
}


void phy_show(uint8_t port) __banked
{
	uint16_t v;

	// The actual PHY speed is in a Realtek propriatary register
	print_string("\nLink speed: ");
	phy_read(port, PHY_MMD_CTRL, 0xA434);
	v = SFR_DATA_U16;
	v = ((v & 0x0600) >> 7) | ((v & 0x0030) >> 4);
	switch(v) {
	case 0:
		print_string("10M");
		break;
	case 1:
		print_string("100M");
		break;
	case 2:
		print_string("1000M");
		break;
	case 3:
		print_string("500M");
		break;
	case 4:
		print_string("10G");
		break;
	case 5:
		print_string("2500M");
		break;
	case 6:
		print_string("5G");
		break;
	default:
		print_string("10M");
	}
	if (v & 0x8)
		print_string(" full duplex");
	else
		print_string(" half duplex");

	phy_read(port,  PHY_MMD_AN, 0x00);
	v = SFR_DATA_U16;
	if (!(v & 0x1000)) { // AN disabled, we are in forced mode
		phy_read(port, PHY_MMD_PMAPMD, 0);
		v = SFR_DATA_U16;
		print_string("\nForced speed: "); print_short(v); write_char('\n');
		uint8_t s1 = ((v & 0x40) ? 0x2 : 0x0) | ((v & 0x2000) ? 0x1 : 0x0);
		uint8_t s2 = (v >> 2) & 0xf;
		switch(s1) {
		case 0:
			print_string("10M\n");
			break;
		case 1:
			print_string("100M\n");
			break;
		case 2:
			print_string("1000M\n");
			break;
		case 3:
			switch (s2) {
			case 0:
				print_string("10G\n");
				break;
			case 6:
				print_string("2500M\n");
				break;
			case 7:
				print_string("5G\n");
				break;
			default:
				print_string("Unknown\n");
			}
			break;
		default:
			print_string("Unknown\n");
		}
		phy_read(port, PHY_MMD_CTRL, 0xa400);
		v = SFR_DATA_U16;
		print_string("Duplex: "); print_short(v); print_string(" enabled: ");
		if (v & 0x100)
			print_string("yes");
		else
			print_string("no");
		write_char('\n');

	} else {
		print_string("\nAN enabled, advertising:");
		phy_read(port, PHY_MMD_AN, 0x10);
		v = SFR_DATA_U16;
		if (v & 0x0040)
			print_string(" 10Base-Half");
		if (v & 0x0040)
			print_string(" 10Base-Full");
		if (v & 0x0080)
			print_string(" 100Base-Half");
		if (v & 0x0100)
			print_string(" 100Base-Full");
		phy_read(port, PHY_MMD_CTRL, 0xa412);
		v = SFR_DATA_U16;
		if (v & 0x0200)
			print_string(" 1000Base-Full");
		phy_read(port, PHY_MMD_AN, 0x10);
		v = SFR_DATA_U16;
		if (v & 0x0080)
			print_string(" 2500BaseN-Full");
		
	}
	phy_read(port, PHY_MMD_AN, 0x13);
	v = SFR_DATA_U16;
	print_string("\nLink Partner advertises:");
	if (v & 0x0020)
		print_string(" 10Base-Half");
	if (v & 0x0040)
		print_string(" 10Base-Full");
	if (v & 0x0080)
		print_string(" 100Base-Half");
	if (v & 0x0100)
		print_string(" 100Base-Full");
	phy_read(port, PHY_MMD_CTRL, 0xa414);
	v = SFR_DATA_U16;
	if (v & 0x0400)
		print_string(" 1000Base-Half");
	if (v & 0x0800)
		print_string(" 1000Base-Full");
	phy_read(port, PHY_MMD_AN, 33);
	v = SFR_DATA_U16;
	if (v & 0x0020)
		print_string(" 2500Base-Full");
	if (v & 0x0040)
		print_string(" 5000Base-Full");
	if (v & 0x0800)
		print_string(" 10GBase-Full");
	write_char('\n');
}


void phy_reset(uint8_t port) __banked
{
	uint16_t v;
	phy_read(port, PHY_MMD_CTRL, 0xa610);
	v = SFR_DATA_U16;
	// If PHY off, do nothing
	if (v & 0x0800)
		return;

	// Disable PHY
	phy_write(port, PHY_MMD_CTRL, 0xa610, v | 0x0800);
	delay(2);
	// Re-enable PHY
	phy_write(port, PHY_MMD_CTRL, 0xa610, v & 0xf7ff);
}
