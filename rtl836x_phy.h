#ifndef _RTL836X_PHY_H_
#define _RTL836X_PHY_H_

/*
 * RTL836x PHY driver interface (rtl836x_phy.c).
 *
 * Included through rtlos_phy.h, which owns the declarations both dies share
 * (PHY_SPEED_*, struct phy_settings) and picks this header or rtl837x_phy.h per
 * SOC_FAMILY. Only the entry points this die implements are declared: there is
 * no RTL8224/RTL8261 here.
 */

void phy_config(uint8_t phy) __banked;
void phy_set_speed(void) __banked;
void phy_set_duplex(void) __banked;
void phy_show(uint8_t port) __banked;
void phy_reset(uint8_t port) __banked;

#endif
