#include <stdint.h>
#include <stdio.h>

#include "health_idle.h"
#include <test/support.h>

#define PER_TICK 52083u

static void add(uint16_t tick_s, uint16_t t2_s, uint16_t tick_e, uint16_t t2_e)
{
	health_t2_per_tick = PER_TICK;
	health_sleep_tick = tick_s;
	health_sleep_t2 = t2_s;
	health_now_tick = tick_e;
	health_now_t2 = t2_e;
	health_idle_add();
}

static uint8_t pct(uint16_t idle, uint16_t sub, uint16_t window)
{
	health_t2_per_tick = PER_TICK;
	health_idle_ticks = idle;
	health_idle_sub = sub;
	health_window = window;
	return health_busy_pct();
}

static void reset(void)
{
	health_idle_ticks = 0;
	health_idle_sub = 0;
}

static void t_asleep(void)
{
	printf("[test] time asleep from two timer2 stamps\n");
	reset(); add(10, 14453, 10, 19453);
	CHECK(health_idle_ticks == 0 && health_idle_sub == 5000, "within one tick: the count difference");
	reset(); add(10, 50000, 11, 16453);
	CHECK(health_idle_ticks == 0 && health_idle_sub == 18536, "across a reload: rest of the tick plus the new count");
	reset(); add(10, 40000, 13, 40000);
	CHECK(health_idle_ticks == 3 && health_idle_sub == 0, "three whole ticks");
	reset(); add(0xffff, 30000, 0x0001, 30000);
	CHECK(health_idle_ticks == 2 && health_idle_sub == 0, "the 16-bit tick wraps between the stamps");
	reset(); add(7, 13453, 7, 13453);
	CHECK(health_idle_ticks == 0 && health_idle_sub == 0, "woken at once");
	reset(); add(1, 13453, 1, 43453); add(2, 13453, 2, 43453);
	CHECK(health_idle_ticks == 1 && health_idle_sub == 7917, "two sleeps of 30000 counts carry into a tick");
	reset(); add(1, 13453, 1, 13463); add(2, 50000, 3, 16453);
	CHECK(health_idle_ticks == 0 && health_idle_sub == 18546, "a borrow across the reload with a small remainder in hand");
}

static void t_pct(void)
{
	printf("[test] busy percentage of a window\n");
	CHECK(pct(0, 0, 200) == 100, "never slept: 100");
	CHECK(pct(200, 0, 200) == 0, "slept the whole second: 0");
	CHECK(pct(190, 0, 200) == 5, "190 of 200 ticks asleep: 5");
	CHECK(pct(190, PER_TICK / 2 + 1, 200) == 4, "half a tick rounds to the nearest tick");
	CHECK(pct(500, 0, 1000) == 50, "a window stretched to 5 s by a stall still gives a percentage");
	CHECK(pct(300, 0, 200) == 0, "more sleep than window is clamped, not wrapped");
}

int main(void)
{
	printf("== health idle tests ==\n");
	t_asleep();
	t_pct();
	printf("\n%d checks, %d failed\n", tests_run, tests_failed);
	return tests_failed ? 1 : 0;
}
