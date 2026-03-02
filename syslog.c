#include "machine.h"
#include "syslog.h"
#include "uip/uip.h"
#include "rtl837x_common.h"

#pragma codeseg BANK2
#pragma constseg BANK2

#define SYSLOG_P ((__xdata uint8_t *)uip_appdata)

__xdata char logbuf[LOGBUF_SIZE];
__xdata struct syslog_state syslog_state;
__xdata uip_ipaddr_t server_ip;

#define state syslog_state 

void syslog_init(void) __banked
{
	state.enabled = 0;
	state.syslog_conn = 0;
	state.writeptr = 0;
	state.readptr = 0;
	state.line_available = 0;
	state.server_ip[0] = 0; state.server_ip[1] = 0; state.server_ip[2] = 0; state.server_ip[3] = 0;// Default to 0.0.0.0
}

void syslog_start(void) __banked
{
	if (state.syslog_conn == 0) {
		uip_ipaddr(server_ip, state.server_ip[0], state.server_ip[1], state.server_ip[2], state.server_ip[3]);
		state.syslog_conn = uip_udp_new(&server_ip, HTONS(514));
		if (state.syslog_conn == 0) {
			print_string_no_syslog("Failed to create a new UDP client\n");
			return;
		}
		print_string_no_syslog("Started syslog to IP ");
		itoa(state.server_ip[0]); write_char('.'); itoa(state.server_ip[1]); write_char('.');
		itoa(state.server_ip[2]); write_char('.'); itoa(state.server_ip[3]); write_char('\n');
		state.enabled = 1;
	}
	else {
		print_string_no_syslog("Syslog is already running\n");
	}
}

void syslog_stop(void) __banked
{
	state.enabled = 0;
	if (state.syslog_conn != 0) {
		uip_udp_remove(state.syslog_conn);
		state.syslog_conn = 0;
		print_string_no_syslog("Stopped syslog\n");
	} else {
		print_string_no_syslog("Syslog is not running\n");
	}
}

void syslog_callback(uint16_t lport) __banked
{
	if (lport != state.syslog_conn->lport)
		return;

	if ((state.readptr != state.writeptr) && state.line_available)
	{
		int16_t log_size = state.writeptr - state.readptr;
		if (log_size < 0)
			log_size += LOGBUF_SIZE;
		
		// Skipping linefeeds at the start of the log line
		uint16_t log_start = state.readptr;
		while (log_size > 0 && logbuf[log_start] == '\n') {
			log_start = (log_start + 1) & (LOGBUF_SIZE - 1);
			log_size--;
		}

		// Skipping linefeeds and whitespaces at the end of the log line
		uint16_t log_end = state.writeptr;
		while ( (log_size > 0) && 
				((logbuf[(log_end-1) & (LOGBUF_SIZE - 1)] == '\n') ||
				 (logbuf[(log_end-1) & (LOGBUF_SIZE - 1)] == ' ')))
		{
			log_end = (log_end - 1) & (LOGBUF_SIZE - 1);
			log_size--;
		}

		if (log_size == 0) {
			state.readptr = state.writeptr;
			state.line_available = 0;
			return;
		}

		memcpyc(SYSLOG_P, "<14>", 4); // Syslog priority prefix

		if (log_end < log_start) {
			memcpy(SYSLOG_P + 4, logbuf + log_start, LOGBUF_SIZE - log_start);
			memcpy(SYSLOG_P + 4 + LOGBUF_SIZE - log_start, logbuf, log_end);
		} else {
			 memcpy(SYSLOG_P + 4, logbuf + log_start, log_end - log_start);
		}

		uip_udp_send(log_size+4);
		state.readptr = state.writeptr;
		state.line_available = 0;
	}
}
