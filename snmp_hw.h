#ifndef SNMP_HW_H
#define SNMP_HW_H
#include <stdint.h>
#include <stdbool.h>
/* Native ASIC snapshot, most significant byte first; no software extension. */
struct snmp_counter {
	uint8_t bytes[8];
};
/* Startup only, before the listener is installed. Never reset on a request. */
void snmp_hw_init(void) __banked;
bool snmp_hw_counter(uint8_t port, __xdata uint8_t selector,
		     __xdata struct snmp_counter *__xdata value) __banked;
/* Selector 48 only: high selects the RX error word, else the TX error word.
 * The 32-bit word is zero-extended into bytes[4..7], bytes[0..3] are zero. */
bool snmp_hw_counter32(uint8_t port, __xdata uint8_t selector, __xdata bool high,
		       __xdata struct snmp_counter *__xdata value) __banked;
/* Normalized MAC bitmap, matching the GUI's REG_LINKS_STS byte mapping. */
bool snmp_hw_link(uint8_t port, __xdata uint32_t *__xdata value) __banked;
bool snmp_hw_speed(uint8_t port, __xdata uint16_t *__xdata mbps) __banked;
/* ifAdminStatus: 1 up, 2 down. Copper reads PHY MMD31.a610 bit 11; SFP is always up. */
bool snmp_hw_admin(uint8_t port, __xdata uint8_t *__xdata state) __banked;
/* A timed-out statistics request refuses further requests, without touching the
 * engine, until the SFR engine and STAT_GET are both observed idle; the next
 * counter request then recovers. A failed request never alters the caller's counter. */
#endif
