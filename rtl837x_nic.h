#ifndef RTL837X_NIC_H
#define RTL837X_NIC_H

#include <stdbool.h>
#include <stdint.h>

bool nic_rx_header(uint16_t ring_ptr) __reentrant;
bool nic_rx_packet(__xdata uint8_t *buffer, uint16_t ring_ptr) __reentrant;

/* One physical DMA transfer, in eight-byte units, without crossing the RX
 * ring boundary. Implemented beside the SFR access in rtlplayground.c. */
bool nic_rx_dma(__xdata uint8_t *buffer, uint16_t ring_ptr, uint8_t words) __reentrant;

#endif
