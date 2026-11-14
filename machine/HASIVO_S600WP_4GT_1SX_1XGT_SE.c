#include "machine_defs.h"

__code const struct machine machine = {
    MACHINE_COMMON_SETTING,
    .n_sfp = 1,
    .n_10g = 1,

    /*
     * Same RTL8372 port routing as the supported S600W-4GT-2XGT-SE.
     * Logical port 3 / SDS0 is the RTL8261BE 10GBASE-T port.
     * Logical port 8 / SDS1 is the SFP+ cage.
     */
    .log_to_phys_port = {0, 0, 0, 6, 1, 2, 3, 4, 5},
    .phys_to_log_port = {4, 5, 6, 7, 8, 3, 0, 0, 0},
    .is_sfp = {0, 0, 0, 0, 0, 0, 0, 0, 1},

    /* SFP+ wiring recovered from the Hasivo stock firmware. */
    .sfp_port[0].pin_detect = GPIO30_ACL_BIT3_EN,
    .sfp_port[0].pin_los = GPIO37,
    .sfp_port[0].pin_tx_disable = GPIO_NA,
    .sfp_port[0].sds = 1,
    .sfp_port[0].i2c = I2CBUS(GPIO39_I2C_SDA4, GPIO40_I2C_SCL3_MDC1),

    .reset_pin = GPIO54_ACL_BIT2_EN,

    /* Reuse the S600W LED topology until hardware testing proves otherwise. */
    .high_leds = { .mux = LED_28_SYS | LED_29, .enable = LED_28_SYS | LED_29 },
    .port_led_set = { 0, 0, 0, 1, 0, 0, 0, 0, 1 },

    /* Ports 1-4: Green: 2.5G, Orange: 10/100/1000M
     * Port 5: RTL8261BE 10GBASE-T
     * Port 6: SFP+
     * High-speed ports retain the known S600W LED set for initial bring-up.
     */
    .led_sets = {
        {
            LEDS_2G5 | LEDS_1G | LEDS_100M | LEDS_10M | LEDS_LINK | LEDS_ACT,
            LEDS_2G5 | LEDS_LINK,
            LEDS_1G | LEDS_100M | LEDS_10M | LEDS_LINK,
            0,
        },
        {
            LEDS_10G | LEDS_5G | LEDS_2G5 | LEDS_1G | LEDS_100M | LEDS_10M | LEDS_LINK | LEDS_ACT,
            LEDS_10G | LEDS_LINK,
            LEDS_5G | LEDS_2G5 | LEDS_LINK,
            0,
        },
    },

    .led_mux_custom = 1,
    .led_mux = {
        0x3f, 0x3f, 0x3f, 0x3f,
        0x3f, 0x3f, 0x3f, 0x3f,

        // Swapped port 3 / port 8 LED sources
        0x23, 0x0c, 0x0d, 0x0e,

        // Ports 1-4 unchanged
        0x10, 0x11, 0x12, 0x14,
        0x15, 0x16, 0x18, 0x19,
        0x1a, 0x1c, 0x1d, 0x1e,

        // Swapped port 3 / port 8 LED sources
        0x20, 0x21, 0x22, 0x0f
    },
};
