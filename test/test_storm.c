#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "rtl837x_common.h"
#include "rtl837x_regs.h"
#include "rtl837x_storm.h"
#include "rtl837x_meter.h"
#include "support.h"
#include "hw_mock.h"

static uint8_t midx(uint8_t port, uint8_t type)
{
	uint32_t w = hw_reg_get(RTL837X_STORM_MIDX + type * 8 + (port / 5) * 4);

	return (w >> ((port % 5) * 6)) & 0x3f;
}

static int enabled(uint8_t port, uint8_t type)
{
	return (hw_reg_get(RTL837X_STORM_CTRL + type * 4) >> port) & 1;
}

static uint32_t rate(uint8_t m)
{
	return hw_reg_get(RTL837X_METER_RATE + m * 4);
}

static int pps(uint8_t m)
{
	return (hw_reg_get(RTL837X_METER_MODE + (m / 32) * 4) >> (m % 32)) & 1;
}

static void t_alloc(void)
{
	printf("[test] storm takes meters from the top as limits are set\n");
	hw_reset();
	CHECK(storm_set(3, 3, 1000, 0) && enabled(3, 3) && midx(3, 3) == 63 && rate(63) == 1000 && !pps(63),
	      "the first limit takes meter 63");
	CHECK(storm_set(6, 0, 500, 1) && enabled(6, 0) && midx(6, 0) == 62 && rate(62) == 500 && pps(62),
	      "the second limit takes meter 62");
	CHECK(storm_set(4, 3, 300, 0) && midx(4, 3) == 61 && midx(3, 3) == 63,
	      "a port in the same MIDX word keeps its own meter");
	CHECK(storm_set(3, 3, 2000, 0) && midx(3, 3) == 63 && rate(63) == 2000 && rate(61) == 300,
	      "changing a limit keeps its meter");
	CHECK(storm_meter(6, 0) == 62, "storm_meter reads the meter back");
	storm_off(3, 3);
	CHECK(!enabled(3, 3) && enabled(4, 3), "off clears only its own enable bit");
	CHECK(storm_set(1, 1, 100, 0) && midx(1, 1) == 63, "a freed meter is taken again");
	storm_off(1, 1);
	storm_off(1, 1);
	CHECK(storm_set(2, 2, 100, 0) && midx(2, 2) == 63, "off on a limit that is already off frees nothing twice");
	storm_off(2, 2);
	storm_off(4, 3);
	storm_off(6, 0);
}

static void t_full(void)
{
	uint8_t m;

	printf("[test] storm refuses a limit when no meter is free\n");
	hw_reset();
	for (m = 0; m < METERS; m++)
		meter_claim(m);
	CHECK(meter_alloc() == METER_NONE, "every meter is taken");
	CHECK(!storm_set(5, 1, 100, 0) && !enabled(5, 1), "a limit without a meter is not enabled");
	meter_release(10);
	CHECK(storm_set(5, 1, 100, 0) && midx(5, 1) == 10, "a released meter is used");
}

int main(void)
{
	printf("== rtl837x_storm.c meter tests ==\n");
	t_alloc();
	t_full();
	printf("\n%d checks, %d failed\n", tests_run, tests_failed);
	return tests_failed ? 1 : 0;
}
