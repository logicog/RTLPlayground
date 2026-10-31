#ifndef _RTLOS_HAL_H_
#define _RTLOS_HAL_H_

#include "machine.h"

extern volatile __xdata uint8_t sfr_data[4];

// put time sec into sfr_data
void get_time_sec(void);
void read_reg_timer(__xdata uint32_t * tmr);

// Delay for given number of ticks without doing housekeeping
// actual delay is between t-1 to t ticks
void delay(uint16_t t);

#endif
