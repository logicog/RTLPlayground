/*
 * "passwd" command
 */

#include "cmd_parse_util.h"
#include "cmd_parse_passwd.h"
#include "sessions.h"

#pragma codeseg BANK2
#pragma constseg BANK2

extern __xdata char passwd[21];

void cmd_parse_passwd(void)
{
	// cmd_words_len can be more then 2 if a SPACE in the password.
	if (cmd_words_len >= 2) {
		uint8_t i = cmd_words_b[1];		
		uint8_t c = 0;
		uint8_t j = 0;
		do {
			c = cmd_buffer[i++];
			passwd[j++] = c;
		} while (c != NUL && j < 20);
		passwd[j] = NUL;
		/* Logins made with the old password end here. With one session the next
		 * login pushed a leftover one out; with several it would stay valid. */
		session_init();
		return;
	}
	cmd_error("Missing password\n");
}
