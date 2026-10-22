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
    .max_port = 8,
    .is_sfp = {[8] = 1}}; /* logical 8 is the SFP cage; speed/link keep using LINKS_89 */
/* Bounded request: selector write, optional busy pre-check and 32 polls, plus slack. */
#define MAX_REQUEST_COMMANDS 40
static uint32_t link_register = 0x00080000, speed_register = 5u << 12;
static uint32_t sfp_speed_register;
static uint16_t a610[9] = {0x2058, 0x2058, 0x2058, 0x2058, 0x2058,
			   0x2058, 0x2058, 0x2058, 0x2058};
static unsigned commands, selectors, smi_reads, stick_at;
static bool stuck_sfr, stuck_selector, selector_inflight, advance_live, seen_high;
static bool native_order = true, pending;
static uint32_t latch_high, latch_low;
static uint16_t pending_reg, pending_word;
static uint8_t phase, pending_command, last_smi_phy;
static uint8_t expected_selector = 7, expected_port = 3;
static uint32_t high = 0x80000001, low = 0;
static void word(uint32_t v)
{
	SFR_DATA_24 = v >> 24;
	SFR_DATA_16 = v >> 16;
	SFR_DATA_8 = v >> 8;
	SFR_DATA_0 = v;
}
static void latch(void)
{
	latch_high = high;
	latch_low = low;
	phase = 0;
	seen_high = false;
}
/* Hardware ignores a STAT_GET write while a selector is in flight; the model
 * treats that as a firmware bug because it corrupts the pending snapshot. */
static void selector_write(void)
{
	uint16_t w = ((uint16_t)expected_selector << 5) | (expected_port << 1) | 1;
	assert(SFR_DATA_24 == 0 && SFR_DATA_16 == 0 && SFR_DATA_8 == (w >> 8) &&
	       SFR_DATA_0 == (uint8_t)w);
	assert(!selector_inflight);
	selectors++;
	selector_inflight = stuck_selector;
	latch();
}
void snmp_hw_command(uint16_t reg, uint8_t command)
{
	commands++;
	if (stick_at && commands == stick_at)
		stuck_sfr = true;
	if (stuck_sfr) {
		/* Engine accepted the command but never completes it (yet). */
		pending = true;
		pending_reg = reg;
		pending_command = command;
		pending_word = ((uint16_t)SFR_DATA_8 << 8) | SFR_DATA_0;
		SFR_EXEC_STATUS = 1;
		return;
	}
	SFR_EXEC_STATUS = 0;
	if (command == SFR_EXEC_READ_SMI) {
		uint8_t phy = SFR_SMI_PHY;
		assert(SFR_SMI_REG_U16 == 0xa610 && SFR_SMI_DEV == ((31 << 3) | 2));
		assert(phy < 9 && !machine.is_sfp[phy]);
		smi_reads++;
		last_smi_phy = phy;
		/* Host SFR aliases are independent variables; keep every view coherent. */
		SFR_DATA_U16 = a610[phy];
		SFR_DATA_8 = a610[phy] >> 8;
		SFR_DATA_0 = a610[phy];
		return;
	}
	if (command == SFR_EXEC_WRITE_REG) {
		assert(reg == RTL837X_STAT_GET);
		selector_write();
		return;
	}
	if (reg == RTL837X_STAT_GET) {
		word(stuck_selector ? 1 : 0);
		if (!stuck_selector)
			phase = 1;
	} else if (reg == RTL837X_STAT_V_HIGH) {
		assert(phase >= 1);
		word(latch_high);
		seen_high = true;
		if (advance_live && ++low == 0)
			++high;
	} else if (reg == RTL837X_STAT_V_LOW) {
		assert(phase >= 1 && (!native_order || seen_high));
		word(latch_low);
	} else if (reg == RTL837X_REG_LINKS_STS)
		word(link_register);
	else if (reg == RTL837X_REG_LINKS)
		word(speed_register);
	else if (reg == RTL837X_REG_LINKS_89)
		word(sfp_speed_register);
	else
		assert(0);
}
/* The stuck engine eventually completes; a timed-out selector write latches
 * only now, so its snapshot is stale for whoever timed out. */
static void finish_engine(void)
{
	assert(stuck_sfr || SFR_EXEC_STATUS);
	stuck_sfr = false;
	stick_at = 0;
	SFR_EXEC_STATUS = 0;
	if (pending && pending_command == SFR_EXEC_WRITE_REG && pending_reg == RTL837X_STAT_GET) {
		/* A busy engine's selector must not have been overwritten by a new request. */
		assert(SFR_DATA_8 == (pending_word >> 8) && SFR_DATA_0 == (uint8_t)pending_word);
		assert(!selector_inflight);
		selectors++;
		selector_inflight = stuck_selector;
		latch();
	}
	pending = false;
}
static void selector_done(void)
{
	stuck_selector = false;
	selector_inflight = false;
}

struct sfr_snapshot {
	uint16_t addr, smi_reg;
	uint8_t go, status, smi_dev, d24, d16, d8, d0;
};
static void save(struct sfr_snapshot *s)
{
	s->addr = SFR_REG_ADDR_U16;
	s->smi_reg = SFR_SMI_REG_U16;
	s->go = SFR_EXEC_GO;
	s->status = SFR_EXEC_STATUS;
	s->smi_dev = SFR_SMI_DEV;
	s->d24 = SFR_DATA_24;
	s->d16 = SFR_DATA_16;
	s->d8 = SFR_DATA_8;
	s->d0 = SFR_DATA_0;
}
static bool same(const struct sfr_snapshot *a)
{
	struct sfr_snapshot b;
	save(&b);
	return !memcmp(a, &b, sizeof b);
}

/* kind 0: native selector 7; 1/2: counter32 selector 48 (error pair) high/low. */
static const uint8_t kind_selector[] = {7, 48, 48};
static bool counter_call(uint8_t kind, struct snmp_counter *c)
{
	expected_selector = kind_selector[kind];
	native_order = !kind;
	return kind ? snmp_hw_counter32(expected_port, expected_selector, kind == 1, c)
		    : snmp_hw_counter(expected_port, expected_selector, c);
}
static void sentinel(struct snmp_counter *c)
{
	memset(c->bytes, 0xee, 8);
}
static bool is_sentinel(const struct snmp_counter *c)
{
	for (uint8_t i = 0; i < 8; i++)
		if (c->bytes[i] != 0xee)
			return false;
	return true;
}
static void be32(uint8_t *p, uint32_t v)
{
	p[0] = v >> 24;
	p[1] = v >> 16;
	p[2] = v >> 8;
	p[3] = v;
}
/* Counter32 is zero-extended: only the requested half, upper bytes cleared. */
static void expect_counter(uint8_t kind, const struct snmp_counter *c, uint32_t hi, uint32_t lo)
{
	uint8_t e[8] = {0};
	if (!kind) {
		be32(e, hi);
		be32(e + 4, lo);
	} else
		be32(e + 4, kind == 1 ? hi : lo);
	assert(!memcmp(c->bytes, e, 8));
}
/* SFR engine busy: every request is refused without touching the engine. */
static void expect_busy_refusals(void)
{
	struct snmp_counter c;
	struct sfr_snapshot s;
	uint32_t link;
	uint16_t speed;
	uint8_t state = 0xaa;
	unsigned c0 = commands, s0 = selectors, m0 = smi_reads;
	assert(SFR_EXEC_STATUS);
	save(&s);
	for (uint8_t kind = 0; kind < 3; kind++) {
		sentinel(&c);
		assert(!counter_call(kind, &c) && is_sentinel(&c));
	}
	assert(!snmp_hw_link(3, &link) && !snmp_hw_speed(3, &speed) &&
	       !snmp_hw_admin(3, &state));
	assert(commands == c0 && selectors == s0 && smi_reads == m0 && same(&s));
}
static void expect_recovered(void)
{
	struct snmp_counter c;
	uint32_t link;
	uint16_t speed;
	uint8_t state = 0xaa;
	high = 0x01020304;
	low = 0xa0b0c0d0;
	for (uint8_t kind = 0; kind < 3; kind++) {
		unsigned s = selectors;
		sentinel(&c);
		assert(counter_call(kind, &c));
		expect_counter(kind, &c, high, low);
		assert(selectors == s + 1); /* fresh selector, never a stale latch */
	}
	assert(snmp_hw_link(3, &link) && link == 8 && snmp_hw_speed(3, &speed) && speed == 2500);
	assert(snmp_hw_admin(3, &state) && state == 1);
}

/* A busy/stuck SFR engine at each step of a request is refused until it
 * completes; then a new request takes a fresh snapshot. */
static void test_transient_sfr(void)
{
	for (uint8_t kind = 0; kind < 3; kind++) {
		for (uint8_t n = 1; n <= (kind ? 3 : 4); n++) {
			struct snmp_counter c;
			unsigned before = commands;
			high = 0x55667788;
			low = 0x99aabbcc;
			sentinel(&c);
			stick_at = commands + n;
			assert(!counter_call(kind, &c) && is_sentinel(&c));
			assert(stuck_sfr && SFR_EXEC_STATUS && commands == before + n);
			expect_busy_refusals();
			finish_engine();
			expect_recovered();
		}
	}
	/* Single-command SMI read, bounded and recoverable. */
	uint8_t state = 0xaa;
	unsigned before = commands;
	stick_at = commands + 1;
	assert(!snmp_hw_admin(3, &state) && commands == before + 1);
	expect_busy_refusals();
	finish_engine();
	expect_recovered();
	puts("PASS transient SFR timeout at every step recovers only after the engine completes");
}

/* A timed-out selector stays quarantined while STAT_GET or the SFR engine is busy. */
static void test_selector_recovery(void)
{
	for (uint8_t kind = 0; kind < 3; kind++) {
		struct snmp_counter c;
		unsigned before = commands, s = selectors;
		high = 0x55667788;
		low = 0x99aabbcc;
		sentinel(&c);
		stuck_selector = true;
		assert(!counter_call(kind, &c) && is_sentinel(&c));
		assert(commands - before <= MAX_REQUEST_COMMANDS && selectors == s + 1);
		/* SFR idle, selector still busy: never overwrite the in-flight selector. */
		before = commands;
		assert(!counter_call(kind, &c) && is_sentinel(&c));
		assert(commands - before <= MAX_REQUEST_COMMANDS && selectors == s + 1);
		/* Both busy, then only the selector cleared: SFR busy still blocks. */
		SFR_EXEC_STATUS = 1;
		expect_busy_refusals();
		selector_done();
		/* Link status must recover independently: discovery can ask it before a counter. */
		SFR_EXEC_STATUS = 0;
		uint32_t recovered_link;
		assert(snmp_hw_link(3, &recovered_link) && recovered_link == 8);
		SFR_EXEC_STATUS = 1;
		expect_busy_refusals();
		/* SFR idle again and selector clear: a new request succeeds, fresh data. */
		SFR_EXEC_STATUS = 0;
		high = 0x01020304;
		low = 0xa0b0c0d0;
		assert(counter_call(kind, &c) && selectors == s + 2);
		expect_counter(kind, &c, high, low);
	}
	/* Reverse order: SFR clears first while the selector remains busy. */
	{
		struct snmp_counter c;
		unsigned s = selectors;
		sentinel(&c);
		stuck_selector = true;
		assert(!counter_call(0, &c));
		SFR_EXEC_STATUS = 1;
		expect_busy_refusals();
		SFR_EXEC_STATUS = 0;
		assert(!counter_call(0, &c) && is_sentinel(&c) && selectors == s + 1);
		selector_done();
		expect_recovered();
	}
	puts("PASS selector timeout recovers only when STAT_GET and SFR engine are both idle");
}

static void test_counter32(void)
{
	static const uint8_t ports[] = {3, 8};
	struct snmp_counter c;
	high = 0x89abcdef;
	low = 0x12345678;
	for (uint8_t p = 0; p < 2; p++) {
		expected_port = ports[p];
		for (uint8_t kind = 1; kind < 3; kind++) {
			memset(c.bytes, 0xee, 8); /* any surviving 0xee is a high-byte leak */
			unsigned s = selectors;
			assert(counter_call(kind, &c) && selectors == s + 1);
			expect_counter(kind, &c, high, low);
		}
	}
	expected_port = 3;
	unsigned before = commands;
	/* Native API owns selectors 0..7 only; counter32 owns only 48 (discards unverified). */
	native_order = true;
	assert(!snmp_hw_counter(3, 8, &c) && !snmp_hw_counter(3, 48, &c) &&
	       !snmp_hw_counter(3, 255, &c));
	for (unsigned sel = 0; sel < 256; sel++) {
		if (sel == 48)
			continue;
		assert(!snmp_hw_counter32(3, sel, true, &c) && !snmp_hw_counter32(3, sel, false, &c));
	}
	assert(!snmp_hw_counter32(9, 48, true, &c) && !snmp_hw_counter32(255, 48, false, &c));
	if (machine.min_port)
		assert(!snmp_hw_counter32(machine.min_port - 1, 48, true, &c));
	assert(commands == before);
	puts("PASS counter32 error-pair high/low zero extension, selector 8 refused, range checks");
}

static void test_admin(void)
{
	static const struct {
		uint16_t reg;
		uint8_t state;
	} cases[] = {{0x2058, 1}, {0x2858, 2}, {0xf7ff, 1}, {0x0800, 2}, {0x0000, 1}, {0xffff, 2}};
	uint8_t state;
	for (uint8_t port = machine.min_port; port <= machine.max_port; port++) {
		for (unsigned n = 0; n < sizeof cases / sizeof *cases; n++) {
			unsigned before = smi_reads;
			state = 0xaa;
			/* SFP is up regardless of the PHY register or link: no SMI at all. */
			a610[port] = cases[n].reg;
			assert(snmp_hw_admin(port, &state));
			if (machine.is_sfp[port])
				assert(state == 1 && smi_reads == before);
			else
				assert(state == cases[n].state && smi_reads == before + 1 &&
				       last_smi_phy == port);
		}
		a610[port] = 0x2058;
	}
	unsigned before_invalid = commands;
	state = 0xaa;
	assert(!snmp_hw_admin(9, &state) && !snmp_hw_admin(255, &state));
	if (machine.min_port)
		assert(!snmp_hw_admin(machine.min_port - 1, &state));
	assert(commands == before_invalid);
	puts("PASS real admin up/down from MMD31.a610 bit 11, SFP always up, invalid ports");
}

int main(int argc, char **argv)
{
	struct snmp_counter c;
	uint32_t link;
	fault = 0xa5; /* host C zero initialization must not hide target XDATA state */
	snmp_hw_init();
	assert(!fault && snmp_hw_link(3, &link) && link == 8);
	commands = 0;
	assert(!snmp_hw_counter(9, 0, &c) && !snmp_hw_counter(255, 0, &c) &&
	       !snmp_hw_counter(3, 8, &c));
	assert(!commands);
	assert(snmp_hw_counter(3, 7, &c));
	const uint8_t expected[] = {0x80, 0, 0, 1, 0, 0, 0, 0};
	assert(!memcmp(c.bytes, expected, 8) && selectors == 1 && commands >= 4 &&
	       commands <= 5); /* optional pre-check of a prior selector */
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
	test_counter32();
	test_admin();
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
	test_transient_sfr();
	test_selector_recovery();

	native_order = true;
	expected_selector = 7;
	/* Mode-selected fault injected at the start of a request, then recovery. */
	const char *mode = argc > 1 ? argv[1] : "";
	unsigned before = commands;
	uint8_t state = 0xaa;
	if (!strcmp(mode, "selector"))
		stuck_selector = true;
	else if (!strcmp(mode, "prebusy"))
		SFR_EXEC_STATUS = 1;
	else
		stuck_sfr = true;
	if (!strcmp(mode, "link"))
		assert(!snmp_hw_link(3, &link));
	else if (!strcmp(mode, "speed"))
		assert(!snmp_hw_speed(3, &speed));
	else if (!strcmp(mode, "admin"))
		assert(!snmp_hw_admin(3, &state));
	else {
		sentinel(&c);
		assert(!snmp_hw_counter(3, 7, &c) && is_sentinel(&c));
	}
	assert(commands - before <= MAX_REQUEST_COMMANDS);
	before = commands;
	if (!strcmp(mode, "selector")) {
		/* Link/SFR traffic is not constrained; the pending selector must survive. */
		unsigned s = selectors;
		assert(!snmp_hw_counter(3, 7, &c) && selectors == s);
		assert(commands - before <= MAX_REQUEST_COMMANDS);
		selector_done();
	} else {
		expect_busy_refusals();
		if (!strcmp(mode, "prebusy"))
			SFR_EXEC_STATUS = 0;
		else
			finish_engine();
	}
	expect_recovered();
	puts("PASS bounded timeout, no engine/selector overwrite, idle recovery with fresh snapshot");
	return 0;
}
