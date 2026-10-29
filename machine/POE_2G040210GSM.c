#include "machine_defs.h"

/*
 * MokerLink POE-2G040210GSM, PCB 2M-PCB49-V1.1: 4 x 2.5G PoE + 2 x 10G SFP+,
 * RTL8372. Started from the non-PoE 2G040210GSM (2M-PCB43-V1.1,
 * KP_9000_6XHML_X2_V1_1); every pin below was checked on this board.
 * See doc/devices/2M-PCB49-V1.1.md. PoE isn't configured.
 */
__code const struct machine machine = {
	MACHINE_COMMON_SETTING,
	// The stock firmware keeps the switch's MAC address here
	.mac_flash_offset = 0x1FC000,
	.n_sfp = 2,
	.log_to_phys_port = {0, 0, 0, 5, 1, 2, 3, 4, 6},
	.phys_to_log_port = {4, 5, 6, 7, 3, 8, 0, 0, 0},
	.is_sfp = {0, 0, 0, 1, 0, 0, 0, 0, 2},
	// SFP pins, seen on this board by moving a module between the cages: module
	// detect is low while a module is in, and the EEPROM answers on the bus
	// listed. No GPIO input changed for LOS, so it isn't used. A fibre
	// between the two cages links at 10G both ways with these SerDes numbers.
	// Left SFP port (5)
	.sfp_port[0].pin_detect = GPIO38,
	.sfp_port[0].pin_los = GPIO_NA,
	.sfp_port[0].pin_tx_disable = GPIO_NA,
	.sfp_port[0].sds = 0,
	.sfp_port[0].i2c = I2CBUS( GPIO39_I2C_SDA4, GPIO40_I2C_SCL3_MDC1 ),
	// Right SFP port (6)
	.sfp_port[1].pin_detect = GPIO37,
	.sfp_port[1].pin_los = GPIO_NA,
	.sfp_port[1].pin_tx_disable = GPIO_NA,
	.sfp_port[1].sds = 1,
	.sfp_port[1].i2c = I2CBUS( GPIO41_I2C_SDA3_MDIO1, GPIO40_I2C_SCL3_MDC1 ),
	// Reset button: GPIO48, low while pressed (seen on this board)
	.reset_pin = GPIO48_I2C_SCL1,
	// LED 28 lights the front's "S" LED, steadily: the system LED states
	// (blinking, off) had no effect on it in a live test
	.high_leds = { .mux = LED_27 | LED_28_SYS | LED_29, .enable = LED_28_SYS | LED_29 },
	// RJ45 ports use set 0, the SFP ports set 1
	.port_led_set = { 0, 0, 0, 1, 0, 0, 0, 0, 1},
	.led_sets = {
		{   /* RJ45: LED 0 amber, LED 1 green */
			LEDS_1G | LEDS_100M | LEDS_10M | LEDS_LINK | LEDS_ACT,
			LEDS_2G5 | LEDS_10G | LEDS_LINK | LEDS_ACT,
			0,
			0,
		}, { /* SFP: single green LED */
			LEDS_2G5 | LEDS_1G | LEDS_100M | LEDS_10M | LEDS_LINK | LEDS_ACT | LEDS_10G,
			0,
			0,
			0,
		}},
	// LED pads, found by lighting them in turn: 9/10 port 1 amber/green, 12/15 port 2,
	// 18/21 port 3, 24/25 port 4, 26 port 5 (left SFP), 6 port 6 (right SFP; enabled
	// in POE_2G040210GSM_init.c)
	.led_mux_custom = 1,
	.led_mux = { 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, // 65e0
		     0x3f, 0x20, 0x3f, 0x3f, 0x10, // 65e4
		     0x11, 0x3f, 0x14, 0x3f, 0x3f, // 65e8
		     0x15, 0x3f, 0x3f, 0x18, 0x3f, // 65ec
		     0x3f, 0x19, 0x3f, 0x3f, 0x1c, // 65f0
		     0x1d, 0x0c, 0x3f },
};
