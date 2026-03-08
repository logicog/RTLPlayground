#ifndef _RTL837X_BANDWIDTH_H_
#define _RTL837X_BANDWIDTH_H_

#include <stdint.h>

void bandwidth_setup(void) __banked;
void bandwidth_ingress_set(uint8_t port, __xdata uint32_t bw) __banked;
void bandwidth_ingress_disable(uint8_t port) __banked;
void bandwidth_ingress_drop(uint8_t port) __banked;
void bandwidth_ingress_fc(uint8_t port) __banked;
void bandwidth_egress_set(uint8_t port, __xdata uint32_t bw) __banked;
void bandwidth_egress_disable(uint8_t port) __banked;
void bandwidth_status(uint8_t port) __banked;

#endif
