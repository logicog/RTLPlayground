/*
 * "flash" command
 */

#include "cmd_parse_util.h"
#include "cmd_parse_flash.h"
#include "rtl837x_flash.h"

#pragma codeseg BANK2
#pragma constseg BANK2

extern __xdata struct flash_region_t flash_region;

void cmd_parse_flash(void)
{
	/* "flash" takes exactly one option; any other form is not a command. */
	if (cmd_words_len != 2) {
		cmd_error("Unknown command\n");
		return;
	}
	uint8_t c = cmd_buffer[cmd_words_b[1]];
	if (c == 's') {
		print_string("\nSECURITY REGISTERS\n");
		// The following will only show something else than 0xff if it was programmed for a managed switch
		print_string("Region 1: ");
		flash_region.addr = 0x0001000;
		flash_region.len = 40;
		flash_read_security();
		print_string("\nRegion 2: ");
		flash_region.addr = 0x0002000;
		flash_region.len = 40;
		flash_read_security();
		print_string("\nRegion 3: ");
		flash_region.addr = 0x0003000;
		flash_region.len = 40;
		flash_read_security();
	} else if (c == 'j') {
		print_string("\nJEDEC ID\n");
		flash_read_jedecid();
	} else if (c == 'u') {
		print_string("\nUNIQUE ID (note: only 4 bytes are likely correct here!)\n");
		flash_read_uid();
	}
}
