#ifndef _CMD_PARSER_H_
#define _CMD_PARSER_H_

#include <stdint.h>

#include "rtl837x_common.h"
#include "cmd_parse_health.h"
#include "cmd_parse_1.h"

extern __xdata uint8_t cmd_buffer[CMD_BUF_SIZE];
extern __bit cmd_available;
extern __xdata uint8_t err_status;

void cmd_tokenize(void) __banked;
void cmd_parser(void) __banked;
void execute_config(void) __banked;
void execute_commands(__xdata uint8_t *p) __banked;
void print_ip(__xdata uint8_t *ptr) __banked;
void print_mac(__xdata uint8_t *ptr) __banked;

#endif
