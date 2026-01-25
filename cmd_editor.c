#include "cmd_parser.h"
#include "machine.h"

#pragma codeseg BANK2
#pragma constseg BANK2

// Position in the serial buffer
__xdata uint8_t l;
// Properties of currently edited command line in cmd_buffer[CMD_BUF_SIZE]
__xdata uint8_t cursor;
__xdata uint8_t cmd_line_len;
__xdata uint8_t current_cmdline[CMD_BUF_SIZE];
__xdata uint16_t history_editptr;


extern __xdata uint8_t cmd_history[CMD_HISTORY_SIZE];
extern __xdata uint16_t cmd_history_ptr;

void cmd_editor_init(void) __banked
{
	l = sbuf_ptr; // We have printed out entered characters until l
	cursor = 0;
	cmd_line_len = 0;
	cmd_available = 0;
	history_editptr = 0xffff;
}

/*
 * Allows editing the current command line held in cmd_buffer[CMD_BUF_SIZE] by
 * identifying new characters typed or up to 4-byte escape sequences in the
 * serial buffer ring sbuf[SBUF_SIZE].
 * Upon detecting new characters or escape sequences, the cmd_buffer and the
 * representation of the command line in the terminal are updated.
 * To debug, the easist is to interpose a tty-interceptor between the physical
 * serial device and a logical one created by interceptty:
 * sudo interceptty -s 'ispeed 115200 ospeed 115200' /dev/ttyUSB0 /dev/tmpS
 * picocom -b 115200 /dev/tmpS
 */
void cmd_edit(void) __banked
{
	while (l != sbuf_ptr) {
		if (sbuf[l] >= ' ' && sbuf[l] < 127) { // A printable character, copy to command line
			if (cmd_line_len >= CMD_BUF_SIZE)
				continue;
			write_char(sbuf[l]);
			// Shift buffer to right
			for (uint8_t i = cmd_line_len; i > cursor; i--)
				cmd_buffer[i] = cmd_buffer[i-1];
			// Insert char in comand buffer
			cmd_buffer[cursor++] = sbuf[l];
			cmd_line_len++;
			// Print rest of line
			for (uint8_t i = cursor; i < cmd_line_len; i++)
				write_char(cmd_buffer[i]);
			// Move backwards
			for (uint8_t i = cursor; i < cmd_line_len; i++)
				write_char('\010'); // BS works like cursor-left
		} else if (sbuf[l] == '\033') { // ESC-Sequence
			// Wait until we have at least 3 characters including the ESC character in the serial buffer
			if (((sbuf_ptr + SBUF_SIZE - l) & SBUF_MASK) < 3)
				continue;
			if (((sbuf_ptr > l ? sbuf_ptr - l : SBUF_SIZE + sbuf_ptr - l) >= 4)
				   && sbuf[l] == '\033' && sbuf[(l + 1) & SBUF_MASK] == '[' && sbuf[(l + 2) & SBUF_MASK] == '3' && sbuf[(l + 3) & SBUF_MASK] == '~') { // DEL
				if (cursor < cmd_line_len) {
					write_char('\033'); write_char('['); write_char('1'); write_char('P'); // Delete to end of line
					cmd_line_len--;
					for (uint8_t i = cursor; i < cmd_line_len; i++) {
						cmd_buffer[i] = cmd_buffer[i+1];
						write_char(cmd_buffer[i]);
					}
					for (uint8_t i = cursor; i < cmd_line_len; i++)
						write_char('\010');
				}
				l += 4;
				l &= SBUF_MASK;
				continue;
			} else if (sbuf[l] == '\033' && sbuf[(l + 1) & SBUF_MASK] == '[' && sbuf[(l + 2) & SBUF_MASK] == 'D') { // <CURSOR-LEFT>
				if (cursor) {
					write_char('\010'); // BS works like cursor-left
					cursor--;
				}
				l += 3;
				l &= SBUF_MASK;
				continue;
			} else if (sbuf[l] == '\033' && sbuf[(l + 1) & SBUF_MASK] == '[' && sbuf[(l + 2) & SBUF_MASK] == 'C') { // <CURSOR-RIGHT>
				if (cursor < cmd_line_len) {
					write_char('\033'); write_char('['); write_char('C');
					cursor++;
				}
				l += 3;
				l &= SBUF_MASK;
				continue;
			} else if (sbuf[l] == '\033' && sbuf[(l + 1) & SBUF_MASK] == '[' && sbuf[(l + 2) & SBUF_MASK] == 'A') { // <CURSOR-UP>
				for (uint8_t i = 0; i < cmd_line_len; i++)
					current_cmdline[i] = cmd_buffer[i];
				current_cmdline[cmd_line_len] = 0;
				__xdata uint16_t p;
				if (history_editptr == 0xffff)
					p = (cmd_history_ptr - 2) & CMD_HISTORY_MASK;
				else
					p = history_editptr;
				// Move cursor to beginning of line
				write_char('\033'); write_char('['); itoa(cursor + 2); write_char('D');
				cursor = 0;
				while (cmd_history[p] && cmd_history[p] != '\n') {
					cursor++;
					p--;
					p &= CMD_HISTORY_MASK;
				}
				history_editptr = (p - 1) & CMD_HISTORY_MASK;
				p = (p + 1) & CMD_HISTORY_MASK;
				if (cursor) {
					print_string("\033[2K> "); // Clear entire line: ^[[2K and print new prompt
					for (uint8_t i = 0; i < cursor; i++) {
						cmd_buffer[i] = cmd_history[p];
						write_char(cmd_buffer[i]);
						p = (p+1) & CMD_HISTORY_MASK;
					}
					cmd_line_len = cursor;
				} else {
					print_string("\033[2C"); // Move 2 right to start of editing space
				}
				l += 3;
				l &= SBUF_MASK;
				continue;
			} else if (sbuf[l] == '\033' && sbuf[(l + 1) & SBUF_MASK] == '[' && sbuf[(l + 2) & SBUF_MASK] == 'B') { // <CURSOR-DOWN>
				if (history_editptr != 0xffff) {
					__xdata uint16_t p = (history_editptr + 2) & CMD_HISTORY_MASK;
					// Move cursor to beginning of line
					write_char('\033'); write_char('['); itoa(cursor + 2); write_char('D');
					print_string("\033[2K> "); // Clear entire line: ^[[2K and print new prompt
					uint8_t i = 0;
					while (cmd_history[p] && cmd_history[p] != '\n') {
						p = (p + 1) & CMD_HISTORY_MASK;
					}
					p = (p + 1) & CMD_HISTORY_MASK;
					while (cmd_history[p] && cmd_history[p] != '\n') {
						cmd_buffer[i] = cmd_history[p];
						write_char(cmd_buffer[i++]);
						p = (p + 1) & CMD_HISTORY_MASK;
					}
					history_editptr = (p - 1) & CMD_HISTORY_MASK;
					if (!i) {
						if (current_cmdline[i]) {
							while (current_cmdline[i]) {
								cmd_buffer[i] = current_cmdline[i];
								write_char(cmd_buffer[i++]);
							}
						} else {
							write_char('\033'); write_char('['); write_char('C');
						}
						history_editptr = 0xffff;
						// Move cursor right
					}
					cmd_line_len = i;
					cursor = 0;
					// Move cursor again to beginning of line
					write_char('\033'); write_char('['); itoa(i); write_char('D');
				} else {
					// If we are at the last entry of the history, just move the cursor to the end of the line
					if (cursor < cmd_line_len) {
						write_char('\033'); write_char('['); itoa(cmd_line_len - cursor); write_char('C');
					}
					cursor = cmd_line_len;
				}
				l += 3;
				l &= SBUF_MASK;
				continue;
			} else { // An unknown or not yet complete Escape sequence: wait
				continue;
			}
		} else if (sbuf[l] == 127) {  // Backspace
			if (cursor > 0) {
				write_char('\010');
				for (uint8_t i = cursor; i < cmd_line_len; i++)
					write_char(cmd_buffer[i]);
				write_char(' '); // Overwrite end of line
				// Move backwards n steps:
				for (uint8_t i = cursor; i <= cmd_line_len; i++)
					write_char('\010');
				cursor--;
				for (uint8_t i = cursor; i <= cmd_line_len; i++)
					cmd_buffer[i] = cmd_buffer[i+1];
				cmd_line_len--;
			}
		}
		// If the command buffer is currently in use, we cannot copy to it
		if (cmd_available)
			break;
		// Check whether return was pressed:
		if (sbuf[l] == '\n' || sbuf[l] == '\r') {
			write_char('\n');
			cmd_buffer[cmd_line_len] = '\0';
//			write_char('>'); print_string_x(cmd_buffer); write_char('<');
			// If there is a command we print the prompt after execution
			// otherwise immediately because there is nothing to execute
			if (cmd_line_len)
				cmd_available = 1;
			else
				print_string("\n> ");
			cursor = 0;
			cmd_line_len = 0;
			history_editptr = 0xffff;
		}
		l++;
		l &= SBUF_MASK;
	}
}
