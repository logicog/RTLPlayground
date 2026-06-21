#ifndef _POE_H_
#define _POE_H_

/*
 * Generic, chip-agnostic PoE interface.
 *
 * A per-chip driver implements these operations (e.g. poe_rtl8238b.c, selected by the machine's
 * POE_CHIP_* macro - see machine.h). The console (`poe ...`), the /poe.json endpoint and the boot
 * bring-up call ONLY this interface and never touch the controller themselves. HOW a port is
 * enabled or telemetry obtained - direct register I/O, a host-command MCU, ... - is entirely the
 * driver's business and does not leak out here. Parameters pass in xdata globals (never function
 * locals) to keep the 8051 overlay segment small.
 */

/* Bring PoE up: the driver does whatever its chip needs (e.g. upload firmware + enable ports).
 * Run at boot (post-PHY) and on `poe load`; afterwards the controller powers detected PDs. */
void poe_bringup(void) __banked;

/* Enable/disable PoE on a single port (`poe port <n> <on|off>`). */
extern __xdata uint8_t poe_port_num;	// port 0-based
extern __xdata uint8_t poe_port_on;	// 1 = enable, 0 = disable
void poe_port_set(void) __banked;

/* Enable/disable ALL ports at once (`poe global <on|off>` + the web UI's "All PoE"). */
extern __xdata uint8_t poe_global_on;	// 1 = enable all, 0 = disable all
void poe_global_set(void) __banked;

/*
 * Normalized per-port status. The driver decodes its (chip-specific) hardware into these
 * chip-agnostic fields, so the consumers - /poe.json and the web UI - present PoE status with no
 * knowledge of the controller at all. Set poe_port_num (0-based), call poe_get_port(), read back:
 */
extern __xdata uint8_t  poe_st_admin;	// 1 = administratively enabled
extern __xdata uint8_t  poe_st_on;	// 1 = actually delivering power to a PD
extern __xdata uint8_t  poe_st_class;	// PD class 0..8 (0xff = none / unknown)
extern __xdata uint8_t  poe_st_volt;	// port voltage, volts
extern __xdata uint16_t poe_st_ma;	// port current, mA
void poe_get_port(void) __banked;

#endif
