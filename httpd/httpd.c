
#include "httpd.h"
#include "page_impl.h"
#include "../rtl837x_common.h"
#include "../rtl837x_regs.h"
#include "../cmd_parser.h"
#include "../rtl837x_flash.h"
#include "uip.h"
#include "../html_data.h"

#define SESSION_ID_LENGTH 12
#define SESSION_TIMEOUT 200

// Upload Firmware to 1M
#define FIRMWARE_UPLOAD_START 0x100000

// SPI FLASH MEMORY PAGE SIZE.
#define FLASHMEM_PAGE_SIZE 0x100

#define CMARK_S 6

#pragma codeseg BANK1
#pragma constseg BANK1

extern volatile __xdata uint8_t sfr_data[4];
extern __code uint8_t * __code hex;
extern __code struct f_data f_data[];
extern __code char * __code mime_strings[];
extern __xdata struct flash_region_t flash_region;

// Flash buffer to optimize flash writing speed, write_len is the current filling position
extern __xdata uint8_t flash_buf[512];
__xdata uint32_t uptr; // Current flash write position
__xdata uint16_t write_len;

__xdata uint8_t outbuf[TCP_OUTBUF_SIZE];
__xdata uint8_t entry;
__xdata uint16_t slen;
__xdata uint16_t o_idx;
__xdata uint16_t mpos;
__xdata uint16_t len_left;
__xdata uint16_t cont_len;
__xdata uint32_t cont_addr;

// HTTP header properties
__xdata uint8_t boundary[72];
__xdata uint8_t *content_type = 0;
__xdata uint8_t *session = 0;

// Global variables holding POST state
__xdata uint16_t bindex; // Current index into the boundary
__xdata uint8_t verify_crc;
__xdata uint32_t max_upload;
__xdata uint16_t short_parsed;

__xdata char passwd[21];
__xdata char session_id[SESSION_ID_LENGTH + 1];
__xdata uint8_t authenticated;
__xdata uint32_t now;
__xdata uint8_t *timeptr;
__xdata uint32_t last_session_use;

#define TSTATE_NONE	0
#define TSTATE_TX	1
#define TSTATE_ACKED 	2
#define TSTATE_CLOSED 	3
#define TSTATE_POST 	4

extern __xdata uint16_t crc_value;
__xdata uint16_t crc_final;
void crc16(__xdata uint8_t *v) __naked;


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


uint8_t find_entry(__xdata uint8_t *e)
{
	uint8_t i, j;

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


char strcmp(__xdata uint8_t *c, __code uint8_t * __xdata d)
{
	uint8_t i = 0;

	while (d[i] && (d[i] == c[i]))
		i++;

	if (c[i] < d[i])
		return -1;
	else if (c[i] > d[i])
		return 1;
	return 0;
}


char is_word(__xdata uint8_t *c, __code uint8_t * __xdata d)
{
	uint8_t i = 0;

	while (d[i] && (d[i] == c[i]))
		i++;

	if (d[i])
		return 0;
	if (c[i] != ' ' && c[i] != '\t' && c[i] != ':' && c[i] != '?' && c[i] != '=' && c[i] != '\n' && c[i] != '\r' && c[i])
		return 0;
	return 1;
}


char is_word_x(__xdata uint8_t *c, __xdata uint8_t *d)
{
	register uint8_t i = 0;

	while (d[i] && (d[i] == c[i]))
		i++;

	if (d[i])
		return 0;
	if (c[i] != ' ' && c[i] != '\t' && c[i] != ':' && c[i] != '?' && c[i] != '=' && c[i] != '\n' && c[i] != '\r' && c[i])
		return 0;
	return 1;
}


uint8_t parse_short(__xdata uint8_t *p)
{
	uint8_t err = 1;
	uint8_t c = 0;

	short_parsed = 0;
	while(1) {
		c = *p++ - '0';
		if (c > 9) { break; }
		err = 0;
		short_parsed = (short_parsed * 10) + c;
	}
	return err;
}


void send_not_found(void)
{
	slen = strtox(outbuf, "HTTP/1.1 404 Not found\r\nContent-Type: text/html\r\n\r\n" \
			      "<!DOCTYPE HTML PUBLIC>\n<title>404 Not Found</title>\n<h1>Not Found</h1>\n");
}


void send_bad_request(void)
{
	slen = strtox(outbuf, "HTTP/1.1 400 Bad Request\r\nContent-Type: text/html\r\n\r\n" \
			      "<!DOCTYPE HTML PUBLIC>\n<title>400 Bad Request</title>\n<h1>Bad Request</h1>\n");
}


void send_to_login(void)
{
	slen = strtox(outbuf, "HTTP/1.1 302 Found\r\n" \
			      "Location: login.html\r\n\r\n");
}


void send_unauthorized(void)
{
	slen = strtox(outbuf, "HTTP/1.1 401 Unauthorized\r\n\r\n");
}


__xdata uint8_t *skip_boundary(__xdata uint8_t *p)
{
	while (*p) {
		if (is_word_x(p, boundary))
			return p + strlen_x(boundary);
		p++;
	}
	return p;
}


__xdata uint8_t *scan_header(__xdata uint8_t *p)
{
	content_type = 0;
	session = 0;
	authenticated = 0;

	while (*p != '\r' || *(p + 1) != '\n' || *(p + 2) != '\r' || *(p + 3) != '\n') {
		write_char(*p);
		if (!*p++)
			break;
		if (is_word(p, "\nContent-Type:"))
			content_type = p + 15;
		else if (is_word(p, "\nCookie:"))
			session = p + 17;
	}
	if (content_type && is_word(content_type, "multipart/form-data; boundary")) {
		print_string("\nFound multipart\n");
		content_type += 30;
		uint8_t i = 0;
		while (content_type[i] != '\r' && content_type[i] != '\n') {
			boundary[i + 4] = content_type[i];
			i++;
		}
		// The boundary between parts is "\r\n--" + the boundary given in the header
		boundary[0] = '\r';
		boundary[1] = '\n';
		boundary[2] = '-';
		boundary[3] = '-';
		boundary[i + 4] = 0;
	}

	read_reg_timer(&now);

	if (session) {
		if (now - last_session_use > SESSION_TIMEOUT) {
			print_string("Session expired\n");
		} else {
			if (is_word_x(session, session_id))
				authenticated = 1;
			else
				print_string("Invalid session cookie!\n");
		}
	}
	return p;
}


void gen_random_bytes(__xdata uint8_t *b, uint8_t bytes)
{
	__xdata uint8_t i = 0;
	while (bytes) {
		if (!i)
			get_random_32();
		b[--bytes] = itohex(sfr_data[i]);
		if (!bytes) { break; }
		b[--bytes] = itohex(sfr_data[i] >> 4 | sfr_data[i] << 4);
		i = (i + 1) & 0x3;
	}
}


/*
 * Reads post data from the http stream and writes it into flash memory
 * Input: the current position in the TCP buffer (uip_appdata)
 * Returns 1: More data to read, 0: Upload complete, all parts reads
 */
uint8_t stream_upload(uint16_t bptr)
{
	__xdata uint8_t *p = uip_appdata;
	__xdata struct httpd_state * __xdata s = &(uip_conn->appstate);

	print_string("Stream_upload called: ");
	print_short(bptr); write_char('\n');

	do {
		if (bptr >= uip_len) {
			s->tstate = TSTATE_POST;
			return 1;
		}
		// Have we reached the end of the part?
		if (!boundary[bindex]) {
			s->tstate = TSTATE_NONE;
			print_string("len 2: "); print_short(write_len); write_char(' ');
			flash_region.addr = uptr;
			flash_region.len = write_len;
			flash_write_bytes(flash_buf);
			uptr += write_len;
			write_len = 0;
			// TODO: This is a bit premature, what about a nice web-page saying the device will reset???
			if (verify_crc) {
				print_string("CRC16: "); print_short(crc_final); write_char('\n');
				if (crc_final == 0xb001) {
					print_string("Checksum OK.");
				} else {
					print_string("Checksum incorrect!");
				}
				print_string("Upload to flash done, will reset!\n");
				reset_chip();
			}
			// Make sure there is a 0 at the end of the uploaded data
			flash_buf[0] = 0;
			flash_region.addr = uptr;
			flash_region.len = 1;
			flash_write_bytes(flash_buf);
			if (bptr >= uip_len)
				return 0;
			return 1;
		}
		if (p[bptr] == boundary[bindex]) {
			if (!bindex)
				crc_final = crc_value;
			crc16(p + bptr);
			bptr++;
			bindex++;
		} else {
			if (bindex) {
				memcpy(flash_buf + write_len, boundary, bindex);
				write_len += bindex;
				bindex = 0;
			}
			crc16(p + bptr);
			flash_buf[write_len++] = p[bptr++];
			if (write_len >= FLASHMEM_PAGE_SIZE) {
				print_string("len: "); print_short(write_len); write_char(' ');
				print_string("CRC16: "); print_short(crc_value); write_char('\n');
				flash_region.addr = uptr;
				flash_region.len = FLASHMEM_PAGE_SIZE;
				flash_write_bytes(flash_buf);
				uptr += FLASHMEM_PAGE_SIZE;
				write_len -= FLASHMEM_PAGE_SIZE;

				// Copy the remaining byte for the next page to the beginning of the buffer.
				if (write_len > 0) {
					memcpy(flash_buf, flash_buf + FLASHMEM_PAGE_SIZE, write_len);
				}
			}
			bindex = 0;
		}
	} while(1);
}


void handle_post(void)
{
	__xdata struct httpd_state * __xdata s = &(uip_conn->appstate);
	__xdata uint8_t *p = uip_appdata;
	__xdata uint8_t *request_path = p + 6;

	print_string("Is POST\n");
	p += 5;  // Skip post
	// Find end of request path
	while (*p && !is_separator(*p))
		p++;
	*p++ = '\0';

	// Find end of request header
	boundary[0] ='\0';
	p = scan_header(p);
	print_string("Boundary: >"); print_string_x(boundary); print_string("<\n");
	if (!*p || !content_type) {
		print_string("Bad Request!\n");
		send_not_found();
		return;
	}

	if (is_word(request_path, "cmd")) {
		register uint8_t i = 0;
		p += 4;
		if (!authenticated) {
			send_unauthorized();
			return;
		}
		while (*p && *p != '\n' && *p != '\r')
			cmd_buffer[i++] = *p++;
		cmd_buffer[i] = '\0';
		if (i)
			cmd_available = 1;
	} else if (is_word(request_path, "login")) {
		print_string("POST login\n");
		p += 8; // Read also over "pwd="
		if (is_word_x(p, passwd)) {
			print_string("Password accepted!\n");
			read_reg_timer(&last_session_use);
			gen_random_bytes(session_id, SESSION_ID_LENGTH);
			session_id[SESSION_ID_LENGTH] = '\0';
			slen = strtox(outbuf, "HTTP/1.1 302 Found\r\nLocation: index.html\r\n" \
					      "Set-Cookie: session=");
			for (register uint8_t i = 0; i < SESSION_ID_LENGTH; i++)
				outbuf[slen++] = session_id[i];
			slen += strtox(outbuf + slen, "; SameSite=Strict\r\n\r\n");
		} else {
			slen = strtox(outbuf, "HTTP/1.1 302 Found\r\nLocation: login.html\r\n\r\n");
		}
		return;
	} else if (is_word(request_path, "upload") || is_word(request_path, "config")) {
		print_string("POST upload/config request\n");
		if (!authenticated) {
			send_unauthorized();
			return;
		}
		if (!boundary[0]) {
			print_string("Bad request, no boundary!\n");
			send_bad_request();
			return;
		}
		// We skip the intial parts as part of the header
		do {
			p = skip_boundary(p);
			if (!*p)
				goto bad_request;
			p = scan_header(p);
			if (!*p)
				goto bad_request;
			if (!content_type) // We are waiting for the part with the octet stream
				continue;
		} while (!is_word(content_type, "application/octet-stream"));
		print_string("Have content octets\n");
		p += 4; // Skip \r\n\r\n sequence at end of preamble of part

		if (is_word(request_path, "upload")) {
			uptr = FIRMWARE_UPLOAD_START;
			verify_crc = 1;
			max_upload = 1024576;
		} else {
			print_string("Configuration upload, erasing config mem!\n");
			uptr = CONFIG_START;
			verify_crc = 0;
			max_upload = 2048;
			flash_region.addr = CONFIG_START;
			flash_sector_erase();
		}
		crc_value = 0;
		bindex = 0;
		write_len = 0;
		stream_upload(p - uip_appdata);

		print_string("Done reading first fragment\n");
		return;

	} else {
		send_not_found();
		return;
	}
	slen = strtox(outbuf, "HTTP/1.1 200 OK\r\n\r\n");
	return;
bad_request:
	send_bad_request();
	return;
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
			s->tstate = TSTATE_TX;
		} else if (slen > 0) {
			print_string("Sending B: "); print_short(slen); write_char('\n');
			uip_send(outbuf + o_idx, slen);
			s->tstate = TSTATE_TX;
		} else if (cont_len) {
			print_string("CONT cont_len: "); print_short(cont_len);
			slen = cont_len > uip_mss() ? uip_mss() : cont_len;
			if (slen > TCP_OUTBUF_SIZE)
				slen = TCP_OUTBUF_SIZE;
			flash_region.addr = cont_addr;
			flash_region.len = slen;
			flash_read_bulk(outbuf);
			uip_send(outbuf, slen);
			cont_len -= slen;
			cont_addr += slen;
			s->tstate = TSTATE_TX;
		}
	} else if (uip_newdata() && s->tstate == TSTATE_POST) {
		// Check here maxupload by subtracting uip_len and close socekt if fails!
		if (max_upload - uip_len > 0) {
			stream_upload(0);
		} else {
			send_bad_request();
			goto do_send;
		}
	} else if (uip_newdata() && s->tstate != TSTATE_TX) {
		cont_len = 0;
		write_char('<'); print_short(uip_len); write_char('\n');
		__xdata uint8_t *p = uip_appdata;
		// Mark end of request header with \0
		p[uip_len] = 0;
		while (*p)
			write_char(*p++);
		write_char('\n');
		p = uip_appdata;
		if (is_word(p, "POST")) {
			handle_post();
			// If this is an ongoing post stream, then wait for the next packet
			if (s->tstate == TSTATE_POST) {
				uip_len = 0;
				return;
			}
			goto do_send;
		}

		if (is_word(p, "GET"))
			print_string("GET request ");
		p += 4;
		scan_header(p);
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
			if (!authenticated) {
				print_string("Not authorized!\n");
				send_unauthorized();
				goto do_send;
			}
			print_string("Not file entry\n");
			if (!strcmp(q, "/status.json")) {
				send_status();
			} else if (!strcmp(q, "/information.json")) {
				send_basic_info();
			} else if (!strcmp(q, "/vlan.json")) {
				parse_short(q + 15);
				send_vlan(short_parsed);
			} else if (is_word(q, "/counters.json")) {
				send_counters(q[19]-'0');
			} else if (is_word(q, "/eee.json")) {
				send_eee();
			} else if (is_word(q, "/mirror.json")) {
				send_mirror();
			} else if (is_word(q, "/lag.json")) {
				send_lag();
			} else if (is_word(q, "/config")) {
				send_config();
			} else if (is_word(q, "/cmd_log")) {
				send_cmd_log();
			} else {
				send_not_found();
			}
		} else {
			print_string("Have entry, authenticated: "); print_byte(authenticated); write_char('\n');
			if (!authenticated && !(f_data[entry].start == FDATA_START_login_html
						|| f_data[entry].start == FDATA_START_style_css)) {
				send_to_login();
				goto do_send;
			}
			// A web-page is actively accessed, we can reset session time-out
			reg_read_m(RTL837X_REG_SEC_COUNTER);
			timeptr = (uint8_t*)&last_session_use; // last_session_use is Little endian
			timeptr[0] = sfr_data[3]; timeptr[1] = sfr_data[2]; timeptr[2] = sfr_data[1]; timeptr[3] = sfr_data[0];

			slen = strtox(outbuf, "HTTP/1.1 200 OK\r\nContent-Type: ");
			slen += strtox(outbuf + slen, mime_strings[f_data[entry].mime]);
			slen += strtox(outbuf + slen, "; charset=UTF-8\r\nCache-Control: max-age=60, must-revalidate\r\n\r\n");
			len_left = f_data[entry].len;
			if (len_left > (TCP_OUTBUF_SIZE - slen)) {
				cont_len = len_left - (TCP_OUTBUF_SIZE - slen);
				len_left = TCP_OUTBUF_SIZE - slen;
				cont_addr = f_data[entry].start + len_left;
			}
			print_string("MIME: "); print_string(mime_strings[f_data[entry].mime]); write_char('\n');
			flash_region.addr = f_data[entry].start;
			flash_region.len = len_left;
			flash_read_bulk(outbuf + slen);
			slen += len_left;
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
