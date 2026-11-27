/*
 * "isolate" command
 */

#include "cmd_parse_util.h"
#include "cmd_parse_isolate.h"
#include "rtl837x_port.h"
#include "machine.h"

#pragma codeseg BANK2
#pragma constseg BANK2

void cmd_parse_isolate(void)
{
	uint16_t members = 0;

	if (cmd_words_len < 3)
		goto err;

	print_string("\nISOLATE ");

	if (cmd_arg_port_separator(cmd_words_b[1]) == 0)
		goto err;
	uint8_t port_configured = atoi_results_u8;

	print_byte(port_configured); write_char('\n');

	if (cmd_compare(2, "show")) {
		members = port_isolation_get(port_configured);
		for (uint8_t i = 0; i < 10; i++) {
			if (members & 1) {
				print_phys_port(i);
				write_char(' ');
			}
			members >>= 1;
		}
		return;
    }

	if (cmd_compare(2, "off")) {
		for (uint8_t i = MACHINE_MIN_PORT; i <= MACHINE_MAX_PORT; i++)
			members |= ((uint16_t)1) << i;
		members |= PMASK_CPU; // CPU-port
		port_isolate(port_configured, members);
		return;
	}

	uint8_t w = 2;
	while (w < cmd_words_len) {
		if (cmd_arg_port_cpu_separator(cmd_words_b[w++]) == 0)
			goto err;
		uint8_t port = atoi_results_u8;
		members |= ((uint16_t)1) << port;
	}

	port_isolate(port_configured, members);
	return;

err:
	cmd_error("isolate <port> [show|off] [port]...\n");
}
