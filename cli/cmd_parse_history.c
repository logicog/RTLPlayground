/*
 * "history" command
 */

#include "cmd_parse_util.h"
#include "cmd_parse_history.h"

#pragma codeseg BANK2
#pragma constseg BANK2

__xdata uint8_t cmd_history[CMD_HISTORY_SIZE];
__xdata uint16_t cmd_history_ptr;

void cmd_parse_history(void)
{
	__xdata uint16_t p = (cmd_history_ptr + 1) & CMD_HISTORY_MASK;
	__xdata uint8_t found_begin = 0;
//			print_string("History ptr: ");
//			print_short(cmd_history_ptr); write_char('\n');
	while (p != cmd_history_ptr) {
//				print_short(p); write_char(' ');
		if (!cmd_history[p] || cmd_history[p] == '\n')
			found_begin = 1;
		if (found_begin && cmd_history[p])
			write_char(cmd_history[p]);
		p = (p + 1) & CMD_HISTORY_MASK;
	}
}

void clear_command_history(void) __banked
{
	for (cmd_history_ptr = 0; cmd_history_ptr < CMD_HISTORY_SIZE; cmd_history_ptr++)
		cmd_history[cmd_history_ptr] = 0;
	cmd_history_ptr = 0;
	return;
}

/* Append the command line just executed to the history. The caller checks save_cmd and err_status. */
void cmd_history_save(void)
{
	// Find end of the cmd-buffer, looking for the NUL-byte.
	uint8_t i = cmd_words_b[cmd_words_len - 1];
	do {
		i++;
	} while(cmd_buffer[i] != NUL);

	// Copy last cmd-buffer to history.
	cmd_history_ptr = (cmd_history_ptr + i) & CMD_HISTORY_MASK;
	__xdata uint16_t p = cmd_history_ptr;
	cmd_history[cmd_history_ptr] = '\n';
	cmd_history_ptr = (cmd_history_ptr + 1) & CMD_HISTORY_MASK;
	do {
		i--;
		cmd_history[--p & CMD_HISTORY_MASK] = cmd_buffer[i];
	} while (i);
}
