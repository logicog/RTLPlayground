#ifndef _HEALTH_IDLE_H_
#define _HEALTH_IDLE_H_

#include <stdint.h>

static __xdata uint16_t health_t2_per_tick;
static __xdata uint16_t health_sleep_tick;
static __xdata uint16_t health_sleep_t2;
static __xdata uint16_t health_now_tick;
static __xdata uint16_t health_now_t2;
static __xdata uint16_t health_idle_ticks;
static __xdata uint16_t health_idle_sub;
static __xdata uint16_t health_window;

static void health_idle_add(void)
{
	static __xdata uint16_t base;
	static __xdata uint16_t d;

	base = 0 - health_t2_per_tick;
	health_idle_ticks += health_now_tick - health_sleep_tick;
	if ((uint16_t)(health_now_t2 - base) >= (uint16_t)(health_sleep_t2 - base)) {
		d = health_now_t2 - health_sleep_t2;
		if (health_idle_sub >= health_t2_per_tick - d) {
			health_idle_sub -= health_t2_per_tick - d;
			health_idle_ticks++;
		} else {
			health_idle_sub += d;
		}
	} else {
		d = health_sleep_t2 - health_now_t2;
		if (health_idle_sub >= d) {
			health_idle_sub -= d;
		} else {
			health_idle_sub = health_t2_per_tick - (d - health_idle_sub);
			health_idle_ticks--;
		}
	}
}


static uint8_t health_busy_pct(void)
{
	static __xdata uint16_t idle;
	static __xdata uint16_t window;

	idle = health_idle_ticks;
	window = health_window;
	if (health_idle_sub >= health_t2_per_tick >> 1)
		idle++;
	if (idle > window)
		idle = window;
	while (window > 600) {
		window >>= 1;
		idle >>= 1;
	}
	return 100 - (uint8_t)((idle * 100 + (window >> 1)) / window);
}

#endif
