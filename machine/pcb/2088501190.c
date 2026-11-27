#define MACHINE_CHIPSET RTL8372
#include "machine_defs.h"

__code const struct machine machine = {
	MACHINE_COMMON_SETTING,
	.mac_flash_offset = 0x1FC000,
	.phy_mdi_reverse = PHY_MDI_REVERSE(0x3),
	.n_sfp = 2,
	.log_to_phys_port = {0, 0, 0, 6, 1, 2, 3, 4, 5},
	.phys_to_log_port = {4, 5, 6, 7, 8, 3, 0, 0, 0},
	.is_sfp = {0, 0, 0, 2, 0, 0, 0, 0, 1},
	.gpio_exp = { .addr = 0x21, .i2c = I2CBUS( GPIO49_I2C_SDA1, GPIO48_I2C_SCL1 ),
		      .out = { 0x1f, 0x00 }, .dir = { 0x1a, 0x00 } },
	// Left SFP+ cage, port 5
	.sfp_port[0].pin_detect = GPIO_EXP(3),
	.sfp_port[0].pin_los = GPIO_NA,
	.sfp_port[0].pin_tx_disable = GPIO_EXP(2),
	.sfp_port[0].sds = 1,
	.sfp_port[0].i2c = I2CBUS( GPIO41_I2C_SDA3_MDIO1, GPIO40_I2C_SCL3_MDC1 ),
	// Right SFP+ cage, port 6
	.sfp_port[1].pin_detect = GPIO_EXP(1),
	.sfp_port[1].pin_los = GPIO_NA,
	.sfp_port[1].pin_tx_disable = GPIO_EXP(0),
	.sfp_port[1].sds = 0,
	.sfp_port[1].i2c = I2CBUS( GPIO47_I2C_SDA0, GPIO46_I2C_SCL0 ),
	.reset_pin = GPIO_EXP(4),
	.high_leds = { .mux = 0, .enable = LED_27 },
	.port_led_set = { 0, 0, 0, 1, 0, 0, 0, 0, 1},
	.led_sets = {
		{   /* RJ45: green 2.5G, yellow 10/100/1000M, blink on activity */
			0,
			LEDS_2G5 | LEDS_LINK | LEDS_ACT,
			LEDS_1G | LEDS_100M | LEDS_10M | LEDS_LINK | LEDS_ACT,
			0,
		}, { /* SFP+: green 10G, yellow 2.5G/1G/100M, blink on activity */
			0,
			LEDS_10G | LEDS_LINK | LEDS_ACT,
			LEDS_2G5 | LEDS_1G | LEDS_100M | LEDS_LINK | LEDS_ACT,
			0,
		}},
	.led_mux_custom = 1,
	.led_mux = { 0x3f, 0x0d, 0x0e, 0x3f, 0x11, 0x12, 0x15, 0x3f,
		     0x16, 0x3f, 0x3f, 0x3f, 0x19, 0x3f, 0x3f, 0x1a,
		     0x3f, 0x3f, 0x1d, 0x3f, 0x3f, 0x1e, 0x3f, 0x3f,
		     0x3f, 0x3f, 0x21, 0x22 },
};

// Match the stock LED setup: pads 26/27 have the inverted active level,
// pads 0-27 and bit 30 are IO enabled. Bits 28/29 stay as leds_setup() left them.
MACHINE_CUSTOM_INIT(
	reg_read_m(RTL837X_REG_LED_GLB_ACTIVE);
	sfr_mask_data(3, 0x0f, 0x0c);
	sfr_mask_data(2, 0xff, 0x00);
	sfr_mask_data(1, 0xff, 0x00);
	sfr_mask_data(0, 0xff, 0x00);
	reg_write_m(RTL837X_REG_LED_GLB_ACTIVE);

	reg_read_m(RTL837X_REG_LED_GLB_IO_EN);
	sfr_mask_data(3, 0x4f, 0x4f);
	sfr_mask_data(2, 0xff, 0xff);
	sfr_mask_data(1, 0xff, 0xff);
	sfr_mask_data(0, 0xff, 0xff);
	reg_write_m(RTL837X_REG_LED_GLB_IO_EN);

	// Stock TX polarity of the copper PHYs, the inverse of the reset value 0xa695
	REG_SET(RTL837X_CFG_PHY_TX_POLARITY_SWAP, 0x0000596a);
)
