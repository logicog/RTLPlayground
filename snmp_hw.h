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
/* Normalized MAC bitmap, matching the GUI's REG_LINKS_STS byte mapping. */
bool snmp_hw_link(uint8_t port, __xdata uint32_t *__xdata value) __banked;
bool snmp_hw_speed(uint8_t port, __xdata uint16_t *__xdata mbps) __banked;
/* A timed-out engine remains quarantined until reboot. */
#endif
