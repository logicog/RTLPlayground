/* Generic NIC envelope guards, deliberately built without SNMP. */
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include "rtl837x_common.h"
#include "uip.h"
#include "uip/uip_rx.h"
uint16_t uip_len;
static unsigned order,arp_calls,ip_calls;
static uint16_t arp_len,ip_len;
void uip_arp_ipin(void) { assert(order++==0);arp_calls++;arp_len=uip_len; }
void uip_process(uint8_t flag) { (void)flag;assert(order++==1);ip_calls++;ip_len=uip_len; }
int main(void)
{
	const unsigned cap=(UIP_BUFSIZE+2u)&~7u;
	assert(uip_rx_dma_fits(0)&&uip_rx_dma_fits(cap));
	assert(!uip_rx_dma_fits(cap+1)&&!uip_rx_dma_fits(65535));
	for (unsigned n=0;n<UIP_LLH_LEN+UIP_IPH_LEN;n++) {
		uip_len=n;uip_rx_ip_input();assert(!uip_len&&!arp_calls&&!ip_calls);
	}
	uip_len=UIP_BUFSIZE+1;uip_rx_ip_input();assert(!uip_len&&!arp_calls);
	uip_len=UIP_LLH_LEN+UIP_IPH_LEN;uip_rx_ip_input();
	assert(order==2&&arp_len==UIP_LLH_LEN+UIP_IPH_LEN&&ip_len==UIP_IPH_LEN);
	order=0;uip_len=UIP_BUFSIZE;uip_rx_ip_input();
	assert(arp_calls==2&&ip_calls==2&&ip_len==UIP_BUFSIZE-UIP_LLH_LEN);
	puts("PASS generic RX guards: DMA rounding, truncated/oversized frames, ARP-before-IP order and exact captured IP length");
	return 0;
}
