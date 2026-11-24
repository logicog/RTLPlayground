#define MACHINE_CHIPSET RTL8372
#include "machine_defs.h"

/*
 * Xiaomi SW-106T-A_XM (RTL8372N, 4 x 2.5G RJ45 + 2 x SFP+).
 * Copper PHYs need MDI pair polarity 0x03 on 0xa90 (see .mdi_reverse);
 * with the reset default physical ports 1 and 2 never link.
 *
 * Physical RJ45 1-4 map to logical 4-7; the left SFP+ cage is logical 8
 * (SDS1, detect GPIO38) and the right one logical 3 (SDS0, detect GPIO37).
 */

__code const struct machine machine = {
    MACHINE_COMMON_SETTING,
    .n_sfp = 2,
    .log_to_phys_port = {0, 0, 0, 6, 1, 2, 3, 4, 5},
    .phys_to_log_port = {4, 5, 6, 7, 8, 3, 0, 0, 0},
    .is_sfp = {0, 0, 0, 2, 0, 0, 0, 0, 1},

    // SFP port on SDS0 / logical port 3
    .sfp_port[0].pin_detect = GPIO38,
    .sfp_port[0].pin_los = GPIO_NA,
    .sfp_port[0].pin_tx_disable = GPIO_NA,
    .sfp_port[0].sds = 1,
    .sfp_port[0].i2c = I2CBUS(GPIO39_I2C_SDA4, GPIO40_I2C_SCL3_MDC1),

    .sfp_port[1].pin_detect = GPIO37,
    .sfp_port[1].pin_los = GPIO_NA,
    .sfp_port[1].pin_tx_disable = GPIO_NA,
    .sfp_port[1].sds = 0,
    .sfp_port[1].i2c = I2CBUS(GPIO41_I2C_SDA3_MDIO1, GPIO40_I2C_SCL3_MDC1),

    .reset_pin = GPIO_NA,
    .flash_sio = 0,
    .mdi_reverse = 0x03,
    .high_leds = { .mux = LED_28_SYS | LED_29, .enable = LED_27 | LED_28_SYS | LED_29 },
    .port_led_set = { 0, 0, 0, 3, 2, 2, 0, 0, 3 },
    .mac_flash_offset = 0,
    .led_sets = {
        { // RJ45 ports 3-4 (normal wiring)
            LEDS_1G | LEDS_100M | LEDS_10M | LEDS_LINK | LEDS_ACT,
            LEDS_2G5 | LEDS_LINK | LEDS_ACT,
            0,
            0
        },
        { // unused
            0,
            0,
            0,
            0
        },
        { // RJ45 ports 1-2 (reversed wiring)
            LEDS_2G5 | LEDS_LINK | LEDS_ACT,
            LEDS_1G | LEDS_100M | LEDS_10M | LEDS_LINK | LEDS_ACT,
            0,
            0
        },
        { // SFP ports 5-6
            LEDS_10G | LEDS_LINK | LEDS_ACT,
            LEDS_2G5 | LEDS_1G | LEDS_LINK | LEDS_ACT,
            0,
            0
        },
    },
    .led_mux_custom = 1,
    .led_mux = {
		0x20, 0x3f, 0x3f, 0x11, 0x10, 0x3f, 0x15, 0x3f, 0x14, 0x3f, 0x18, 0x3f, 0x19, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x1c, 0x3f, 0x3f, 0x1d, 0x3f, 0x3f, 0x3f, 0x0c, 0x3f, 0x3f
	},
};

MACHINE_CUSTOM_INIT(
	/* V0.2.5: all LEDs off when no link; per-port activity on the correct LED. */
	REG_SET(RTL837X_REG_LED_GLB_IO_EN, 0xffffffff);
	REG_SET(RTL837X_REG_LED_GLB_ACTIVE, 0x3fffffff);
)