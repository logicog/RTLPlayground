#include "cmd_parser.h"
#include "machine.h"

#pragma codeseg BANK2
#pragma constseg BANK2

// Position in the serial buffer
__xdata uint8_t l;
// Properties of currently edited command line in cmd_buffer[CMD_BUF_SIZE]
__xdata uint8_t cursor;
__xdata uint8_t cmd_line_len;


void cmd_editor_init(void) __banked
{
	l = sbuf_ptr; // We have printed out entered characters until l
	cursor = 0;
	cmd_line_len = 0;
	cmd_available = 0;
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
			write_char('>'); print_string_x(cmd_buffer); write_char('<');
			// If there is a command we print the prompt after execution
			// otherwise immediately because there is nothing to execute
			if (cmd_line_len)
				cmd_available = 1;
			else
				print_string("\n> ");
			cursor = 0;
			cmd_line_len = 0;
		}
		l++;
		l &= SBUF_MASK;
	}
}
