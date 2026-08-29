#ifndef _LLDP_H_
#define _LLDP_H_

#include <stdint.h>

#define LLDP_ETHERTYPE       0x88cc
#define LLDP_DST_LEN         6
#define LLDP_SRC_LEN         6
#define LLDP_MAX_FRAME       256
#define LLDP_TX_INTERVAL_SEC 30

void lldp_on(void);
void lldp_off(void);

void lldp_init(const uint8_t mac[6], const char *system_name);
void lldp_tick(void);
uint16_t put_eth_header(uint8_t *p);
uint16_t lldp_put_tlv(uint8_t *p, uint8_t type, const uint8_t *value, uint8_t value_len);
uint16_t lldp_put_local_string_tlv(uint8_t *p, uint8_t type, const char *string);
uint16_t lldp_put_ttl_tlv(uint8_t *p, uint16_t ttl);
void lldp_send(void);

#endif
