/*
 * "hostname" command
 */

#include "cmd_parse_util.h"
#include "cmd_parse_hostname.h"

#pragma codeseg BANK2
#pragma constseg BANK2

void cmd_parse_hostname(void)
{
	/* "hostname" alone reports the current name; "hostname <text>"
	 * sets it, sanitized to JSON-safe printable ASCII. A name with
	 * spaces would tokenize into several words - reject it instead
	 * of silently keeping the first one. */
	if (cmd_words_len == 1) {
		print_string_x(hostname);
		write_char('\n');
	} else if (cmd_words_len == 2) {
		__xdata uint8_t *hp = &cmd_buffer[cmd_words_b[1]];
		__xdata char *dst = hostname;
		for (uint8_t hn = 0; hn < sizeof(hostname) - 1; hn++) {
			uint8_t c = *hp++;
			if (c == NUL || c == '\r' || c == '\n')
				break;
			*dst++ = cmd_name_char(c);
		}
		*dst = NUL;
	} else {
		cmd_error("hostname [name] - the name must not contain spaces\n");
	}
}
