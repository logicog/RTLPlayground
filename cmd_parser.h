#ifndef _CMD_PARSER_H_
#define _CMD_PARSER_H_

#include <stdint.h>

#include "rtl837x_common.h"

extern __xdata uint8_t cmd_buffer[CMD_BUF_SIZE];
extern __bit __at(0x03) cmd_available;
extern __xdata uint8_t err_status;

/* Health instrumentation, built with HEALTH=1. Without it the hooks below
 * compile to nothing and the "health" command does not exist. */
#ifdef HEALTH
/* Phase ids, in main-loop order */
#define HEALTH_PH_LINK	0
#define HEALTH_PH_SFP	1
#define HEALTH_PH_RX	2
#define HEALTH_PH_TX	3
#define HEALTH_PH_STP	4
#define HEALTH_PH_CMD	5
#define HEALTH_PHASES	6
#define HEALTH_LLDP     7

extern __xdata uint16_t health_rx_frames;
#define health_rx_frame()	health_rx_frames++

void health_loop_start(void) __banked;	/* once per idle() pass, resets the phase baseline */
void health_phase(uint8_t id) __banked;	/* after each phase; accounts ticks since last checkpoint */
void health_stack_paint(void) __banked;	/* paint the unused stack area once at boot */
void health_show(void) __banked;	/* "health" CLI dump */
#else
#define health_rx_frame()
#define health_loop_start()
#define health_phase(id)
#define health_stack_paint()
#endif

void cmd_tokenize(void) __banked;
void cmd_parser(void) __banked;
void execute_config(void) __banked;
void execute_commands(__xdata uint8_t *p) __banked;
void print_ip(__xdata uint8_t *ptr) __banked;
void print_mac(__xdata uint8_t *ptr) __banked;
void print_sw_version(void) __banked;
void clear_command_history(void) __banked;

#endif
