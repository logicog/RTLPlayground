#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "machine.h"
#undef __sfr
#undef __sfr16
#undef __sfr32
#undef __sbit
#define __sfr volatile uint8_t
#define __sfr16 volatile uint16_t
#define __sfr32 volatile uint32_t
#define __sbit bool
#include "rtl837x_sfr.h"
#include "rtl837x_regs.h"
#include "snmp_hw.h"
/* Include the real implementation to poison its private XDATA startup state.
 * No production test hooks or reset-on-request paths are introduced. */
#include "../snmp_hw.c"
const struct machine machine = {
#ifdef SNMP_HW_SIX
    .min_port = 3,
#else
    .min_port = 0,
#endif
    .max_port = 8};
static uint32_t link_register = 0x00080000, speed_register = 5u << 12;
static uint32_t sfp_speed_register;
static unsigned commands, selectors;
static bool stuck_sfr, stuck_selector, advance_live;
static uint32_t latch_high, latch_low;
static uint8_t phase, expected_selector = 7;
static uint32_t high = 0x80000001, low = 0;
static void word(uint32_t v)
{
	SFR_DATA_24 = v >> 24;
	SFR_DATA_16 = v >> 16;
	SFR_DATA_8 = v >> 8;
	SFR_DATA_0 = v;
}
void snmp_hw_command(uint16_t reg, uint8_t command)
{
	commands++;
	if (stuck_sfr) {
		SFR_EXEC_STATUS = 1;
		return;
	}
	SFR_EXEC_STATUS = 0;
	if (command == SFR_EXEC_WRITE_REG) {
		assert(reg == RTL837X_STAT_GET && SFR_DATA_24 == 0 && SFR_DATA_16 == 0 &&
		       SFR_DATA_8 == 0);
		selectors++;
		assert(SFR_DATA_0 == ((expected_selector << 5) | (3 << 1) | 1));
		latch_high = high;
		latch_low = low;
		phase = 0;
		return;
	}
	if (reg == RTL837X_STAT_GET) {
		word(stuck_selector ? 1 : 0);
		if (!stuck_selector)
			phase = 1;
	} else if (reg == RTL837X_STAT_V_HIGH) {
		assert(phase == 1);
		word(latch_high);
		phase = 2;
		if (advance_live && ++low == 0)
			++high;
	} else if (reg == RTL837X_STAT_V_LOW) {
		assert(phase == 2);
		word(latch_low);
		phase = 3;
	} else if (reg == RTL837X_REG_LINKS_STS)
		word(link_register);
	else if (reg == RTL837X_REG_LINKS)
		word(speed_register);
	else if (reg == RTL837X_REG_LINKS_89)
		word(sfp_speed_register);
	else
		assert(0);
}
int main(int argc, char **argv)
{
	struct snmp_counter c;
	uint32_t link;
	fault = 0xa5; /* host C zero initialization must not hide target XDATA state */
	assert(!snmp_hw_link(3, &link) && !commands);
	snmp_hw_init();
	assert(!fault && snmp_hw_link(3, &link) && link == 8);
	commands = 0;
	assert(!snmp_hw_counter(9, 0, &c) && !snmp_hw_counter(255, 0, &c) &&
	       !snmp_hw_counter(3, 8, &c));
	assert(!commands);
	assert(snmp_hw_counter(3, 7, &c));
	const uint8_t expected[] = {0x80, 0, 0, 1, 0, 0, 0, 0};
	assert(!memcmp(c.bytes, expected, 8) && selectors == 1 && commands == 4);
	assert(snmp_hw_counter(3, 7, &c));
	assert(!memcmp(c.bytes, expected,
		       8)); /* repeated snapshots are non-destructive in this model */
	high = 0x123;
	low = 0xffffffff;
	advance_live = true;
	assert(snmp_hw_counter(3, 7, &c));
	const uint8_t before_roll[] = {0, 0, 1, 0x23, 255, 255, 255, 255};
	assert(!memcmp(c.bytes, before_roll, 8)); /* low half remains the same latched sample */
	advance_live = false;
	assert(snmp_hw_counter(3, 7, &c));
	const uint8_t after_roll[] = {0, 0, 1, 0x24, 0, 0, 0, 0};
	assert(!memcmp(c.bytes, after_roll, 8));
	high = low = 0;
	assert(snmp_hw_counter(3, 7, &c));
	const uint8_t reset[8] = {0};
	assert(!memcmp(c.bytes, reset, 8));
	for (expected_selector = 0; expected_selector < 8; expected_selector++)
		assert(snmp_hw_counter(3, expected_selector, &c));
	expected_selector = 7;
	puts(
	    "PASS modeled latch coherence, live rollover, repeated reads, reset and all selectors");
	assert(snmp_hw_link(3, &link) && link == 8);
	uint16_t speed;
	assert(snmp_hw_speed(3, &speed) && speed == 2500);
	/* Match the GUI register layout, with unrelated A4/A7 bits set.
	 * Exercise every MAC, both nibble positions and SFP's separate register. */
	for (uint8_t port = machine.min_port; port <= machine.max_port; port++) {
		link_register = 0xa500005a | (port < 8 ? 1u << (16 + port) : 1u << 8);
		assert(snmp_hw_link(port, &link) && link == (1u << port));
		for (uint8_t n = 0; n < 3; n++) {
			const uint8_t codes[] = {1, 2, 5};
			const uint16_t mbps[] = {100, 1000, 2500};
			speed_register = port < 8 ? (uint32_t)codes[n] << (4 * port) : 0;
			sfp_speed_register = codes[n];
			assert(snmp_hw_speed(port, &speed) && speed == mbps[n]);
		}
		link_register = 0xa500005a;
		unsigned before_down = commands;
		assert(snmp_hw_speed(port, &speed) && speed == 0 && commands == before_down + 1);
	}
	unsigned before_invalid = commands;
	assert(!snmp_hw_link(9, &link) && !snmp_hw_speed(255, &speed));
	assert(commands == before_invalid);
	if (machine.min_port)
		assert(!snmp_hw_link(machine.min_port - 1, &link));
	link_register = 0x00080000;
	speed_register = 5u << 12;
	puts("PASS dirty XDATA startup, GUI byte mapping, all MACs, SFP, down/100M/1G/2.5G and "
	     "invalid ports");
	unsigned before = commands;
	if (argc > 1 && !strcmp(argv[1], "selector"))
		stuck_selector = true;
	else if (argc > 1 && !strcmp(argv[1], "prebusy"))
		SFR_EXEC_STATUS = 1;
	else
		stuck_sfr = true;
	if (argc > 1 && !strcmp(argv[1], "link"))
		assert(!snmp_hw_link(3, &link));
	else if (argc > 1 && !strcmp(argv[1], "speed"))
		assert(!snmp_hw_speed(3, &speed));
	else
		assert(!snmp_hw_counter(3, 7, &c));
	assert(commands - before <= 33);
	before = commands;
	assert(!snmp_hw_link(3, &link) && !snmp_hw_counter(3, 7, &c));
	assert(commands == before);
	puts("PASS native high/low snapshot, range checks, bounded timeout and quarantine");
	return 0;
}
