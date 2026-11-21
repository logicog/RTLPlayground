#ifndef _RTL8221_PTP_H_
#define _RTL8221_PTP_H_

#include <stdint.h>
#include "rtl837x_ptp.h"
#include "machine_modules.h"

#ifdef RTL8221_PTP
void ptp_rtl8221_init(__code const struct ptp_module *self) __banked;
void ptp_rtl8221_read_time(__code const struct ptp_module *self) __banked;
void ptp_rtl8221_get_time(__code const struct ptp_module *self) __banked;
void ptp_rtl8221_set_time(__code const struct ptp_module *self) __banked;
#endif

#endif
