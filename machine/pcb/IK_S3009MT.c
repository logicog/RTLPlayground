#include "machine_defs.h"

__code const struct machine machine = {
	MACHINE_COMMON_SETTING,
	.mac_flash_offset = 0x1FC000,
	.n_sfp = 1,
	.log_to_phys_port = { 1, 2, 3, 4, 5, 6, 7, 8, 9 },
	.phys_to_log_port = { 0, 1, 2, 3, 4, 5, 6, 7, 8 },
	.is_sfp = { 0, 0, 0, 0, 0, 0, 0, 0, 1 },

	.sfp_port[0].pin_detect = GPIO38,
	.sfp_port[0].pin_los = GPIO_NA,
	.sfp_port[0].pin_tx_disable = GPIO_NA,
	.sfp_port[0].sds = 1,
	.sfp_port[0].i2c = I2CBUS( GPIO39_I2C_SDA4, GPIO40_I2C_SCL3_MDC1 ),

	.reset_pin = GPIO_NA,
	.flash_sio = 1,
	.high_leds = { .mux = LED_27 | LED_28_SYS | LED_29, .enable = LED_27 | LED_28_SYS | LED_29 },
	.port_led_set = { 0, 0, 0, 0, 0, 0, 0, 0, 1 },

	.led_sets = {
		{   /* Set 0 (RJ45):
		     * 2.5G: Green (link + act)
		     * 1000M / 100M / 10M: Yellow (link + act)
		     */
			LEDS_2G5 | LEDS_LINK | LEDS_ACT,
			LEDS_2G5 | LEDS_LINK | LEDS_ACT,
			LEDS_TWO_PAIR_1G | LEDS_1G | LEDS_500M | LEDS_100M | LEDS_10M | LEDS_LINK | LEDS_ACT,
			LEDS_TWO_PAIR_1G | LEDS_1G | LEDS_500M | LEDS_100M | LEDS_10M | LEDS_LINK | LEDS_ACT,
		},
		{   /* Set 1 (SFP+):
		     * 1000M lamp on channels 0, 2 (Pad 26)
		     * 10G lamp on channels 1, 3 (Pad 27)
		     */
			LEDS_TWO_PAIR_1G | LEDS_1G | LEDS_500M | LEDS_100M | LEDS_10M | LEDS_LINK | LEDS_ACT,
			LEDS_10G | LEDS_5G | LEDS_2G5 | LEDS_LINK | LEDS_ACT,
			LEDS_TWO_PAIR_1G | LEDS_1G | LEDS_500M | LEDS_100M | LEDS_10M | LEDS_LINK | LEDS_ACT,
			LEDS_10G | LEDS_5G | LEDS_2G5 | LEDS_LINK | LEDS_ACT,
		},
	},
	.led_mux_custom = 0,
};
