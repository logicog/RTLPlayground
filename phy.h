#ifndef _PHY_H_
#define _PHY_H_

/*
 * The RTL8272/RTL8273 appear to comprise an RTL8224 PHY
 * which in turn is a 4x RTL8221B PHY
 * These phys are all Clause 45
 * The defines below are taken from the RTL8221B datasheet
 */

/*
 * Define PHY pages
 */
#define PHY_MMD_PMAPMD	1
#define PHY_MMD_AN	7
#define PHY_MMD30	30
#define PHY_MMD31	31

/*
 * Define registers in Auto-Negotiation page
 */
#define PHY_ANEG_CTRL		0x00
#define PHY_ANEG_ADV		0x10
#define PHY_ANEG_LP_ABILITY	0x13
#define PHY_ANEG_MGBASE_CTRL	0x20
#define PHY_ANEG_MGBASE_ADV	0x21
#define PHY_EEE_ADV		0x3c
#define PHY_EEE_LP_ABILITY	0x3d
#define PHY_EEE_ADV2		0x3e
#define PHY_EEE_LP_ABILITY2	0x3f

// Register bits for EEE capabilities at a given speed
// PHY_EEE_ADV2
#define PHY_EEE_BIT_2G5		0x01
#define PHY_EEE_BIT_5G		0x02
// PHY_EEE_ADV
#define PHY_EEE_BIT_1G		0x04
#define PHY_EEE_BIT_100M	0x02
#define PHY_EEE_BIT_10G		0x08

/*
 * MMD 31 Registers
 */
#define PHY_MMD31_FEDCR     0xa400
#define PHY_MMD31_GBCR      0xa412
#define PHY_MMD31_GANLPAR   0xa414
#define PHY_MMD31_PHYCR2    0xa432
#define PHY_MMD31_PHYSR     0xa434



/*
 * Define registers in Control page
 */
#define PHY_CTRL_5	0x7582

/*
 * Duplex settings
 */
#define PHY_DUPLEX_HALF		0
#define PHY_DUPLEX_FULL		1
#define PHY_DUPLEX_BOTH		2

// RTL8221 PTP, MMD31. Realtek datasheet Rev.1.0, sections 8.7.30-61.
#define RTL8221_PTP_CTRL		0xE400
#define RTL8221_PTP_ENABLE	0x0001
#define RTL8221_PTP_CLK_CFG	0xE410
#define RTL8221_PTP_CLK_EXEC	0x0001
#define RTL8221_PTP_CLK_READ	2
#define RTL8221_PTP_CLK_WRITE	3
#define RTL8221_PTP_RATE_READ	6
#define RTL8221_PTP_RATE_WRITE	7
#define RTL8221_PTP_CLK_INPUT_MASK	0x0070
#define RTL8221_PTP_CLK_FREE_RUNNING	0x0020
#define RTL8221_PTP_CAPTURE_MASK	0x001C
#define RTL8221_PTP_NS_LO		0xE412
#define RTL8221_PTP_NS_HI		0xE414
#define RTL8221_PTP_S_LO		0xE416
#define RTL8221_PTP_TS_STATUS	0xE430
#define RTL8221_PTP_TS_READ	0x0001
#define RTL8221_PTP_TS_INFO	0xE440
#define RTL8221_PTP_TS_HASH	0xE442
#define RTL8221_PTP_TS_SEQ		0xE444
#define RTL8221_PTP_TS_NS_LO	0xE446
#define RTL8221_PTP_TS_S_LO	0xE44A
#define RTL8221_PTP_INTERNAL_CTRL	0xE500
#define RTL8221_PTP_INTERNAL_MASK	0x7200
#define RTL8221_PTP_INTERNAL_25M	0x1200
#endif
