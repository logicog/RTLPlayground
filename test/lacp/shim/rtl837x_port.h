/* Host-build shim: the harness mocks trunk programming and records calls */
#ifndef _SHIM_RTL837X_PORT_H_
#define _SHIM_RTL837X_PORT_H_
#include <stdint.h>
void port_lag_members_set(uint8_t lag, uint16_t members);
void port_isolate(uint8_t port, uint16_t pmask);
#endif
