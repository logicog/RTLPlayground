#ifndef _CMD_PARSER_H_
#define _CMD_PARSER_H_

#include <stdint.h>

#include "rtl837x_common.h"

extern __xdata uint8_t cmd_buffer[CMD_BUF_SIZE];
extern __xdata uint8_t cmd_available;
extern __xdata uint8_t err_status;

/* Word matching and number parsing over cmd_buffer, for command handlers
 * outside cmd_parser.c; plain calls, so the caller has to be in BANK2. */
uint8_t cmd_compare(uint8_t start, __code uint8_t * cmd);
uint8_t atoi_short(uint8_t idx);
uint8_t parse_ip(uint8_t idx);

void cmd_tokenize(void) __banked;
void cmd_parser(void) __banked;
void execute_config(void) __banked;
void execute_commands(__xdata uint8_t *p) __banked;
void print_ip(__xdata uint8_t *ptr) __banked;
void print_mac(__xdata uint8_t *ptr) __banked;
void print_sw_version(void) __banked;
void clear_command_history(void) __banked;

#endif
