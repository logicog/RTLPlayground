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

struct phy_settings {
	uint8_t duplex;
	uint8_t port;
	uint8_t speed;
};

extern __xdata struct phy_settings phy_settings;

void rtl8224_phy_enable(void) __banked;
void phy_config(uint8_t phy) __banked;
void phy_config_8224(void) __banked;
void phy_set_speed(void) __banked;
void phy_set_duplex(void) __banked;
void phy_show(uint8_t port) __banked;
void phy_reset(uint8_t port) __banked;
void rtl8224_read_reg_u16(uint16_t reg) __banked;
void rtl8224_write_reg_u16(uint16_t reg, uint16_t val) __banked;
void rtl8224_sds_write(uint16_t sds_cmd, uint16_t val) __banked;

#define	RTL8224_SDS_WRITE(sds_id, page, reg, v) uint16_t _sdscmd = (uint16_t)(sds_id & 0x01) | (1 << 14) | (1 << 15); \
	_sdscmd |= (page & 0x3F) << 1; \
	_sdscmd |= ((uint16_t)(reg & 0x1f)) << 7; \
	print_string("CMD: "); print_short(_sdscmd); \
	write_char('-'); print_short(v); \
    rtl8224_sds_write(_sdscmd, v);

#endif
