#include "machine_defs.h"

/*
 * SL-SG008W: 8x Gigabit copper on the RTL8370 (RTL8367B die).
 *
 * Selects the SOC_FAMILY == RTL836X code paths (machine.h maps MACHINE_CHIPSET
 * RTL8370 to RTL836X). Port map and LED wiring are the bring-up defaults from
 * the RTL8370 session; refine them as boards are tested.
 *
 * Eight front ports means max_port is 7, not 8: min_port..max_port is the
 * range of *front* ports every table/UI loop iterates, and the CPU port is
 * not one of them -- it is CPU_PORT (rtl837x_common.h) and is added
 * explicitly wherever it matters.
 */
__code const struct machine machine = {
	MACHINE_COMMON_SETTING,
	.n_sfp = 0,
	.log_to_phys_port = {1, 2, 3, 4, 5, 6, 7, 8, 0},
	.phys_to_log_port = {0, 1, 2, 3, 4, 5, 6, 7, 0xff},
	.is_sfp = {0, 0, 0, 0, 0, 0, 0, 0, 0},
	.reset_pin = GPIO_NA,
	.high_leds = { .mux = 0, .enable = 0 },
	.port_led_set = { 0, 0, 0, 0, 0, 0, 0, 0, 0},
	.led_sets = { { 0, 0, 0, 0 },
		    },
	.mac_flash_offset = 0x1FC000,
};
