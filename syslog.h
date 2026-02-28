#ifndef _SYSLOG_H_
#define _SYSLOG_H_

#include <stdint.h>

#define LOGBUF_SIZE 512

extern __xdata char logbuf[LOGBUF_SIZE];

void syslog_init(void) __banked;
void handle_syslog(void) __banked;

#endif
