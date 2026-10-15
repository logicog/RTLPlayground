/* The unchanged ARP unit retains an unused SDCC XDATA byte. Do not remove
 * it: that would change the default firmware memory layout. Suppress only
 * this pre-existing host warning within this included production unit. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#include "../uip/uip_arp.c"
#pragma GCC diagnostic pop
