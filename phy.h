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
#define PHY_MMD_AN	7
#define PHY_SDS_CTRL	30
#define PHY_MMD_CTRL	31


/*
 * Define registers in Auto-Negotiation page
 */
#define PHY_ANEG_CTRL		0x00
#define PHY_EEE_ADV		0x3c
#define PHY_EEE_LP_ABILITY	0x3d
#define PHY_EEE_ADV2		0x3e
#define PHY_EEE_LP_ABILITY2	0x3f
// Register bits for EEE capabilities at a given speed
#define PHY_EEE_BIT_2G5		0x01
#define PHY_EEE_BIT_1G		0x04
#define PHY_EEE_BIT_100M	0x02

/*
 * Define registers in Control page
 */
#define PHY_CTRL_5	0x7582
#endif
