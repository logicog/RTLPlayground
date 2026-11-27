/*
 * "mirror" command
 */

#include "cmd_parse_util.h"
#include "cmd_parse_mirror.h"
#include "rtl837x_port.h"

#pragma codeseg BANK2
#pragma constseg BANK2

extern volatile __xdata uint8_t sfr_data[4];

void cmd_parse_mirror(void)
{
	__xdata uint8_t mirroring_port = 0;
	__xdata uint16_t rx_pmask = 0;
	__xdata uint16_t tx_pmask = 0;

	if (cmd_compare(1, "status")) {
		reg_read_m(RTL837x_MIRROR_CTRL);
		uint8_t mPort = sfr_data[3];
		if (mPort & 1) {
			print_string("Enabled: ");
		} else {
			print_string("NOT Enabled: ");
		}
		print_string("Mirroring port: ");
		print_phys_port(mPort >> 1);
		reg_read_m(RTL837x_MIRROR_CONF);
		uint16_t m = sfr_data[0];
		m = (m << 8) | sfr_data[1];
		print_string(", Port mask RX: ");
		print_short(m);
		m = sfr_data[2];
		m = (m << 8) | sfr_data[3];
		print_string(", Port mask TX: ");
		print_short(m);
		write_char('\n');
		return;
	} else if (cmd_compare(1, "off")) {
		port_mirror_del();
		return;
	}

	if (cmd_words_len < 2)
		goto err;

	// The first argument is the mirroring (monitor) port
	uint8_t ret = cmd_arg_port_separator(cmd_words_b[1]);
	if (ret == 0)
		goto err;
	mirroring_port = atoi_results_u8;

	uint8_t w = 2;
	uint8_t port;
	while (w < cmd_words_len) {
		uint8_t idx = cmd_words_b[w++];
		ret = cmd_arg_port(idx);
		if (ret == 0)
			goto err;

		idx += ret;
		port = atoi_results_u8;

		ret = cmd_buffer[idx];
		if (ret == 't' || ret == 'r')
			idx++;
		if (!cmd_is_space_or_nul(idx))
			goto err;
		uint16_t pmask = ((uint16_t)1) << port;
		if (ret != 't')
			rx_pmask |= pmask;
		if (ret != 'r')
			tx_pmask |= pmask;
	}
	port_mirror_set(mirroring_port, rx_pmask, tx_pmask);
	return;

err:
	cmd_error("Port/command missing: mirror [status/off/<mirroring port> [port][t/r]]...\n");
	return;
}
