#include "machine_defs.h"

/*
 * Xiaomi SW-106T-A (PCB: SW-106T-A_XM)
 *
 * 4 x 2.5GBASE-T RJ45 + 2 x SFP+ cage, RTL8372N, 32 Mbit SPI NOR flash.
 *
 * Port mapping measured on the board:
 *   - physical ports 1-4 -> logical ports 4-7
 *   - physical port 5 (left SFP+ cage)  -> logical port 8, SDS1, detect GPIO38
 *   - physical port 6 (right SFP+ cage) -> logical port 3, SDS0, detect GPIO37
 *   - no LOS and no TX-disable pin is wired on either cage, link state comes
 *     from the SerDes
 *   - the reset button footprint is unpopulated, so reset_pin stays GPIO_NA
 *
 * The copper PHYs of this board need MDI pair polarity 0x03 instead of the
 * 0x0c reset default, otherwise ports 1 and 2 never link:
 * see .mdi_reverse below.
 *
 * LED wiring is not the stock mux, so .led_mux is set explicitly. LEDs are
 * disabled while the port has no link; copper ports use one LED each and the
 * SFP+ cages use one LED each. Verified on hardware.
 */

__code const struct machine machine = {
	MACHINE_COMMON_SETTING,
	.n_sfp = 2,		// SerDes drive the SFP cages directly, no external 10G PHY
	.log_to_phys_port = {0, 0, 0, 6, 1, 2, 3, 4, 5},
	.phys_to_log_port = {4, 5, 6, 7, 8, 3, 0, 0, 0},
	.is_sfp = {0, 0, 0, 2, 0, 0, 0, 0, 1},

	// Left SFP+ cage (physical port 5) -> logical port 8, SerDes 1
	.sfp_port[0].pin_detect = GPIO38,
	.sfp_port[0].pin_los = GPIO_NA,
	.sfp_port[0].pin_tx_disable = GPIO_NA,
	.sfp_port[0].sds = 1,
	.sfp_port[0].i2c = I2CBUS( GPIO39_I2C_SDA4, GPIO40_I2C_SCL3_MDC1 ),

	// Right SFP+ cage (physical port 6) -> logical port 3, SerDes 0
	.sfp_port[1].pin_detect = GPIO37,
	.sfp_port[1].pin_los = GPIO_NA,
	.sfp_port[1].pin_tx_disable = GPIO_NA,
	.sfp_port[1].sds = 0,
	.sfp_port[1].i2c = I2CBUS( GPIO41_I2C_SDA3_MDIO1, GPIO40_I2C_SCL3_MDC1 ),

	.reset_pin = GPIO_NA,

	// Copper ports 1 and 2 do not link with the reset default (0x0c)
	.mdi_reverse = 0x03,

	// The fitted flash works in DIO mode, so flash_sio stays 0.

	.high_leds = { .mux = LED_28_SYS | LED_29, .enable = LED_28_SYS | LED_29 },
	.port_led_set = { 0, 0, 0, 1, 0, 0, 0, 0, 1 },
	.led_sets = {
		{	/* Copper ports 1-4: single LED per port, link/activity */
			LEDS_2G5 | LEDS_1G | LEDS_100M | LEDS_10M | LEDS_LINK | LEDS_ACT,
			LEDS_2G5 | LEDS_LINK,
			LEDS_1G | LEDS_100M | LEDS_10M | LEDS_LINK,
			0,
		},
		{	/* SFP+ cages 5-6: single LED per cage, link/activity */
			LEDS_10G | LEDS_5G | LEDS_2G5 | LEDS_1G | LEDS_100M | LEDS_10M | LEDS_LINK | LEDS_ACT,
			LEDS_10G | LEDS_LINK,
			LEDS_5G | LEDS_2G5 | LEDS_1G | LEDS_LINK,
			0,
		},
	},
	/* LED pad mux: value = logical port * 4 + LED id, 0x3f = unused.
	 * pads 12-23: copper ports 4-7
	 * pads 24-27: logical port 8 (left SFP+ cage)
	 * pads 8-11:  logical port 3 (right SFP+ cage)
	 */
	.led_mux_custom = 0,
	.led_mux = {
		0x3f, 0x3f, 0x3f, 0x3f, 0x3f,
		0x3f, 0x3f, 0x3f, 0x23, 0x0c,
		0x0d, 0x0e, 0x10, 0x11, 0x12,
		0x14, 0x15, 0x16, 0x18, 0x19,
		0x1a, 0x1c, 0x1d, 0x1e, 0x20,
		0x21, 0x22, 0x0f
	},
};