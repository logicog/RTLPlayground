/*
 * Shared helpers of the command parser: word comparison, number, port, IP and
 * MAC argument parsing, and the IP/MAC printers.
 */

#include "cmd_parse_util.h"
#include "rtl837x_common.h"
#include "rtl837x_port.h"
#include "machine.h"

#pragma codeseg BANK2
#pragma constseg BANK2

extern __code const struct machine machine;

// Temporary for str to hex convertion value.
// Support up to 32_bits.
__xdata uint8_t hexvalue[4] = { 0 };

__xdata uint8_t ip[4];
__xdata uint8_t mac_parse_result[6];

__xdata uint16_t atoi_results_short;
__xdata uint8_t atoi_results_u8;

uint8_t cmd_compare(uint8_t start, __code const uint8_t * cmd)
{
	if (cmd_words_len == 0 || start > (cmd_words_len - 1)) {
		return 0;
	}
	uint8_t i = cmd_words_b[start];
	uint8_t j = 0;

	do {
		uint8_t c = cmd[j];
		uint8_t b = cmd_buffer[i];

		// cmd is guaranteed to be NUL-terminated.
        if (c == NUL) {
            if ((b == ' ') || (b == NUL)) {
				// Match
                return 1;
            }
            break;
        }
        if (b != c) {
            break;
        }

		j += 1;
        i += 1;
	} while (i < CMD_BUF_SIZE);

	// No match
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

		if (c == NUL || c == ' ') {
			break;
		}

		if (h_idx == 8) {
			h_idx = 0;
			break;
		}

		// swap hex nibbles
		val = (val >> 4) | (val << 4);

		if ((uint8_t)(c - '0') < 10) {
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

	if (h_idx & 1) {
		hexvalue[h_idx >> 1] <<= 4;
		hexvalue[3] = hexvalue[3] >> 4 | (hexvalue[2] << 4);
		hexvalue[2] = hexvalue[2] >> 4 | (hexvalue[1] << 4);
		hexvalue[1] = hexvalue[1] >> 4 | (hexvalue[0] << 4);
		hexvalue[0] >>= 4;
	}

	return ((h_idx + 1) >> 1);
}


// Returns 0 or the number of digits taken into account for conversion.
// Stops at any non-digit '0'-'9' char or more than 3 bytes.
uint8_t atoi_byte(uint8_t idx)
{
	uint8_t cnt = 0;
	uint8_t num = 0;

	uint8_t * ptr = &cmd_buffer[idx];

	while (1) {
		uint8_t val = *ptr++ - '0';
		if (val > 9)
			break;
		if (num > 25 || (num == 25 && val > 5) || cnt >= 3)
			return 0;
		num = (num * 10) + val;
		cnt++;
	}

	atoi_results_u8 = num;
	return cnt;
}


// Returns 0 or the number of digits taken into account for conversion.
// Stops at any non-digit '0'-'9' char or bytes is more then 5.
uint8_t atoi_short(uint8_t idx)
{
	uint8_t cnt = 0;
	atoi_results_short = 0;

	uint8_t *ptr = &cmd_buffer[idx];

	while (1) {
		uint8_t val = *ptr++ - '0';
		if (val > 9)
			break;
		if (atoi_results_short > 6553 || (atoi_results_short == 6553 && val > 5) || cnt >= 5)
			return 0;
		atoi_results_short = (atoi_results_short * 10) + val;
		cnt++;
	}

	return cnt;
}

/* Parse, validate and translate phys_to_log_port physical port argument.
 * The CPU-port, i.e. port 0, is not a valid argument.
 * returns 0 when on parser error or invalid value.
 * returns non-zero number of characters consumed.
 * Store the value in atoi_results_u8.
 */
uint8_t cmd_arg_port(uint8_t idx) {
	uint8_t port = cmd_buffer[idx] - '0' - 1;
	if (port > 8)
		return 0;

	port = machine.phys_to_log_port[port];
	if (!MACHINE_PORT_VALID(port))
		return 0;

	atoi_results_u8 = port;
	return 1;
}


// Same as cmd_arg_port() but additionally check for trailing SPACE or NUL.
// returns 0 when on parser error or invalid value or no SPACE or no NUL.
// returns non-zero number of characters consumed including the SPACE.
uint8_t cmd_arg_port_separator(uint8_t idx) {
	uint8_t ret = cmd_arg_port(idx);
	if (ret != 0) {
		idx += ret;
		uint8_t c = cmd_buffer[idx];
		if (c == ' ') {
			ret++;
		} else if (c != NUL)
			ret = 0;
	}
	return ret;
}

// Same as cmd_arg_port_separator() but additionally allow CPU-port.
// returns 0 when on parser error or invalid value or no SPACE or no NUL.
// returns number of characters consumed including the SPACE.
uint8_t cmd_arg_port_cpu_separator(uint8_t idx) {
	uint8_t ret = cmd_arg_port(idx);

	if (ret == 0 && cmd_buffer[idx] == '0') {
		ret = 1;
		atoi_results_u8 = CPU_LOGICAL_PORT;
	}

	if (ret != 0) {
		idx += ret;
		uint8_t c = cmd_buffer[idx];
		if (c == ' ') {
			ret++;
		} else if (c != NUL)
			ret = 0;
	}
	return ret;
}


// check if the cmd_buffer[idx] is a SPACE.
__bit cmd_is_space(uint8_t idx) {
	return cmd_buffer[idx] == ' ';
}

// check if the cmd_buffer[idx] is a SPACE or NUL.
__bit cmd_is_space_or_nul(uint8_t idx) {
	uint8_t c = cmd_buffer[idx];
	return c == ' ' || c == NUL;
}

// Parse an IPv4 address
// returns 0 when on parse error or invalid value or it don't ends with SPACE or NUL.
// returns non-zero number of characters consumed including the SPACE.
uint8_t parse_ip(uint8_t idx)
{
	uint8_t b = 0;
	uint8_t ret;
	uint8_t idx_start = idx;

	while(1) {
		ret = atoi_byte(idx);
		if (ret == 0)
			goto err;

		idx += ret;
		ip[b++] = atoi_results_u8;

		ret = cmd_buffer[idx];
		if (b == 4) {
			if (ret == ' ') {
				idx++;
				break;
			}
			if (ret == NUL)
				break;
			goto err;
		}
		idx++;

		if (ret != '.')
			goto err;
	}
	return idx - idx_start;

err:
	print_string("Error in IP format\n");
	return 0;
}

// Prints an IPv4 address.
void print_ip(__xdata uint8_t * ptr) __banked
{
	uint8_t idx = 0;
	uint8_t num;

	while(1) {
		num = *ptr++;
		itoa(num);
		if (++idx == 4)
			break;

		write_char('.');
	}
}

// Prints a MAC address.
void print_mac(__xdata uint8_t *ptr) __banked
{
	uint8_t idx = 0;
	uint8_t num;

	while (1) {
		num = *ptr++;
		print_byte(num);
		if (++idx == 6)
			break;

		write_char(':');
	}
}

// Parses a MAC address "aa:bb:cc:dd:ee:ff" (":" or "-" separators) from
// cmd_buffer starting at idx into the global mac_parse_result[6]. Each octet
// must be exactly two hex digits with a separator between octets and nothing
// trailing, so repeated separators, missing digits or a trailing hex digit are
// rejected. Returns the number of bytes consumed, or 0 on error.
uint8_t parse_mac(uint8_t idx)
{
	uint8_t b = 0;
	uint8_t c = ':';

	do {
		if (c != ':' && c != '-')
			goto err;

		__bit again = true;
		uint8_t v = 0;

		while(1) {
			c = cmd_buffer[idx++];
			c -= '0';
			if (c > 9) {
				c |= 0x20;
				c += '0' - 'a';
				if (c > 5)
					goto err;
				c += 10;
			}
			// force swap-instr.
			v = (v << 4) | (v >> 4);
			v |= c;

			if (again)
				again = false;
			else
				break;
		}
		mac_parse_result[b++] = v;

		c = cmd_buffer[idx++];
	} while(b < 6);

	if (c == ' ' || c == NUL)
		return idx;

err:
	return 0;
}

void cmd_error(__code const char *msg)
{
	err_status = ERR_INVALID_ARGUMENT;
	print_string("Error: ");
	print_string(msg);
}


uint8_t cmd_name_char(uint8_t c)
{
	if (c < 0x20 || c > 0x7e || c == '"' || c == '\\')
		return '.';
	return c;
}
