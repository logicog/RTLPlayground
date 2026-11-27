/*
 * "bw" command
 */

#include "cmd_parse_util.h"
#include "cmd_parse_bw.h"
#include "rtl837x_bandwidth.h"

#pragma codeseg BANK2
#pragma constseg BANK2

void cmd_parse_bw(void)
{
	if (cmd_words_len < 3) // Check for at least 3 arguments
		goto err;

	if (cmd_arg_port_separator(cmd_words_b[2]) == 0)
		goto err;
	uint8_t port = atoi_results_u8;

	if (cmd_compare(1, "status")) {
		bandwidth_status(port);
		return;
	}

	if (cmd_words_len < 4) // Check for at least 4 arguments
		goto err;

	// Ensure first argument is only `in` or `out`.
	__bit is_in = cmd_compare(1, "in") != 0;
	if (!(is_in || cmd_compare(1, "out")))
		goto err;

	if (cmd_compare(3, "drop")) {
		if (is_in) {
			bandwidth_ingress_drop(port);
			return;
		}
		goto err;
	}

	if (cmd_compare(3, "fc")) {
		if (is_in) {
			bandwidth_ingress_fc(port);
			return;
		}
		goto err;
	}

	if (cmd_compare(3, "off")) {
		if (is_in) {
			bandwidth_ingress_disable(port);
		} else  {
			bandwidth_egress_disable(port);
		}
		return;
	}

	__xdata uint32_t bw = 0;
	uint8_t hex_size = atoi_hex(cmd_words_b[3]);
	if (hex_size == 0 || hex_size > 4)
		goto err;

	uint8_t i = 0;
	do {
		hex_size--;
		*(((uint8_t *) &bw) + hex_size) = hexvalue[i++];
	} while (hex_size);

	if (bw < 16)
		goto err;

	if (is_in) {
		bandwidth_ingress_set(port, bw);
	} else {
		bandwidth_egress_set(port, bw);
	}
	return;

err:
	cmd_error("bw [in|out|status] <port> [<hexvalue>|off|drop|fc]\n");
}
