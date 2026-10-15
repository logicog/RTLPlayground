/* SDCC has byte-aligned structures. Match only DHCP's wire packet structure
 * on the host; preload public headers so shared ABI structures remain native. */
#include "../rtl837x_common.h"
#include "../dhcp.h"
#include "../uip/uip.h"
#pragma pack(push, 1)
#include "../dhcp.c"
#pragma pack(pop)
