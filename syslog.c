#include "machine.h"
#include "syslog.h"
#include "uip/uip.h"
#include "rtl837x_common.h"

#pragma codeseg BANK2
#pragma constseg BANK2

#define SYSLOG_O ((__xdata uint8_t *)&uip_buf[RTL_TAG_SIZE + VLAN_TAG_SIZE])

__xdata char logbuf[LOGBUF_SIZE];
__xdata uint16_t logptr_w = 0;
__xdata uint16_t logptr_r = 0;
__xdata uint8_t full_line_available = 0;
__xdata uint8_t syslog_enabled = 0;
__xdata uip_ipaddr_t syslog_addr;

#define DEST_OFFSET (0)
#define SOURCE_OFFSET (DEST_OFFSET + 6)
#define ETHERTYPE_OFFSET (SOURCE_OFFSET + 6)
#define IP_HEADER_OFFSET (ETHERTYPE_OFFSET + 2)
#define UDP_HEADER_OFFSET (IP_HEADER_OFFSET + 20)	
#define UDP_PAYLOAD_OFFSET (UDP_HEADER_OFFSET + 8)

void syslog_init(void) __banked
{
	syslog_enabled = 0;
	logptr_w = 0;
	logptr_r = 0;
	full_line_available = 0;
	syslog_addr[0] = 0xffff; syslog_addr[1] = 0xffff; // Default to broadcast
}

void handle_syslog(void) __banked
{
	if ((logptr_r != logptr_w) && full_line_available)
	{
		int16_t log_size = logptr_w - logptr_r;
		if (log_size < 0)
			log_size += LOGBUF_SIZE;
		
		if (log_size <= 4) {
			// Only some newline; not worth sending to syslog, skip it
			logptr_r = logptr_w;
			full_line_available = 0;
			return;
		}

		SYSLOG_O[DEST_OFFSET]     = 255;  SYSLOG_O[DEST_OFFSET + 1] = 255; SYSLOG_O[DEST_OFFSET + 2] = 255; 
		SYSLOG_O[DEST_OFFSET + 3] = 255;  SYSLOG_O[DEST_OFFSET + 4] = 255; SYSLOG_O[DEST_OFFSET + 5] = 255; // broadcast
		memcpy(SYSLOG_O + SOURCE_OFFSET, uip_ethaddr.addr, 6); // Source MAC address

		SYSLOG_O[ETHERTYPE_OFFSET] = 0x08; SYSLOG_O[ETHERTYPE_OFFSET + 1] = 0x00; // Ethertype: IPv4

		SYSLOG_O[IP_HEADER_OFFSET     ] = 0x45; SYSLOG_O[IP_HEADER_OFFSET +  1] = 0x00; // IPv4, no options
		SYSLOG_O[IP_HEADER_OFFSET +  2] = (20+8+4+log_size) >> 8; SYSLOG_O[IP_HEADER_OFFSET + 3] = (20+8+4+log_size) & 0xff; // Total Length (IP header + UDP header + payload)
		SYSLOG_O[IP_HEADER_OFFSET +  4] = 0x00; SYSLOG_O[IP_HEADER_OFFSET +  5] = 0x00; // Identification
		SYSLOG_O[IP_HEADER_OFFSET +  6] = 0x00; SYSLOG_O[IP_HEADER_OFFSET +  7] = 0x00; // Flags, Fragment Offset
		SYSLOG_O[IP_HEADER_OFFSET +  8] = 0x40; SYSLOG_O[IP_HEADER_OFFSET +  9] = 0x11; // TTL, Protocol (UDP)
		SYSLOG_O[IP_HEADER_OFFSET + 10] = 0x00; SYSLOG_O[IP_HEADER_OFFSET + 11] = 0x00; // Header Checksum (not calculated)
		SYSLOG_O[IP_HEADER_OFFSET + 12] = uip_hostaddr[0] & 0xff; SYSLOG_O[IP_HEADER_OFFSET + 13] = uip_hostaddr[0] >> 8; 
		SYSLOG_O[IP_HEADER_OFFSET + 14] = uip_hostaddr[1] & 0xff; SYSLOG_O[IP_HEADER_OFFSET + 15] = uip_hostaddr[1] >> 8;	// Source IP
		SYSLOG_O[IP_HEADER_OFFSET + 16] = syslog_addr[0] & 0xff;  SYSLOG_O[IP_HEADER_OFFSET + 17] = syslog_addr[0] >> 8; 
		SYSLOG_O[IP_HEADER_OFFSET + 18] = syslog_addr[1] & 0xff;  SYSLOG_O[IP_HEADER_OFFSET + 19] = syslog_addr[1] >> 8;	// Destination IP
		
		SYSLOG_O[UDP_HEADER_OFFSET    ] = 0x02; SYSLOG_O[UDP_HEADER_OFFSET + 1] = 0x02; // Source Port
		SYSLOG_O[UDP_HEADER_OFFSET + 2] = 0x02; SYSLOG_O[UDP_HEADER_OFFSET + 3] = 0x02; // Destination Port
		SYSLOG_O[UDP_HEADER_OFFSET + 4] = (8+4+log_size) >> 8; SYSLOG_O[UDP_HEADER_OFFSET + 5] = (8+4+log_size) & 0xff; // Length (UDP header + payload)
		SYSLOG_O[UDP_HEADER_OFFSET + 6] = 0x00; SYSLOG_O[UDP_HEADER_OFFSET + 7] = 0x00; // Header Checksum (not calculated)

		memcpyc(SYSLOG_O + UDP_PAYLOAD_OFFSET, "<14>", 4); // Syslog priority prefix

		if (logptr_w < logptr_r) {
			memcpy(SYSLOG_O + UDP_PAYLOAD_OFFSET + 4, logbuf + logptr_r, LOGBUF_SIZE - logptr_r);
			memcpy(SYSLOG_O + UDP_PAYLOAD_OFFSET + 4 + LOGBUF_SIZE - logptr_r, logbuf, logptr_w);
		} else {
			 memcpy(SYSLOG_O + UDP_PAYLOAD_OFFSET + 4, logbuf + logptr_r, logptr_w - logptr_r);
		}
	 	// SYSLOG_O[UDP_PAYLOAD_OFFSET + 4 + log_size - 1] = 0; // Null-terminate the payload

		uip_len = UDP_PAYLOAD_OFFSET+4+log_size;
		tcpip_output();

		logptr_r += log_size;
		logptr_r &= (LOGBUF_SIZE - 1);

		full_line_available = 0;
	}
}
