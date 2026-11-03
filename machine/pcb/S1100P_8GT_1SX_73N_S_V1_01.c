#include "machine_defs.h"

__code const struct machine machine = {
	MACHINE_COMMON_SETTING,
	.mac_flash_offset = 0x1FC000,
	.n_sfp = 1,
	.log_to_phys_port = {1, 2, 3, 4, 5, 6, 7, 8, 9},
	.phys_to_log_port = {0, 1, 2, 3, 4, 5, 6, 7, 8},
	.is_sfp = {0, 0, 0, 0, 0, 0, 0, 0, 1},
	.sfp_port[0].pin_detect = GPIO38,
	.sfp_port[0].pin_los = GPIO37,
	.sfp_port[0].pin_tx_disable = GPIO_NA,
	.sfp_port[0].sds = 1,
	.sfp_port[0].i2c = I2CBUS( GPIO39_I2C_SDA4, GPIO40_I2C_SCL3_MDC1 ),
	.reset_pin = GPIO54_ACL_BIT2_EN,
	.high_leds = { .mux = LED_27 | LED_28_SYS | LED_29, .enable = LED_28_SYS | LED_29 },
	.port_led_set = {0, 0, 0, 0, 0, 0, 0, 0, 1},
	.led_sets = {
		{
			LEDS_1G | LEDS_100M | LEDS_10M | LEDS_LINK | LEDS_ACT, // LED0: orange
			0,
			LEDS_2G5 | LEDS_LINK | LEDS_ACT, // LED2: green
			0
		},
		{
			LEDS_10G | LEDS_5G | LEDS_2G5 | LEDS_1G | LEDS_100M | LEDS_LINK | LEDS_ACT, // SFP LED0: green (both SFP LEDs are green)
			0, 0, 0
		}
	},
	.led_mux_custom = 1,
	.led_mux = {
		0x00, 0x02, 0x04, 0x06, 0x08, 0x0a, 0x0c, 0x09,
		0x0e, 0x10, 0x12, 0x0e, 0x14, 0x11, 0x12, 0x16,
		0x15, 0x16, 0x18, 0x19, 0x1a, 0x1a, 0x1d, 0x1e,
		0x1c, 0x1e, 0x20, 0x21
	},
};
