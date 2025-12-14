#ifndef _RTL837X_PHY_H_
#define _RTL837X_PHY_H_

#define PHY_SPEED_10M	0x2
#define PHY_SPEED_100M	0x3
#define PHY_SPEED_1G	0x4
#define PHY_SPEED_2G5	0x5
#define PHY_SPEED_5G	0x6
#define PHY_SPEED_10G	0x7
#define PHY_SPEED_AUTO	0x10
#define PHY_OFF		0xff

void rtl8224_phy_enable(void) __banked;
void phy_config(uint8_t phy) __banked;
void phy_config_8224(void) __banked;
void phy_set_speed(uint8_t port, uint8_t speed) __banked;
void phy_set_duplex(uint8_t port, uint8_t fullduplex) __banked;
void phy_show(uint8_t port) __banked;
void phy_reset(uint8_t port) __banked;

#endif
