
#include "httpd.h"
#include "../rtl837x_common.h"
#include "uip.h"

#pragma codeseg BANK1

static __code uint8_t page_main[] = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n"
"\r\n<!DOCTYPE html>"
"<html>"
"  <head>"
"    <title>FreeSwitchOS Main Page</title>"
"  </head>"
"  <body>"
"    <h1>Port configuration:</h1>"
"    <p>TBD</p>"
"  </body>"
"</html>\r\n";

__xdata uint8_t outbuf[2048];

inline uint8_t is_space(uint8_t c)
{
	return c == ' ' || c == '\t';
}


void httpd_init(void) __banked
{
	// Start listening to port 80
	uip_listen(HTONS(80));
}


void httpd_appcall(void)
{
	__xdata struct httpd_state *s = &(uip_conn->appstate);

	write_char('P');
	if(uip_connected()) {
		print_string("Connected...\n");
		s->transmitted = 0;
	} else if (uip_closed()) {
		print_string("Connection closed\n");
	} else if (uip_poll()) {
		uip_len = 0;
		if (s->transmitted)
			uip_close();
//		write_char('p');
	} else if (uip_newdata()) {
		write_char('<'); print_short(uip_len); write_char('\n');
		__xdata uint8_t *p = uip_appdata;
		while (*p)
			write_char(*p++);
		write_char('\n');
		p = uip_appdata;
		if (p[0] == 'G' && p[1] == 'E' && p[2] == 'T' && p[3] == ' ')
			print_string("GET request ");
		p += 4;
		__xdata uint8_t *q = p;
		while (!is_space(*p))
			p++;
		*p = '\0';
		print_string_x(q);
		write_char('\n');
		// For now, we do not have anything to send...
		uip_len = strlen(page_main);
		strtox(outbuf, page_main);
		s->transmitted = 1;
		uip_send(outbuf, uip_len);
	} else if (uip_rexmit()) { // Connection established, need to rexmit?
		write_char('r');
		uip_len = 0;
	} else {
		uip_len = 0;
	}
}
