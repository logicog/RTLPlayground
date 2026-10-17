#include "rtl837x_common.h"
#include "rtl837x_regs.h"
#include "rtl837x_nic.h"

extern __xdata uint8_t rx_headers[16];

static bool nic_rx_copy(__xdata uint8_t *buffer, uint16_t ring_ptr, uint8_t words) __reentrant
{
	uint8_t chunk;
	uint16_t bytes;

	if (ring_ptr > RTL837X_NIC_RX_RING_BYTES || (ring_ptr & 7))
		return false;
	if (ring_ptr == RTL837X_NIC_RX_RING_BYTES)
		ring_ptr = 0;

	while (words) {
		chunk = words;
		bytes = RTL837X_NIC_RX_RING_BYTES - ring_ptr;
		if (((uint16_t)chunk << 3) > bytes)
			chunk = bytes >> 3;
		if (!nic_rx_dma(buffer, ring_ptr, chunk))
			return false;
		bytes = (uint16_t)chunk << 3;
		buffer += bytes;
		ring_ptr += bytes;
		if (ring_ptr == RTL837X_NIC_RX_RING_BYTES)
			ring_ptr = 0;
		words -= chunk;
	}
	return true;
}


bool nic_rx_header(uint16_t ring_ptr) __reentrant
{
	if (ring_ptr >= RTL837X_NIC_RX_RING_BYTES)
		return false;
	return nic_rx_copy(rx_headers, ring_ptr, 1);
}


bool nic_rx_packet(__xdata uint8_t *buffer, uint16_t ring_ptr) __reentrant
{
	uint16_t len = ((uint16_t)rx_headers[5] << 8) | rx_headers[4];
	uint16_t rounded;
	uint16_t whole;

	if (!len || len > UIP_CONF_BUFFER_SIZE)
		return false;
	rounded = (len + 7) & ~((uint16_t)7);
	if (rounded <= sizeof(uip_buf))
		return nic_rx_copy(buffer, ring_ptr, rounded >> 3);

	/* At the maximum frame size the final DMA word exceeds the two spare
	 * bytes in uip_buf. Use the unused half of rx_headers for that word and
	 * copy only its actual payload, preserving the descriptor's first half. */
	whole = len & ~((uint16_t)7);
	if (!nic_rx_copy(buffer, ring_ptr, whole >> 3))
		return false;
	ring_ptr += whole;
	if (ring_ptr >= RTL837X_NIC_RX_RING_BYTES)
		ring_ptr -= RTL837X_NIC_RX_RING_BYTES;
	if (!nic_rx_copy(rx_headers + 8, ring_ptr, 1))
		return false;
	memcpy(buffer + whole, rx_headers + 8, len - whole);
	return true;
}
