/*
 * Minimal SNTP (RFC 4330) client. The switch has no RTC, so wall-clock
 * time only exists after a reply from the configured server: unix time
 * is then unix_at_sync + (SEC_COUNTER - boot_at_sync). Re-synced every
 * 4 hours; retried every 10 seconds while unsynced.
 */
#include "machine.h"
#include "sntp.h"
#include "rtl837x_common.h"
#include "cmd_parser.h"
#include "uip.h"

#pragma codeseg BANK3
#pragma constseg BANK3

__xdata struct sntp_state_t sntp_state;
static __xdata uip_ipaddr_t sntp_server_addr;
static __xdata uint32_t sntp_now;

#define st sntp_state

#define SNTP_RESYNC	14400UL
#define SNTP_RETRY	10UL
/* Unix epoch 1970-01-01 minus NTP epoch 1900-01-01 in seconds */
#define NTP_TO_UNIX	2208988800UL


void sntp_init(void) __banked
{
	st.enabled = 0;
	st.synced = 0;
	st.conn = 0;
	st.server[0] = 0; st.server[1] = 0; st.server[2] = 0; st.server[3] = 0;
	st.poll_ticks = 1;
}


void sntp_start(void) __banked
{
	if (st.conn) {
		uip_udp_remove(st.conn);
		st.conn = 0;
	}
	uip_ipaddr(sntp_server_addr, st.server[0], st.server[1], st.server[2], st.server[3]);
	st.conn = uip_udp_new(&sntp_server_addr, HTONS(123));
	if (st.conn == 0) {
		print_string("Failed to create a new UDP client\n");
		return;
	}
	st.enabled = 1;
	st.next_poll = 0;	/* request immediately */
	print_string("SNTP polling ");
	print_ip(st.server);
	write_char('\n');
}


void sntp_stop(void) __banked
{
	st.enabled = 0;
	if (st.conn) {
		uip_udp_remove(st.conn);
		st.conn = 0;
		print_string("SNTP stopped\n");
	} else {
		print_string("SNTP is not running\n");
	}
}


uint32_t sntp_unix_now(void) __banked
{
	if (!st.synced)
		return 0;
	read_reg_timer(&sntp_now);
	sntp_now -= st.boot_at_sync;
	sntp_now += st.unix_at_sync;
	return sntp_now;
}


void sntp_callback(uint16_t lport) __banked
{
	static __xdata uint8_t * __xdata p;

	p = uip_appdata;

	if (!st.conn || lport != st.conn->lport)
		return;

	if (uip_newdata()) {
		/* Mode must be server (4) or broadcast (5), stratum nonzero */
		if (uip_len < 44 || (p[0] & 0x07) < 4 || p[1] == 0)
			return;
		/* Transmit timestamp, seconds part, at offset 40 */
		st.unix_at_sync = ((uint32_t)p[40] << 24) | ((uint32_t)p[41] << 16)
				| ((uint32_t)p[42] << 8) | p[43];
		st.unix_at_sync -= NTP_TO_UNIX;
		read_reg_timer(&sntp_now);
		st.boot_at_sync = sntp_now;
		st.next_poll = sntp_now + SNTP_RESYNC;
		if (!st.synced)
			print_string("SNTP: time synchronized\n");
		st.synced = 1;
		return;
	}

	if (!st.enabled)
		return;

	/* Rate-limit the SEC_COUNTER read to once a second */
	if (--st.poll_ticks)
		return;
	st.poll_ticks = SYS_TICK_HZ;

	read_reg_timer(&sntp_now);
	if (sntp_now < st.next_poll)
		return;
	st.next_poll = sntp_now + (st.synced ? SNTP_RESYNC : SNTP_RETRY);

	for (uint8_t i = 0; i < 48; i++)
		p[i] = 0;
	p[0] = 0x23;	/* LI=0, VN=4, Mode=3 (client) */
	uip_udp_send(48);
}


void sntp_status_print(void) __banked
{
	print_string("SNTP: ");
	if (!st.enabled) {
		print_string("disabled\n");
		return;
	}
	print_string("server ");
	print_ip(st.server);
	if (st.synced) {
		print_string(", synced, unix time: ");
		print_long(sntp_unix_now());
	} else {
		print_string(", not synced yet");
	}
	write_char('\n');
}
