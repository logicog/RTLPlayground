#ifndef __PAGE_IMPL_H__
#define __PAGE_IMPL_H__

void send_counters(char port);
void send_status(void);
void send_vlan(uint16_t vlan);
void send_basic_info(void);
void send_eee(void);
void send_mirror(void);
void send_mtu(void);
void send_config(void);
void send_cmd_log(void);
void send_lag(void);

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
