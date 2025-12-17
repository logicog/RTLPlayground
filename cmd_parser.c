/*
 * A Command parser for RTL Switch configuration
 */

// #define DEBUG
// #define REGDBG 1

#include "rtl837x_common.h"
#include "rtl837x_port.h"
#include "rtl837x_flash.h"
#include "rtl837x_phy.h"
#include "rtl837x_regs.h"
#include "rtl837x_sfr.h"
#include "rtl837x_stp.h"
#include "rtl837x_igmp.h"
#include "uip/uip.h"
#include "version.h"

#include "machine.h"

#pragma codeseg BANK1
#pragma constseg BANK1

extern __code struct machine machine;
extern __xdata uint16_t mpos;
extern __xdata uint8_t stpEnabled;
extern __code uint8_t log_to_phys_port[9];

extern volatile __xdata uint32_t ticks;
extern volatile __xdata uint8_t sfr_data[4];

extern __code uint8_t * __code greeting;
extern __code uint8_t * __code hex;

extern __xdata uint8_t flash_buf[512];
extern __xdata struct flash_region_t flash_region;

extern __xdata char passwd[21];

__xdata uint8_t vlan_names[VLAN_NAMES_SIZE];
__xdata uint16_t vlan_ptr;
__xdata uint8_t gpio_last_value[8] = { 0 };

// Temporatly for str to hex convertion value.
// Support up to 32_bits.
__xdata uint8_t hexvalue[4] = { 0 };


// Buffer for writing to flash 0x1fd000, copy to 0x1fe000
__xdata uint8_t cmd_buffer[SBUF_SIZE];
__xdata uint8_t cmd_available;

__xdata	uint8_t l;
__xdata uint8_t line_ptr;
__xdata	char is_white;
__xdata	char save_cmd;

__xdata uint8_t ip[4];

#define N_WORDS SBUF_SIZE
__xdata signed char cmd_words_b[N_WORDS];

__xdata uint8_t cmd_history[CMD_HISTORY_SIZE];
__xdata uint16_t cmd_history_ptr;


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
//		print_byte(i); write_char(':'); print_byte(j); write_char('#'); print_string("\n");
//		write_char('>'); write_char(cmd[j]); write_char('-'); write_char(cmd_buffer[i]); print_string("\n");
		if (!cmd[j] && !isletter(cmd_buffer[i]))
			return 1;
		if (cmd_buffer[i] != cmd[j++])
			break;
	}
//	write_char('.'); print_byte(i); write_char(':'); print_byte(j); write_char(','); print_byte (cmd[j-1]);
//	write_char(','); print_byte(cmd[j]);
	if (i == cmd_words_b[start + 1] || (cmd_buffer[i] == ' ' && !cmd[j]))
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


void parse_lag(void)
{
	__xdata uint8_t group;
	__xdata uint16_t members = 0;

	if (cmd_words_b[1] > 0 && cmd_compare(1, "show")) {
		print_string("LAG status:\n");
		for (uint8_t i = 0; i < 4; i++) {
			write_char(' '); write_char('1' + i);
			reg_read_m(RTL837X_TRK_MBR_CTRL_BASE + (i << 2));
			members = ((uint16_t)sfr_data[2]) << 8 | sfr_data[3]; 
			if (!members) {
				print_string(" disabled\n");
				continue;
			}
			print_string(" member ports: ");
			for (uint8_t j = 0; j < 10; j++) {
				if (members & 1) {
					write_char('0' + machine.log_to_phys_port[j]);
					write_char(' ');
				}
				members >>= 1;
			}
			print_string(" (hash: 0x"); 
			reg_read_m(RTL837X_TRK_HASH_CTRL_BASE + (i << 2));
			print_byte(sfr_data[3]);
			print_string(")\n");
		}
		return;
	}

	if (cmd_words_b[2] <= 0 || !isnumber(cmd_buffer[cmd_words_b[1]]))
		goto err;
	group = cmd_buffer[cmd_words_b[1]] - '0';

	uint8_t w = 2;
	while (cmd_words_b[w + 1] > 0) {
//		write_char('|'); print_byte(w); write_char(':'); write_char(cmd_buffer[cmd_words_b[w]]); write_char('-');
		uint8_t port;
		if (isnumber(cmd_buffer[cmd_words_b[w]])) {
			port = cmd_buffer[cmd_words_b[w]] - '1';
			if (isnumber(cmd_buffer[cmd_words_b[w] + 1]))
				port = (port + 1) * 10 + cmd_buffer[cmd_words_b[w] + 1] - '1';
				port = machine.phys_to_log_port[port];
		} else {
			goto err;
		}
		if (port > machine.max_port)
			goto err;
		members |= ((uint16_t)1) << port;
		w++;
	}
	port_lag_members_set(group, members);
	return;
err:
	print_string("Error: lag <lag> [port]...");
}


void parse_lag_hash(void)
{
	__xdata uint8_t group;
	__xdata uint8_t hash = 0;

	group = cmd_buffer[cmd_words_b[1]] - '0';

	uint8_t w = 2;
	while (cmd_words_b[w + 1] > 0) {
		if (cmd_compare(w, "spa"))
			hash |= LAG_HASH_SOURCE_PORT_NUMBER;
		else if (cmd_compare(w, "smac"))
			hash |= LAG_HASH_L2_SMAC;
		else if (cmd_compare(w, "dmac"))
			hash |= LAG_HASH_L2_DMAC;
		else if (cmd_compare(w, "sip"))
			hash |= LAG_HASH_L3_SIP;
		else if (cmd_compare(w, "dip"))
			hash |= LAG_HASH_L3_DIP;
		else if (cmd_compare(w, "sport"))
			hash |= LAG_HASH_L4_SPORT;
		else if (cmd_compare(w, "dport"))
			hash |= LAG_HASH_L4_DPORT;
		else {
			print_string("Error: invalid hash type:");
			print_string_x(&cmd_buffer[cmd_words_b[w]]);
			write_char('\n');
		}
		w++;
	}
	port_lag_hash_set(group, hash);
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
						port = machine.phys_to_log_port[port];
					if (cmd_buffer[cmd_words_b[w] + 1] == 't')
						tagged |= ((uint16_t)1) << port;
				}
				if (port > machine.max_port)
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

	if (cmd_words_b[1] > 0 && cmd_compare(1, "status")) {
		reg_read_m(RTL837x_MIRROR_CTRL);
		uint8_t mPort = sfr_data[3];
		if (mPort & 1) {
			print_string("Enabled: ");
		} else {
			print_string("NOT Enabled: ");
		}
		print_string("Mirroring port: ");
		write_char('0' + machine.log_to_phys_port[mPort >> 1]);
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
	} else if (cmd_words_b[1] > 0 && cmd_compare(1, "off")) {
		port_mirror_del();
		return;
	}

	if (!isnumber(cmd_buffer[cmd_words_b[1]])) {
		print_string("Port missing: mirror <mirroring port> [port][t/r]...");
		return;
	}

	mirroring_port = cmd_buffer[cmd_words_b[1]] - '1';
	if (isnumber(cmd_buffer[cmd_words_b[1] + 1]))
		mirroring_port = (mirroring_port + 1) * 10 + cmd_buffer[cmd_words_b[1] + 1] - '1';
		mirroring_port = machine.phys_to_log_port[mirroring_port];

	uint8_t w = 2;
	while (cmd_words_b[w] > 0) {
		uint8_t port;
		if (isnumber(cmd_buffer[cmd_words_b[w]])) {
			port = cmd_buffer[cmd_words_b[w]] - '1';
			if (isnumber(cmd_buffer[cmd_words_b[w] + 1])) {
				port = (port + 1) * 10 + cmd_buffer[cmd_words_b[w] + 1] - '1';
				port = machine.phys_to_log_port[port];
				if (cmd_buffer[cmd_words_b[w] + 2] == 'r')
					rx_pmask |= ((uint16_t)1) << port;
				else if (cmd_buffer[cmd_words_b[w] + 2] == 't')
					tx_pmask |= ((uint16_t)1) << port;
				else {
					rx_pmask |= ((uint16_t)1) << port;
					tx_pmask |= ((uint16_t)1) << port;
				}
			} else {
				port = machine.phys_to_log_port[port];
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


void parse_port(void)
{
	print_string("\nPORT ");
	uint8_t p = cmd_buffer[cmd_words_b[1]] - '1';
	p = machine.phys_to_log_port[p];
	print_byte(p);
	if (machine.is_sfp[p]) {
		print_string(" is SFP no PHY information available.\n");
		return;
	}
	if (cmd_words_b[2] > 0 && cmd_compare(2, "10m")) {
		print_string(" 10M\n");
		phy_set_speed(p, PHY_SPEED_10M);
	}
	if (cmd_words_b[2] > 0 && cmd_compare(2, "100m")) {
		print_string(" 100M\n");
		phy_set_speed(p, PHY_SPEED_100M);
	}
	if (cmd_words_b[2] > 0 && cmd_compare(2, "2g5")) {
		print_string(" 2.5G\n");
		phy_set_speed(p, PHY_SPEED_2G5);
	}
	if (cmd_words_b[2] > 0 && cmd_compare(2, "1g")) {
		print_string(" 1G\n");
		phy_set_speed(p, PHY_SPEED_1G);
	}
	if (cmd_words_b[2] > 0 && cmd_compare(2, "auto")) {
		print_string(" AUTO\n");
		phy_set_speed(p, PHY_SPEED_AUTO);
	}
	if (cmd_words_b[2] > 0 && cmd_compare(2, "off")) {
		print_string(" OFF\n");
		phy_set_speed(p, PHY_OFF);
	}
	if (cmd_words_b[2] > 0 && cmd_compare(2, "duplex")) {
		print_string(" DUPLEX\n");
		if (cmd_words_b[3] > 0 && cmd_compare(3, "full"))
			phy_set_duplex(p, 1);
		else
			phy_set_duplex(p, 0);
	}
	if (cmd_words_b[2] > 0 && cmd_compare(2, "show")) {
		phy_show(p);
	}
}


void sfp_print_measurements(uint8_t sfp)
{
	print_string("Options: "); print_byte(sfp_read_reg(sfp, 92)); write_char('\n');
	if (!(sfp_read_reg(sfp, 92) & 0x40))
		return;
	print_string("Temp: "); print_byte(sfp_read_reg(sfp, 224)); print_byte(sfp_read_reg(sfp, 225)); write_char('\n');
	print_string("Vcc: "); print_byte(sfp_read_reg(sfp, 226)); print_byte(sfp_read_reg(sfp, 227)); write_char('\n');
	print_string("TX Bias: "); print_byte(sfp_read_reg(sfp, 228)); print_byte(sfp_read_reg(sfp, 229)); write_char('\n');
	print_string("TX Power: "); print_byte(sfp_read_reg(sfp, 230)); print_byte(sfp_read_reg(sfp, 231)); write_char('\n');
	print_string("RX Power: "); print_byte(sfp_read_reg(sfp, 232)); print_byte(sfp_read_reg(sfp, 233)); write_char('\n');
	print_string("Laser: "); print_byte(sfp_read_reg(sfp, 234)); print_byte(sfp_read_reg(sfp, 235)); write_char('\n');
	print_string("State: "); print_byte(sfp_read_reg(sfp, 238)); write_char('\n');
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


void parse_rnd(void)
{
	// In order to get a new random numner, this bit has to be set each time!
	reg_bit_set(RTL837X_RLDP_RLPP, RLDP_RND_EN);
	reg_read_m(RTL837X_RAND_NUM1);
	print_byte(sfr_data[2]);
	print_byte(sfr_data[3]);
	reg_read_m(RTL837X_RAND_NUM0);
	print_byte(sfr_data[0]);
	print_byte(sfr_data[1]);
	print_byte(sfr_data[2]);
	print_byte(sfr_data[3]);
	write_char('\n');
}


void parse_passwd(void)
{
	if (cmd_words_b[2] > 0) {
		signed char i;
		signed char j = 0;
		for (i = cmd_words_b[1]; (i != cmd_words_b[2] && i - cmd_words_b[1] < 20); i++)
			passwd[j++] = cmd_buffer[i];
		passwd[j] = '\0';
		return;
	}
	print_string("Missing password\n");
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

// Show software version
void print_sw_version(void) __banked {
	print_string("Software version: ");
	print_string(VERSION_SW);
	write_char('\n');
}


// Identify command
void cmd_parser(void) __banked
{
#ifdef DEBUG
	print_long(ticks);
	print_string("Parsing command\n");
	print_string_x(&cmd_buffer[0]);
	write_char('<'); write_char('\n');
	print_string("CMD-words: ");
	print_byte(cmd_words_b[0]); write_char(' ');
	print_byte(cmd_words_b[1]); write_char(' ');
	print_byte(cmd_words_b[2]); write_char(' ');
	print_byte(cmd_words_b[3]); write_char(' ');
	print_byte(cmd_words_b[4]); write_char(' ');
	print_byte(cmd_words_b[5]); write_char(' ');
	print_byte(cmd_words_b[6]); write_char('\n');
#endif
	if (cmd_words_b[0] >= 0 && cmd_words_b[1] >= 0) {
		if (cmd_compare(0, "reset")) {
			print_string("\nRESET\n\n");
			reset_chip();
		} else if (cmd_compare(0, "sfp")) {
			print_string("\nSlot 1 - Rate: "); print_byte(sfp_read_reg(0, 12));
			print_string("  Encoding: "); print_byte(sfp_read_reg(0, 11));
			print_string("\n");
			sfp_print_info(0);
			sfp_print_measurements(0);
			if (machine.n_sfp == 2) {
				print_string("\nSlot 2 - Rate: "); print_byte(sfp_read_reg(1, 12));
				print_string("  Encoding: "); print_byte(sfp_read_reg(1, 11));
				print_string("\n");
				sfp_print_info(1);
				sfp_print_measurements(1);
			}
		} else if (cmd_compare(0, "stat")) {
			port_stats_print();
		} else if (cmd_compare(0, "flash") && cmd_words_b[1] > 0 && cmd_buffer[cmd_words_b[1]] == 'r') {
			print_string("\nPRINT SECURITY REGISTERS\n");
			// The following will only show something else than 0xff if it was programmed for a managed switch
			flash_region.addr = 0x0001000;
			flash_region.len = 40;
			flash_read_security();
			flash_region.addr = 0x0002000;
			flash_region.len = 40;
			flash_read_security();
			flash_region.addr = 0x0003000;
			flash_region.len = 40;
			flash_read_security();
		} else if (cmd_compare(0, "flash") && cmd_words_b[1] > 0 && cmd_buffer[cmd_words_b[1]] == 'd') {
			print_string("\nDUMPING FLASH\n");
			flash_region.addr = 0;
			flash_region.len = 255;
			flash_dump(255);
		} else if (cmd_compare(0, "flash") && cmd_words_b[1] > 0 && cmd_buffer[cmd_words_b[1]] == 'j') {
			print_string("\nJEDEC ID\n");
			flash_read_jedecid();
		} else if (cmd_compare(0, "flash") && cmd_words_b[1] > 0 && cmd_buffer[cmd_words_b[1]] == 'u') {
			print_string("\nUNIQUE ID\n");
			flash_read_uid();
		} else if (cmd_compare(0, "flash") && cmd_words_b[1] > 0 && cmd_buffer[cmd_words_b[1]] == 's') {
			print_string("\nFLASH FAST MODE\n"); // Switch to flash 62.5 MHz mode
			flash_init(1);
			print_string("\nNow dumping flash\n");
			flash_region.addr = 0;
			flash_region.len = 255;
			flash_dump(255);
		} else if (cmd_compare(0, "flash") && cmd_words_b[1] > 0 && cmd_buffer[cmd_words_b[1]] == 'e') {
			print_string("\nFLASH erase\n");
			flash_region.addr = 0x20000;
			flash_sector_erase();
		} else if (cmd_compare(0, "flash") && cmd_words_b[1] > 0 && cmd_buffer[cmd_words_b[1]] == 'w') {
			print_string("\nFLASH write\n");
			for (uint8_t i = 0; i < 20; i++)
				flash_buf[i] = greeting[i];
			flash_region.addr = 0x200000;
			flash_region.len = 20;
			flash_write_bytes(flash_buf);
		} else if (cmd_compare(0, "port") && cmd_words_b[1] > 0) {
			parse_port();
		} else if (cmd_compare(0, "ip")) {
			print_string("Got ip command: ");
			if (!parse_ip(cmd_words_b[1]))
				uip_ipaddr(&uip_hostaddr, ip[0], ip[1], ip[2], ip[3]);
			else
				print_string("Invalid IP address\n");
			print_byte(ip[0]); print_byte(ip[1]); print_byte(ip[2]); print_byte(ip[3]);
			write_char('\n');
		} else if (cmd_compare(0, "gw")) {
			print_string("Got gw command: ");
			if (!parse_ip(cmd_words_b[1]))
				uip_ipaddr(&uip_draddr, ip[0], ip[1], ip[2], ip[3]);
			else
				print_string("Invalid IP address\n");
			print_byte(ip[0]); print_byte(ip[1]); print_byte(ip[2]); print_byte(ip[3]);
			write_char('\n');
		} else if (cmd_compare(0, "netmask")) {
			print_string("Got netmask command: ");
			if (!parse_ip(cmd_words_b[1]))
				uip_ipaddr(&uip_netmask, ip[0], ip[1], ip[2], ip[3]);
			else
				print_string("Invalid IP address\n");
			print_byte(ip[0]);print_byte(ip[1]);print_byte(ip[2]);print_byte(ip[3]);
			write_char('\n');
		} else if (cmd_compare(0, "l2")) {
			if (cmd_words_b[1] > 0 && cmd_compare(1, "forget"))
				port_l2_forget();
			else
				port_l2_learned();
		} else if (cmd_compare(0, "igmp")) {
			if (cmd_words_b[1] > 0 && cmd_compare(1, "on"))
				igmp_enable();
			else if (cmd_words_b[1] > 0 && cmd_compare(1, "show"))
				igmp_show();
			else
				igmp_setup();  // Reverts to default with IP-MC being flooded
		} else if (cmd_compare(0, "stp")) {
			if (cmd_words_b[1] > 0 && cmd_compare(1, "on")) {
				print_string("STP enabled\n");
				stpEnabled = 1;
				stp_setup();
			} else {
				print_string("STP disabled\n");
				stp_off();
				stpEnabled = 0;
			}
		} else if (cmd_compare(0, "pvid") && cmd_words_b[1] > 0 && cmd_words_b[2] > 0) {
			__xdata uint16_t pvid;
			uint8_t port;
			port = cmd_buffer[cmd_words_b[1]] - '1';
			port = machine.phys_to_log_port[port];
			if (!atoi_short(&pvid, cmd_words_b[2]))
				port_pvid_set(port, pvid);
		} else if (cmd_compare(0, "vlan")) {
			parse_vlan();
		} else if (cmd_compare(0, "mirror")) {
			parse_mirror();
		} else if (cmd_compare(0, "lag")) {
			parse_lag();
		} else if (cmd_compare(0, "laghash")) {
			parse_lag_hash();
		} else if (cmd_compare(0, "sds")) {
			print_reg(RTL837X_REG_SDS_MODES);
		} else if (cmd_compare(0, "gpio")) {
			print_gpio_status();
		} else if (cmd_compare(0, "regget")) {
			parse_regget();
		} else if (cmd_compare(0, "regset")) {
			parse_regset();
		} else if (cmd_compare(0, "rnd")) {
			parse_rnd();
		} else if (cmd_compare(0, "passwd")) {
			parse_passwd();
		} else if (cmd_compare(0, "eee")) {
			int8_t port = -1;
			if (cmd_words_b[3] > 0) {
				port = cmd_buffer[cmd_words_b[2]] - '1';
				port = machine.phys_to_log_port[port];
			}
			if (cmd_words_b[1] > 0 && cmd_compare(1, "on")) {
				if (port >= 0)
					port_eee_enable(port);
				else
					port_eee_enable_all();
			} else if (cmd_words_b[1] > 0 && cmd_compare(1, "off")) {
				if (port >= 0)
					port_eee_disable(port);
				else
					port_eee_disable_all();
			} else if (cmd_words_b[1] > 0 && cmd_compare(1, "status")) {
				if (port >= 0)
					port_eee_status(port);
				else
					port_eee_status_all();
			}
		} else if (cmd_compare(0, "version")) {
			print_sw_version();
		} else if (cmd_compare(0, "history")) {
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
		if (save_cmd) {
			uint8_t i;
			for (i = 0; i < N_WORDS; i++) {
				if (cmd_words_b[i] < 0)
					break;
			}
			if (i < N_WORDS) {
				i = cmd_words_b[--i];
				cmd_history_ptr = (cmd_history_ptr + i) & CMD_HISTORY_MASK;
				__xdata uint16_t p = cmd_history_ptr;
				cmd_history[cmd_history_ptr++] = '\n';
				do {
					i--;
					cmd_history[--p & CMD_HISTORY_MASK] = cmd_buffer[i];
				} while (i);
			}
		}
	}
}

#define FLASH_READ_BURST_SIZE 0x100
#define PASSWORD "1234"
void execute_config(void) __banked
{
	__xdata uint32_t pos = CONFIG_START;
	__xdata uint16_t len_left = CONFIG_LEN;

	// Set default password, it can be overwritten in the configuration file
	strtox(passwd, PASSWORD);
	save_cmd = 0;

	do {
		flash_region.addr = pos;
		flash_region.len = FLASH_READ_BURST_SIZE;
		flash_read_bulk(flash_buf);

		uint8_t cfg_idx = 0;
		uint8_t c = 0;
		do {
			for (uint8_t cmd_idx = 0; cmd_idx < (SBUF_SIZE - 1); cmd_idx++) {
				c = flash_buf[cfg_idx++];
				if (c == 0 || c == '\n') {
					cmd_buffer[cmd_idx] = '\0';
					if (cmd_idx && !cmd_tokenize())
						cmd_parser();
					if (c == 0)
						goto config_done;
					break;
				}

				cmd_buffer[cmd_idx] = c;
			}
		} while(cfg_idx);

		len_left -= FLASH_READ_BURST_SIZE;
		pos += FLASH_READ_BURST_SIZE;
	} while(len_left);

config_done:
	// Start saving commands to cmd_history
	save_cmd = 1;
	for (cmd_history_ptr = 0; cmd_history_ptr < CMD_HISTORY_SIZE; cmd_history_ptr++)
		cmd_history[cmd_history_ptr] = 0;
	cmd_history_ptr = 0;
}
