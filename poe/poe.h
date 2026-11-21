#ifndef _POE_H_
#define _POE_H_

/*
 * Chip-agnostic PoE interface, as proposed in upstream PRs #265 (RTL8238B)
 * and #712 (RTL8238C): a driver per PSE chip implements it, and the console
 * (`poe ...`), /poe.json and the boot call only this.
 *
 * A machine selects its PSE in machine/<MACHINE>.h:
 *   #define MACHINE_POE_CHIP POE_TPS23880
 * plus whatever its driver needs (see poe/tps23880.c). Without it, or when
 * built with NO_POE=1, no PoE code is compiled in.
 *
 * Ports are the switch's port numbers as the console and the web interface
 * show them (1-based; on a two-chip board the combined numbers). Multi-field
 * status comes back in xdata globals to keep the 8051 overlay small.
 */

#include <stdint.h>
#include "machine.h"	// MACHINE_POE_CHIP comes from machine/<MACHINE>.h

#define POE_NONE	0
#define POE_TPS23880	1

#ifndef MACHINE_POE_CHIP
#define MACHINE_POE_CHIP POE_NONE
#endif

#if (MACHINE_POE_CHIP != POE_NONE) && !defined(NO_POE)
#define POE_PRESENT
#endif

#if MACHINE_POE_CHIP == POE_TPS23880
#define POE_CHIP_NAME "TPS23880"
#endif

#ifdef POE_PRESENT

/* At boot, before the config is replayed: bring the PSE up unless it already
 * runs (a switch reboot without a power cut leaves it powering its PDs, and
 * the ports it has in use stay enabled). */
void poe_init(void) __banked;

/* Full bring-up: reset the PSE, load what it needs, set the ports up as they
 * are enabled; all ports start disabled until the config or the console
 * enables them (`poe load`). Returns 0 on success. */
uint8_t poe_bringup(void) __banked;

/* Enable or disable PoE on one port (1-based). Returns 0 if the port has PoE. */
uint8_t poe_port_set(uint8_t port, __xdata uint8_t on) __banked;

/* Enable or disable all PoE ports */
void poe_global_set(uint8_t on) __banked;

/* Per-port status: set poe_port_num (1-based), call poe_get_port(), read back */
extern __xdata uint8_t  poe_port_num;
extern __xdata uint8_t  poe_st_valid;	// 0: the port has no PoE, the rest is not set
extern __xdata uint8_t  poe_st_admin;	// 1 = enabled
extern __xdata uint8_t  poe_st_on;	// 1 = delivering power to a PD
extern __xdata uint8_t  poe_st_class;	// PD class 0..8, 0xff = none / unknown
extern __xdata uint16_t poe_st_mv;	// port voltage, mV
extern __xdata uint16_t poe_st_ma;	// port current, mA
void poe_get_port(void) __banked;

/* Controller status: name, firmware running, input voltage, temperature */
extern __xdata uint8_t  poe_fw;		// firmware version running, 0 = none
extern __xdata uint16_t poe_vin_mv;	// supply voltage, mV
extern __xdata int8_t   poe_temp;	// degrees C
uint8_t poe_get_global(void) __banked;	// 0 if the controller answered

#if MACHINE_POE_CHIP == POE_TPS23880
/* `poe reg`: raw register access, see poe/tps23880.c */
uint16_t poe_reg(uint8_t dev, __xdata uint8_t reg, __xdata uint16_t val) __banked;
#endif

#endif /* POE_PRESENT */

#endif
