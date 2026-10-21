#include "machine_defs.h"

__code const struct machine machine = {
	.machine_name = MACHINE_MODEL,
	.isRTL8373 = 1,
	.min_port = 0,
	.max_port = 8,
	.n_sfp = 1,
	.log_to_phys_port = { 1, 2, 3, 4, 5, 6, 7, 8, 9 },
	.phys_to_log_port = { 0, 1, 2, 3, 4, 5, 6, 7, 8 },
	.is_sfp = { 0, 0, 0, 0, 0, 0, 0, 0, 1 },
	.sfp_port[0].pin_detect = GPIO30_ACL_BIT3_EN,
	.sfp_port[0].pin_los = GPIO37,
	.sfp_port[0].pin_tx_disable = GPIO_NA,
	.sfp_port[0].sds = 1,
	.sfp_port[0].i2c = I2CBUS( GPIO39_I2C_SDA4, GPIO40_I2C_SCL3_MDC1 ),
	.reset_pin = GPIO54_ACL_BIT2_EN,
	.high_leds = { .mux = LED_27 | LED_29, .enable = LED_28_SYS | LED_29 },
	.port_led_set = { 0, 0, 0, 0, 0, 0, 0, 0, 1 },
	.led_sets = {
		{   /* Set 0 for RJ45 connectors */
			/* LED0: Right Green */
			LEDS_2G5 | LEDS_1G | LEDS_100M | LEDS_10M | LEDS_LINK | LEDS_ACT,
			/* LED1: Left Orange */
			LEDS_2G5 | LEDS_LINK,
			/* LED2: Left Green */
			LEDS_1G | LEDS_LINK,
			/* LED3: None */
			0,
		}, { /* Set 1 for SFP port */
			/* LED0: Single Green "9" LED */
			LEDS_2G5 | LEDS_TWO_PAIR_1G | LEDS_1G | LEDS_500M | LEDS_100M | LEDS_10M | LEDS_LINK | LEDS_ACT | LEDS_10G | LEDS_TWO_PAIR_5G | LEDS_5G | LEDS_TWO_PAIR_2G5,
			/* LED1: D23 LED on PCB */
			LEDS_1G | LEDS_LINK,
			/* LED2: D22 LED on PCB */
			LEDS_2G5 | LEDS_LINK,
			/* LED3: Unused */
			LEDS_COL | LEDS_DUPLEX,
		}, { /* Set 2: Unused, but set to same things as stock firmware for consistency */
			LEDS_100M | LEDS_10M | LEDS_LINK | LEDS_ACT,
			LEDS_1G | LEDS_LINK | LEDS_ACT,
			LEDS_2G5 | LEDS_LINK | LEDS_ACT | LEDS_5G,
			LEDS_10G | LEDS_ACT | LEDS_LINK,
		}, { /* Set 3: Unused, but set to same things as stock firmware for consistency */
			LEDS_TX,
			LEDS_RX,
			LEDS_2G5 | LEDS_TWO_PAIR_1G | LEDS_1G | LEDS_500M | LEDS_100M | LEDS_10M | LEDS_ACT | LEDS_10G | LEDS_TWO_PAIR_5G | LEDS_5G | LEDS_TWO_PAIR_2G5,
			LEDS_2G5 | LEDS_TWO_PAIR_1G | LEDS_1G | LEDS_500M | LEDS_100M | LEDS_10M | LEDS_LINK | LEDS_10G | LEDS_TWO_PAIR_5G | LEDS_5G | LEDS_TWO_PAIR_2G5,
		},
	},
};
