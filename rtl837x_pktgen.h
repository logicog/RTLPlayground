#ifndef _RTL837X_PKTGEN_H_
#define _RTL837X_PKTGEN_H_

#include <stdint.h>
#include <stdbool.h>

#define PKTGEN_COUNT 4

void pktgen_show(void) __banked __reentrant;
void pktgen_sel(uint8_t g, uint8_t sel) __banked __reentrant;
void pktgen_clock(uint8_t g, bool xg) __banked __reentrant;
void pktgen_burst(uint8_t g, uint16_t count, uint16_t size) __banked __reentrant;
void pktgen_gap(uint8_t g, uint16_t len) __banked __reentrant;
void pktgen_len(uint8_t g, uint16_t start, uint16_t end, uint8_t mode) __banked __reentrant;
void pktgen_type(uint8_t g, uint16_t ethertype, bool on) __banked __reentrant;
void pktgen_payload_mode(uint8_t g, uint8_t mode, uint8_t mask) __banked __reentrant;
void pktgen_mac(uint8_t g, bool dst, __xdata const uint8_t *mac) __banked __reentrant;
void pktgen_payload_write(uint8_t addr, __xdata const uint8_t *data, uint8_t len) __banked __reentrant;
void pktgen_payload_read(uint8_t addr) __banked __reentrant;
void pktgen_run(uint8_t g, bool on) __banked __reentrant;

#endif
