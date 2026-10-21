#include "machine_defs.h"

__code const struct machine machine = {
	MACHINE_COMMON_SETTING,
	.n_sfp = 2,
	/* Same 2M-PCB43-V1.1 board family as the KP-9000-6XH-X2 (see
	 * machine/KP_9000_6XHML_X2.c), but the two SFP+ cages (physical ports 5 and
	 * 6) are wired the other way round: physical 5 is on SDS1, physical 6 on
	 * SDS0. Copper ports 1-4 match the KP-9000 layout. */
	.log_to_phys_port = {0, 0, 0, 6, 1, 2, 3, 4, 5},
	.phys_to_log_port = {4, 5, 6, 7, 8, 3, 0, 0, 0},
	.is_sfp = {0, 0, 0, 2, 0, 0, 0, 0, 1},
	// Left SFP cage (physical port 5) = SDS1
	.sfp_port[0].pin_detect = GPIO30_ACL_BIT3_EN,
	.sfp_port[0].pin_los = GPIO37,
	.sfp_port[0].pin_tx_disable = GPIO_NA,
	.sfp_port[0].sds = 1,
	.sfp_port[0].i2c = I2CBUS( GPIO39_I2C_SDA4, GPIO40_I2C_SCL3_MDC1 ),
	// Right SFP cage (physical port 6) = SDS0
	.sfp_port[1].pin_detect = GPIO50_I2C_SCL2_UART1_TX,
	.sfp_port[1].pin_los = GPIO10_LED10,
	.sfp_port[1].pin_tx_disable = GPIO_NA,
	.sfp_port[1].sds = 0,
	.sfp_port[1].i2c = I2CBUS( GPIO41_I2C_SDA3_MDIO1, GPIO40_I2C_SCL3_MDC1 ),
	/* The board has a 3-hole reset button footprint, but it is unpopulated and
	 * the case has no opening for it, so reset_pin stays GPIO_NA. GPIO54, the
	 * reset pin on the KP-9000 layout, reads as permanently pressed here. */
	.reset_pin = GPIO_NA,
	/* The fitted T25S40 does not work in DIO mode on this board, so read the flash
	 * with single-IO; see doc/devices/2G5F4_10G2_V1.01.md. */
	.flash_sio = 1,
	.high_leds = { .mux = LED_27 | LED_29, .enable = LED_28_SYS | LED_29 },
	.port_led_set = { 0, 0, 0, 0, 0, 0, 0, 0, 0},
	/* Conditions for LED on:
	 * dual led orange: ledset_0 & ledset_2
	 * dual led green: ledset_2 & !ledset_0
	 * single right led green: ledset_0 & !ledset_1
	 *
	 * Measured on this board: LED-ID0 is the copper blue LED, LED-ID1 the
	 * copper white one (setting LED-ID0 to 0 killed blue while leaving white on
	 * at 2.5G only) and LED-ID2 carries the SFP single LED. The "solid"
	 * conditions (LINK/speed) barely drive the LEDs here and ACT is what
	 * actually lights them, which is why the white LED used to glow faintly at
	 * non-2.5G speeds; the fix for that is LED_GLB_IO_EN bit 6, set in
	 * machine_init.c. */
	.led_sets = { { LEDS_2G5 | LEDS_1G | LEDS_100M | LEDS_10M | LEDS_LINK | LEDS_ACT | LEDS_10G,
			LEDS_2G5 | LEDS_LINK | LEDS_10G,
			LEDS_1G | LEDS_LINK,
			0 },
		    },
};
