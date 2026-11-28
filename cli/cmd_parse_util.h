#ifndef _CMD_PARSE_UTIL_H_
#define _CMD_PARSE_UTIL_H_

/*
 * Internals shared by the command parser (cmd_parser.c) and the per-verb
 * parsers (cmd_parse_<verb>.c). Nothing outside cli/ needs this header;
 * cmd_parser.h is the public interface.
 */

#include <stdint.h>

#include "rtl837x_common.h"
#include "cmd_parser.h"

/* The command line: cmd_buffer holds the NUL-terminated text, cmd_words_b[n]
 * is the index in cmd_buffer where word n starts and cmd_words_len is the
 * number of words (valid indices are 0 .. cmd_words_len - 1). A fixed-capacity
 * vector of at most N_WORDS words. Filled in by cmd_tokenize(). */
#define N_WORDS 15
extern __xdata uint8_t cmd_words_len;
extern __xdata uint8_t cmd_words_b[N_WORDS];

/* Cleared by execute_config() while the startup configuration is applied, so
 * those commands do not end up in the command history. */
extern __xdata char save_cmd;

/* Results of the argument parsers below */
extern __xdata uint8_t hexvalue[4];
extern __xdata uint8_t ip[4];
extern __xdata uint8_t mac_parse_result[6];
extern __xdata uint16_t atoi_results_short;
extern __xdata uint8_t atoi_results_u8;

/* Does word `start` equal the NUL-terminated string `cmd`? */
uint8_t cmd_compare(uint8_t start, __code const uint8_t * cmd);

uint8_t atoi_hex(uint8_t idx);
uint8_t atoi_byte(uint8_t idx);
uint8_t atoi_short(uint8_t idx);

/* Port arguments: the front panel number is translated to the logical port,
 * which is returned in atoi_results_u8. */
uint8_t cmd_arg_port(uint8_t idx);
uint8_t cmd_arg_port_separator(uint8_t idx);
uint8_t cmd_arg_port_cpu_separator(uint8_t idx);

__bit cmd_is_space(uint8_t idx);
__bit cmd_is_space_or_nul(uint8_t idx);

uint8_t parse_ip(uint8_t idx);
uint8_t parse_mac(uint8_t idx);

/* Reports a usage or argument error and sets err_status */
void cmd_error(__code const char *msg);

/* Maps characters that are not safe in a name (control characters, quotes, backslash) to '.' */
uint8_t cmd_name_char(uint8_t c);

#endif
