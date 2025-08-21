
#include "httpd.h"
#include "page_impl.h"
#include "../rtl837x_common.h"
#include "../cmd_parser.h"
#include "../rtl837x_flash.h"
#include "uip.h"
#include "../html_data.h"

#define CMARK_S 6

#pragma codeseg BANK1

extern __code struct f_data f_data[];
extern __code fcall_ptr f_calls[];
extern __code char * __code mime_strings[];

__xdata uint8_t outbuf[2048];
__xdata uint8_t entry;
__xdata uint16_t slen;
__xdata uint16_t o_idx;
__xdata uint16_t mpos;
__xdata uint16_t len_left;


#define TSTATE_NONE	0
#define TSTATE_TX	1
#define TSTATE_ACKED 	2
#define TSTATE_CLOSED 	3


inline uint8_t is_separator(uint8_t c)
{
	return c == ' ' || c == '\t' || c == '?' || c == '=';
}


void httpd_init(void) __banked
{
	__xdata struct httpd_state * __xdata s = &(uip_conn->appstate);
	// Start listening to port 80
	uip_listen(HTONS(80));
	s->tstate = TSTATE_CLOSED;
}


uint8_t find_entry(__xdata uint8_t * __xdata e)
{
	register uint8_t i, j;

	for (i = 0; f_data[i].len; i++) {
		j = 0;
		while (f_data[i].file[j] && (f_data[i].file[j] == e[j])) {
			j++;
		}
		if ((!f_data[i].file[j]) && (!e[j])) {
			return i;
		}
	}
	return 0xff;
}


char strcmp(__xdata uint8_t * __xdata c, __code uint8_t * __xdata d)
{
	register uint8_t i = 0;

	while (d[i] && (d[i] == c[i])) {
		i++;
	}

	if (c[i] < d[i])
		return -1;
	else if (c[i] > d[i])
		return 1;
	return 0;
}


uint8_t parse_short(register uint16_t *n, __xdata uint8_t * __xdata p)
{
	uint8_t err = 1;
	*n = 0;

	while (*p >= '0' && *p <= '9') {
		err = 0;
		*n = (*n * 10) + *p - '0';
		p++;
	}
	return err;
}


void send_not_found()
{
	slen = strtox(outbuf, "HTTP/1.1 404 Not found\r\nContent-Type: text/html\r\n\r\n");
	print_string("slen: "); print_short(slen); write_char('\n');
	slen += strtox(outbuf + slen, "<!DOCTYPE HTML PUBLIC>\n<title>404 Not Found</title>\n<h1>Not Found</h1>\n");
}

void handle_post(void)
{
	__xdata uint8_t *p = uip_appdata;
	while (*p != '\r' || *(p + 1) != '\n' || *(p + 2) != '\r' || *(p + 3) != '\n') {
		write_char(*p);
		if (!*p++)
			break;
	}
	if (!*p) {
		print_string("No body found!\n");
		send_not_found();
		return;
	}
	register uint8_t i = 0;
	p += 4;
	while (*p && *p != '\n' && *p != '\r')
		cmd_buffer[i++] = *p++;
	cmd_buffer[i] = '\0';

	if (i && !cmd_tokenize())
		cmd_parser();
	slen = strtox(outbuf, "HTTP/1.1 200 OK\r\n\r\n");
}


void httpd_appcall(void)
{
	__xdata struct httpd_state * __xdata s = &(uip_conn->appstate);

	write_char('P');
	if(uip_connected() && s->tstate == TSTATE_CLOSED) {
		print_string("Connected...\n");
		s->tstate = TSTATE_NONE;
	} else if (uip_closed()) {
		print_string("Connection closed\n");
		s->tstate = TSTATE_CLOSED;
	} else if (uip_aborted()) {
		print_string("Connection aborted\n");
		uip_close();
		s->tstate = TSTATE_CLOSED;
	} else if (uip_poll()) {
		uip_len = 0;
		if (s->tstate == TSTATE_ACKED) {
			print_string("Closing because everything has been transmitted\n");
			uip_close();
			s->tstate = TSTATE_CLOSED;
		}
	} else if (uip_acked() && s->tstate == TSTATE_TX) {
		print_string("ACK\n");
		if (slen > uip_mss()) {
			slen -= uip_mss();
			o_idx += uip_mss();
		} else {
			slen = 0;
			o_idx += slen;
		}

		s->tstate = TSTATE_ACKED;

		if (slen > uip_mss()) {
			print_string("Sending A: "); print_short(slen); write_char('\n');
			uip_send(outbuf + o_idx, uip_mss());
			print_string("Sending A done\n");
			s->tstate = TSTATE_TX;
		} else if (slen > 0) {
			print_string("Sending B: "); print_short(slen); write_char('\n');
			uip_send(outbuf + o_idx, slen);
			print_string("Sending B done\n");
			s->tstate = TSTATE_TX;
		}
	} else if (uip_newdata() && s->tstate != TSTATE_TX) {
		write_char('<'); print_short(uip_len); write_char('\n');
		__xdata uint8_t *p = uip_appdata;
		p[uip_len] = 0;
		while (*p)
			write_char(*p++);
		write_char('\n');
		p = uip_appdata;
		if (p[0] == 'P' && p[1] == 'O' && p[2] == 'S' && p[3] == 'T'  && p[4] == ' ') {
			handle_post();
			goto do_send;
		}

		if (p[0] == 'G' && p[1] == 'E' && p[2] == 'T' && p[3] == ' ')
			print_string("GET request ");
		p += 4;
		__xdata uint8_t *q = p;
		while (!is_separator(*p))
			p++;
		*p = '\0';
		print_string_x(q);
		write_char('\n');

		s->tstate = TSTATE_NONE;
		entry = find_entry(q);
		print_string("Entry is: "); print_byte(entry); write_char('\n');
		if (entry == 0xff) {
			print_string("Not file entry\n");
			if (!strcmp(q, "/status.json")) {
				send_status();
			} else if (!strcmp(q, "/vlan.json")) {
				__xdata uint16_t vlan;
				parse_short(&vlan, q + 15);
				send_vlan(vlan);
			} else if (!strcmp(q, "/counters.json")) {
				send_counters(q[19]-'0');
			} else {
				send_not_found();
			}
		} else {
			print_string("Have entry\n");
			slen = strtox(outbuf, "HTTP/1.1 200 OK\r\nContent-Type: ");
			slen += strtox(outbuf + slen, mime_strings[f_data[entry].mime]);
			slen += strtox(outbuf + slen, "\r\nCache-Control: max-age=2592000");
			slen += strtox(outbuf + slen, "\r\n\r\n");
			len_left = f_data[entry].len;
			if (f_data[entry].mime == mime_HTML) {
				print_string("MIME is html len is "); print_short(len_left); write_char('\n');
				mpos = 0;
				flash_find_mark(f_data[entry].start, len_left, "#{");
				print_string("mpos: "); print_short(mpos); write_char('\n');
				while (mpos != 0xffff) {
					print_string("Entry-len:"); print_short(len_left); write_char('\n');
					mpos = len_left - mpos;
					print_string("l/pos: "); print_short(mpos); write_char('\n');
					flash_read_bulk(outbuf + slen, f_data[entry].start + f_data[entry].len - len_left, mpos + CMARK_S);  // call marker is e.g. #{001}
					slen += mpos;
					write_char('@'); write_char(outbuf[slen + 2]); write_char(outbuf[slen + 3]); write_char(outbuf[slen + 4]);
					fcall_ptr ptr = f_calls[(outbuf[slen + 2] - '0') * 100 + (outbuf[slen + 3]-'0') * 10 + outbuf[slen + 4] - '0'];
					slen -= CMARK_S; // Overwrite marker with generated html
					print_string("Call location is: "); print_short((uint16_t)ptr); write_char('\n');
//					f_calls[outbuf[slen + 2] * 100 + outbuf[slen + 3] * 10 + outbuf[slen + 4]]();
					ptr();
					print_string("call done\n");
					mpos += CMARK_S;
					len_left -= mpos;
					flash_find_mark(f_data[entry].start + mpos, len_left, "#{");
					print_string("mpos now: "); print_short(mpos); write_char('\n');
				}
				print_string("At end mpos: "); print_short(mpos); write_char('\n');
				flash_read_bulk(outbuf + slen, f_data[entry].start + f_data[entry].len - len_left, len_left);
				slen += len_left;
			} else {
				print_string("MIME: "); print_string(mime_strings[f_data[entry].mime]); write_char('\n');
				flash_read_bulk(outbuf + slen, f_data[entry].start, len_left);
				slen += len_left;
			}
		}
do_send:
		print_string("slen: "); print_short(slen); write_char('\n');
		o_idx = 0;
		if (slen > uip_mss()) {
			print_string("Sending a: "); print_short(slen); write_char('\n');
			uip_send(outbuf + o_idx, uip_mss());
			print_string("Sending a done\n");
		} else {
			print_string("Sending b: "); print_short(slen); write_char('\n');
			uip_send(outbuf + o_idx, slen);
			print_string("Sending b done\n");
		}
		s->tstate = TSTATE_TX;
	} else if (uip_rexmit()) { // Connection established, need to rexmit?
		print_string("RETRANSMIT requested\n");
		if (slen > uip_mss()) {
			print_string("Sending C: "); print_short(slen); write_char('\n');
			uip_send(outbuf + o_idx, uip_mss());
			print_string("Sending C done\n");
		} else if (slen > 0) {
			print_string("Sending D: "); print_short(slen); write_char('\n');
			uip_send(outbuf + o_idx, slen);
			print_string("Sending D done\n");
		}
		s->tstate = TSTATE_TX;
		uip_len = 0;
	} else {
		uip_len = 0;
	}
}
