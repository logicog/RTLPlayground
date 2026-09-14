#ifndef _SFLOW_H_
#define _SFLOW_H_

#include <stdint.h>

#define SFLOW_PORT_DEFAULT	6343
#define SFLOW_INTERVAL_DEFAULT	20

struct uip_udp_conn;

struct sflow_state {
	uint8_t enabled;
	uint8_t collector[4];
	uint16_t port;
	uint16_t interval;	/* seconds between two samples of one port */
	uint8_t next;		/* logical port sampled next */
	uint32_t last;		/* ticks when the last sample went out */
	uint32_t seq;		/* datagrams sent */
	struct uip_udp_conn *conn;
};

extern __xdata struct sflow_state sflow_state;

void sflow_init(void) __banked;
void sflow_start(void) __banked;
void sflow_stop(void) __banked;
void sflow_interval(uint16_t seconds) __banked __reentrant;
void sflow_callback(uint16_t lport) __banked __reentrant;
void sflow_parse(void) __banked __reentrant;

#endif
