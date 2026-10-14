#include "machine_defs.h"

__code const struct machine machine = {
    .machine_name = MACHINE_MODEL,
    .isRTL8373 = 0,
    .min_port = 3,
    .max_port = 8,
    .n_sfp = 2,
    .log_to_phys_port = {0, 0, 0, 5, 1, 2, 3, 4, 6},
    .phys_to_log_port = {4, 5, 6, 7, 3, 8, 0, 0, 0},
    .is_sfp = {0, 0, 0, 1, 0, 0, 0, 0, 2},

    // SFP port on SDS0 / logical port 3
    .sfp_port[0].pin_detect = GPIO37,
    .sfp_port[0].pin_los = GPIO_NA,
    .sfp_port[0].pin_tx_disable = GPIO_NA,
    .sfp_port[0].sds = 0,
    .sfp_port[0].i2c = I2CBUS( GPIO41_I2C_SDA3_MDIO1, GPIO40_I2C_SCL3_MDC1 ),

    // SFP port on SDS1 / logical port 8
    .sfp_port[1].pin_detect = GPIO38,
    .sfp_port[1].pin_los = GPIO_NA,
    .sfp_port[1].pin_tx_disable = GPIO_NA,
    .sfp_port[1].sds = 1,
    .sfp_port[1].i2c =  I2CBUS( GPIO39_I2C_SDA4, GPIO40_I2C_SCL3_MDC1 ),

    .reset_pin = GPIO_NA,
    .high_leds = { .mux =  LED_28_SYS | LED_29, .enable = LED_27 | LED_28_SYS | LED_29 },
    .port_led_set = { 0, 0, 0, 1, 0, 0, 0, 0, 1},
    .led_sets = {
                    {
                            LEDS_2G5 | LEDS_LINK | LEDS_ACT,
                            LEDS_1G | LEDS_100M | LEDS_10M | LEDS_LINK | LEDS_ACT,
                            LEDS_DUPLEX,
                            LEDS_2G5 | LEDS_LINK | LEDS_ACT
                    },
                    {
                            LEDS_2G5 | LEDS_1G | LEDS_100M | LEDS_LINK | LEDS_ACT,
                            LEDS_10G | LEDS_LINK | LEDS_ACT,
                            LEDS_2G5 | LEDS_LINK,
                            LEDS_COL | LEDS_DUPLEX
                    },
     },
    .led_mux_custom = 1,
    .led_mux = {
                            0x00,0x01,0x04,0x05,0x08,0x09,0x0c,0x3f,0x0d,0x10,0x11,0x0e,0x14,0x11,0x12,0x15,0x15,0x16,0x18,0x19,0x1a,0x19,0x1d,0x1e,0x1c,0x1d,0x20,0x21
            },
    };
