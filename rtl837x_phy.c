/*
 * This is a driver implementation for the Internal PHYs and RTL8221/RTL8224 PHYs
 * for the RTL827x platform
 * This code is in the Public Domain
 */

#include <stdint.h>
#include "rtl837x_common.h"
#include "rtl837x_sfr.h"
#include "rtl837x_regs.h"

#pragma codeseg BANK1

extern __code uint16_t bit_mask[16];

/*
P000001.1e000400:4480 P000001.1e0003f8:c842 p001e.03f8:4842
P000001.1e000400:0400 P000001.1e0003f8:c9c2 p001e.03f8:49c2
P000001.1e000400:6d02 P000001.1e0003f8:cc42 p001e.03f8:4c42
P000001.1e000400:424e P000001.1e0003f8:cdc2 p001e.03f8:4dc2
P000001.1e000400:0002 P000001.1e0003f8:cec2 p001e.03f8:4ec2
P000001.1e000400:1390 P000001.1e0003f8:ce6c p001e.03f8:4e6c
P000001.1e000400:003f P000001.1e0003f8:ca6c p001e.03f8:4a6c
P000001.1e000400:0200 P000001.1e0003f8:c86c p001e.03f8:486c
P000001.1e000400:0080 P000001.1e0003f8:c25c p001e.03f8:425c
P000001.1e000400:0408 P000001.1e0003f8:c35c p001e.03f8:435c
P000001.1e000400:020d P000001.1e0003f8:c3dc p001e.03f8:43dc
P000001.1e000400:0601 P000001.1e0003f8:c4dc p001e.03f8:44dc
P000001.1e000400:222c P000001.1e0003f8:c5dc p001e.03f8:45dc
P000001.1e000400:a217 P000001.1e0003f8:c65c p001e.03f8:465c
P000001.1e000400:fe40 P000001.1e0003f8:c6dc p001e.03f8:46dc
P000001.1e000400:f5c1 P000001.1e0003f8:cadc p001e.03f8:4adc
P000001.1e000400:0443 P000001.1e0003f8:cb5c p001e.03f8:4b5c
P000001.1e000400:abb0 P000001.1e0003f8:cedc p001e.03f8:4edc
P000001.1e000400:5078 P000001.1e0003f8:c90c p001e.03f8:490c
P000001.1e000400:c45c P000001.1e0003f8:c18c p001e.03f8:418c

*/

__code uint16_t rtl8224_ca[41] = {
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
	// p001e.0a90:00f3 R02f8-000000f3 R02f4-000000fc P000001.1e000a90:00fc
	print_string("\r\nrtl8224_phy_enable called\r\n");
	phy_read(0, 0x1e, 0xa90);
	uint16_t pval = SFR_DATA_8;
	pval <<= 8;
	pval |= SFR_DATA_0;

	// PHY Initialization:
	REG_WRITE(0x2f8, 0, 0, pval >> 8, pval);

	delay(10);

	pval &= 0xfff0;
	pval |= 0x0c;
	REG_WRITE(0x2f4, 0, 0, pval >> 8, pval);

	delay(10);

	phy_write(0x1, 0x1e, 0xa90, pval);

	phy_read(0, 0x1e, 0xa90);
	pval = SFR_DATA_8;
	pval <<= 8;
	pval |= SFR_DATA_0;
	print_string("\r\nrtl8224_phy_enable done\r\n");
}


void phy_config(uint8_t phy) __banked
{
	uint16_t pval;
	print_string("\r\nphy_config: ");
	write_char('0' + phy);

	delay(20);
	// PHY configuration: External 8221B?
//	p081e.75f3:ffff P000100.1e0075f3:fffe
	phy_read(phy, 0x1e, 0x75f3);
	pval = SFR_DATA_8;
	pval <<= 8;
	pval |= SFR_DATA_0 & 0xfe;
	phy_write(bit_mask[phy], 0x1e, 0x75f3, pval);
	delay(20);

//	p081e.697a:ffff P000100.1e00697a:ffc1 / p031e.697a:0003 P000008.1e00697a:0001
	// SERDES OPTION 1 Register (MMD 30.0x6) bits 0-5: 0x01: Set HiSGMII+SGMII
	phy_read(phy, 0x1e, 0x697a);
	pval = SFR_DATA_8;
	pval <<= 8;
	pval |= SFR_DATA_0 & 0xc0 | 0x01;
	phy_write(bit_mask[phy], 0x1e, 0x697a, pval);
	delay(20);

//	p031f.a432:0811 P000008.1f00a432:0831
	// PHYCR2 PHY Specific Control Register 2, MMD 31. 0xA432), set bit 5: enable EEE
	phy_read(phy, 0x1f, 0xa432);
	pval = SFR_DATA_8;
	pval <<= 8;
	pval |= SFR_DATA_0 | 0x20;
	phy_write(bit_mask[phy], 0x1f, 0xa432, pval);

//	p0307.003e:0000 P000008.0700003e:0001
	// EEE avertisment 2 register MMMD 7.0x003e, set bit 0: 2.5G has EEE capability
	phy_read(phy, 0x7, 0x3e);
	pval = SFR_DATA_8;
	pval <<= 8;
	pval |= SFR_DATA_0 | 0x1;
	phy_write(bit_mask[phy], 0x7, 0x3e, pval);
	delay(20);

//	p031f.a442:043c P000008.1f00a442:0430
	// Unknown, but clear bits 2/3
	phy_read(phy, 0x1f, 0xa442);
	pval = SFR_DATA_8;
	pval <<= 8;
	pval |= SFR_DATA_0 & 0xf3;
	phy_write(bit_mask[phy], 0x1f, 0xa442, pval);
	delay(20);

	// P000100.1e0075b5:e084
	phy_write(bit_mask[phy], 0x1e, 0x75b5, 0xe084);
	delay(20);

//	p031e.75b2:0000 P000008.1e0075b2:0060
	// set bits 5/6
	phy_read(phy, 0x1e, 0x75b2);
	pval = SFR_DATA_8;
	pval <<= 8;
	pval |= SFR_DATA_0 | 0x60;
	phy_write(bit_mask[phy], 0x1e, 0x75b2, pval);
	delay(20);

//	p081f.d040:ffff P000100.1f00d040:feff
	// LCR6 (LED Control Register 6, MMD 31.D040), set bits 8/9 to 0b10
	phy_read(phy, 0x1e, 0xd040);
	pval = (SFR_DATA_8 & 0xfc) | 0x02;
	pval <<= 8;
	pval |= SFR_DATA_0;
	phy_write(bit_mask[phy], 0x1e, 0xd040, pval);
	delay(20);

//	p081f.a400:ffff P000100.1f00a400:ffff, then: p081f.a400:ffff P000100.1f00a400:bfff
//	p031f.a400:1040 P000008.1f00a400:5040, then: p031f.a400:5040 P000008.1f00a400:1040
	// FEDCR (Fast Ethernet Duplex Control Register, MMD 31.0xA400)
	// Set bit 14, sleep, then clear again, according to the datasheet these bits are reserved

	phy_read(phy, 0x1f, 0xa400);
	pval = SFR_DATA_8 | 0x40;
	pval <<= 8;
	pval |= SFR_DATA_0;
	phy_write(bit_mask[phy], 0x1f, 0xa400, pval);
	delay(20);

	phy_read(phy, 0x1f, 0xa400);
	pval = SFR_DATA_8 & 0xbf;
	pval <<= 8;
	pval |= SFR_DATA_0;
	phy_write(bit_mask[phy], 0x1f, 0xa400, pval);
	delay(20);

	print_string("\r\n  phy config done\r\n");
}


void phy_config_8224(void) __banked
{
	// p001e.7b20:0bff R02f8-00000bff R02f4-00000bff P000001.1e007b20:0bff p001e.7b20:0bff R02f8-00000bff R02f4-00000bff P000001.1e007b20:0bff p001e.7b20:0bff R02f8-00000bff R02f4-00000bed P000001.1e007b20:0bed

	uint16_t pval;
	print_string("\r\nphy_config_8224 called\r\n");
	// p001e.7b20:0bff R02f8-00000bff R02f4-00000bed P000001.1e007b20:0bed
	phy_read(0, 0x1e, 0x7b20);
	pval = SFR_DATA_8;
	pval <<= 8;
	pval |= SFR_DATA_0;
	phy_write(0x01, 0x1e, 0x7b20, pval);

	delay(20);

	REG_WRITE(0x2f8, 0, 0, pval >> 8, pval);

	delay(20);
	pval &= 0xfed;
	REG_WRITE(0x2f4, 0, 0, pval >> 8, pval);

	delay(20);
	phy_write(0x01, 0x1e, 0x7b20, pval);

	delay(100);

	uint8_t i = 0;
	while (rtl8224_ca[i]) {
		phy_write(0x1, 0x1e, 0x400, rtl8224_ca[i]);
		i++;
		delay(10);
		phy_write(0x1, 0x1e, 0x3f8, rtl8224_ca[i]);
		i++;
		do {
			phy_read(0, 0x1e, 0x3f8);
		} while (SFR_DATA_8 & 0x80);
	}

/*
	print_string("\r\nS");
	i = 0;
	while (rtl8224_cb[i] != 0xffff) {
		phy_write(0x1, 0x1e, 0x400, rtl8224_cb[i]);
		i++;
		phy_write(0x1, 0x1e, 0x3f8, rtl8224_cb[i]);
		i++;
		do {
			phy_read(0, 0x1e, 0x3f8);
		} while (SFR_DATA_8 & 0x80);
		phy_write(0x1, 0x1e, 0x3f8, rtl8224_cb[i]);
		i++;
		do {
			phy_read(0, 0x1e, 0x3f8);
		} while (SFR_DATA_8 & 0x80);
		do {
			phy_read(0, 0x1e, 0x3fc);
		} while (SFR_DATA_8 & 0x80);
	}

	// P000001.1e000400:4000 P000001.1e0003f8:c2ec p001e.03f8:42ec
	phy_write(0x1, 0x1e, 0x400, 0x4000);
	phy_write(0x1, 0x1e, 0x3f8, 0xc2ec);
	do {
		phy_read(0, 0x1e, 0x3f8);
	} while (SFR_DATA_8 & 0x80);

	// P000001.1e000400:001f P000001.1e0003f8:c13e p001e.03f8:413e P000001.1e0003f8:8abe p001e.03f8:0abe p001e.03fc:0057
	phy_write(0x1, 0x1e, 0x400, 0x1f);
	phy_write(0x1, 0x1e, 0x3f8, 0xc13e);
	do {
		phy_read(0, 0x1e, 0x3f8);
	} while (SFR_DATA_8 & 0x80);
	phy_write(0x1, 0x1e, 0x3f8, 0x8abe);
	do {
		phy_read(0, 0x1e, 0x3f8);
	} while (SFR_DATA_8 & 0x80);
	do {
		phy_read(0, 0x1e, 0x3fc);
	} while (SFR_DATA_8 & 0x80);
	sleep (10);

	// P000001.1e0003f8:800a p001e.03f8:000a p001e.03fc:100d P000001.1e0003f8:800a p001e.03f8:000a p001e.03fc:100d
	phy_write(0x1, 0x1e, 0x3f8, 0x800a);
	do {
		phy_read(0, 0x1e, 0x3f8);
	} while (SFR_DATA_8 & 0x80);
	sleep (10);
	phy_write(0x1, 0x1e, 0x3f8, 0x800a);
	do {
		phy_read(0, 0x1e, 0x3f8);
	} while (SFR_DATA_8 & 0x80);
	sleep (10);
	*/

	print_string("\r\nphy_config_8224 done\r\n");
}
