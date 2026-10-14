#include "machine_defs.h"

__code const struct machine machine = {
	.machine_name = MACHINE_MODEL,
	.isRTL8373 = 0,
	.min_port = 3,
	.max_port = 8,
	.n_sfp = 2,
	.log_to_phys_port = {0, 0, 0, 5, 1, 2, 3, 4, 6},
	.phys_to_log_port = {4, 5, 6, 7, 3, 8, 0, 0, 0},
	.is_sfp = {0, 0, 0, 2, 0, 0, 0, 0, 1},
	.sfp_port[0].pin_detect = 50,
	.sfp_port[0].pin_los = 10,
	.sfp_port[0].pin_tx_disable = GPIO_NA,
	.sfp_port[0].sds = 1,
	.sfp_port[0].i2c = I2CBUS( GPIO41_I2C_SDA3_MDIO1, GPIO40_I2C_SCL3_MDC1 ),
	.sfp_port[1].pin_detect = 30,
	.sfp_port[1].pin_los = 51,
	.sfp_port[1].pin_tx_disable = GPIO_NA,
	.sfp_port[1].sds = 0,
	.sfp_port[1].i2c = I2CBUS( GPIO39_I2C_SDA4, GPIO40_I2C_SCL3_MDC1 ),
	.reset_pin = GPIO_NA,
	.high_leds = { .mux = LED_27 , .enable = LED_27 | LED_29 },
	.port_led_set = { 0, 0, 0, 1, 0, 0, 0, 0, 1},
	/* The Ethernet ports have 1 amber LED (left) and 1 green LED (right)
	 * The SFP ports have also 1 amber LED and 1 green LED
	 * Ethernet ports use LED-set 0, SFP ports use LED-set 1
	 */
	.led_sets = { { LEDS_10M | LEDS_LINK | LEDS_ACT,
			LEDS_1G | LEDS_100M | LEDS_10M | LEDS_2G5 | LEDS_LINK | LEDS_ACT,
			LEDS_2G5 | LEDS_LINK | LEDS_ACT,
			0 },
			{ LEDS_100M | LEDS_10M | LEDS_LINK,
			LEDS_2G5 | LEDS_1G | LEDS_100M | LEDS_10M | LEDS_10M | LEDS_LINK | LEDS_ACT | LEDS_10G,
			LEDS_10G | LEDS_LINK,
			0 },
		    },
};
