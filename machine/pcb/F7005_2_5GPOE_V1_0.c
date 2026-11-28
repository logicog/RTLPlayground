#define MACHINE_CHIPSET RTL8372
#include "machine_defs.h"

/*
 * NPA AP-7024-2.5G on PCB F7005-2.5GPOE-V1.0 (RTL8372N, 4 x 2.5G RJ45 + 2 x SFP+).
 * `regget 4` reports 0x83727000 and the boot console prints "Detecting CPU:
 * RTL8372N"; MACHINE_CHIPSET selects the RTL8372 family, the N variant is
 * detected at boot.
 *
 * Physical RJ45 1-4 map to logical 4-7.  The left SFP+ cage is front-panel
 * port 5 on SDS1 (logical 8), the right one port 6 on SDS0 (logical 3).
 * Neither cage has a LOS or a TX-disable pin wired, link state comes from the
 * SerDes.
 *
 * Unlike the other 4+2 boards the two SFP EEPROMs sit on separate I2C buses,
 * measured on the board:
 *   left cage:  SDA0 = GPIO47, SCL0 = GPIO46
 *   right cage: SDA1 = GPIO49, SCL1 = GPIO48
 *
 * The T25S40 fitted here cannot be read in DIO mode, so the definition sets
 * flash_sio and flash_init() uses single-IO reads.
 *
 * The LED wiring is the one of the FG-4GT-2SX family; the stock firmware's
 * LED global enable value is restored in the init hook below.
 */

__code const struct machine machine = {
	MACHINE_COMMON_SETTING,
	.n_sfp = 2,
	.log_to_phys_port = {0, 0, 0, 6, 1, 2, 3, 4, 5},
	.phys_to_log_port = {4, 5, 6, 7, 8, 3, 0, 0, 0},
	.is_sfp = {0, 0, 0, 2, 0, 0, 0, 0, 1},

	// Left SFP+ cage, front-panel port 5 -> logical port 8
	.sfp_port[0].pin_detect = GPIO38,
	.sfp_port[0].pin_los = GPIO_NA,
	.sfp_port[0].pin_tx_disable = GPIO_NA,
	.sfp_port[0].sds = 1,
	.sfp_port[0].i2c = I2CBUS( GPIO47_I2C_SDA0, GPIO46_I2C_SCL0 ),

	// Right SFP+ cage, front-panel port 6 -> logical port 3
	.sfp_port[1].pin_detect = GPIO37,
	.sfp_port[1].pin_los = GPIO_NA,
	.sfp_port[1].pin_tx_disable = GPIO_NA,
	.sfp_port[1].sds = 0,
	.sfp_port[1].i2c = I2CBUS( GPIO49_I2C_SDA1, GPIO48_I2C_SCL1 ),

	.reset_pin = GPIO_NA,		// no reset button fitted
	.flash_sio = 1,			// T25S40, not readable in DIO mode
	.high_leds = { .mux = LED_27 | LED_28_SYS | LED_29, .enable = LED_28_SYS | LED_29 },
	.port_led_set = { 0, 0, 0, 1, 0, 0, 0, 0, 1},
	/* Ports 1-4 RJ45 use set 0, ports 5-6 SFP use set 1
	 * Ports 1-4: Green: 2.5GBit, Amber: 10/100/1000MBit
	 * Ports 5-6: Green: 100MBit-10GBit
	 */
	.led_sets = {
			{
				LEDS_2G5 | LEDS_LINK | LEDS_ACT,
				LEDS_1G | LEDS_100M | LEDS_10M | LEDS_LINK | LEDS_ACT,
				0,
				LEDS_2G5 | LEDS_LINK | LEDS_ACT
			},
			{
				LEDS_10G | LEDS_5G | LEDS_2G5 | LEDS_1G | LEDS_100M | LEDS_10M | LEDS_LINK | LEDS_ACT,
				LEDS_10G | LEDS_LINK,
				0,
				LEDS_COL | LEDS_DUPLEX
			},
			{
				LEDS_1G | LEDS_100M | LEDS_10M | LEDS_LINK | LEDS_ACT,
				LEDS_2G5 | LEDS_1G | LEDS_LINK,
				LEDS_5G | LEDS_2G5 | LEDS_LINK | LEDS_ACT,
				LEDS_10G | LEDS_LINK | LEDS_ACT
			},
			{
				LEDS_TX,
				LEDS_RX,
				LEDS_10G | LEDS_TWO_PAIR_5G | LEDS_5G | LEDS_TWO_PAIR_2G5 |
					LEDS_2G5 | LEDS_TWO_PAIR_1G | LEDS_1G | LEDS_500M | LEDS_100M | LEDS_10M | LEDS_ACT,
				LEDS_10G | LEDS_TWO_PAIR_5G | LEDS_5G | LEDS_TWO_PAIR_2G5 |
					LEDS_2G5 | LEDS_TWO_PAIR_1G | LEDS_1G | LEDS_500M | LEDS_100M | LEDS_10M | LEDS_LINK
			},
	 },
	.led_mux_custom = 1,
	.led_mux = {
				0x0c, 0x0d, 0x0e, 0x10, 0x11, 0x12, 0x14, 0x3f, 0x15, 0x16,
				0x18, 0x0e, 0x19, 0x11, 0x12, 0x1a, 0x15, 0x16, 0x1c, 0x19,
				0x1a, 0x1d, 0x1d, 0x1e, 0x1e, 0x20, 0x21, 0x22
		},
	};

MACHINE_CUSTOM_INIT(
	/* Stock value for LED_GLB_IO_EN; without it the solid LED conditions do
	 * not drive the port lamps. */
	REG_SET(RTL837X_REG_LED_GLB_IO_EN, 0x7624155b);
)
