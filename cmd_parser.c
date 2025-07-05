/*
 * A Command parser for RTL Switch configuration
 */

#define DEBUG

#include <stdint.h>
#include "rtl837x_common.h"
#include "rtl837x_port.h"
#include "rtl837x_flash.h"
#include "rtl837x_phy.h"

#pragma codeseg BANK1

extern volatile __xdata uint32_t ticks;
extern __xdata char sbuf_ptr;
extern __xdata uint8_t sbuf[SBUF_SIZE];

extern __code uint8_t * __code greeting;
extern __code uint8_t * __code hex;

extern __xdata uint8_t flash_buf[256];


__xdata	char l;
__xdata char line_ptr;
__xdata	char is_white;

#define N_WORDS 16
__xdata signed char cmd_words_b[N_WORDS];


uint8_t cmd_compare(uint8_t start, uint8_t * __code cmd)
{
	signed char i;
	signed char j = 0;

	for (i = cmd_words_b[start]; i < cmd_words_b[start + 1] && sbuf[i] != ' '; i++) {
//		print_short(i); write_char(':'); print_short(j); write_char('#'); print_string("\r\n");
//		write_char('>'); write_char(cmd[j]); write_char('-'); write_char(sbuf[i]); print_string("\r\n");
		if (!cmd[j])
			return 1;
		if (sbuf[i] != cmd[j++])
			break;
	}
//	write_char('.'); print_short(i); write_char(':'); print_short(i);
	if (i >= cmd_words_b[start + 1] || sbuf[i] == ' ')
		return 1;
	return 0;
}


uint8_t atoi_short(uint16_t *vlan, uint8_t idx)
{
	uint8_t err = 1;
	*vlan = 0;

	while (sbuf[idx] >= '0' && sbuf[idx] <= '9') {
		err = 0;
		*vlan = (*vlan * 10) + sbuf[idx] - '0';
		idx++;
	}
	return err;
}


void parse_vlan(void) __banked
{
	__xdata uint16_t vlan;
	__xdata uint16_t members = 0;
	__xdata uint16_t tagged = 0;
	if (!atoi_short(&vlan, cmd_words_b[1])) {
		print_short(vlan);
		if (cmd_words_b[2] > 0 && sbuf[cmd_words_b[2]] == 'd') {
			vlan_delete(vlan);
			return;
		}
		uint8_t w = 2;
		while (cmd_words_b[w] > 0) {
			uint8_t port;
			if (sbuf[cmd_words_b[w]] >= '0' && sbuf[cmd_words_b[w]] <= '9') {
				port = sbuf[cmd_words_b[w]] - '1';
				if (sbuf[cmd_words_b[w] + 1] >= '0' && sbuf[cmd_words_b[w] + 1] <= '9') {
					port = (port + 1) * 10 + sbuf[cmd_words_b[w] + 1] - '1';
					if (sbuf[cmd_words_b[w] + 2] == 't')
						tagged |= ((uint16_t)1) << port;
				} else {
					if (sbuf[cmd_words_b[w] + 1] == 't')
						tagged |= ((uint16_t)1) << port;
				}
				members |= ((uint16_t)1) << port;
			}
			w++;
		}
		vlan_create(vlan, members, tagged);
	}
}


void parse_mirror(void) __banked
{
	__xdata uint8_t mirroring_port;
	__xdata uint16_t rx_pmask = 0;
	__xdata uint16_t tx_pmask = 0;
	uint8_t w = 2;

	if (sbuf[cmd_words_b[1]] < '0' || sbuf[cmd_words_b[1]] > '9') {
		print_string("Port missing: port <mirroring port> [port][t/r]...");
		return;
	}
		
	mirroring_port = sbuf[cmd_words_b[w]] - '1';
	if (sbuf[cmd_words_b[1] + 1] >= '0' && sbuf[cmd_words_b[1] + 1] <= '9')
		mirroring_port = (mirroring_port + 1) * 10 + sbuf[cmd_words_b[1] + 1] - '1';

	while (cmd_words_b[w] > 0) {
		uint8_t port;
		if (sbuf[cmd_words_b[w]] >= '0' && sbuf[cmd_words_b[w]] <= '9') {
			port = sbuf[cmd_words_b[w]] - '1';
			if (sbuf[cmd_words_b[w] + 1] >= '0' && sbuf[cmd_words_b[w] + 1] <= '9') {
				port = (port + 1) * 10 + sbuf[cmd_words_b[w] + 1] - '1';
				if (sbuf[cmd_words_b[w] + 2] == 'r')
					rx_pmask |= ((uint16_t)1) << port;
				else if (sbuf[cmd_words_b[w] + 2] == 't')
					tx_pmask |= ((uint16_t)1) << port;
				else {
					rx_pmask |= ((uint16_t)1) << port;
					tx_pmask |= ((uint16_t)1) << port;
				}
			} else {
				if (sbuf[cmd_words_b[w] + 1] == 'r')
					rx_pmask |= ((uint16_t)1) << port;
				else if (sbuf[cmd_words_b[w] + 1] == 't')
					tx_pmask |= ((uint16_t)1) << port;
				else {
					rx_pmask |= ((uint16_t)1) << port;
					tx_pmask |= ((uint16_t)1) << port;
				}
			}
		}
		w++;
	}
	port_mirror_set(mirroring_port, rx_pmask, tx_pmask);
}


void cmd_parser(void) __banked
{
	while (l != sbuf_ptr) {
		write_char(sbuf[l]);
		// Check whether there is a full line:
		if (sbuf[l] == '\n' || sbuf[l] == '\r') {
#ifdef DEBUG
			print_long(ticks);
#endif
			// Print line and parse command into words
			is_white = 1;
			uint8_t word = 0;
			cmd_words_b[0] = -1;
			while (line_ptr != l) {
				if (is_white && sbuf[line_ptr] != ' ') {
					is_white = 0;
					cmd_words_b[word++] = line_ptr;
				}
				if (sbuf[line_ptr] == ' ')
					is_white = 1;
				write_char(sbuf[line_ptr++]);
				line_ptr &= SBUF_SIZE - 1;
				if (word >= N_WORDS - 1) {
					print_string("\r\ntoo many arguments, truncated");
					line_ptr = l;	// BUG: We should probably ignore the command
					break;
				}
			}
			cmd_words_b[word++] = line_ptr;
			cmd_words_b[word++] = -1;
			line_ptr = (l + 1) & (SBUF_SIZE - 1);

			// Identify command
			signed char i = cmd_words_b[0];
			if (i >= 0 && cmd_words_b[1] >= 0) {
				if (cmd_compare(0, "reset")) {
					print_string("\r\nRESET\n\n");
					reset_chip();
				}
				if (cmd_compare(0, "sfp")) {
					uint8_t rate = sfp_read_reg(12);
					print_string("\r\nRate: "); print_byte(rate);
					print_string("  Encoding: "); print_byte(sfp_read_reg(11));
					print_string("\r\n");
					for (uint8_t i = 20; i < 60; i++) {
						uint8_t c = sfp_read_reg(i);
						if (c)
							write_char(c);
					}
				}
				if (cmd_compare(0, "stat")) {
					port_stats_print();
				}
				if (cmd_compare(0, "flash") && cmd_words_b[1] > 0 && sbuf[cmd_words_b[1]] == 'r') {
					print_string("\r\nPRINT SECURITY REGISTERS\r\n");
					// The following will only show something else then 0xff if it was programmed for a managed switch
					flash_read_security(0x0001000, 40);
					flash_read_security(0x0002000, 40);
					flash_read_security(0x0003000, 40);
				}
				if (cmd_compare(0, "flash") && cmd_words_b[1] > 0 && sbuf[cmd_words_b[1]] == 'd') {
					print_string("\r\nDUMPING FLASH\r\n");
					flash_dump(0, 255);
				}
				if (cmd_compare(0, "flash") && cmd_words_b[1] > 0 && sbuf[cmd_words_b[1]] == 'j') {
					print_string("\r\nJEDEC ID\r\n");
					flash_read_jedecid();
				}
				if (cmd_compare(0, "flash") && cmd_words_b[1] > 0 && sbuf[cmd_words_b[1]] == 'u') {
					print_string("\r\nUNIQUE ID\r\n");
					flash_read_uid();
				}
				// Switch to flash 62.5 MHz mode
				if (cmd_compare(0, "flash") && cmd_words_b[1] > 0 && sbuf[cmd_words_b[1]] == 's') {
					print_string("\r\nFLASH FAST MODE\r\n");
					flash_init(1);
					print_string("\r\nNow dumping flash\r\n");
					flash_dump(0, 255);
				}
				if (cmd_compare(0, "flash") && cmd_words_b[1] > 0 && sbuf[cmd_words_b[1]] == 'e') {
					print_string("\r\nFLASH erase\r\n");
					flash_block_erase(0x20000);
				}
				if (cmd_compare(0, "flash") && cmd_words_b[1] > 0 && sbuf[cmd_words_b[1]] == 'w') {
					print_string("\r\nFLASH write\r\n");
					for (uint8_t i = 0; i < 20; i++)
						flash_buf[i] = greeting[i];
					flash_write_bytes(0x20000, flash_buf, 20);
				}
				if (cmd_compare(0, "port") && cmd_words_b[1] > 0) {
					print_string("\r\nPORT ");
					uint8_t p = sbuf[cmd_words_b[1]] - '1';
					print_byte(p);
					if (cmd_words_b[2] > 0 && cmd_compare(2, "2g5")) {
						print_string(" 2.5G\r\n");
						phy_set_mode(p, PHY_SPEED_2G5, 0, 0);
					}
					if (cmd_words_b[2] > 0 && cmd_compare(2, "1g")) {
						print_string(" 1G\r\n");
						phy_set_mode(p, PHY_SPEED_1G, 0, 0);
					}
					if (cmd_words_b[2] > 0 && cmd_compare(2, "auto")) {
						print_string(" AUTO\r\n");
						phy_set_mode(p, PHY_SPEED_AUTO, 0, 0);
					}
					if (cmd_words_b[2] > 0 && cmd_compare(2, "off")) {
						print_string(" OFF\r\n");
						phy_set_mode(p, PHY_OFF, 0, 0);
					}
				}
				if (cmd_compare(0, "l2")) {
					port_l2_learned();
				}
				if (cmd_compare(0, "pvid") && cmd_words_b[1] > 0 && cmd_words_b[2] > 0) {
					__xdata uint16_t pvid;
					uint8_t port;
					port = sbuf[cmd_words_b[1]] - '0';
					if (!atoi_short(&pvid, cmd_words_b[2]))
						port_pvid_set(port, pvid);
				}
				if (cmd_compare(0, "vlan")) {
					parse_vlan();
				}
				if (cmd_compare(0, "mirror")) {
					parse_mirror();
				}
			}
			print_string("\r\n> ");
		}
		l++;
		l &= (SBUF_SIZE - 1);
	}
}


void cmd_parser_setup(void) __banked
{
	l = sbuf_ptr;
	line_ptr = l;
	is_white = 1;
}

