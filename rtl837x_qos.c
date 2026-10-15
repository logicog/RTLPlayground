#include <stdint.h>
#include "rtl837x_common.h"
#include "rtl837x_sfr.h"
#include "rtl837x_regs.h"
#include "rtl837x_bandwidth.h"
#include "rtl837x_qos.h"
#include "machine.h"

#pragma codeseg BANK3
#pragma constseg BANK3

extern __code const struct machine machine;
extern __xdata uint8_t sfr_data[4];

static void dscp_field_set(uint8_t shift, uint8_t priority)
{
	uint8_t n = shift >> 3;
	uint8_t b = shift & 7;

	sfr_mask_data(n, PRI_REMAP_MASK << b, priority << b);
	if (b > 5)
		sfr_mask_data(n + 1, PRI_REMAP_MASK >> (8 - b), priority >> (8 - b));
}


static uint8_t dscp_field_get(uint8_t shift)
{
	uint8_t n = shift >> 3;
	uint8_t b = shift & 7;
	uint8_t v = sfr_data[3 - n] >> b;

	if (b > 5)
		v |= sfr_data[2 - n] << (8 - b);

	return v & PRI_REMAP_MASK;
}


void qos_defaults(void) __banked
{
	uint8_t dscp = 0;
	uint8_t word = 0;
	uint8_t i;

	while (dscp < QOS_DSCP_VALUES) {
		sfr_set_zero();
		for (i = 0; i < QOS_DSCP_PER_WORD && dscp < QOS_DSCP_VALUES; i++, dscp++)
			dscp_field_set(i * 3, dscp >> 3);
		reg_write_m(RTL837X_DSCP_PRI_REMAP + (word << 2));
		word++;
	}
}


void qos_dot1q_set(uint8_t on) __banked
{
	reg_read_m(RTL837X_PRI_WEIGHT);
	sfr_mask_data(0, PRI_WEIGHT_MASK, on ? PRI_WEIGHT_DOT1Q : 0);
	reg_write_m(RTL837X_PRI_WEIGHT);
}


void qos_dscp_weight_set(uint8_t on) __banked
{
	reg_read_m(RTL837X_PRI_WEIGHT);
	sfr_mask_data(1, PRI_WEIGHT_DSCP_MASK, on ? PRI_WEIGHT_DSCP : 0);
	reg_write_m(RTL837X_PRI_WEIGHT);
}


void qos_pcp_set(uint8_t pcp, uint8_t priority) __banked
{
	uint8_t shift = (pcp & 1) << 2;

	reg_read_m(RTL837X_DOT1Q_PRI_REMAP);
	sfr_mask_data(pcp >> 1, PRI_REMAP_MASK << shift, priority << shift);
	reg_write_m(RTL837X_DOT1Q_PRI_REMAP);
}


void qos_dscp_set(uint8_t dscp, uint8_t priority) __banked
{
	uint16_t reg = RTL837X_DSCP_PRI_REMAP + ((dscp / QOS_DSCP_PER_WORD) << 2);

	reg_read_m(reg);
	dscp_field_set((dscp % QOS_DSCP_PER_WORD) * 3, priority);
	reg_write_m(reg);
}


void qos_queue_set(uint8_t port, uint8_t queue, uint8_t weight) __banked
{
	REG_SET(RTL837X_SCHED_PORT_Q_CTRL + port * 1024 + (queue << 2), weight);
}

void qos_sched_set(uint8_t port, uint8_t wrr) __banked
{
	if (wrr)
		reg_bit_set(RTL837X_SCHED_PORT_ALGO_CTRL, port);
	else
		reg_bit_clear(RTL837X_SCHED_PORT_ALGO_CTRL, port);
}


static void print_queue_limit(uint8_t port, uint8_t queue, uint8_t is_min)
{
	print_string("port ");
	print_phys_port(port);
	print_string(" q");
	write_char('0' + queue);
	print_string(is_min ? " min 0x" : " max 0x");
	sfr_data[1] &= ~BW_LB_EN;
	print_byte(sfr_data[1]);
	print_byte(sfr_data[2]);
	print_byte(sfr_data[3]);
	write_char('0');
	write_char('\n');
}


void qos_queue_max_set(uint8_t port, uint8_t queue, __xdata uint32_t bw) __banked
{
	uint16_t reg = RTL837X_EGBW_PORT_Q_MAX + port * 1024 + (queue << 3);

	if (bw)
		bandwidth_rate_set(reg, bw);
	else
		bandwidth_rate_off(reg);
}


void qos_queue_min_set(uint8_t port, uint8_t queue, __xdata uint32_t bw) __banked
{
	uint16_t reg = RTL837X_EGBW_PORT_Q_ASSURED + port * 1024 + (queue << 2);

	if (bw)
		bandwidth_rate_set(reg, bw);
	else
		bandwidth_rate_off(reg);
}


void qos_show(void) __banked
{
	uint8_t port, i, dscp, word;

	reg_read_m(RTL837X_PRI_WEIGHT);
	print_string("802.1p priority: ");
	print_string((sfr_data[3] & PRI_WEIGHT_MASK) ? "on\npcp:" : "off\npcp:");

	reg_read_m(RTL837X_DOT1Q_PRI_REMAP);
	for (i = 0; i < QOS_PRIORITIES; i++) {
		write_char(' ');
		write_char('0' + ((sfr_data[3 - (i >> 1)] >> ((i & 1) << 2)) & PRI_REMAP_MASK));
	}

	reg_read_m(RTL837X_PRI_WEIGHT);
	print_string("\ndscp: ");
	print_string((sfr_data[2] & PRI_WEIGHT_DSCP_MASK) ? "on" : "off");

	dscp = 0;
	word = 0;
	while (dscp < QOS_DSCP_VALUES) {
		write_char('\n');
		itoa(dscp);
		write_char(':');
		reg_read_m(RTL837X_DSCP_PRI_REMAP + (word << 2));
		for (i = 0; i < QOS_DSCP_PER_WORD && dscp < QOS_DSCP_VALUES; i++, dscp++) {
			write_char(' ');
			write_char('0' + dscp_field_get(i * 3));
		}
		word++;
	}

	print_string("\nqueue weights\n");
	for (port = machine.min_port; port <= machine.max_port; port++) {
		print_string("port ");
		print_phys_port(port);
		reg_read_m(RTL837X_SCHED_PORT_ALGO_CTRL);
		print_string((sfr_data[3 - (port >> 3)] >> (port & 7)) & 1 ? " wrr" : " wfq");
		for (i = 0; i < QOS_QUEUES; i++) {
			reg_read_m(RTL837X_SCHED_PORT_Q_CTRL + port * 1024 + (i << 2));
			write_char(' ');
			if (sfr_data[3] & SCHED_Q_STRICT)
				print_string("sp");
			else
				itoa(sfr_data[3]);
		}
		write_char('\n');
	}

	print_string("queue limits\n");
	for (port = machine.min_port; port <= machine.max_port; port++) {
		for (i = 0; i < QOS_QUEUES; i++) {
			reg_read_m(RTL837X_EGBW_PORT_Q_MAX + port * 1024 + (i << 3));
			if (sfr_data[1] & BW_LB_EN)
				print_queue_limit(port, i, 0);
			reg_read_m(RTL837X_EGBW_PORT_Q_ASSURED + port * 1024 + (i << 2));
			if (sfr_data[1] & BW_LB_EN)
				print_queue_limit(port, i, 1);
		}
	}
}
