#ifndef _RTLOS_PHY_H_
#define _RTLOS_PHY_H_

/*
 * PHY driver interface
 *
 */

#include "machine.h"

/* Speed and duplex codes the console fills into phy_settings before it calls a
 * driver entry point. */
#define PHY_SPEED_10M	0x2
#define PHY_SPEED_100M	0x3
#define PHY_SPEED_1G	0x4
#define PHY_SPEED_2G5	0x5
#define PHY_SPEED_5G	0x6
#define PHY_SPEED_10G	0x7
#define PHY_SPEED_AUTO	0x10
#define PHY_OFF		0xff

struct phy_settings {
	uint8_t duplex;
	uint8_t port;
	uint8_t speed;
	uint8_t is10g_port;
};

extern __xdata struct phy_settings phy_settings;

#if SOC_FAMILY == RTL836X

#include "rtl836x_phy.h"

#else

#include "rtl837x_phy.h"

#endif

#endif
