/*
 * sFlow version 5 counter samples for the RTL837x platform
 * This code is in the Public Domain
 */

#include <stdint.h>
#include "rtl837x_common.h"
#include "rtl837x_sfr.h"
#include "rtl837x_regs.h"
#include "rtl837x_port.h"
#include "uip/uip.h"
#include "machine.h"
#include "sflow.h"

#pragma codeseg BANK3
#pragma constseg BANK3

extern __code struct machine machine;
extern __xdata uint8_t sfr_data[4];
extern volatile __xdata uint32_t ticks;

#define CNT_LOW		0	/* 32-bit counter in STAT_V_LOW */
#define CNT_HIGH	1	/* 32-bit counter in STAT_V_HIGH */
#define CNT_WIDE	2	/* 64-bit counter, STAT_V_HIGH then STAT_V_LOW */
#define CNT_NONE	3	/* not counted by the ASIC, sent as 0xffffffff */

__xdata struct sflow_state sflow_state;
__xdata uip_ipaddr_t sflow_ip;
__xdata uint32_t sflow_gap;
__xdata uint32_t sflow_sample_seq[10];
__xdata uint8_t * __xdata sflow_p;	/* next byte of the datagram being built */
__xdata uint8_t  sflow_port;

/* MIB counter index and part for each counter field of the sFlow records */
static __code const uint8_t sflow_if_in[] = {
	0, CNT_WIDE, 2, CNT_LOW, 3, CNT_LOW, 4, CNT_LOW, 8, CNT_LOW, 48, CNT_HIGH, 0, CNT_NONE
};
static __code const uint8_t sflow_if_out[] = {
	1, CNT_WIDE, 5, CNT_LOW, 6, CNT_LOW, 7, CNT_LOW, 8, CNT_HIGH, 48, CNT_LOW
};
static __code const uint8_t sflow_ether[] = {
	0, CNT_NONE, 15, CNT_LOW, 9, CNT_HIGH, 9, CNT_LOW, 0, CNT_NONE, 10, CNT_HIGH, 10, CNT_LOW,
	11, CNT_HIGH, 0, CNT_NONE, 0, CNT_NONE, 29, CNT_HIGH, 0, CNT_NONE, 11, CNT_LOW
};

/* ifSpeed in bit/s, high and low word, indexed by the speed nibble of the port */
static __code const uint32_t sflow_speed[16] = {
	0, 10000000UL, 0, 100000000UL, 0, 1000000000UL, 0, 0,
	2, 0x540be400UL, 0, 2500000000UL, 1, 0x2a05f200UL, 0, 0
};


static void sflow_put32(uint32_t v) __reentrant
{
	*sflow_p++ = v >> 24;
	*sflow_p++ = v >> 16;
	*sflow_p++ = v >> 8;
	*sflow_p++ = v;
}


/* A small constant: three zero bytes and the value. */
static void sflow_put8(uint8_t v) __reentrant
{
	*sflow_p++ = 0;
	*sflow_p++ = 0;
	*sflow_p++ = 0;
	*sflow_p++ = v;
}


static void sflow_put_reg(uint16_t reg) __reentrant
{
	reg_read_m(reg);
	memcpy(sflow_p, sfr_data, 4);
	sflow_p += 4;
}


static void sflow_put_table(__code const uint8_t *t, uint8_t n) __reentrant
{
	while (n--) {
		if (t[1] == CNT_NONE) {
			sflow_put32(0xffffffffUL);
		} else {
			STAT_GET(t[0], sflow_port);
			if (t[1] != CNT_LOW)
				sflow_put_reg(RTL837X_STAT_V_HIGH);
			if (t[1] != CNT_HIGH)
				sflow_put_reg(RTL837X_STAT_V_LOW);
		}
		t += 2;
	}
}


static void sflow_sample(void) __reentrant
{
	uint8_t speed, up;

	reg_read_m(RTL837X_REG_LINKS_STS);
	up = ((sfr_data[1] | ((uint16_t)sfr_data[2] << 8)) >> sflow_port) & 1;
	reg_read_m(sflow_port >= 8 ? RTL837X_REG_LINKS_89 : RTL837X_REG_LINKS);
	speed = sfr_data[3 - ((sflow_port & 7) >> 1)];
	speed = (sflow_port & 1) ? speed >> 4 : speed & 0xf;
	speed = up ? (speed & 7) << 1 : 6;

	sflow_p = uip_appdata;
	sflow_put8(5);
	sflow_put8(1);
	memcpy(sflow_p, uip_hostaddr, 4);
	sflow_p += 4;
	sflow_put8(0);
	sflow_put32(++sflow_state.seq);
	sflow_put32(ticks * (1000 / SYS_TICK_HZ));
	sflow_put8(1);

	sflow_put8(2);
	sflow_put8(168);
	sflow_put32(++sflow_sample_seq[sflow_port]);
	sflow_put8(machine.log_to_phys_port[sflow_port]);
	sflow_put8(2);

	sflow_put8(1);
	sflow_put8(88);
	sflow_put8(machine.log_to_phys_port[sflow_port]);
	sflow_put8(6);
	sflow_put32(sflow_speed[speed]);
	sflow_put32(sflow_speed[speed + 1]);
	sflow_put8(0);
	sflow_put8(up ? 3 : 1);
	sflow_put_table(sflow_if_in, sizeof(sflow_if_in) / 2);
	sflow_put_table(sflow_if_out, sizeof(sflow_if_out) / 2);
	sflow_put8(1);

	sflow_put8(2);
	sflow_put8(52);
	sflow_put_table(sflow_ether, sizeof(sflow_ether) / 2);

	uip_udp_send(sflow_p - (__xdata uint8_t *)uip_appdata);
}


void sflow_interval(uint16_t seconds) __banked __reentrant
{
	uint8_t n = machine.max_port - machine.min_port + 1;

	sflow_state.interval = seconds;
	sflow_gap = (uint32_t)(seconds / n) * SYS_TICK_HZ + (seconds % n) * SYS_TICK_HZ / n;
}


void sflow_init(void) __banked
{
	memset((__xdata uint8_t *)&sflow_state, 0, sizeof(sflow_state));
	sflow_state.port = SFLOW_PORT_DEFAULT;
	sflow_interval(SFLOW_INTERVAL_DEFAULT);
}


void sflow_start(void) __banked
{
	sflow_state.enabled = 1;
	if (sflow_state.conn)
		return;
	if (!(sflow_state.collector[0] | sflow_state.collector[1] | sflow_state.collector[2] | sflow_state.collector[3]))
		return;
	uip_ipaddr(sflow_ip, sflow_state.collector[0], sflow_state.collector[1],
		   sflow_state.collector[2], sflow_state.collector[3]);
	sflow_state.conn = uip_udp_new(&sflow_ip, HTONS(sflow_state.port));
	if (!sflow_state.conn) {
		print_string("sFlow: no free UDP connection\n");
		return;
	}
	sflow_state.next = machine.min_port;
	sflow_state.last = ticks;
}


void sflow_stop(void) __banked
{
	if (sflow_state.conn) {
		uip_udp_remove(sflow_state.conn);
		sflow_state.conn = 0;
	}
}


void sflow_callback(uint16_t lport) __banked __reentrant
{
	if (!sflow_state.conn || lport != sflow_state.conn->lport)
		return;
	if (ticks - sflow_state.last < sflow_gap)
		return;
	sflow_state.last = ticks;
	sflow_port = sflow_state.next;
	if (++sflow_state.next > machine.max_port)
		sflow_state.next = machine.min_port;
	sflow_sample();
}
