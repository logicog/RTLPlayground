/*
 * "version" command
 */

#include "cmd_parse_util.h"
#include "cmd_parse_version.h"
#include "version.h"
#include "machine.h"

#pragma codeseg BANK2
#pragma constseg BANK2

extern __code const struct machine machine;

// Show software version
void print_sw_version(void) __banked {
	print_string("Software version: " VERSION_SW);
	print_string("\nBuild date: " BUILD_DATE);
	print_string("\nCompiler: SDCC " STR(__SDCC_VERSION_MAJOR) "." STR(__SDCC_VERSION_MINOR) "." STR(__SDCC_VERSION_PATCH) " #" STR(__SDCC_REVISION));
	print_string("\nToolchain: " SDCC_TOOLCHAIN);
	print_string("\nHardware: ");
	print_string(machine.machine_name);
	write_char('\n');
}

void cmd_parse_version(void)
{
	print_sw_version();
}
