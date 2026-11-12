#ifndef _RTL8224_PTP_H_
#define _RTL8224_PTP_H_

#include <stdint.h>
#include "rtl837x_ptp.h"

#ifdef RTL8224_PTP
#define PTP_CLK_FREQ 0x10000000

void ptp_rtl8224_init(__code const struct ptp_module *self) __banked;
void ptp_rtl8224_read_time(__code const struct ptp_module *self) __banked;
void ptp_rtl8224_get_time(__code const struct ptp_module *self) __banked;
void ptp_rtl8224_set_time(__code const struct ptp_module *self) __banked;
#endif

#endif
