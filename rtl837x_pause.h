#ifndef _RTL837X_PAUSE_H_
#define _RTL837X_PAUSE_H_

#include <stdint.h>
#include <stdbool.h>

// port_pause_state() bits
#define PAUSE_ADV	0x01	// we advertise pause
#define PAUSE_ADV_ASYM	0x02	// we advertise asymmetric pause
#define PAUSE_LP	0x04	// the link partner offers pause
#define PAUSE_LP_ASYM	0x08	// the link partner offers asymmetric pause
#define PAUSE_NEG_TX	0x10	// negotiated: the port sends pause frames
#define PAUSE_NEG_RX	0x20	// negotiated: the port honours pause frames

void port_pause_set(__xdata uint8_t port, __xdata bool on) __banked;
uint8_t port_pause_state(uint8_t port) __banked;

#endif
