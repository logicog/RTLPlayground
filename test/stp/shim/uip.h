#ifndef _SHIM_UIP_H_
#define _SHIM_UIP_H_
#include <stdint.h>
#define UIP_CONF_BUFFER_SIZE 600
struct uip_eth_addr { uint8_t addr[6]; };
extern uint16_t uip_len;
void tcpip_output(void);
#endif
