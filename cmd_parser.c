/*
 * A Command parser for RTL Switch configuration
 */

// #define DEBUG
// #define REGDBG 1

#define CONFIG_START 0x70000
#define CONFIG_LEN 0x1000

#include "rtl837x_common.h"
#include "rtl837x_port.h"
#include "rtl837x_flash.h"
#include "rtl837x_phy.h"
#include "rtl837x_regs.h"
#include "rtl837x_sfr.h"
#include "rtl837x_stp.h"
#include "uip/uip.h"

#pragma codeseg BANK1

extern __xdata uint8_t minPort;
extern __xdata uint8_t maxPort;
extern __xdata uint8_t nSFPPorts;
extern __xdata uint8_t isRTL8373;
extern __xdata uint16_t mpos;
extern __xdata uint8_t stpEnabled;

extern volatile __xdata uint32_t ticks;
extern volatile __xdata uint8_t sfr_data[4];

extern __code uint8_t * __code greeting;
extern __code uint8_t * __code hex;

extern __xdata uint8_t flash_buf[256];
__xdata uint8_t vlan_names[VLAN_NAMES_SIZE];
__xdata uint16_t vlan_ptr;
__xdata uint8_t gpio_last_value[8] = { 0 };

// Temporatly for str to hex convertion value.
// Support up to 32_bits.
__xdata uint8_t hexvalue[4] = { 0 };


// Buffer for writing to flash 0x1fd000, copy to 0x1fe000
__xdata uint8_t cmd_buffer[SBUF_SIZE];

__xdata	uint8_t l;
__xdata uint8_t line_ptr;
__xdata	char is_white;

__xdata uint8_t ip[4];

#define N_WORDS SBUF_SIZE
__xdata signed char cmd_words_b[N_WORDS];

// Maps the physical port (starting from 0) to the logical port
__code uint8_t phys_to_log_port[6] = {
	4, 5, 6, 7, 3, 8
};


inline uint8_t isletter(uint8_t l)
{
	// return (l >= 'a' && l <= 'z') || (l >= 'A' && l <= 'Z');

	// Make it lowercase
	l |= 0x20;
	l -= 'a';
	return (l <= ('z'-'a'));
}


inline uint8_t isnumber(uint8_t l)
{
	// return (l >= '0' && l <= '9');
	l -= '0';
	return (l <= ('9'-'0'));
}


uint8_t cmd_compare(uint8_t start, uint8_t * __code cmd)
{
	signed char i;
	signed char j = 0;

	for (i = cmd_words_b[start]; i != cmd_words_b[start + 1] && cmd_buffer[i] != ' '; i++) {
		i &= SBUF_SIZE - 1;
//		print_short(i); write_char(':'); print_short(j); write_char('#'); print_string("\n");
//		write_char('>'); write_char(cmd[j]); write_char('-'); write_char(cmd_buffer[i]); print_string("\n");
		if (!cmd[j])
			return 1;
		if (cmd_buffer[i] != cmd[j++])
			break;
	}
//	write_char('.'); print_short(i); write_char(':'); print_short(i);
	if (i == cmd_words_b[start + 1] || cmd_buffer[i] == ' ')
		return 1;
	return 0;
}


/* Converts ascii-hex array into value.
	returns number of hexvalue[] entries has been written.
	return value = 0 means error.
*/
uint8_t atoi_hex(uint8_t idx)
{
	uint8_t h_idx = 0;
	uint8_t val = 0;
	uint8_t c;

	while(1) {
		c = cmd_buffer[idx];

		if (c == '\0' || c == ' ') {
			break;
		}

		// swap hex nibbles
		val = (val >> 4) | (val << 4);

		if (c - '0' < 10) {
			val |= c - '0';
		} else {
			c |= 0x20;
			c -= 'a';
			if (c > 5) {
				h_idx = 0;
				break;
			}
			val |= c + 10;
		}

		idx++;
		hexvalue[h_idx >> 1] = val;

		if (h_idx & 1 == 1) {
			val = 0;
		}
		h_idx++;
	}

	return ((h_idx + 1) >> 1);
}

uint8_t atoi_short(register uint16_t *vlan, register uint8_t idx)
{
	uint8_t err = 1;
	*vlan = 0;

	while (isnumber(cmd_buffer[idx])) {
		err = 0;
		*vlan = (*vlan * 10) + cmd_buffer[idx] - '0';
		idx++;
	}
	return err;
}


uint8_t parse_ip(register uint8_t idx)
{
	__xdata uint8_t b;

	for (b = 0; b < 4; b++) {
		ip[b] = 0;
		while (isnumber(cmd_buffer[idx])) {
			ip[b] = (ip[b] * 10) + cmd_buffer[idx] - '0';
			idx++;
		}
		if (b < 3 && cmd_buffer[idx++] != '.') {
			print_string("Error in IP format, expecting '.'\n");
			return -1;
		}
	}
	return 0;
}


void parse_trunk(void)
{
	__xdata uint8_t group;
	__xdata uint16_t members = 0;

	group = cmd_buffer[cmd_words_b[1]] - '0';

	uint8_t w = 2;
	while (cmd_words_b[w] > 0) {
		uint8_t port;
		if (isnumber(cmd_buffer[cmd_words_b[w]])) {
			port = cmd_buffer[cmd_words_b[w]] - '1';
			if (port > maxPort)
				goto err;
			members |= ((uint16_t)1) << port;
		}
		w++;
	}
	trunk_set(group, members);
	return;
err:
	print_string("Error: trunk <trunk-id> [port]...");
}


void parse_vlan(void)
{
	__xdata uint16_t vlan;
	__xdata uint16_t members = 0;
	__xdata uint16_t tagged = 0;
	if (!atoi_short(&vlan, cmd_words_b[1])) {
		if (cmd_words_b[2] > 0 && cmd_buffer[cmd_words_b[2]] == 'd' && cmd_words_b[3] < 0) {
			vlan_delete(vlan);
			return;
		}
		uint8_t w = 2;
		write_char('#');
		print_byte(cmd_words_b[w] );
		write_char('#'); write_char(cmd_buffer[cmd_words_b[w]]);
		if (cmd_words_b[w] > 0 && isletter(cmd_buffer[cmd_words_b[w]])) {
			register uint8_t i = 0;
			vlan_names[vlan_ptr++] = hex[(vlan >> 8) & 0xf];
			vlan_names[vlan_ptr++] = hex[(vlan >> 4) & 0xf] ;
			vlan_names[vlan_ptr++] = hex[vlan & 0xf];
			print_string("COPYING: >");
			while(cmd_buffer[cmd_words_b[w] + i] != ' ') {
				write_char(cmd_buffer[cmd_words_b[w] + i]);
				vlan_names[vlan_ptr++] = cmd_buffer[cmd_words_b[w] + i++];
			}
			vlan_names[vlan_ptr++] = ' '; vlan_names[vlan_ptr] = '\0';
			w++;
			print_string("<\n");
		}
		while (cmd_words_b[w] > 0) {
			uint8_t port;
			if (isnumber(cmd_buffer[cmd_words_b[w]])) {
				port = cmd_buffer[cmd_words_b[w]] - '1';
				if (isnumber(cmd_buffer[cmd_words_b[w] + 1])) {
					port = (port + 1) * 10 + cmd_buffer[cmd_words_b[w] + 1] - '1';
					if (cmd_buffer[cmd_words_b[w] + 2] == 't')
						tagged |= ((uint16_t)1) << port;
				} else {
					if (!isRTL8373)
						port = phys_to_log_port[port];
					if (cmd_buffer[cmd_words_b[w] + 1] == 't')
						tagged |= ((uint16_t)1) << port;
				}
				if (port > maxPort)
					goto err;
				members |= ((uint16_t)1) << port;
			}
			w++;
		}
		vlan_create(vlan, members, tagged);
	}
	if (cmd_words_b[2] > 0 && isletter(cmd_buffer[cmd_words_b[2]])) {
		print_string("vlan_ptr "); print_short(vlan_ptr); write_char(':');
		write_char('>'); print_string_x(&vlan_names[0]); write_char('<'); write_char('\n');
	}
	return;
err:
	print_string("Error: vlan <vlan-id> [port][t/u]...");
}


void parse_mirror(void)
{
	__xdata uint8_t mirroring_port;
	__xdata uint16_t rx_pmask = 0;
	__xdata uint16_t tx_pmask = 0;

	if (!isnumber(cmd_buffer[cmd_words_b[1]])) {
		print_string("Port missing: port <mirroring port> [port][t/r]...");
		return;
	}

	mirroring_port = cmd_buffer[cmd_words_b[1]] - '1';
	if (isnumber(cmd_buffer[cmd_words_b[1] + 1]))
		mirroring_port = (mirroring_port + 1) * 10 + cmd_buffer[cmd_words_b[1] + 1] - '1';
	if (!isRTL8373)
		mirroring_port = phys_to_log_port[mirroring_port];

	uint8_t w = 2;
	while (cmd_words_b[w] > 0) {
		uint8_t port;
		if (isnumber(cmd_buffer[cmd_words_b[w]])) {
			port = cmd_buffer[cmd_words_b[w]] - '1';
			if (isnumber(cmd_buffer[cmd_words_b[w] + 1])) {
				port = (port + 1) * 10 + cmd_buffer[cmd_words_b[w] + 1] - '1';
				if (!isRTL8373)
					port = phys_to_log_port[port];
				if (cmd_buffer[cmd_words_b[w] + 2] == 'r')
					rx_pmask |= ((uint16_t)1) << port;
				else if (cmd_buffer[cmd_words_b[w] + 2] == 't')
					tx_pmask |= ((uint16_t)1) << port;
				else {
					rx_pmask |= ((uint16_t)1) << port;
					tx_pmask |= ((uint16_t)1) << port;
				}
			} else {
				if (!isRTL8373)
					port = phys_to_log_port[port];
				if (cmd_buffer[cmd_words_b[w] + 1] == 'r')
					rx_pmask |= ((uint16_t)1) << port;
				else if (cmd_buffer[cmd_words_b[w] + 1] == 't')
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


void parse_regget(void)
{
	uint16_t reg = 0;

	if (cmd_words_b[1] < 0) {
		goto err;
	}

	uint8_t hex_size = atoi_hex(cmd_words_b[1]);

	if (hex_size == 0 || hex_size > 2) {
		goto err;
	}

	reg = hexvalue[0];
	if (hex_size == 2) {
		reg <<= 8;
		reg |= hexvalue[1];
	}

	print_string("REGGET: ");
	print_short(reg);
	print_string(": VAL: ");

	reg_read_m(reg);
	print_sfr_data();
	return;

err:
	print_string("usage: regget <hexvalue>\n\tlike: regget 0BB0 or regget 0c");
	return;
}


void parse_regset(void)
{
	uint16_t reg = 0;

	if (cmd_words_b[2] < 0) {
		goto err;
	}

	uint8_t hex_size = atoi_hex(cmd_words_b[1]);
	if (hex_size == 0 || hex_size > 2) {
		goto err;
	}

	reg = hexvalue[0];
	if (hex_size == 2) {
		reg <<= 8;
		reg |= hexvalue[1];
	}

	hex_size = atoi_hex(cmd_words_b[2]);
	if (hex_size == 0 || hex_size > 4) {
		goto err;
	}

	// zero sfr memory data
	sfr_set_zero();

	// copy data over sfr memory
	uint8_t offset = 4 - hex_size;
	while(hex_size) {
		hex_size -= 1;
		sfr_data[offset + hex_size] = hexvalue[hex_size];
	}
	print_string("REGSET: ");
	print_short(reg);

	reg_write_m(reg);

	print_string(": VAL: ");
	print_sfr_data();
	return;

err:
	print_string("usage: regset <hexvalue> <hexvalue>\n\tlike regset 0b abcd1234.");
}


// Parse command into words
uint8_t cmd_tokenize(void) __banked
{
#ifdef DEBUG
	print_string("Tokenizing command\n");
	print_string_x(&cmd_buffer[0]);
	write_char('<'); write_char('\n');
#endif
	line_ptr = 0;
	is_white = 1;
	uint8_t word = 0;
	cmd_words_b[0] = -1;
	while (cmd_buffer[line_ptr] && line_ptr < SBUF_SIZE - 1) {
		if (is_white && cmd_buffer[line_ptr] != ' ') {
			is_white = 0;
			cmd_words_b[word++] = line_ptr;
		}
		if (cmd_buffer[line_ptr] == ' ')
			is_white = 1;
		line_ptr++;
		if (word >= N_WORDS - 1) {
			print_string("\ntoo many arguments, truncated");
			return 1;
		}
	}
	if (line_ptr == SBUF_SIZE - 1)
		return 1;
	cmd_words_b[word++] = line_ptr;
	cmd_words_b[word++] = -1;

	return 0;
}

// Print GPIO status
void print_gpio_status(void) {
	for (uint8_t idx = 0; idx < 2; idx++) {
		reg_read(RTL837X_REG_GPIO_00_31_INPUT + (idx * 4));
		print_string("GPIO ");
		write_char(idx + '0');
		write_char(':');
		write_char(' ');

		print_byte(SFR_DATA_24);
		print_byte(SFR_DATA_16);
		print_byte(SFR_DATA_8);
		print_byte(SFR_DATA_0);

		write_char(' ');
		print_byte( gpio_last_value[(idx *4)] ^ SFR_DATA_24);
		gpio_last_value[(idx *4)] = SFR_DATA_24;
		print_byte( gpio_last_value[(idx *4) + 1] ^ SFR_DATA_16);
		gpio_last_value[(idx *4) + 1] = SFR_DATA_16;
		print_byte( gpio_last_value[(idx *4) + 2] ^ SFR_DATA_8);
		gpio_last_value[(idx *4) + 2] = SFR_DATA_8;
		print_byte( gpio_last_value[(idx *4) + 3] ^ SFR_DATA_0);
		gpio_last_value[(idx *4) + 3] = SFR_DATA_0;
		write_char('\n');
	}
}


// Identify command
void cmd_parser(void) __banked
{
#ifdef DEBUG
	print_long(ticks);
	print_string("Parsing command\n");
	print_string_x(&cmd_buffer[0]);
	write_char('<'); write_char('\n');
#endif
	signed char i = cmd_words_b[0];
	if (i >= 0 && cmd_words_b[1] >= 0) {
		if (cmd_compare(0, "reset")) {
			print_string("\nRESET\n\n");
			reset_chip();
		}
		if (cmd_compare(0, "sfp")) {
			uint8_t rate = sfp_read_reg(0, 12);
			print_string("\nRate: "); print_byte(rate);
			print_string("  Encoding: "); print_byte(sfp_read_reg(0, 11));
			print_string("\n");
			for (uint8_t i = 20; i < 60; i++) {
				uint8_t c = sfp_read_reg(0, i);
				if (c)
					write_char(c);
			}
		}
		if (cmd_compare(0, "stat")) {
			port_stats_print();
		}
		if (cmd_compare(0, "flash") && cmd_words_b[1] > 0 && cmd_buffer[cmd_words_b[1]] == 'r') {
			print_string("\nPRINT SECURITY REGISTERS\n");
			// The following will only show something else than 0xff if it was programmed for a managed switch
			flash_read_security(0x0001000, 40);
			flash_read_security(0x0002000, 40);
			flash_read_security(0x0003000, 40);
		}
		if (cmd_compare(0, "flash") && cmd_words_b[1] > 0 && cmd_buffer[cmd_words_b[1]] == 'd') {
			print_string("\nDUMPING FLASH\n");
			flash_dump(0, 255);
		}
		if (cmd_compare(0, "flash") && cmd_words_b[1] > 0 && cmd_buffer[cmd_words_b[1]] == 'j') {
			print_string("\nJEDEC ID\n");
			flash_read_jedecid();
		}
		if (cmd_compare(0, "flash") && cmd_words_b[1] > 0 && cmd_buffer[cmd_words_b[1]] == 'u') {
			print_string("\nUNIQUE ID\n");
			flash_read_uid();
		}
		// Switch to flash 62.5 MHz mode
		if (cmd_compare(0, "flash") && cmd_words_b[1] > 0 && cmd_buffer[cmd_words_b[1]] == 's') {
			print_string("\nFLASH FAST MODE\n");
			flash_init(1);
			print_string("\nNow dumping flash\n");
			flash_dump(0, 255);
		}
		if (cmd_compare(0, "flash") && cmd_words_b[1] > 0 && cmd_buffer[cmd_words_b[1]] == 'e') {
			print_string("\nFLASH erase\n");
			flash_block_erase(0x20000);
		}
		if (cmd_compare(0, "flash") && cmd_words_b[1] > 0 && cmd_buffer[cmd_words_b[1]] == 'w') {
			print_string("\nFLASH write\n");
			for (uint8_t i = 0; i < 20; i++)
				flash_buf[i] = greeting[i];
			flash_write_bytes(0x20000, flash_buf, 20);
		}
		if (cmd_compare(0, "port") && cmd_words_b[1] > 0) {
			print_string("\nPORT ");
			uint8_t p = cmd_buffer[cmd_words_b[1]] - '1';
			print_byte(p);
			if (cmd_words_b[2] > 0 && cmd_compare(2, "2g5")) {
				print_string(" 2.5G\n");
				phy_set_mode(p, PHY_SPEED_2G5, 0, 0);
			}
			if (cmd_words_b[2] > 0 && cmd_compare(2, "1g")) {
				print_string(" 1G\n");
				phy_set_mode(p, PHY_SPEED_1G, 0, 0);
			}
			if (cmd_words_b[2] > 0 && cmd_compare(2, "auto")) {
				print_string(" AUTO\n");
				phy_set_mode(p, PHY_SPEED_AUTO, 0, 0);
			}
			if (cmd_words_b[2] > 0 && cmd_compare(2, "off")) {
				print_string(" OFF\n");
				phy_set_mode(p, PHY_OFF, 0, 0);
			}
		}
		if (cmd_compare(0, "ip")) {
			print_string("Got ip command: ");
			if (!parse_ip(cmd_words_b[1]))
				uip_ipaddr(&uip_hostaddr, ip[0], ip[1], ip[2], ip[3]);
			else
				print_string("Invalid IP address\n");
			print_byte(ip[0]); print_byte(ip[1]); print_byte(ip[2]); print_byte(ip[3]);
			write_char('\n');
		}
		if (cmd_compare(0, "gw")) {
			print_string("Got gw command: ");
			if (!parse_ip(cmd_words_b[1]))
				uip_ipaddr(&uip_draddr, ip[0], ip[1], ip[2], ip[3]);
			else
				print_string("Invalid IP address\n");
			print_byte(ip[0]); print_byte(ip[1]); print_byte(ip[2]); print_byte(ip[3]);
			write_char('\n');
		}
		if (cmd_compare(0, "netmask")) {
			print_string("Got netmask command: ");
			if (!parse_ip(cmd_words_b[1]))
				uip_ipaddr(&uip_netmask, ip[0], ip[1], ip[2], ip[3]);
			else
				print_string("Invalid IP address\n");
			print_byte(ip[0]);print_byte(ip[1]);print_byte(ip[2]);print_byte(ip[3]);
			write_char('\n');
		}
		if (cmd_compare(0, "l2")) {
			if (cmd_words_b[1] > 0 && cmd_compare(1, "forget"))
				port_l2_forget();
			else
				port_l2_learned();
		}
		if (cmd_compare(0, "stp")) {
			if (cmd_words_b[1] > 0 && cmd_compare(1, "on")) {
				print_string("STP enabled\n");
				stpEnabled = 1;
				stp_setup();
			} else {
				print_string("STP disabled\n");
				stp_off();
				stpEnabled = 0;
			}
		}
		if (cmd_compare(0, "pvid") && cmd_words_b[1] > 0 && cmd_words_b[2] > 0) {
			__xdata uint16_t pvid;
			uint8_t port;
			port = cmd_buffer[cmd_words_b[1]] - '1';
			if (!isRTL8373)
				port = phys_to_log_port[port];
			if (!atoi_short(&pvid, cmd_words_b[2]))
				port_pvid_set(port, pvid);
		}
		if (cmd_compare(0, "vlan")) {
			parse_vlan();
		}
		if (cmd_compare(0, "mirror")) {
			parse_mirror();
		}
		if (cmd_compare(0, "trunk")) {
			parse_trunk();
		}
		if (cmd_compare(0, "sds")) {
			print_reg(RTL837X_REG_SDS_MODES);
		}
		if (cmd_compare(0, "gpio")) {
			print_gpio_status();
		}
		if (cmd_compare(0, "regget")) {
			parse_regget();
		}
		if (cmd_compare(0, "regset")) {
			parse_regset();
		}
	}
}

void execute_config(void) __banked
{
	__xdata uint32_t pos = CONFIG_START;
	__xdata uint16_t len_left = CONFIG_LEN;
	do {
		flash_find_mark(pos, len_left, "\n");
		if (mpos != 0xffff) {
			__xdata uint16_t len = len_left - mpos;
			flash_read_bulk(&cmd_buffer[0], pos, len > SBUF_SIZE ? SBUF_SIZE : len);
			cmd_buffer[len > SBUF_SIZE ? SBUF_SIZE : len] = '\0';
			len++;
			pos += len;
			len_left -= len;
			if (len && !cmd_tokenize())
				cmd_parser();
		}
	} while (mpos != 0xffff);
}

