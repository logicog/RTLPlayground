#include "machine_defs.h"

__code const struct machine machine = {
    .machine_name = MACHINE_MODEL,
    .isRTL8373 = 1,
    .min_port = 0,
    .max_port = 8,
    .n_sfp = 1,
    .log_to_phys_port = {1, 2, 3, 4, 5, 6, 7, 8, 9},
    .phys_to_log_port = {0, 1, 2, 3, 4, 5, 6, 7, 8},
    .is_sfp = {0, 0, 0, 0, 0, 0, 0, 0, 1},

    .sfp_port[0].pin_detect = GPIO30_ACL_BIT3_EN,
    .sfp_port[0].pin_los = GPIO37,
    .sfp_port[0].pin_tx_disable = GPIO_NA,
    .sfp_port[0].sds = 1,
    .sfp_port[0].i2c = I2CBUS( GPIO39_I2C_SDA4, GPIO40_I2C_SCL3_MDC1 ),
    .reset_pin = GPIO_NA,

    .high_leds = {
        .mux = LED_27 | LED_29,
        .enable = LED_28_SYS | LED_29
    },

    /* Ports 1-8 use set 0, port 9 SFP uses set 1 */
    .port_led_set = {0, 0, 0, 0, 0, 0, 0, 0, 1},

    .led_sets = {
        {   /* Set 0: RJ45 copper ports
             * Amber = 2.5G
             * Green = 1G/100M/10M with activity
             */
            LEDS_2G5 | LEDS_LINK | LEDS_ACT, /* Amber */
            0,
            LEDS_1G | LEDS_100M | LEDS_10M | LEDS_LINK | LEDS_ACT, /* Green */
            0
        },
        {   /* Set 1: SFP port, single green LED for all valid speeds */
            LEDS_10G | LEDS_2G5 | LEDS_1G | LEDS_100M | LEDS_10M | LEDS_LINK | LEDS_ACT,
            0,
            0,
            0
        },
    },
};
