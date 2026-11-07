#include <rtlos_hal.h>
#include <8051.h>
#include <rtl837x_sfr.h>

extern volatile __xdata uint32_t time_sec;
// extern volatile __xdata uint16_t sleep_ticks;

uint32_t get_time_sec(void)
{
	uint32_t ret_time;
	__bit	save_ea;
	save_ea = EA;	// save global IRQ
	EA = 0;
	ret_time = time_sec;
	EA = save_ea;	// restore global IRQ
	return ret_time;
}

/*
void delay (uint16_t t)
{
	__bit	save_ea;
	uint16_t shadow_sleep_tick;
	save_ea = EA;	// save global IRQ
	EA = 0;
	sleep_ticks = t;
	EA = save_ea;	// restore global IRQ
	shadow_sleep_tick = t;
	while ( shadow_sleep_tick > 0 ) {
		PCON |= 1;	// cpu gating
		while ( shadow_sleep_tick != sleep_ticks ) shadow_sleep_tick = sleep_ticks;
	}
}
*/
