#ifndef __SNTP_H__
#define __SNTP_H__

#include <stdint.h>
#include "uip.h"

struct sntp_state_t {
	uint8_t enabled;
	uint8_t synced;
	uint8_t server[4];
	__xdata struct uip_udp_conn *conn;
	uint32_t unix_at_sync;	/* unix seconds when the last reply arrived */
	uint32_t boot_at_sync;	/* SEC_COUNTER value at that moment */
	uint32_t next_poll;	/* SEC_COUNTER value of the next request */
	uint16_t poll_ticks;	/* system ticks until the next 1s check */
};

extern __xdata struct sntp_state_t sntp_state;

void sntp_init(void) __banked;
void sntp_start(void) __banked;	/* uses sntp_state.server */
void sntp_stop(void) __banked;
void sntp_callback(uint16_t lport) __banked;
/* Current unix time, 0 while not synced */
uint32_t sntp_unix_now(void) __banked;
void sntp_status_print(void) __banked;

#endif
