#ifndef _RTL837X_PHY_H_
#define _RTL837X_PHY_H_

#define PHY_SPEED_AUTO	0x1
#define PHY_SPEED_1G	0x2
#define PHY_SPEED_2G5	0x3
#define PHY_OFF		0xff

void rtl8224_phy_enable(void) __banked;
void phy_config(uint8_t phy) __banked;
void phy_config_8224(void) __banked;
void phy_set_mode(uint8_t port, uint8_t speed, uint8_t flow_control, uint8_t duplex) __banked;
void phy_reset(uint8_t port) __banked;

#endif
