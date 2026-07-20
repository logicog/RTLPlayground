/* Host-build shim: SFR access is hardware-only; nothing needed for LACP logic */
#ifndef _SHIM_RTL837X_SFR_H_
#define _SHIM_RTL837X_SFR_H_
#endif

/* Host-build shim: register read is mocked; TX-complete polls see "done" at once */
extern unsigned char sfr_data[4];
#define reg_read_m(r) do { (void)(r); sfr_data[3] = 0; } while (0)
