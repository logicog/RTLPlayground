#include "machine_defs.h"

/*
 * POE Plus S604GTP-2SX-SL  (4x copper PoE + 2x SFP+, RTL8372)
 *
 * Starting point:
 *   - Copper ports / LEDs: taken from HASIVO_S600W_4GT_2XGT_SE (copper LEDs confirmed working on this board)
 *   - SFP cages:           port/SerDes/I2C layout from FG_4GT_2SX_V2_0, GPIOs measured on this board
 *
 * SFP GPIOs found with the `gpio` console command (detect and LOS are active low):
 *   left cage:  detect GPIO30, LOS GPIO37
 *   right cage: detect GPIO50, no LOS pin (link state comes from the SerDes)
 */

__code const struct machine machine = {
	.machine_name = MACHINE_MODEL,
	.isRTL8373 = 0,
	.min_port = 3,
	.max_port = 8,
	.n_sfp = 2,
	.n_10g = 0,		// SerDes drive the SFP cages directly, no external 10G PHY
	.log_to_phys_port = {0, 0, 0, 6, 1, 2, 3, 4, 5},
	.phys_to_log_port = {4, 5, 6, 7, 8, 3, 0, 0, 0},
	.is_sfp = {0, 0, 0, 2, 0, 0, 0, 0, 1},

	// Left SFP cage (front-panel port 5) -> logical port 8, SerDes 1
	.sfp_port[0].pin_detect = GPIO30_ACL_BIT3_EN,	// verified: GPIO 0 bit 30 low = module present
	.sfp_port[0].pin_los = GPIO37,			// verified: GPIO 1 bit 5 low = signal OK
	.sfp_port[0].pin_tx_disable = GPIO_NA,
	.sfp_port[0].sds = 1,
	.sfp_port[0].i2c = I2CBUS( GPIO39_I2C_SDA4, GPIO40_I2C_SCL3_MDC1 ),

	// Right SFP cage (front-panel port 6) -> logical port 3, SerDes 0
	.sfp_port[1].pin_detect = GPIO50_I2C_SCL2_UART1_TX,	// verified: GPIO 1 bit 18 low = module present
	.sfp_port[1].pin_los = GPIO_NA,			// none found: GPIO51 never changes, GPIO9/10 are this cage's LED pads
	.sfp_port[1].pin_tx_disable = GPIO_NA,
	.sfp_port[1].sds = 0,
	.sfp_port[1].i2c = I2CBUS( GPIO41_I2C_SDA3_MDIO1, GPIO40_I2C_SCL3_MDC1 ),

	.reset_pin = GPIO54_ACL_BIT2_EN,		// verified: GPIO 1 bit 22 low = button pressed

	// LED config from the Hasivo profile, which lights the copper LEDs on this board
	.high_leds = { .mux = LED_28_SYS | LED_29, .enable = LED_28_SYS | LED_29 },
	.port_led_set = { 0, 0, 0, 1, 0, 0, 0, 0, 1 },
	.led_sets = {
		{	/* copper ports 1-4 */
			LEDS_2G5 | LEDS_1G | LEDS_100M | LEDS_10M | LEDS_LINK | LEDS_ACT,
			LEDS_2G5 | LEDS_LINK,
			LEDS_1G | LEDS_100M | LEDS_10M | LEDS_LINK,
			0,
		},
		{	/* SFP ports 5-6 */
			LEDS_10G | LEDS_5G | LEDS_2G5 | LEDS_1G | LEDS_100M | LEDS_10M | LEDS_LINK | LEDS_ACT,
			LEDS_10G | LEDS_LINK,
			LEDS_5G | LEDS_2G5 | LEDS_1G | LEDS_LINK,
			0,
		},
	},
	/* LED pad mux: value = logical_port * 4 + led_id, 0x3f = unused
	 * pads 12-23: copper ports 4-7 (known good)
	 * pads 24-27: port 8 = left cage (link seen on pads 25/26)
	 * pads 8-11:  port 3 = right cage
	 * (Hasivo had ports 3 and 8 the other way round on these pads) - physical SFP LEDs unverified */
	.led_mux_custom = 1,
	.led_mux = {
		0x3f, 0x3f, 0x3f, 0x3f, 0x3f,
		0x3f, 0x3f, 0x3f, 0x23, 0x0c,
		0x0d, 0x0e, 0x10, 0x11, 0x12,
		0x14, 0x15, 0x16, 0x18, 0x19,
		0x1a, 0x1c, 0x1d, 0x1e, 0x20,
		0x21, 0x22, 0x0f
	},
};
