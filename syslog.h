#ifndef _SYSLOG_H_
#define _SYSLOG_H_

#include <stdint.h>

#define LOGBUF_SIZE 512

void syslog_init(void) __banked;
void syslog_start(void) __banked;
void syslog_stop(void) __banked;
void syslog_callback(void) __banked;

#endif
