#ifndef _SYSLOG_H_
#define _SYSLOG_H_

#include <stdint.h>

#define LOGBUF_SIZE 512

struct syslog_state {
    uint8_t enabled;
    uint8_t line_available;
    uint16_t writeptr ;
    uint16_t readptr;
    uint8_t server_ip[4];

    struct uip_udp_conn *syslog_conn;
};

extern __xdata struct syslog_state syslog_state;
extern __xdata char logbuf[LOGBUF_SIZE];

void syslog_init(void) __banked;
void syslog_start(void) __banked;
void syslog_stop(void) __banked;
void syslog_callback(uint16_t lport) __banked;

#endif
