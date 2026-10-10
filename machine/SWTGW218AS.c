#include "machine_defs.h"


__code const struct machine machine = {
	.machine_name = MACHINE_MODEL,
	.isRTL8373 = 1,
	.mac_flash_offset = 0x1FC000,
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
	.reset_pin = GPIO54_ACL_BIT2_EN,
	.high_leds = { .mux = LED_27 | LED_28_SYS | LED_29, .enable = LED_28_SYS | LED_29 },
	.port_led_set = { 0, 0, 0, 0, 0, 0, 0, 0, 1},
	.led_sets = { { LEDS_2G5 | LEDS_LINK | LEDS_ACT, // Green LED (right)
					0, // unused
					LEDS_1G | LEDS_100M | LEDS_10M | LEDS_LINK | LEDS_ACT, // Amber LED (left)
					0
				  }, // unused
				  { LEDS_10G | LEDS_5G | LEDS_2G5 | LEDS_1G | LEDS_100M | LEDS_LINK | LEDS_ACT, // SFP LED
					0, // unused
					0, // unused
					0
				  }, // unused		    	},
				},
};
