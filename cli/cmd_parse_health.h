#ifndef _CMD_PARSE_HEALTH_H_
#define _CMD_PARSE_HEALTH_H_

#include <stdint.h>

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
#define HEALTH_PH_LLDP	6
#define HEALTH_PHASES	(HEALTH_PH_LLDP + 1)	/* last id + 1: sizes the arrays in cmd_parse_health.c */

extern __xdata uint16_t health_rx_frames;
#define health_rx_frame()	health_rx_frames++

void health_loop_start(void) __banked;	/* once per idle() pass, resets the phase baseline */
void health_sleep_start(void) __banked;
void health_sleep_end(void) __banked;
void health_phase(uint8_t id) __banked;	/* after each phase; accounts ticks since last checkpoint */
void health_stack_paint(void) __banked;	/* paint the unused stack area once at boot */
void health_show(void) __banked;	/* "health" CLI dump */
void cmd_parse_health(void);	/* the "health" command */
#else
#define health_rx_frame()
#define health_loop_start()
#define health_sleep_start()
#define health_sleep_end()
#define health_phase(id)
#define health_stack_paint()
#endif

#endif
