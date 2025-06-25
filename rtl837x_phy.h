#ifndef _RTL837X_PHY_H_
#define _RTL837X_PHY_H_

void rtl8224_phy_enable(void) __banked;
void phy_config(uint8_t phy) __banked;
void phy_config_8224(void) __banked;

#endif
