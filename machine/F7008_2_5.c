#include "machine_defs.h"

__code const struct machine machine = {
	.machine_name = MACHINE_MODEL,
	.isRTL8373 = 1,
	.min_port = 0,
	.max_port = 8,
	.n_sfp = 1,
	.log_to_phys_port = {1, 2, 3, 4, 5, 6, 7, 8, 9},
	.phys_to_log_port = {0, 1, 2, 3, 4, 5, 6, 7, 8},
	.is_sfp = {0, 0, 0, 0, 0, 0, 0, 0, 1},
	.sfp_port[0].pin_detect = GPIO30_ACL_BIT3_EN,
	.sfp_port[0].pin_los = GPIO37,
	.sfp_port[0].pin_tx_disable = GPIO_NA,
	.sfp_port[0].sds = 1,
	.sfp_port[0].i2c = I2CBUS( GPIO39_I2C_SDA4, GPIO40_I2C_SCL3_MDC1 ),
	.reset_pin = GPIO_NA,
	.high_leds = { .mux = LED_27 | LED_29, .enable = LED_28_SYS | LED_29 },
	.port_led_set = {0, 0, 0, 0, 0, 0, 0, 0, 1},
	.led_sets = {
		{	/* SET0 */
			LEDS_2G5 | LEDS_LINK,				/* LEDID0 - green: 2.5G link */
			LEDS_2G5 | LEDS_1G | LEDS_100M | LEDS_10M | LEDS_LINK | LEDS_ACT,	/* LEDID1 - yellow: traffic */
			0,
			0,
		},
		{	/* SET1 - SFP+ */
			LEDS_10G | LEDS_5G | LEDS_2G5 | LEDS_LINK,
			LEDS_10G | LEDS_5G | LEDS_2G5 | LEDS_1G | LEDS_LINK | LEDS_ACT,
			0,
			0,
		},
	},
	.led_mux_custom = 1,
	.led_mux = {
		0x00, 0x01, 0x04, 0x05, 0x08, 0x09, 0x0c, 0x02,
		0x0d, 0x10, 0x11, 0x02, 0x14, 0x02, 0x02, 0x15,
		0x02, 0x02, 0x18, 0x02, 0x02, 0x19, 0x02, 0x02,
		0x1c, 0x1d, 0x02, 0x02,
	},
};
