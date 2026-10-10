#include "machine_defs.h"

__code const struct machine machine = {
    .machine_name = MACHINE_MODEL,
    .isRTL8373 = 0,
    .min_port = 3,
    .max_port = 8,
    .n_sfp = 0,
    .n_10g = 2,
    .log_to_phys_port = {0, 0, 0, 5, 1, 2, 3, 4, 6},
    .phys_to_log_port = {4, 5, 6, 7, 3, 8, 0, 0, 0},
    .is_sfp = {0, 0, 0, 0, 0, 0, 0, 0, 0},
    .reset_pin = GPIO54_ACL_BIT2_EN,
    .high_leds = { .mux = LED_28_SYS | LED_29, .enable = LED_28_SYS | LED_29 },
    .port_led_set = { 0, 0, 0, 1, 0, 0, 0, 0, 1 },
    /* Ports 1-4: Green: 2.5GBit, Orange: 10/100/1000MBit
     * Ports 5-6: Green: 10GBit, Orange: 5/2.5GBit, None: 10/100/1000MBit
     */
    .led_sets = {
        {
            LEDS_2G5 | LEDS_1G | LEDS_100M | LEDS_10M | LEDS_LINK | LEDS_ACT, // Yellow
            LEDS_2G5 | LEDS_LINK, // Green
            LEDS_1G | LEDS_100M | LEDS_10M | LEDS_LINK, // Orange
            0, // Unused
        },
        {
            LEDS_10G | LEDS_5G | LEDS_2G5 | LEDS_1G | LEDS_100M | LEDS_10M | LEDS_LINK | LEDS_ACT, // Yellow
            LEDS_10G | LEDS_LINK, // Green
            LEDS_5G | LEDS_2G5 | LEDS_LINK, // Orange
            0, // Unused
        },
    },
    .led_mux_custom = 1,
    .led_mux = {
        0x3f, 0x3f, 0x3f, 0x3f, 0x3f,
        0x3f, 0x3f, 0x3f, 0x0f, 0x20,
        0x21, 0x22, 0x10, 0x11, 0x12,
        0x14, 0x15, 0x16, 0x18, 0x19,
        0x1a, 0x1c, 0x1d, 0x1e, 0x0c,
        0x0d, 0x0e, 0x23
    },

};
