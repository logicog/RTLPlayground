#ifndef _RTL837X_TRAP_H_
#define _RTL837X_TRAP_H_

#include <stdint.h>
#include "rtl837x_common.h"

#define TRAP_RX_HDR_SIZE	(12 + RTL_TAG_SIZE)

#define TRAP_RX_BODY() \
	((uip_buf[TRAP_RX_HDR_SIZE] == 0x81 && uip_buf[TRAP_RX_HDR_SIZE + 1] == 0x00) \
	 ? TRAP_RX_HDR_SIZE + VLAN_TAG_SIZE : TRAP_RX_HDR_SIZE)

void trap_init(void) __banked;
void trap_rma_set(uint16_t reg, uint8_t act) __banked;

#endif
