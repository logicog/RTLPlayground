/*
 * test_tick_gate.c - the timers of the main loop keep time by the system
 * tick, however often the loop passes. The gate is a periodic timer from
 * timer.c; the STP block of handle_tick() is restated here, so this
 * checks the arithmetic, not that function.
 */
#include <stdint.h>
#include <stdio.h>

#include "rtl837x_common.h"
#include "rtl837x_stp.h"
#include "timer.h"
#include "support.h"

volatile uint32_t ticks;

#define STP_CATCH_UP 8

struct timer stp_step;
static unsigned stp_calls;

/* One pass of the loop: the STP block of handle_tick() */
static void pass(void)
{
	uint8_t n = STP_CATCH_UP;
	while (n-- && timer_expired(&stp_step)) {
		timer_reset(&stp_step);
		stp_calls++;
	}
}

/* Run `seconds` of wall clock with one pass every `pass_us` microseconds. */
static void run(double pass_us, unsigned seconds)
{
	double t = 0, next_tick = 0, tick_us = 1e6 / SYS_TICK_HZ, end = seconds * 1e6;
	ticks = 0;
	timer_set(&stp_step, STP_TICK_STEP);
	stp_calls = 0;
	while (t < end) {
		while (next_tick <= t) { ticks++; next_tick += tick_us; }
		pass();
		t += pass_us;
	}
}

static void t_rates(void)
{
	printf("[test] STP time follows the tick whatever the loop does\n");
	run(5000, 10);
	CHECK(stp_calls == 10 * STP_HZ, "one pass per tick: 500 STP steps in 10 s");
	run(16000, 10);
	CHECK(stp_calls >= 10 * STP_HZ - 1 && stp_calls <= 10 * STP_HZ, "a 16 ms pass, as under an IPv4 stream: still 500 (was 156 when the clock counted passes)");
	run(600, 10);
	CHECK(stp_calls == 10 * STP_HZ, "a 0.6 ms pass, the loop no longer sleeping: still 500 (was 4166)");
	run(1300000, 13);
	CHECK(stp_calls <= 10 * STP_CATCH_UP, "a 1.3 s pass loses time instead of firing 65 steps at once");
}

static void t_wrap(void)
{
	printf("[test] the 16-bit clock survives the tick counter wrapping\n");
	ticks = 0xfffffffeUL;
	timer_set(&stp_step, STP_TICK_STEP);
	stp_calls = 0;
	for (int i = 0; i < 8; i++) { ticks++; pass(); }
	CHECK(stp_calls == 2, "two STP steps across 0xffffffff -> 0x00000006");
}

int main(void)
{
	printf("== tick gate tests ==\n");
	t_rates();
	t_wrap();
	printf("\n%d checks, %d failed\n", tests_run, tests_failed);
	return tests_failed ? 1 : 0;
}
