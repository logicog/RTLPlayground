#ifndef _RTL837X_STP_H_
#define _RTL837X_STP_H_

#include <stdint.h>
void stp_in(void) __banked;
void stp_setup(void) __banked;
void stp_timers(void) __banked;
void stp_off(void) __banked;

#define TIME_HELLO 0x80 // 2 sec (stp_timers runs at ~64 Hz: main loop ~256 Hz / STP_TICK_DIVIDER+1)

/* Bridge identifier as carried in a BPDU (priority, extension, MAC). */
struct bridge {
	uint8_t prio;
	uint8_t ext;
	uint8_t mac[6];
};

/* Protocol state, exposed read-only for the web UI (page_impl.c send_stp())
 * - the elected root bridge and our path cost to it. Owned by rtl837x_stp.c;
 * stpEnabled is owned by rtlplayground.c. */
extern __xdata uint8_t stpEnabled;
extern __xdata struct bridge root_bridge;
extern __xdata uint32_t root_bridge_cost;

#endif
