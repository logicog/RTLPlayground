/*
 * Host-build shim for rtl837x_sfr.h - SFR access is hardware-only, so the
 * read is mocked: TX-complete and table-busy polls see "done" at once, which
 * keeps the bounded wait loops from spinning out their guard counters.
 */
#ifndef _SHIM_RTL837X_SFR_H_
#define _SHIM_RTL837X_SFR_H_

#include <stdint.h>

extern unsigned char sfr_data[4];
#define reg_read_m(r) do { (void)(r); sfr_data[3] = 0; } while (0)
/* A write of the mocked register block is recorded by the harness. */
void reg_write_m(uint16_t reg);

#endif
