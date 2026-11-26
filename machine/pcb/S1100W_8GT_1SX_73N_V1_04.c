#define MACHINE_CHIPSET RTL8373
#include "machine_defs.h"

__code const struct machine machine = {
	MACHINE_COMMON_SETTING,
	.n_sfp = 1,
	.log_to_phys_port = {1, 2, 3, 4, 5, 6, 7, 8, 9},
	.phys_to_log_port = {0, 1, 2, 3, 4, 5, 6, 7, 8},
	.is_sfp = {0, 0, 0, 0, 0, 0, 0, 0, 2},
	.sfp_port[0].pin_detect = GPIO38,
	.sfp_port[0].pin_los = GPIO37,
	.sfp_port[0].pin_tx_disable = GPIO_NA,
	.sfp_port[0].sds = 1,
	.sfp_port[0].i2c = I2CBUS( GPIO39_I2C_SDA4, GPIO40_I2C_SCL3_MDC1 ),
	.reset_pin = GPIO48_I2C_SCL1,
	.high_leds = { .mux = LED_27 | LED_28_SYS | LED_29, .enable = LED_28_SYS | LED_29 },
	.port_led_set = { 0, 0, 0, 0, 0, 0, 0, 0, 1 },

    .led_sets = {
        { /* Set 0: RJ45 copper ports (index 0 = Yellow, index 1 = Green) */
            LEDS_2G5 | LEDS_LINK,                                                /* Yellow: solid on 2.5G */
            LEDS_2G5 | LEDS_1G | LEDS_100M | LEDS_10M | LEDS_LINK | LEDS_ACT,    /* Green: link + act all speeds */
            0,
            0
        },
        { /* Set 1: SFP+ port */
            LEDS_10G | LEDS_2G5 | LEDS_1G | LEDS_LINK | LEDS_ACT,
            0,
            0,
            0
        },
        { 0, 0, 0, 0 },
        { 0, 0, 0, 0 }
    },

	.led_mux_custom = 1,
    .led_mux = {
        0x00, 0x01,             /* GPIO 0-1:   Port 1 (col 1, col 2) */
        0x04, 0x05,             /* GPIO 2-3:   Port 2 (col 1, col 2) */
        0x08, 0x09,             /* GPIO 4-5:   Port 3 (col 1, col 2) */
        0x0c, 0x3f, 0x0d,       /* GPIO 6, 8:  Port 4 (GPIO 7 unused) */
        0x10, 0x11, 0x3f,       /* GPIO 9-10:  Port 5 (GPIO 11 unused) */
        0x14, 0x3f, 0x3f, 0x15, /* GPIO 12,15: Port 6 (GPIO 13-14 unused) */
        0x3f, 0x3f,             /* GPIO 16-17: unused */
        0x18, 0x3f, 0x3f, 0x19, /* GPIO 18,21: Port 7 (GPIO 19-20 unused) */
        0x3f, 0x3f,             /* GPIO 22-23: unused */
        0x1c, 0x1d,             /* GPIO 24-25: Port 8 (col 1, col 2) */
        0x20, 0x3f              /* GPIO 26:    Port 9 SFP (GPIO 27 unused) */
    },
};

MACHINE_CUSTOM_INIT(
    /* Set SYS_LED (pin 28) to active-high so Power LED stays ON after boot */
    reg_read_m(RTL837X_REG_LED_GLB_ACTIVE);
    sfr_mask_data(3, 0x10, 0);
    reg_write_m(RTL837X_REG_LED_GLB_ACTIVE);
)