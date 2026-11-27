// #define DEBUG

#include "cmd_parse_util.h"
#include "cmd_parse_1.h"
#include "cmd_parse_stp.h"
#include "cmd_parse_health.h"
#include "rtl837x_flash.h"
#include "version.h"

#pragma codeseg BANK2
#pragma constseg BANK2

extern volatile __xdata uint32_t ticks;
extern __xdata uint8_t flash_buf[FLASH_BUF_SIZE];
extern __xdata struct flash_region_t flash_region;
extern __xdata char passwd[21];

// Buffer for writing to flash 0x1fd000, copy to 0x1fe000
__xdata uint8_t cmd_buffer[CMD_BUF_SIZE];

__xdata	char save_cmd;

// These variables combined create a Fixed-capacity vector/bounded buffer.
// `N_WORDS`: The total number of command arguments that can be tracked.
// `cmd_words_len` stores the number of arguments found inside `cmd_buffer`
// `cmd_words_b` stores the index into `cmd_buffer`, check `cmd_words_len` is index is valid.
__xdata uint8_t cmd_words_len;
__xdata uint8_t cmd_words_b[N_WORDS];

// Error set by commands
__xdata uint8_t err_status;

// Parse command into words
// cmd_words_len contains the number of words found.
// cmd_words_b[] contains only start of a word offset.
// Returns the parsing status via `err_status`-variable.
void cmd_tokenize(void) __banked
{
#ifdef DEBUG
	print_string("Tokenizing command\n");
	print_string_x(&cmd_buffer[0]);
	write_char('<'); write_char('\n');
#endif
	err_status = ERR_OK;
	uint8_t line_ptr = 0;
	uint8_t is_white = 1;
	uint8_t word = 0;
	uint8_t c = 0;

	while(1) {
		c = cmd_buffer[line_ptr];
		
		if (c == NUL) {
			// Store the word count
			cmd_words_len = word;
			break;
		}

		if (line_ptr == CMD_BUF_SIZE - 1) {
			err_status = ERR_CMD_TOO_LONG;
			return;
		}

		if (is_white && c != ' ') {
			is_white = 0;

			cmd_words_b[word++] = line_ptr;
			if (word >= N_WORDS) {
				cmd_words_len = 0;
				print_string("\nSyntax error: too many arguments.");
				err_status = ERR_TOO_MANY_ARGUMENTS;
				return;
			}
		} else if (c == ' ') {
			is_white = 1;
		}

		line_ptr++;
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
	print_string("CMD-words: ");
	print_byte(cmd_words_len); write_char(' ');
	print_byte(cmd_words_b[0]); write_char(' ');
	print_byte(cmd_words_b[1]); write_char(' ');
	print_byte(cmd_words_b[2]); write_char(' ');
	print_byte(cmd_words_b[3]); write_char(' ');
	print_byte(cmd_words_b[4]); write_char(' ');
	print_byte(cmd_words_b[5]); write_char(' ');
	print_byte(cmd_words_b[6]); write_char('\n');
#endif

	if (cmd_words_len < 1)
		return;

	if (cmd_compare(0, "debug")) {
		cmd_parse_debug();
	} else if (cmd_compare(0, "feature")) {
		cmd_parse_feature();
	} else if (cmd_compare(0, "lag")) {
		cmd_parse_lag();
	} else if (cmd_compare(0, "help")) {
		cmd_parse_help();
	} else if (cmd_compare(0, "port")) {
		cmd_parse_port();
	} else if (cmd_compare(0, "remove")) {
		cmd_parse_remove();
	} else if (cmd_compare(0, "show")) {
		cmd_parse_show();
	} else if (cmd_compare(0, "stp")) {
		cmd_parse_stp();
	} else if (cmd_compare(0, "system")) {
		cmd_parse_system();
	} else if (cmd_compare(0, "vlan")) {
		cmd_parse_vlan();
	}

// OLD HANDLERS HERE:
	if (cmd_words_len >= 1) {
		if (cmd_compare(0, "reset")) {
			cmd_parse_reset();
		} else if (cmd_compare(0, "factory-reset")) {
			cmd_parse_factory_reset();
		} else if (cmd_compare(0, "sfp")) {
			cmd_parse_sfp();
		} else if (cmd_compare(0, "stat")) {
			cmd_parse_stat();
		} else if (cmd_compare(0, "flash")) {
			cmd_parse_flash();
		} else if (cmd_compare(0, "port")) {
			cmd_parse_port__();
		} else if (cmd_compare(0, "mtu")) {
			cmd_parse_mtu();
		} else if (cmd_compare(0, "syslog")) {
			cmd_parse_syslog();
		} else if (cmd_compare(0, "dns")) {
			cmd_parse_dns();
		} else if (cmd_compare(0, "ntp") || cmd_compare(0, "time")) {
			cmd_parse_ntp();
		} else if (cmd_compare(0, "ip")) {
			cmd_parse_ip();
		} else if (cmd_compare(0, "gw")) {
			cmd_parse_gw();
		} else if (cmd_compare(0, "netmask")) {
			cmd_parse_netmask();
		} else if (cmd_compare(0, "l2")) {
			cmd_parse_l2();
		} else if (cmd_compare(0, "igmp")) {
			cmd_parse_igmp();
		} else if (cmd_compare(0, "mac")) {
			cmd_parse_mac();
		} else if (cmd_compare(0, "hostname")) {
			cmd_parse_hostname();
		} else if (cmd_compare(0, "stp")) {
			cmd_parse_stp__();
#ifdef HEALTH
		} else if (cmd_compare(0, "health")) {
			cmd_parse_health();
#endif
		} else if (cmd_compare(0, "lacp")) {
			cmd_parse_lacp();
		} else if (cmd_compare(0, "pvid")) {
			cmd_parse_pvid();
		} else if (cmd_compare(0, "vlan")) {
			cmd_parse_vlan__();
		} else if (cmd_compare(0, "isolate")) {
			cmd_parse_isolate();
		} else if (cmd_compare(0, "mirror")) {
			cmd_parse_mirror();
		} else if (cmd_compare(0, "lag")) {
			cmd_parse_lag__();
		} else if (cmd_compare(0, "laghash")) {
			cmd_parse_laghash();
		} else if (cmd_compare(0, "sds")) {
			cmd_parse_sds();
		} else if (cmd_compare(0, "gpio")) {
			cmd_parse_gpio();
		} else if (cmd_compare(0, "leds")) {
			cmd_parse_leds();
		} else if (cmd_compare(0, "regget")) {
			cmd_parse_regget();
		} else if (cmd_compare(0, "regset")) {
			cmd_parse_regset();
		} else if (cmd_compare(0, "sdsget")) {
			cmd_parse_sdsget();
		} else if (cmd_compare(0, "sdsset")) {
			cmd_parse_sdsset();
		} else if (cmd_compare(0, "phyget")) {
			cmd_parse_phyget();
		} else if (cmd_compare(0, "physet")) {
			cmd_parse_physet();
		} else if (cmd_compare(0, "rnd")) {
			cmd_parse_rnd();
		} else if (cmd_compare(0, "passwd")) {
			cmd_parse_passwd();
		} else if (cmd_compare(0, "eee")) {
			cmd_parse_eee();
		} else if (cmd_compare(0, "bw")) {
			cmd_parse_bw();
		} else if (cmd_compare(0, "temp")) {
			cmd_parse_temp();
		} else if (cmd_compare(0, "storm")) {
			cmd_parse_storm();
		} else if (cmd_compare(0, "version")) {
			cmd_parse_version();
		} else if (cmd_compare(0, "history")) {
			cmd_parse_history();
		} else if (cmd_compare(0, "ingress")) {
			cmd_parse_ingress();
		} else if (cmd_compare(0, "session")) {
			cmd_parse_session();
		} else if (cmd_compare(0, "lldp")) {
			cmd_parse_lldp();
		} else {
			cmd_error("Unknown command\n");
		}

		if (save_cmd && cmd_words_len && err_status == ERR_OK)
			cmd_history_save();
	}
}

#define FLASH_READ_BURST_SIZE 0x100
#define PASSWORD "1234"

#if CONFIG_LEN % FLASH_READ_BURST_SIZE
	#error "CONFIG_LEN not a multiple of FLASH_READ_BURST_SIZE"
#endif
void execute_config(void) __banked
{
	__xdata uint32_t pos = CONFIG_START;
	__xdata uint8_t pages_left = CONFIG_LEN / FLASH_READ_BURST_SIZE;

	// Set default password, it can be overwritten in the configuration file
	strtox(passwd, PASSWORD);
	save_cmd = 0;

	uint8_t cmd_idx = 0;
	uint8_t skipping = 0;
	do {
		flash_region.addr = pos;
		flash_region.len = FLASH_READ_BURST_SIZE;
		flash_read_bulk(flash_buf);

		__xdata uint8_t cfg_idx = 0;
		uint8_t c = 0;
		do {
			if (cmd_idx >= (CMD_BUF_SIZE - 1)) {
				cmd_buffer[cmd_idx] = NUL;
				print_string("ERROR: Command too long, skipped: ");
				print_string_x(cmd_buffer);
				write_char('\n');
				cmd_idx = 0;
				skipping = 1;
			}
			c = flash_buf[cfg_idx++];
			if (c == 0 || c == '\n' || c == '\r') {
				if (skipping) {
					skipping = 0;
				} else {
					cmd_buffer[cmd_idx] = NUL;
					if (cmd_idx) {
						cmd_tokenize();
						if (err_status == ERR_OK)
							cmd_parser();
					}
				}
				if (c == 0)
					goto config_done;
				cmd_idx = 0;
				continue;
			}

			if (!skipping) {
				cmd_buffer[cmd_idx] = c;
				cmd_idx++;
			}
		} while (cfg_idx);

		pages_left--;
		pos += FLASH_READ_BURST_SIZE;
	} while(pages_left);

config_done:
	// Start saving commands to cmd_history
	clear_command_history();
	save_cmd = 1;
}


// Execute multiple commands
// If a command is too long or can't be tokenized, remaining commands are not executed
// Returns the status via `err_status`-variable.
void execute_commands(__xdata uint8_t *p) __banked {
	__xdata uint8_t failed = ERR_OK;
	err_status = ERR_OK;
	uint8_t cmd_idx = 0;
	while (1) {
		if (*p == 0 || *p == '\n' || *p == '\r') {
			if (cmd_idx) {
				cmd_buffer[cmd_idx] = NUL;
				cmd_tokenize();
				if (err_status != ERR_OK)
					return;
				cmd_parser();
				if (failed == ERR_OK)
					failed = err_status;
			}
			if (*p == 0) {
				err_status = failed;
				return;
			}
			cmd_idx = 0;
		} else {
			if (cmd_idx < (CMD_BUF_SIZE - 1)) {
				cmd_buffer[cmd_idx++] = *p;
			} else {
				cmd_buffer[CMD_BUF_SIZE - 1] = NUL;
				print_string("ERROR: Command too long: ");
				print_string_x(cmd_buffer);
				write_char('\n');
				err_status = ERR_CMD_TOO_LONG;
				return;
			}
		}
		p++;
	};
}
