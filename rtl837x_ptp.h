#ifndef _RTL837X_PTP_H_
#define _RTL837X_PTP_H_

#include <stdbool.h>

// Parse the clock and timestamp commands for RTL837x, RTL8221 or RTL8224.
void ptp_parse(bool phy) __banked;
void ptp_rtl8224_init(void) __banked;
void ptp_rtl8224_get_time(void) __banked;
void ptp_rtl8224_set_time(void) __banked;

#endif
