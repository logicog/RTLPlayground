#ifndef _SYSLOG_H_
#define _SYSLOG_H_

#include <stdint.h>

#define LOGBUF_SIZE 512

void syslog_init(void) __banked;
void syslog_write_char(void) __banked;
void handle_syslog(void) __banked;

#endif
