/*
 * "session" command
 */

#include "cmd_parse_util.h"
#include "cmd_parse_session.h"

#pragma codeseg BANK2
#pragma constseg BANK2

extern __xdata uint16_t session_timeout;

void cmd_parse_session(void)
{
	if (cmd_words_len >= 2) {
		/* atoi_short() returns zero for no digits and for a value past
		 * 65535; a zero-second timeout is not useful either, so both
		 * tests are needed. */
		if (!atoi_short(cmd_words_b[1]) || !atoi_results_short) {
			cmd_error("Must be 1-65535 s\n");
			return;
		}
		session_timeout = atoi_results_short;
	}
	/* Also the confirmation after setting: one message for both. */
	print_string("Session timeout: ");
	itoa_short(session_timeout);
	print_string(" s\n");
}
