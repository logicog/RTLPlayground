#define MACHINE_CHIPSET RTL8373
#include "machine_defs.h"

/*
 * PCB-AFG-SWTG118ANS-V1.2-17459 (RealHD PSW8-25MG / ZX-AFGW-SWTG218ANS-150,
 * boards shipped with OEM firmware V100.x). Same switch as V2.0.2_23308 but a
 * different LED wiring.
 *
 * LED configuration taken from the OEM V100.9.9 LED init routine:
 *   LED1_0_SET0 = 0x01410141, LED3_2_SET0 = 0x01410174, LED3_0_SET1[7:0] = 0
 *   LED1_0_SET1 = 0x00000140, LED3_2_SET1[15:0] = 0x0155, LED3_0_SET1[19:16] = 0xf
 *   PORT_SET_SEL[17:16] = 1 (SFP port uses set 1)
 *   PIN_MUX_0 bits 27, 29 = 1; LED_GLB_IO_EN bit 29 = 1, bit 27 = 0
 *   LED_GLB_MUX_1..6 are NOT written: hardware default pad mux.
 * Everything else (ports, SFP, reset pin, MAC offset) as V2.0.2_23308.
 */
__code const struct machine machine = {
	MACHINE_COMMON_SETTING,
	.mac_flash_offset = 0x1FC000,
	.n_sfp = 1,
	.log_to_phys_port = {1, 2, 3, 4, 5, 6, 7, 8, 9},
	.phys_to_log_port = {0, 1, 2, 3, 4, 5, 6, 7, 8},
	.is_sfp = {0, 0, 0, 0, 0, 0, 0, 0, 1},
	.sfp_port[0].pin_detect = GPIO38,
	.sfp_port[0].pin_los = GPIO_NA,
	.sfp_port[0].pin_tx_disable = GPIO_NA,
	.sfp_port[0].sds = 1,
	.sfp_port[0].i2c = I2CBUS( GPIO39_I2C_SDA4, GPIO40_I2C_SCL3_MDC1 ),
	.reset_pin = GPIO48_I2C_SCL1,
	// OEM: pads 27 and 29 muxed to LED, only 29 enabled. The SYS LED (28) is
	// left as RTLPlayground drives it on the V2.0.2 board.
	.high_leds = { .mux = LED_27 | LED_28_SYS | LED_29, .enable = LED_28_SYS | LED_29 },
	.port_led_set = { 0, 0, 0, 0, 0, 0, 0, 0, 1},
	.led_mux_custom = 0,
	.led_sets = { { LEDS_2G5 | LEDS_LINK | LEDS_ACT,			// LED0 0x0141
			LEDS_2G5 | LEDS_LINK | LEDS_ACT,			// LED1 0x0141
			LEDS_1G | LEDS_100M | LEDS_10M | LEDS_LINK | LEDS_ACT,	// LED2 0x0174
			LEDS_2G5 | LEDS_LINK | LEDS_ACT				// LED3 0x0141
		      },
		      { LEDS_10G | LEDS_TWO_PAIR_5G | LEDS_5G | LEDS_TWO_PAIR_2G5
			| LEDS_LINK | LEDS_ACT,					// LED0 0xf0140
			0,							// LED1
			LEDS_2G5 | LEDS_1G | LEDS_100M | LEDS_LINK | LEDS_ACT,	// LED2 0x0155
			0							// LED3
		      },
		},
};

/*
 * The OEM V100.9.9 LED init sets LED_MODE (0x6520) bits [12:9] = 0 and
 * [8:5] = 7, where RTLPlayground's default has 3 and 5. LED_GLB_IO_EN is left
 * at its default here (the V2.0.2 board overwrites it).
 */
MACHINE_CUSTOM_INIT(
	reg_read_m(RTL837X_REG_LED_MODE);
	sfr_mask_data(0, 0xe0, 0xe0);	// bits 7:5 = 1 (with bit 8 = 0: field [8:5] = 7)
	sfr_mask_data(1, 0x1f, 0x00);	// bit 8 = 0, bits 12:9 = 0
	reg_write_m(RTL837X_REG_LED_MODE);
)
