#ifndef _RTL837X_RLDP_H_
#define _RTL837X_RLDP_H_

#include <stdint.h>

#define RLDP_BLOCK_SECS	60
#define RLDP_BLOCK_MAX_SHIFT	2

extern __xdata uint8_t rldp_on;
extern __xdata uint16_t rldp_off_mask;
extern __xdata uint8_t rldp_block[10];

void rldp_init(void) __banked;
void rldp_enable(uint8_t on) __banked;
void rldp_port(uint8_t port, __xdata uint8_t on) __banked;
void rldp_tick(void) __banked;
void rldp_show(void) __banked;

#endif
