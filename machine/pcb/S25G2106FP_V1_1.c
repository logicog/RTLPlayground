#include "machine_defs.h"

/*
 * Sirivision S25G2106FP V1.1: 5 x 2.5G RJ45 + 1 x SFP+ (RTL8372)
 * Sold as VIMIN VM-S250501M and Nicgiga S25-0501-M.
 *
 * Derived from S25G2206FP_V1_1 (4+2). The left SFP+ cage of the 4+2
 * (chip port 8 / SDS1) is replaced by an RTL8221B PHY (U3) driving front
 * port 5. The OEM firmware sets up SMI MAC type, polling and the MDIO
 * address for chip port 8 before running its RTL8221B init, so the PHY is
 * on port 8 / SDS1 here, not on port 3 / SDS0 as on the other 5+1 boards.
 * The remaining SFP+ cage is the 4+2's right cage (J2) on port 3 / SDS0.
 * Front ports: 1-4 internal PHYs, 5 RTL8221B, 6 SFP+.
 */
__code const struct machine machine = {
	MACHINE_COMMON_SETTING,
	.mac_flash_offset = 0x1FC000,
	.n_sfp = 1,
	.phy8221_p8 = 1,
	.log_to_phys_port = {0, 0, 0, 6, 1, 2, 3, 4, 5},
	.phys_to_log_port = {4, 5, 6, 7, 8, 3, 0, 0, 0},
	.is_sfp = {0, 0, 0, 1, 0, 0, 0, 0, 0},
	// SFP+ cage on SDS0 / logical port 3 (pins of the S25G2206FP right cage J2)
	.sfp_port[0].pin_detect = GPIO50_I2C_SCL2_UART1_TX,
	.sfp_port[0].pin_los = GPIO51_I2C_SDA2_UART1_RX,
	.sfp_port[0].pin_tx_disable = GPIO_NA,
	.sfp_port[0].sds = 0,
	.sfp_port[0].i2c = I2CBUS( GPIO41_I2C_SDA3_MDIO1, GPIO40_I2C_SCL3_MDC1 ),
	.reset_pin = GPIO36_PWM_OUT,
	.high_leds = { .mux = LED_27 | LED_29, .enable = LED_28_SYS | LED_29 },
	.port_led_set = { 0, 0, 0, 1, 0, 0, 0, 0, 0},
	.led_sets = {
		{ /* RJ45: Green LED */
			LEDS_1G | LEDS_100M | LEDS_10M | LEDS_LINK | LEDS_ACT | LEDS_10G,
			0,
			/* Amber LED */
			LEDS_2G5 | LEDS_LINK | LEDS_ACT,
			0
		}, { /* SFP PORT: SINGLE GREEN LED */
			LEDS_10M | LEDS_100M | LEDS_1G | LEDS_2G5 | LEDS_10G | LEDS_LINK | LEDS_ACT,
			0,
			0,
			0,
		},
	},
};
