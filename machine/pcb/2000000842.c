#include "machine_defs.h"

__code const struct machine machine = {
    MACHINE_COMMON_SETTING,
    .n_sfp = 0,
    .log_to_phys_port = {0, 0, 0, 5, 1, 2, 3, 4, 0},
    .phys_to_log_port = {4, 5, 6, 7, 3, 0, 0, 0, 0},
    .is_sfp = {0},
    .reset_pin = GPIO_NA, /* No reset button; SW3 is the loop selector. */
    /* Power LED has no established software control. */
    .high_leds = {.mux = 0, .enable = 0},
    .port_led_set = {0},
    /* OEM set0: 2.5G, 1G, 10/100M, each with link/activity. */
    .led_sets = {{
        LEDS_2G5 | LEDS_LINK | LEDS_ACT,
        LEDS_1G | LEDS_LINK | LEDS_ACT,
        LEDS_100M | LEDS_10M | LEDS_LINK | LEDS_ACT,
        0
    }},
    .led_mux_custom = 1,
    /* OEM writes to 65e0..65f4, decoded as 6-bit fields. Unused
     * outputs remain disabled by the explicit IO enable mask below.
     */
    .led_mux = {
        12, 13, 14, 16, 17, 18, 20, 0, 21, 22,
        24, 0, 26, 0, 0, 25, 0, 0, 28, 0,
        0, 29, 0, 0, 30, 32, 33, 34
    },
    .mac_flash_offset = 0x1fc000,
};
