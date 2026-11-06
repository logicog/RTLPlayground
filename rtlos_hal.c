#include <rtlos_hal.h>
#include <8051.h>

extern volatile __xdata uint32_t time_sec;

void get_time_sec(void)
{
	__bit	save_ea;
	__xdata uint8_t * ptr = & time_sec;
	save_ea = EA;	// save global IRQ
	EA = 0;
	sfr_data[0] = *(ptr + 3);
	sfr_data[1] = *(ptr + 2);
	sfr_data[2] = *(ptr + 1);
	sfr_data[3] = *ptr;
	EA = save_ea;	// restore global IRQ
}


void read_reg_timer(__xdata uint32_t * tmr)
{
	__bit	save_ea;
	save_ea = EA;	// save global IRQ
	EA = 0;
	*tmr = time_sec;
	EA = save_ea;	// restore global IRQ
}
