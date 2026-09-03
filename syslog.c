#include "machine.h"
#include "syslog.h"
#include "cmd_parser.h"
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
	state.server_port = SYSLOG_PORT_DEFAULT;
}

void syslog_start(void) __banked
{
	if (state.syslog_conn == 0) {
		uip_ipaddr(server_ip, state.server_ip[0], state.server_ip[1], state.server_ip[2], state.server_ip[3]);
		state.syslog_conn = uip_udp_new(&server_ip, HTONS(state.server_port));
		if (state.syslog_conn == 0) {
			print_string_newline_no_syslog("Failed to create a new UDP client");
			return;
		}
		print_string_newline_no_syslog("Started syslog to IP ");
		print_ip(state.server_ip);
		write_char(':'); itoa_short(state.server_port); write_char('\n');
		state.enabled = 1;
	}
	else {
		print_string_newline_no_syslog("Syslog is already running");
	}
}

void syslog_stop(void) __banked
{
	state.enabled = 0;
	if (state.syslog_conn != 0) {
		uip_udp_remove(state.syslog_conn);
		state.syslog_conn = 0;
		print_string_newline_no_syslog("Stopped syslog");
	} else {
		print_string_newline_no_syslog("Syslog is not running");
	}
}

void syslog_callback(uint16_t lport) __banked
{
	uint16_t syslog_hdr;
	if (lport != state.syslog_conn->lport)
		return;

	if ((state.readptr != state.writeptr) && state.line_available)
	{
		__xdata int16_t log_size = state.writeptr - state.readptr;
		if (log_size < 0)
			log_size += LOGBUF_SIZE;
		
		// Skipping linefeeds at the start of the log line
		__xdata uint16_t log_start = state.readptr;
		while (log_size > 0 && logbuf[log_start] == '\n') {
			log_start = (log_start + 1) & (LOGBUF_SIZE - 1);
			log_size--;
		}

		// Skipping linefeeds and whitespaces at the end of the log line
		__xdata uint16_t log_end = state.writeptr;
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

		/* RFC 3164 puts a hostname between the priority and the text, and
		 * we were leaving that slot empty. A receiver still has to fill
		 * the field, so it takes the first word of the message instead:
		 * every line arrives attributed to "STP:" or "REGGET:" or
		 * whatever the log happens to start with, and the sender cannot
		 * be selected on at all. Send our own name and the field means
		 * something. Skipped when the name is empty, so we never emit a
		 * lone separator. */
		syslog_hdr = strlen_x(hostname);
		if (syslog_hdr) {
			memcpy(SYSLOG_P + 4, hostname, syslog_hdr);
			SYSLOG_P[4 + syslog_hdr] = ' ';
			syslog_hdr += 5;
		} else {
			syslog_hdr = 4;
		}

		if (log_end < log_start) {
			memcpy(SYSLOG_P + syslog_hdr, logbuf + log_start, LOGBUF_SIZE - log_start);
			memcpy(SYSLOG_P + syslog_hdr + LOGBUF_SIZE - log_start, logbuf, log_end);
		} else {
			 memcpy(SYSLOG_P + syslog_hdr, logbuf + log_start, log_end - log_start);
		}

		uip_udp_send(log_size + syslog_hdr);
		state.readptr = state.writeptr;
		state.line_available = 0;
	}
}
