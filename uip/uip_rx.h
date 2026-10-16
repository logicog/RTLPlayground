#ifndef UIP_RX_H
#define UIP_RX_H
/* The NIC DMA rounds to eight bytes; reject before programming its length. */
#define uip_rx_dma_fits(length) ((length) <= ((UIP_BUFSIZE + 2u) & ~7u))
/* Driver input is a whole Ethernet frame. Preserve ARP-before-IP ordering;
 * uIP itself expects the captured IP byte count, excluding the link header. */
#define uip_rx_ip_input() do { \
	if (uip_len < UIP_LLH_LEN + UIP_IPH_LEN || uip_len > UIP_BUFSIZE) \
		uip_len = 0; \
	else { \
		uip_arp_ipin(); \
		uip_len -= UIP_LLH_LEN; \
		uip_input(); \
	} \
} while (0)
#endif
