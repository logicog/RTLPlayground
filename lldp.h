#ifndef _LLDP_H_
#define _LLDP_H_

#include <stdint.h>

#define LLDP_ETHERTYPE       0x88cc
#define LLDP_MAX_FRAME       256
#define LLDP_MAC_ADDR_LEN    6
#define LLDP_TX_INTERVAL_SEC 30

void lldp_init(void);
void lldp_tick(void);
void lldp_send(void);

extern uint8_t enable_lldp;

#endif
