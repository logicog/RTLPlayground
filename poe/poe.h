#ifndef _POE_H_
#define _POE_H_

#include <stdint.h>
#include <stdbool.h>

#define POE_NONE	0x1000
#define RTL8238C	0x8239

#ifndef MACHINE_POE_CHIP
#define MACHINE_POE_CHIP POE_NONE
#endif

#if (MACHINE_POE_CHIP == RTL8238C)
#include "rtl8238c.h"
#elif (MACHINE_POE_CHIP != POE_NONE)
#error "Unsupported MACHINE_POE_CHIP"
#endif

/* PoE is present when the machine selects a controller. NO_POE=1 builds a PoE
 * board without it: the driver and every chip-agnostic consumer drop out, and
 * the `poe` command answers that the machine has no PoE. */
#if (MACHINE_POE_CHIP != POE_NONE) && !defined(NO_POE)
#define POE_PRESENT
#endif

void poe_init(void) __banked;

#ifdef POE_PRESENT

/* Bring PoE up: the driver does whatever its chip needs (e.g. upload firmware
 * and enable ports). Run at boot and on `poe load`; afterwards the controller
 * powers detected PDs. */
void poe_bringup(void) __banked;

/* Enable/disable PoE on a single port (`poe port <n> <on|off>`). port_num is
 * 0-based; port_on: 1 = enable, 0 = disable. */
void poe_port_set(uint8_t port_num, uint8_t port_on) __banked;

/* Enable/disable ALL ports at once (`poe global <on|off>` + the web UI's "All
 * PoE"). on: 1 = enable all, 0 = disable all. */
void poe_global_set(uint8_t on) __banked;

/*
 * Normalized per-port status. The driver decodes its (chip-specific) hardware
 * into these chip-agnostic fields, so the consumers - /poe.json and the web UI -
 * present PoE status with no knowledge of the controller at all. Set
 * poe_port_num (0-based), call poe_get_port(), read back:
 */
extern __xdata uint8_t poe_port_num;	// poe_get_port() input: port 0-based
extern __xdata uint8_t  poe_st_admin;	// 1 = administratively enabled
extern __xdata uint8_t  poe_st_on;	// 1 = actually delivering power to a PD
extern __xdata uint8_t  poe_st_class;	// PD class 0..8 (0xff = none / unknown)
extern __xdata uint8_t  poe_st_volt;	// port voltage, volts
extern __xdata uint16_t poe_st_ma;	// port current, mA
void poe_get_port(void) __banked;

#endif /* POE_PRESENT */

#endif
