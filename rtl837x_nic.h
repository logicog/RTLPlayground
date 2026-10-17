#ifndef RTL837X_NIC_H
#define RTL837X_NIC_H

#include <stdbool.h>
#include <stdint.h>

// #define RXTXDBG 1

bool nic_rx_header(uint16_t ring_ptr) __reentrant;
bool nic_rx_packet(__xdata uint8_t *buffer, uint16_t ring_ptr) __reentrant;

/* buffer starts at the eight-byte NIC descriptor; len excludes that
 * descriptor. ring_ptr is the ASIC TX position in eight-byte slots. */
void nic_tx_packet(__xdata uint8_t *buffer, uint16_t len, uint16_t ring_ptr);

/* RX DMA in eight-byte units. ring_ptr must start inside the configured
 * RX ring; the hardware wraps transfers that cross its end. */
bool nic_rx_dma(__xdata uint8_t *buffer, uint16_t ring_ptr, uint8_t words) __reentrant;

#endif
