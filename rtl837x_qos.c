#include <stdint.h>
#include "rtl837x_common.h"
#include "rtl837x_sfr.h"
#include "rtl837x_regs.h"
#include "rtl837x_qos.h"
#include "machine.h"

#pragma codeseg BANK3
#pragma constseg BANK3

extern __code const struct machine machine;
extern __xdata uint8_t sfr_data[4];

static __xdata uint8_t qos_i;

void qos_dot1q_set(uint8_t on) __banked
{
	reg_read_m(RTL837X_PRI_WEIGHT);
	sfr_mask_data(0, PRI_WEIGHT_MASK, on ? PRI_WEIGHT_DOT1Q : 0);
	reg_write_m(RTL837X_PRI_WEIGHT);
}


void qos_pcp_set(uint8_t pcp, uint8_t priority) __banked
{
	uint8_t shift = (pcp & 1) << 2;

	reg_read_m(RTL837X_DOT1Q_PRI_REMAP);
	sfr_mask_data(pcp >> 1, PRI_REMAP_MASK << shift, priority << shift);
	reg_write_m(RTL837X_DOT1Q_PRI_REMAP);
}


uint8_t qos_queue_set(uint8_t port, uint8_t queue, uint8_t weight) __banked
{
	if (queue == QOS_CTRL_QUEUE && weight != SCHED_Q_STRICT) {
		for (qos_i = 0; qos_i < QOS_CTRL_QUEUE; qos_i++) {
			reg_read_m(RTL837X_SCHED_PORT_Q_CTRL + port * 1024 + (qos_i << 2));
			if (sfr_data[3] & SCHED_Q_STRICT)
				return 0;
		}
	}
	REG_SET(RTL837X_SCHED_PORT_Q_CTRL + port * 1024 + (queue << 2), weight);
	if (weight == SCHED_Q_STRICT && queue != QOS_CTRL_QUEUE)
		REG_SET(RTL837X_SCHED_PORT_Q_CTRL + port * 1024 + (QOS_CTRL_QUEUE << 2), SCHED_Q_STRICT);
	return 1;
}


void qos_show(void) __banked
{
	uint8_t port, i;

	reg_read_m(RTL837X_PRI_WEIGHT);
	print_string("802.1p priority: ");
	print_string((sfr_data[3] & PRI_WEIGHT_MASK) ? "on\npcp:" : "off\npcp:");

	reg_read_m(RTL837X_DOT1Q_PRI_REMAP);
	for (i = 0; i < QOS_PRIORITIES; i++) {
		write_char(' ');
		write_char('0' + ((sfr_data[3 - (i >> 1)] >> ((i & 1) << 2)) & PRI_REMAP_MASK));
	}

	print_string("\nqueue weights\n");
	for (port = MACHINE_MIN_PORT; port <= MACHINE_MAX_PORT; port++) {
		print_string("port ");
		print_phys_port(port);
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
}
