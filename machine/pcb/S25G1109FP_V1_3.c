#include "machine_defs.h"

// Sirivision S25G1109FP V1.3 (label: SR-S25G2109F): 8x2.5G RJ45 + 1x10G SFP+,
// 2 MB flash, RTL8373 (non-N) + RTL8224.  Derived from the KP-9000-9XHML-X V3.1
// definition; every value below was confirmed on hardware, see the device doc
// in doc/devices/S25G1109FP_V1_3.md.
__code const struct machine machine = {
	MACHINE_COMMON_SETTING,
	.n_sfp = 1,
	.log_to_phys_port = {1, 2, 3, 4, 5, 6, 7, 8, 9},
	.phys_to_log_port = {0, 1, 2, 3, 4, 5, 6, 7, 8},
	.is_sfp = {0, 0, 0, 0, 0, 0, 0, 0, 1},
	/* ModAbs is GPIO30 (low with a module inserted, high with an empty cage),
	 * LOS is GPIO37 (high without light).  GPIO38, the ModAbs pin of the
	 * KP-9000 8+1 reference, is tied low here and never changes, which made the
	 * firmware believe a module was always present. */
	.sfp_port[0].pin_detect = GPIO30_ACL_BIT3_EN,
	.sfp_port[0].pin_los = GPIO37,
	.sfp_port[0].pin_tx_disable = GPIO_NA,
	.sfp_port[0].sds = 1,
	.sfp_port[0].i2c = I2CBUS( GPIO39_I2C_SDA4, GPIO40_I2C_SCL3_MDC1 ),
	/* Reset button: GPIO54 reads high when idle and low while pressed.
	 * GPIO48, used by the KP-9000 reference, never changes on this board. */
	.reset_pin = GPIO54_ACL_BIT2_EN,
	.high_leds = { .mux = LED_27 | LED_29, .enable = LED_28_SYS | LED_29 },
	.port_led_set = { 0, 0, 0, 0, 0, 0, 0, 0, 1},
	.led_sets = {
		{   /* RJ45: left LED = link/activity, right LED = 2.5G */
			LEDS_2G5 | LEDS_1G | LEDS_100M | LEDS_10M | LEDS_LINK | LEDS_ACT,
			/* speed-only conditions leave the LED dark: the LINK bit has to be
			 * set as well, otherwise the LED controller stays off */
			LEDS_2G5 | LEDS_LINK,
			0,
			0,
		}, { /* SFP PORT, SINGLE GREEN LED */
			LEDS_2G5 | LEDS_1G | LEDS_100M | LEDS_10M | LEDS_LINK | LEDS_ACT | LEDS_10G,
			0,
			0,
			0,
		}},
	/* 6 bits per LED pin, encoded as (port << 2) | led.  This is the chip
	 * default for port-ordered wiring (pin 3n+m = port n+1, function m) with
	 * one change: pin 25 drives the SFP LED with function 0, the only function
	 * populated in led_sets[1].  See the device doc for the details. */
	.led_mux_custom = 1,
	.led_mux = {0x00, 0x01, 0x02, 0x04, 0x05, 0x06, 0x08, 0x09, 0x0a, 0x0c,
				0x0d, 0x0e, 0x10, 0x11, 0x12, 0x14, 0x15, 0x16, 0x18, 0x19,
				0x1a, 0x1c, 0x1d, 0x1e, 0x20, 0x20, 0x22, 0x23},
	/* Factory MAC at flash 0x1FC000, verified in the stock dump.  The stock
	 * value is a locally administered address, which is rejected, so a
	 * UUID-derived MAC is used unless a valid one is stored here. */
	.mac_flash_offset = 0x1FC000,
};
