/*
 * test_tick_gate.c - the timers of the main loop keep time by the system
 * tick, however often the loop passes. The gate is the TICKS_DUE macro from
 * rtl837x_common.h; the loop bodies of idle() and handle_tx() are restated
 * here, so this checks the arithmetic, not those functions.
 */
#include <stdint.h>
#include <stdio.h>

#include "rtl837x_common.h"
#include "rtl837x_stp.h"
#include "support.h"

volatile uint32_t ticks;

#define STP_TICK_STEP (SYS_TICK_HZ / STP_HZ)
#define STP_CATCH_UP 8

static uint8_t  stp_tick_last, tx_tick_last;
static unsigned stp_calls, tx_calls;

/* One pass of the loop: the STP block of idle() and the gate of handle_tx() */
static void pass(void)
{
	uint8_t n = STP_CATCH_UP;
	while (n-- && TICKS_DUE(stp_tick_last, STP_TICK_STEP)) {
		stp_tick_last += STP_TICK_STEP;
		stp_calls++;
	}
	if (TICKS_DUE(tx_tick_last, 1)) {
		tx_tick_last = (uint8_t)ticks;
		tx_calls++;
	}
}

/* Run `seconds` of wall clock with one pass every `pass_us` microseconds. */
static void run(double pass_us, unsigned seconds)
{
	double t = 0, next_tick = 0, tick_us = 1e6 / SYS_TICK_HZ, end = seconds * 1e6;
	ticks = 0;
	stp_tick_last = tx_tick_last = 0;
	stp_calls = tx_calls = 0;
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
	CHECK(tx_calls == 10 * SYS_TICK_HZ, "and 2000 TX periodics");
	run(16000, 10);
	CHECK(stp_calls >= 10 * STP_HZ - 1 && stp_calls <= 10 * STP_HZ, "a 16 ms pass, as under an IPv4 stream: still 500 (was 156 when the clock counted passes)");
	CHECK(tx_calls == 625, "TX periodic once per pass when passes are slower than the tick");
	run(600, 10);
	CHECK(stp_calls == 10 * STP_HZ, "a 0.6 ms pass, the loop no longer sleeping: still 500 (was 4166)");
	CHECK(tx_calls == 10 * SYS_TICK_HZ, "TX periodic capped at the tick rate");
	run(1300000, 13);
	CHECK(stp_calls <= 10 * STP_CATCH_UP, "a 1.3 s pass loses time instead of firing 65 steps at once");
}

static void t_wrap(void)
{
	printf("[test] the byte arithmetic survives the tick counter wrapping\n");
	ticks = 0xfffffffeUL;
	stp_tick_last = (uint8_t)ticks;
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
