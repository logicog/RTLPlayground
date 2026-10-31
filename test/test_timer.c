/*
 * test_timer.c - timer.c against a ticks counter the test moves by hand:
 * expiry on the tick, wrap of the 16-bit clock, reset keeping its phase.
 */
#include <stdint.h>
#include <stdio.h>
#include "rtl837x_common.h"
#include "timer.h"
#include "support.h"

volatile uint32_t ticks;

int main(void)
{
	static struct timer t;
	unsigned n;

	CHECK(sizeof(t.start) == 2, "clock_time_t is 16 bits");

	ticks = 100; timer_set(&t, 5);
	CHECK(!timer_expired(&t), "set: not expired at once");
	ticks = 104; CHECK(!timer_expired(&t), "set: not expired one tick early");
	ticks = 105; CHECK(timer_expired(&t), "set: expired on the tick");

	ticks = 0x0000fff0; timer_set(&t, 0x20);
	ticks = 0x0001000f; CHECK(!timer_expired(&t), "wrap: 31 ticks over 0xffff is not 32");
	ticks = 0x00010010; CHECK(timer_expired(&t), "wrap: 32 ticks over 0xffff expires");

	ticks = 100; timer_set(&t, 4); ticks = 111;
	for (n = 0; n < 8 && timer_expired(&t); n++)
		timer_reset(&t);
	CHECK(n == 2, "reset keeps phase: two steps owed after 11 ticks");
	CHECK(t.start == 108, "reset: start moved by two intervals");

	timer_restart(&t);
	CHECK(t.start == 111, "restart: start is now");
	CHECK(!timer_expired(&t), "restart: not expired");

	timer_set(&t, 0);
	CHECK(timer_expired(&t), "interval 0 expires at once");

	printf("\n%d checks, %d failed\n", tests_run, tests_failed);
	return tests_failed ? 1 : 0;
}
