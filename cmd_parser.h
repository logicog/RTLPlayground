#ifndef _CMD_PARSER_H_
#define _CMD_PARSER_H_

#include <stdint.h>

#include "rtl837x_common.h"

extern __xdata uint8_t cmd_buffer[SBUF_SIZE];

uint8_t cmd_tokenize(void) __banked;
void cmd_parser(void) __banked;
void execute_config(void) __banked;
#endif
