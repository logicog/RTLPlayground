#ifndef __TELNETD_H__
#define __TELNETD_H__

#include <stdint.h>
#include "uip.h"

/* Room for one command's captured output plus echo; a response larger
 * than this is truncated with a marker, like the httpd cmd endpoint.
 * Sized to hold the full `help` output. */
#define TELNET_OUTBUF 2048
#define TELNET_PORT 23

struct telnet_state_t {
	uint8_t enabled;		/* telnet on/off */
	uint8_t bind[4];		/* accept only on this local IP; 0.0.0.0 = any */
	__xdata struct uip_conn *conn;	/* active session, 0 = none */
	uint8_t authed;
	uint8_t tries;
	uint8_t iac;			/* telnet option negotiation parser state */
	uint8_t crseen;			/* swallow the LF/NUL that follows a CR */
	uint8_t close_pending;		/* close once pending output is ACKed */
	uint8_t ll;			/* filled length of the line buffer */
	uint32_t last_rx;
	uint16_t idle_secs;		/* `telnet timeout <secs>`: idle close */
	uint32_t idle_ticks;		/* idle_secs in ticks, converted at set time */
};

extern __xdata struct telnet_state_t telnet_state;
extern __xdata uint8_t telnet_outbuf[TELNET_OUTBUF];
extern __xdata uint16_t telnet_slen;
/* Set while a command runs for a telnet session so write_char_no_syslog()
 * copies its output into telnet_outbuf; 2 means the buffer overflowed. */
extern __xdata uint8_t telnet_capture;

void telnetd_init(void) __banked;
void telnetd_appcall(void) __banked;
void telnet_start(void) __banked;
void telnet_stop(void) __banked;
void telnet_set_timeout(uint16_t secs) __banked;

#endif
