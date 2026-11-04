#ifndef _SFLOW_H_
#define _SFLOW_H_

#include <stdint.h>
#include <stdbool.h>

#define SFLOW_PORT_DEFAULT	6343
#define SFLOW_INTERVAL_DEFAULT	20
#define SFLOW_HDR_MAX		128
#define SFLOW_RATE_MIN		100
#define SFLOW_PMASK		(((uint16_t)2 << MACHINE_MAX_PORT) - ((uint16_t)1 << MACHINE_MIN_PORT))

struct uip_udp_conn;

struct sflow_state {
	bool enabled;
	uint8_t collector[4];
	uint16_t port;
	uint16_t interval;	/* seconds between two samples of one port */
	uint16_t rate;		/* one in rate frames received is sampled, 0 for none */
	uint8_t next;		/* logical port sampled next */
	uint32_t last;		/* ticks when the last sample went out */
	uint32_t seq;		/* datagrams sent */
	uint32_t drops;		/* sampled frames lost while one was still waiting */
	struct uip_udp_conn *conn;
};

extern __xdata struct sflow_state sflow_state;

void sflow_init(void) __banked;
void sflow_start(void) __banked;
void sflow_stop(void) __banked;
void sflow_interval(uint16_t seconds) __banked __reentrant;
bool sflow_rate(uint16_t rate) __banked __reentrant;
void sflow_rx(void) __banked __reentrant;
void sflow_callback(uint16_t lport) __banked __reentrant;
void sflow_parse(void) __banked __reentrant;

#endif
