#include "rtl837x_common.h"
#include "rtl837x_regs.h"
#include "rtl837x_sfr.h"
#include "rtl837x_nic.h"

extern __xdata uint8_t rx_headers[16];


#ifndef RTLP_HOST_TEST
/* One contiguous RX DMA operation. Ring wrapping is handled by the RX
 * copy functions, so this operation always starts and ends inside the ring.
 * Host tests supply a simulated DMA engine for this hardware boundary. */
bool nic_rx_dma(__xdata uint8_t *buffer, uint16_t ring_ptr, uint8_t words) __reentrant
{
	uint16_t guard = 0;

	SFR_NIC_DATA_U16LE = (uint16_t)buffer;
	SFR_NIC_RING_U16LE = ring_ptr;
	SFR_NIC_CTRL = words;
	while (SFR_NIC_CTRL != 0) {
		if (++guard == 0) {
			print_string("NIC: RX transfer did not complete\n");
			return false;
		}
	}
	return true;
}
#endif

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


/*
 * Transfers data in XMEM to the ASIC for transmission by the NIC
 */
void nic_tx_packet(__xdata uint8_t *buffer, uint16_t len, uint16_t ring_ptr)
{
	uint16_t guard = 0;

	SFR_NIC_DATA_U16LE = (uint16_t)(uintptr_t)buffer;

#ifdef RXTXDBG
	print_string("TX: \n");
	for (uint8_t i = 0; i < 100; i++) {
		print_byte(uip_buf[i]);
		write_char(' ');
	}
	write_char('\n');
#endif

	ring_ptr <<= 3;
	ring_ptr |= 0x8000;
	SFR_NIC_RING_U16LE = ring_ptr;

	len += 0xf;
	len >>= 3;
	SFR_NIC_CTRL = len;
	while (SFR_NIC_CTRL != 0) {
		if (++guard == 0) {
			print_string("NIC: TX transfer did not complete\n");
			return;
		}
	}
}
