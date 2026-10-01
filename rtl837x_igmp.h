#ifndef _RTL837X_IGMP_H_
#define _RTL837X_IGMP_H_

#include <stdint.h>

#define IGMP_PROTO		2
#define IGMP_V1_REPORT		0x12
#define IGMP_V2_REPORT		0x16
#define IGMP_V2_LEAVE		0x17
#define IGMP_V3_REPORT		0x22
#define IGMP_V3_TO_INCLUDE	3
#define IGMP_V3_TO_EXCLUDE	4

void igmp_setup(void) __banked;
void igmp_enable(void) __banked;
void igmp_router_port_set(uint16_t pmask) __banked;
uint8_t igmp_packet_handler(void) __banked;
void igmp_show(void) __banked;

#endif
