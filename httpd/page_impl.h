#ifndef __PAGE_IMPL_H__
#define __PAGE_IMPL_H__

#include <stdbool.h>

extern __code const uint8_t * __code const HTTP_RESPONCE_TXT;

void itoa16_html(uint16_t v);

bool send_counters(uint8_t phys_port);
void send_status(void);
void send_vlan(uint16_t vlan);
void send_basic_info(void);
void send_bandwidth(void);
void send_storm(void);

// "eee" of a copper port in /status.json
#define EEE_ADV_100M	0x01	// we advertise EEE at 100M
#define EEE_ADV_1G	0x02	// ... at 1G
#define EEE_ADV_2G5	0x04	// ... at 2.5G
#define EEE_LP_100M	0x08	// the link partner advertises EEE at 100M
#define EEE_LP_1G	0x10	// ... at 1G
#define EEE_LP_2G5	0x20	// ... at 2.5G
#define EEE_ACTIVE	0x40	// EEE is in use on the link

void send_l2(uint16_t idx);
void l2_delete(uint16_t idx);
void send_mirror(void);
void send_mtu(void);
void send_isolation(void);
void send_config(void);
void send_cmd_log(void);
void send_lag(void);
void send_stp(void);
void send_lldp(void);
void send_lacp(void);
void send_vlanlist(void);

/*  Convert only the lower nibble to ascii HEX char.
    For convenience the upper nibble is masked out.
*/
inline char itohex(uint8_t val) {
	// Ignore upper nibble for convenience.
	val &= 0x0f;
	val -= 10;

	// 10 or above
	if ((int8_t)val >= 0)
		val += ('a' - '0' - 10);

	return val + ('0' + 10);
}

#endif
