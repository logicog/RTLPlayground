
#include "httpd.h"
#include "page_impl.h"
#include "rtl837x_common.h"
#include "rtl837x_regs.h"
#include "cmd_parser.h"
#include "rtl837x_flash.h"
#include "uip.h"
#include "html_data.h"

// #define DEBUG
#include "debug.h"

#define SESSION_ID_LENGTH 12
#define SESSION_TIMEOUT 200

#define CMARK_S 6

#pragma codeseg BANK1
#pragma constseg BANK1

extern volatile __xdata uint8_t sfr_data[4];
extern volatile __xdata uint32_t ticks;
extern __xdata uint8_t cmd_capture;	/* owned by rtlplayground.c, see write_char_no_syslog() */
extern __code uint8_t * __code hex;
extern __code struct f_data f_data[];
extern __code char * __code mime_strings[];
extern __xdata struct flash_region_t flash_region;
extern __xdata uint32_t flash_size;

// Flash buffer to optimize flash writing speed, write_len is the current filling position
extern __xdata uint8_t flash_buf[FLASH_BUF_SIZE];
__xdata uint32_t uptr; // Current flash write position
__xdata uint16_t write_len;

__xdata uint8_t outbuf[TCP_OUTBUF_SIZE];
__xdata uint8_t entry;
__xdata uint16_t slen;
__xdata uint16_t o_idx;
__xdata uint16_t len_left;
__xdata uint16_t cont_len;
__xdata uint32_t cont_addr;

// HTTP header properties
__xdata uint8_t boundary[72];

// a client may split the request anywhere, including inside a boundary or a
// part header, so a configuration upload is parsed only once it is complete;
// sized for a full config sector plus the multipart framing around it
#define CONFIG_UPLOAD_BUF (CONFIG_LEN + 384)
__xdata uint8_t config_upload;
__xdata uint8_t config_buf[CONFIG_UPLOAD_BUF];
// bytes buffered in config_buf so far (config body, or a firmware part
// header); accumulates across TCP segments
__xdata uint16_t pre_acc;
__xdata uint8_t * __xdata content_type = 0;
__xdata uint8_t * __xdata session = 0;
__xdata uint16_t content_length;

// Global variables holding POST state
__xdata uint16_t bindex; // Current index into the boundary
__xdata uint8_t verify_crc;
__xdata uint32_t max_upload;
__xdata uint16_t short_parsed;

#define POSTBODY_CMD	1
#define POSTBODY_LOGIN	2
#define POSTBODY_TIMEOUT (5 * SYS_TICK_HZ)
__xdata uint8_t postbody_endpoint;
__xdata uint16_t postbody_start;

__xdata char passwd[21];
// Set when a verified firmware upload awaits its response ACK, after
// which the chip resets to apply the staged image
__xdata uint8_t fw_reset_pending;
__xdata char session_id[SESSION_ID_LENGTH + 1];
__xdata uint8_t authenticated;
__xdata uint32_t now;
__xdata uint8_t * __xdata timeptr;
__xdata uint32_t last_session_use;

#define TSTATE_NONE		0
#define TSTATE_TX		1
#define TSTATE_ACKED 		2
#define TSTATE_CLOSED 		3
#define TSTATE_POST 		4
#define TSTATE_MULTIPART	5
#define TSTATE_POSTBODY		6

extern __xdata uint16_t crc_value;
__xdata uint16_t crc_final;
void crc16(__xdata uint8_t *v) __naked;


inline uint8_t is_separator(uint8_t c)
{
	return c == ' ' || c == '\t' || c == '?' || c == '=';
}


void httpd_init(void) __banked
{
	config_upload = 0; // xdata is not zeroed by the startup code
	__xdata struct httpd_state * __xdata s = &(uip_conn->appstate);
	// Start listening to port 80
	uip_listen(HTONS(80));
	s->tstate = TSTATE_CLOSED;
	fw_reset_pending = 0; // xdata is not zeroed by the startup code
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


bool is_word(__xdata uint8_t *xdata_str_p, __code uint8_t * __xdata code_str_p)
{
	uint8_t u, c;

	while (1) {
		u = *xdata_str_p++;
		c = *code_str_p++;

		if (c == NUL) {
			if (u != NUL && u != ' ' && u != '\t' && u != ':' && u != '?' && u != '=' && u != '\n' && u != '\r')
				return false;
			return true;
		}

		if (c != u) {
			return false;
		}
	}
}


/* name must be lower-case, starting with the '\n' of the previous line's end */
__xdata uint8_t *header_value(__xdata uint8_t *p, __code uint8_t *name)
{
	uint8_t u, c;

	while ((c = *name++)) {
		u = *p++;
		if (u >= 'A' && u <= 'Z')
			u += 'a' - 'A';
		if (u != c)
			return 0;
	}
	while (*p == ' ' || *p == '\t')
		p++;
	return p;
}


bool is_url_word_x(__xdata uint8_t *uri_str_p, __xdata uint8_t *src_str_p)
{
	uint8_t u, s;

	while(1) {
		u = *uri_str_p++;
		s = *src_str_p++;

		if (s == NUL) {
			if (u != NUL && u != ' ' && u != '\t' && u != ':' && u != '?' && u != '=' && u != '\n' && u != '\r')
				return false;
			return true;
		}

		if (u == '%') {
			bool again = true;
			u = 0;

			while(1) {
				// Swap instruction is fine for rotation
				u = (u << 4) | (u >> 4);

				uint8_t p = *uri_str_p++;
				u |= p - '0' < 10 ? (p - '0') : (p - 'A' + 10);

				// force `jbc`-instruction.
				if (again) {
					again = false;
				} else {
					break;
				}
			}
		} else if (u == '+') {
			u = ' ';
		}

		if (s != u) {
			return false;
		}
	}
}


bool is_word_x(__xdata uint8_t * lhs_str_p, __xdata uint8_t * rhs_str_p)
{
	uint8_t u, c;

	while (1) {
		u = *lhs_str_p++;
		c = *rhs_str_p++;

		if (c == NUL) {
			/* ';' separates cookies in a Cookie header, so it ends a value too. */
			if (u != NUL && u != ' ' && u != '\t' && u != ':' && u != '?' && u != '=' && u != '\n' && u != '\r' && u != ';')
				return false;
			return true;
		}

		if (c != u) {
			return false;
		}
	}
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
		if (short_parsed > 6552)
			short_parsed = 0xffff;
		else
			short_parsed = (short_parsed * 10) + c;
	}
	return err;
}


void send_not_found(void)
{
	slen = strtox(outbuf, "HTTP/1.1 404 Not found\r\nConnection: close\r\nContent-Type: text/html\r\n\r\n" \
			      "<!DOCTYPE HTML PUBLIC>\n<title>404 Not Found</title>\n<h1>Not Found</h1>\n");
}


void send_bad_request(void)
{
	slen = strtox(outbuf, "HTTP/1.1 400 Bad Request\r\nConnection: close\r\nContent-Type: text/html\r\n\r\n" \
			      "<!DOCTYPE HTML PUBLIC>\n<title>400 Bad Request</title>\n<h1>Bad Request</h1>\n");
}


void send_to_login(void)
{
	slen = strtox(outbuf, "HTTP/1.1 302 Found\r\nConnection: close\r\n" \
			      "Location: login.html\r\n\r\n");
}


void send_unauthorized(void)
{
	slen = strtox(outbuf, "HTTP/1.1 401 Unauthorized\r\nConnection: close\r\n\r\n");
}


void send_length_required(void)
{
	slen = strtox(outbuf, "HTTP/1.1 411 Length Required\r\nConnection: close\r\n\r\n");
}


void send_ok(void)
{
	slen = strtox(outbuf, "HTTP/1.1 200 OK\r\nConnection: close\r\n\r\n");
}


__xdata uint8_t *scan_header(__xdata uint8_t * __xdata p)
{
	__xdata uint8_t *v;

	content_type = 0;
	content_length = 0;
	session = 0;
	authenticated = 0;

	while (!strstart(p, "\r\n\r\n")) {
		dbg_char(*p);
		if (!*p)
			break;
		p++;
		if ((v = header_value(p, "\ncontent-type:")))
			content_type = v;
		else if ((v = header_value(p, "\ncontent-length:"))) {
			parse_short(v);
			content_length = short_parsed;
		} else if ((v = header_value(p, "\ncookie:"))) {
			/* Scan for the "session" key: the header may hold several
			 * cookies in any order. Match "session" not "session=" -
			 * is_word() requires a separator after the match and '=' is
			 * one, so this also rejects a longer key like "sessionx". */
			while (*v && *v != '\r' && *v != '\n') {
				if (is_word(v, "session")) {
					session = v + 8;	/* past "session=" */
					break;
				}
				v++;
			}
		}
	}
	if (content_type && is_word(content_type, "multipart/form-data; boundary")) {
		dbg_string("\nFound multipart\n");
		content_type += 30;
		uint8_t i = 0;
		while (i < (sizeof(boundary) - 5) &&
				content_type[i] != '\r' && content_type[i] != '\n') {
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
			dbg_string("Session expired\n");
		} else {
			if (is_word_x(session, session_id))
				authenticated = 1;
			else
				dbg_string("Invalid session cookie!\n");
		}
	}
	return p;
}

/*
 * Generate random HEX-chars at the buffer location.
 */
void gen_random_hex_chars(__xdata uint8_t * b, __xdata uint8_t bytes)
{
	uint8_t i = 0;
	while (bytes) {
		if (!i)
			get_random_32();
		b[--bytes] = itohex(sfr_data[i]);
		if (!bytes) { break; }
		b[--bytes] = itohex(sfr_data[i] >> 4 | sfr_data[i] << 4);
		i = (i + 1) & 0x3;
	}
}


/* 0: body incomplete, 1: configuration stored, 2: malformed */
static uint8_t config_take(void)
{
	// #386: needs static, otherwise it still lands in SRAM/DSEG
	static __xdata uint16_t cfg_pos, cfg_hdr, cfg_body, cfg_end, cfg_last;
	__xdata uint8_t cfg_bl;

	cfg_bl = strlen_x(boundary);

	// the body is complete once the closing boundary has arrived
	cfg_last = 0;
	while (1) {
		if (cfg_last + cfg_bl + 1 >= pre_acc)
			return 0;
		if (strstart_x(&config_buf[cfg_last], boundary)
		    && strstart(&config_buf[cfg_last + cfg_bl], "--"))
			break;
		cfg_last++;
	}

	// every part lies ahead of the closing boundary, so it bounds the walk
	cfg_pos = 0;
	while (cfg_pos < cfg_last) {
		if (!strstart_x(&config_buf[cfg_pos], boundary)) {
			cfg_pos++;
			continue;
		}
		cfg_hdr = cfg_pos + cfg_bl;
		cfg_body = cfg_hdr;
		while (1) {
			if (cfg_body + 3 >= cfg_last)
				return 2;
			if (strstart(&config_buf[cfg_body], "\r\n\r\n"))
				break;
			cfg_body++;
		}
		cfg_end = cfg_body;
		cfg_body += 4;
		// reaching cfg_last is a match: the last part ends at the closing boundary
		while (cfg_end < cfg_last && !strstart_x(&config_buf[cfg_end], boundary))
			cfg_end++;
		while (cfg_hdr + 8 < cfg_body) {
			// the part carrying a filename holds the configuration
			if (strstart(&config_buf[cfg_hdr], "filename")) {
				// the payload plus its terminator must fit the sector
				if (cfg_end - cfg_body + 1 > CONFIG_LEN)
					return 2;
				config_buf[cfg_end] = 0;
				flash_region.addr = CONFIG_START;
				flash_sector_erase();
				flash_region.addr = CONFIG_START;
				flash_region.len = cfg_end - cfg_body + 1;
				flash_write_bytes(config_buf + cfg_body);
				return 1;
			}
			cfg_hdr++;
		}
		cfg_pos = cfg_end;
	}
	return 2;
}


// unlike scan_header(), keeps no auth state, so it may run on every buffered segment
static uint16_t preamble_payload_start(uint16_t n)
{
	uint16_t pos;

	for (pos = 0; pos + 24 <= n; pos++) {
		if (strstart(&config_buf[pos], "application/octet-stream"))
			break;
	}
	if (pos + 24 > n)
		return 0;
	pos += 24;
	while (pos + 3 < n && !strstart(&config_buf[pos], "\r\n\r\n"))
		pos++;
	if (pos + 3 >= n)
		return 0;
	return pos + 4;
}


// Source window for stream_upload(); filled by the caller before the call
__xdata struct {
	__xdata uint8_t *p;
	uint16_t bptr;
	uint16_t plen;
} upload_settings;

/*
 * Reads post data from the http stream and writes it into flash memory
 * Input: upload_settings, set by the caller
 * Returns 1: More data to read, 0: Upload complete, all parts reads
 */
uint8_t stream_upload(void)
{
	__xdata struct httpd_state * __xdata s = &(uip_conn->appstate);

	dbg_string("Stream_upload called: ");
	dbg_short(upload_settings.bptr); dbg_char('\n');

	do {
		if (upload_settings.bptr >= upload_settings.plen) {
			s->tstate = TSTATE_POST;
			return 1;
		}
		// Have we reached the end of the part?
		if (!boundary[bindex]) {
			s->tstate = TSTATE_NONE;
			dbg_string("len 2: "); dbg_short(write_len); dbg_char(' ');
			flash_region.addr = uptr;
			flash_region.len = write_len;
			flash_write_bytes(flash_buf);
			uptr += write_len;
			write_len = 0;
			if (verify_crc) {
				dbg_string("CRC16: "); dbg_short(crc_final); dbg_char('\n');
				// Both bodies are 33 bytes; Content-Length lets the
				// browser complete the response without waiting for
				// the connection close (which a reset would swallow)
				if (crc_final == 0xb001) {
					print_string("Checksum OK.\nUpload to flash done, will reset!\n");
					slen = strtox(outbuf, "HTTP/1.1 200 OK\r\nContent-Length: 33\r\n"
						"Content-Type: text/plain\r\n\r\n"
						"OK: checksum verified, rebooting\n");
					// Reset once the response is fully ACKed
					fw_reset_pending = 1;
				} else {
					print_string("Checksum incorrect! Aborting.\n");
					slen = strtox(outbuf, "HTTP/1.1 400 Bad Request\r\nContent-Length: 33\r\n"
						"Content-Type: text/plain\r\n\r\n"
						"NO: checksum failed, not applied\n");
				}
			}
			// Make sure there is a 0 at the end of the uploaded data
			flash_buf[0] = 0;
			flash_region.addr = uptr;
			flash_region.len = 1;
			flash_write_bytes(flash_buf);
			if (upload_settings.bptr >= upload_settings.plen)
				return 0;
			return 1;
		}
		if (upload_settings.p[upload_settings.bptr] == boundary[bindex]) {
			if (!bindex)
				crc_final = crc_value;
			crc16(upload_settings.p + upload_settings.bptr);
			upload_settings.bptr++;
			bindex++;
		} else {
			if (bindex) {
				memcpy(flash_buf + write_len, boundary, bindex);
				write_len += bindex;
				bindex = 0;
			}
			crc16(upload_settings.p + upload_settings.bptr);
			flash_buf[write_len++] = upload_settings.p[upload_settings.bptr++];
			if (write_len >= FLASH_PAGE_SIZE) {
				dbg_string("len: "); dbg_short(write_len); dbg_char(' ');
				dbg_string("CRC16: "); dbg_short(crc_value); dbg_char('\n');
				if (uptr % FLASH_SECTOR_SIZE == 0) {
					flash_region.addr = uptr;
					flash_sector_erase();
				}
				flash_region.addr = uptr;
				flash_region.len = FLASH_PAGE_SIZE;
				flash_write_bytes(flash_buf);
				uptr += FLASH_PAGE_SIZE;
				write_len -= FLASH_PAGE_SIZE;

				// Copy the remaining byte for the next page to the beginning of the buffer.
				if (write_len > 0) {
					memcpy(flash_buf, flash_buf + FLASH_PAGE_SIZE, write_len);
				}
			}
			bindex = 0;
		}
	} while(1);
}


static void handle_config_fragment(__xdata uint8_t *p)
{
	__xdata struct httpd_state * __xdata s = &(uip_conn->appstate);
	__xdata uint16_t frag_len;
	uint8_t taken;

	frag_len = uip_len - (p - uip_appdata);
	if (pre_acc + frag_len >= CONFIG_UPLOAD_BUF) {
		print_string("Configuration too large, aborting.\n");
		config_upload = 0;
		s->tstate = TSTATE_NONE;
		send_bad_request();
		return;
	}
	memcpy(config_buf + pre_acc, p, frag_len);
	pre_acc += frag_len;
	taken = config_take();
	if (!taken) {
		s->tstate = TSTATE_MULTIPART;
		return;
	}
	config_upload = 0;
	s->tstate = TSTATE_NONE;
	if (taken == 2) {
		send_bad_request();
		return;
	}
	send_ok();
}


static void handle_firmware_fragment(__xdata uint8_t *p)
{
	__xdata struct httpd_state * __xdata s = &(uip_conn->appstate);
	__xdata uint16_t frag_len, payload_start;

	frag_len = uip_len - (p - uip_appdata);
	if (pre_acc + frag_len >= CONFIG_UPLOAD_BUF) {
		print_string("Firmware upload header too large, aborting.\n");
		config_upload = 0;
		s->tstate = TSTATE_NONE;
		send_bad_request();
		return;
	}
	memcpy(config_buf + pre_acc, p, frag_len);
	pre_acc += frag_len;
	payload_start = preamble_payload_start(pre_acc);
	if (!payload_start) {
		s->tstate = TSTATE_MULTIPART;
		return;
	}
	dbg_string("Have content octets\n");

	flash_init(0); // Re-initialize flash for non-DIO operation, otherwise flashing fails
	set_sys_led_state(SYS_LED_FAST);

	crc_value = 0;
	bindex = 0;
	write_len = 0;
	// A verdict is only built once the upload part completes;
	// clear any stale response so the completion check in the
	// appcall POST branch cannot send leftovers
	slen = 0;
	upload_settings.p = config_buf;
	upload_settings.bptr = payload_start;
	upload_settings.plen = pre_acc;
	stream_upload();

	dbg_string("Done reading first fragment\n");
}


static void run_cmd_body(__xdata uint8_t *body)
{
	uint16_t hdr_len = strtox(outbuf, HTTP_RESPONCE_TXT);

	slen = hdr_len;
	cmd_capture = 1;
	execute_commands(body);
	if (cmd_capture == 2)
		slen += strtox(outbuf + slen, CMD_TRUNCATED);
	cmd_capture = 0;
	/* Commands that configure something print nothing at all. Saying
	 * so beats an empty body, which reads the same as "nothing ran".
	 * Only on success: a silent failure must not answer with "OK". */
	if (err_status == ERR_OK && slen == hdr_len)
		slen += strtox(outbuf + slen, "OK\n");
	/* On a parse error keep what the parser printed, because that text
	 * is the explanation, and only restate the status. "400 NO" is as
	 * long as "200 OK", so the header does not have to be rebuilt. */
	if (err_status != ERR_OK) {
		outbuf[9] = '4';
		outbuf[10] = '0';
		outbuf[11] = '0';
		outbuf[13] = 'N';
		outbuf[14] = 'O';
	}
}


static void run_login_body(__xdata uint8_t *body)
{
	if (strstart(body, "pwd=") && is_url_word_x(body + 4, passwd)) {
		dbg_string("Password accepted!\n");
		read_reg_timer(&last_session_use);
		gen_random_hex_chars(session_id, SESSION_ID_LENGTH);
		session_id[SESSION_ID_LENGTH] = NUL;
		slen = strtox(outbuf, "HTTP/1.1 302 Found\r\nConnection: close\r\nLocation: index.html\r\n" \
				      "Set-Cookie: session=");
		for (uint8_t i = 0; i < SESSION_ID_LENGTH; i++)
			outbuf[slen++] = session_id[i];
		slen += strtox(outbuf + slen, "; SameSite=Strict\r\n\r\n");
	} else {
		dbg_string("Password invalid!\n");
		slen = strtox(outbuf, "HTTP/1.1 302 Found\r\nConnection: close\r\nLocation: login.html\r\n\r\n");
	}
}


static uint8_t post_body_take(__xdata uint8_t *p)
{
	uint16_t have;

	if (!content_length) {
		send_length_required();
		return 0;
	}
	if (content_length >= CONFIG_UPLOAD_BUF) {
		send_bad_request();
		return 0;
	}
	have = uip_len - (p - uip_appdata);
	if (have >= content_length) {
		p[content_length] = NUL;
		return 1;
	}
	memcpy(config_buf, p, have);
	pre_acc = have;
	postbody_start = ticks;
	uip_conn->appstate.tstate = TSTATE_POSTBODY;
	return 0;
}


static void post_body_continue(void)
{
	uint16_t take;

	// no header scan runs while the body is pending: content_length is this request's
	take = content_length - pre_acc;
	if (take > uip_len)
		take = uip_len;
	memcpy(config_buf + pre_acc, uip_appdata, take);
	pre_acc += take;
	if (pre_acc < content_length) {
		postbody_start = ticks;
		return;
	}
	config_buf[pre_acc] = NUL;
	uip_conn->appstate.tstate = TSTATE_NONE;
	if (postbody_endpoint == POSTBODY_CMD)
		run_cmd_body(config_buf);
	else
		run_login_body(config_buf);
}


void handle_post(void)
{
	__xdata struct httpd_state * __xdata s = &(uip_conn->appstate);
	__xdata uint8_t *p = uip_appdata;
	__xdata uint8_t *request_path = p + 6;

	if (s->tstate == TSTATE_POSTBODY) {
		post_body_continue();
		return;
	}

	// Was the multipart header sent in multiple packets?
	if (s->tstate != TSTATE_MULTIPART) {
		dbg_string("Is POST\n");
		p += 5;  // Skip post
		// Find end of request path
		while (*p && !is_separator(*p))
			p++;
		*p++ = NUL;

		// Find end of request header
		boundary[0] =NUL;
		p = scan_header(p);
		dbg_string("Boundary: >"); dbg_string_x(boundary); dbg_string("<\n");
		if (!*p || !content_type) {
			dbg_string("Bad Request!\n");
			send_not_found();
			return;
		}
		if (is_word(request_path, "upload")) {
			if (flash_size < FIRMWARE_UPLOAD_START*2)
			{
				print_string("Flash too small for firmware upload!\n");
				send_bad_request();
				return;
			}
			print_string("Firmware upload started.");
			config_upload = 0;
			uptr = FIRMWARE_UPLOAD_START;
			verify_crc = 1;
			max_upload = 1024576;
			pre_acc = 0;
		} else if (is_word(request_path, "config")) {
			if (!authenticated) {
				send_unauthorized();
				return;
			}
			dbg_string("Configuration upload\n");
			verify_crc = 0;
			config_upload = 1;
			pre_acc = 0;
		}
		// Check for other POST requests, which are not multipart, below
	} else {
		dbg_string("Multipart request\n");
	}

	if (is_word(request_path, "cmd")) {
		p += 4;
		if (!authenticated) {
			send_unauthorized();
			return;
		}
		postbody_endpoint = POSTBODY_CMD;
		if (!post_body_take(p))
			return;
		run_cmd_body(p);
		return;
	} else if (is_word(request_path, "login")) {
		dbg_string("POST login\n");

		if (!content_type || !is_word(content_type, "application/x-www-form-urlencoded")) {
			dbg_string("Bad request!\n");
			send_bad_request();
			return;
		}

		p += 4;
		postbody_endpoint = POSTBODY_LOGIN;
		if (!post_body_take(p))
			return;
		run_login_body(p);
		return;
	} else if (s->tstate == TSTATE_MULTIPART || is_word(request_path, "upload") || is_word(request_path, "config")) {
		dbg_string("POST upload/config request\n");
		if (!authenticated) {
			send_unauthorized();
			return;
		}
		if (!boundary[0]) {
			dbg_string("Bad request, no boundary!\n");
			send_bad_request();
			return;
		}
		if (config_upload)
			handle_config_fragment(p);
		else
			handle_firmware_fragment(p);
		return;
	} else {
		send_not_found();
		return;
	}
}


void httpd_appcall(void)
{
	__xdata struct httpd_state * __xdata s = &(uip_conn->appstate);

	dbg_char('P');
#ifdef DEBUG
	if (uip_newdata())
		write_char('N');
	print_byte(s->tstate);
	write_char(' ');
#endif
	if(uip_connected() && s->tstate == TSTATE_CLOSED) {
		dbg_string("Connected...\n");
		s->tstate = TSTATE_NONE;
	} else if (uip_closed()) {
		dbg_string("Connection closed\n");
		s->tstate = TSTATE_CLOSED;
	} else if (uip_aborted() || uip_timedout()) {
		dbg_string("Connection aborted\n");
		uip_close();
		s->tstate = TSTATE_CLOSED;
	} else if (uip_poll()) {
		uip_len = 0;
		if (s->tstate == TSTATE_ACKED) {
			dbg_string("Closing because everything has been transmitted\n");
			uip_close();
			s->tstate = TSTATE_CLOSED;
		} else if (s->tstate == TSTATE_POSTBODY
			   && (uint16_t)ticks - postbody_start > POSTBODY_TIMEOUT) {
			dbg_string("Body never arrived\n");
			uip_abort();
			s->tstate = TSTATE_CLOSED;
		}
	} else if (uip_acked() && s->tstate == TSTATE_TX) {
		dbg_string("ACK\n");
		if (slen > uip_mss()) {
			slen -= uip_mss();
			o_idx += uip_mss();
		} else {
			slen = 0;
			o_idx += slen;
		}

		s->tstate = TSTATE_ACKED;

		if (slen > uip_mss()) {
			dbg_string("Sending A: "); dbg_short(slen); dbg_char('\n');
			uip_send(outbuf + o_idx, uip_mss());
			s->tstate = TSTATE_TX;
		} else if (slen > 0) {
			dbg_string("Sending B: "); dbg_short(slen); dbg_char('\n');
			uip_send(outbuf + o_idx, slen);
			s->tstate = TSTATE_TX;
		} else if (cont_len) {
			dbg_string("CONT cont_len: "); dbg_short(cont_len);
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
		} else if (fw_reset_pending) {
			// The upload verdict has been fully ACKed by the client;
			// now it is safe to reset and apply the staged image
			print_string("Resetting to apply update\n");
			reset_chip();
		}
	} else if (uip_newdata() && s->tstate == TSTATE_POST) {
		// Check here maxupload by subtracting uip_len and close socekt if fails!
		if (max_upload - uip_len > 0) {
			upload_settings.p = uip_appdata;
			upload_settings.bptr = 0;
			upload_settings.plen = uip_len;
			stream_upload();
			// A completed part with a built verdict must go out
			// through the normal TX path
			if (s->tstate == TSTATE_NONE && slen)
				goto do_send;
			write_char('.');
		} else {
			send_bad_request();
			goto do_send;
		}
	} else if (uip_newdata() && s->tstate != TSTATE_TX) {
		cont_len = 0;
		dbg_char('<'); dbg_short(uip_len); dbg_char('\n');
		__xdata uint8_t *p = uip_appdata;
		// Mark end of request header with \0
		p[uip_len] = 0;
#ifdef DEBUG
		while (*p)
			dbg_char(*p++);
		dbg_char('\n');
#endif
		p = uip_appdata;
		if (is_word(p, "POST") || s->tstate == TSTATE_MULTIPART
		    || s->tstate == TSTATE_POSTBODY) {
			handle_post();
			// If this is an ongoing post stream, then wait for the next packet
			if (s->tstate == TSTATE_POST || s->tstate == TSTATE_MULTIPART
			    || s->tstate == TSTATE_POSTBODY) {
				uip_len = 0;
				return;
			}
			goto do_send;
		}

		// We only expect a GET request here.
		if (!is_word(p, "GET")) {
			send_bad_request();
			goto do_send;
		}

		dbg_string("GET request ");
		p += 4;
		scan_header(p);
		__xdata uint8_t *q = p;
		while (*p && !is_separator(*p))
			p++;
		*p = NUL;
		dbg_string_x(q);
		dbg_char('\n');

		s->tstate = TSTATE_NONE;
		entry = find_entry(q);
		dbg_string("Entry is: "); dbg_byte(entry); dbg_char('\n');
		if (entry == 0xff) {
			if (!authenticated) {
				dbg_string("Not authorized!\n");
				send_unauthorized();
				goto do_send;
			}
			dbg_string("Not file entry\n");
			if (!strcmp(q, "/status.json")) {
				send_status();
			} else if (!strcmp(q, "/information.json")) {
				send_basic_info();
			} else if (!strcmp(q, "/vlan.json")) {
				parse_short(q + 15);
				send_vlan(short_parsed);
			} else if (is_word(q, "/counters.json")) {
				uint8_t cport = q[20] - '0';
				if (send_counters(cport))
					send_bad_request();
			} else if (is_word(q, "/eee.json")) {
				send_eee();
			} else if (is_word(q, "/bandwidth.json")) {
				send_bandwidth();
			} else if (is_word(q, "/l2.json")) {
				parse_short(q + 13); // e.g.: /l2.json?idx=10
				send_l2(short_parsed);
			} else if (is_word(q, "/l2_del.json")) {
				parse_short(q + 17);
				l2_delete(short_parsed);
			} else if (is_word(q, "/mirror.json")) {
				send_mirror();
			} else if (is_word(q, "/mtu.json")) {
				send_mtu();
			} else if (is_word(q, "/lag.json")) {
				send_lag();
			} else if (is_word(q, "/stp.json")) {
				send_stp();
			} else if (is_word(q, "/lacp.json")) {
				send_lacp();
			} else if (is_word(q, "/vlanlist")) {
				send_vlanlist();
			} else if (is_word(q, "/config")) {
				send_config();
			} else if (is_word(q, "/cmd_log")) {
				send_cmd_log();
			} else if (is_word(q, "/cmd_log_clear")) {
				clear_command_history();
				send_mtu(); // dummy response
			} else if (is_word(q, "/reset")) {
				uip_close();
				delay(1000); //wait for the close packet to be sent, otherwise the browser will retry
				reset_chip();
			} else {
				send_not_found();
			}
		} else {
			dbg_string("Have entry, authenticated: "); dbg_byte(authenticated); dbg_char('\n');
			if (!authenticated && !(f_data[entry].start == FDATA_START_login_html
						|| f_data[entry].start == FDATA_START_port_svg
						|| f_data[entry].start == FDATA_START_sfp_svg
						|| f_data[entry].start == FDATA_START_i18n_js
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
			/* 'unsafe-inline' is needed for the inline onclick handlers
			 * and the inline <script> on login.html. Connection: close is
			 * required because this httpd closes the connection after every
			 * response; without advertising it a browser reuses the socket
			 * from its keep-alive pool and the next request hits the already
			 * closed connection (a POST is then dropped without a retry). */
			slen += strtox(outbuf + slen, "; charset=UTF-8\r\nCache-Control: max-age=60, must-revalidate\r\nConnection: close\r\nAccess-Control-Allow-Origin: *\r\nContent-Security-Policy: default-src 'self'; script-src 'self' 'unsafe-inline'; style-src 'self' 'unsafe-inline'; img-src 'self' data:; connect-src 'self'; form-action 'self'\r\n\r\n");

			len_left = f_data[entry].len;
			if (len_left > (TCP_OUTBUF_SIZE - slen)) {
				cont_len = len_left - (TCP_OUTBUF_SIZE - slen);
				len_left = TCP_OUTBUF_SIZE - slen;
				cont_addr = f_data[entry].start + len_left;
			}
			dbg_string("MIME: "); dbg_string(mime_strings[f_data[entry].mime]); dbg_char('\n');
			flash_region.addr = f_data[entry].start;
			flash_region.len = len_left;
			flash_read_bulk(outbuf + slen);
			slen += len_left;
		}
do_send:
		dbg_string("slen: "); dbg_short(slen); dbg_char('\n');
		o_idx = 0;
		if (slen > uip_mss()) {
			dbg_string("Sending a: "); dbg_short(slen); dbg_char('\n');
			uip_send(outbuf + o_idx, uip_mss());
			dbg_string("Sending a done\n");
		} else {
			dbg_string("Sending b: "); dbg_short(slen); dbg_char('\n');
			uip_send(outbuf + o_idx, slen);
			dbg_string("Sending b done\n");
		}
		s->tstate = TSTATE_TX;
	} else if (uip_rexmit()) { // Connection established, need to rexmit?
		dbg_string("RETRANSMIT requested\n");
		if (slen > uip_mss()) {
			dbg_string("Sending C: "); dbg_short(slen); dbg_char('\n');
			uip_send(outbuf + o_idx, uip_mss());
			dbg_string("Sending C done\n");
		} else if (slen > 0) {
			dbg_string("Sending D: "); dbg_short(slen); dbg_char('\n');
			uip_send(outbuf + o_idx, slen);
			dbg_string("Sending D done\n");
		}
		s->tstate = TSTATE_TX;
		uip_len = 0;
	} else {
		uip_len = 0;
	}
}
