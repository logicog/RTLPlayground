#include "machine.h"
#include "syslog.h"
#include "uip/uip.h"
#include "rtl837x_common.h"

#pragma codeseg BANK2
#pragma constseg BANK2

#define SYSLOG_P ((__xdata uint8_t *)uip_appdata)

__xdata char logbuf[LOGBUF_SIZE];
__xdata uint16_t logptr_w ;
__xdata uint16_t logptr_r;
__xdata uint8_t full_line_available;
__xdata uint8_t syslog_enabled;
__xdata uip_ipaddr_t syslog_addr;

struct uip_udp_conn *syslog_conn;

void syslog_init(void) __banked
{
	syslog_enabled = 0;
	syslog_conn = 0;
	logptr_w = 0;
	logptr_r = 0;
	full_line_available = 0;
	syslog_addr[0] = 0; syslog_addr[1] = 0; // Default to 0.0.0.0
}

void syslog_start(void) __banked
{
	if (syslog_conn == 0) {
		syslog_conn = uip_udp_new(&syslog_addr, HTONS(514));
		if (syslog_conn == 0) {
			print_string("Failed to create a new UDP client\n");
			return;
		}
		print_string("Started syslog to IP ");
		itoa(syslog_addr[0]); write_char('.'); itoa(syslog_addr[0]>>8); write_char('.');
		itoa(syslog_addr[1]); write_char('.'); itoa(syslog_addr[1]>>8); write_char('\n');

		syslog_enabled = 1;
	}
	else {
		print_string("Syslog is already running\n");
	}
}

void syslog_stop(void) __banked
{
	syslog_enabled = 0;
	if (syslog_conn != 0) {
		uip_udp_remove(syslog_conn);
		syslog_conn = 0;
		print_string("Stopped syslog\n");
	} else {
		print_string("Syslog is not running\n");
	}
}

void syslog_callback(void) __banked
{
	if ((logptr_r != logptr_w) && full_line_available)
	{
		int16_t log_size = logptr_w - logptr_r;
		if (log_size < 0)
			log_size += LOGBUF_SIZE;
		
		// Skipping linefeeds at the start of the log line
		uint16_t log_start = logptr_r;
		while (log_size > 0 && logbuf[log_start] == '\n') {
			log_start = (log_start + 1) & (LOGBUF_SIZE - 1);
			log_size--;
		}

		// Skipping linefeeds and whitespaces at the end of the log line
		uint16_t log_end = logptr_w;
		while ( (log_size > 0) && 
				((logbuf[(log_end-1) & (LOGBUF_SIZE - 1)] == '\n') ||
				 (logbuf[(log_end-1) & (LOGBUF_SIZE - 1)] == ' ')))
		{
			log_end = (log_end - 1) & (LOGBUF_SIZE - 1);
			log_size--;
		}

		if (log_size == 0) {
			logptr_r = logptr_w;
			full_line_available = 0;
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
		logptr_r = logptr_w;
		full_line_available = 0;
	}
}
