#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <stddef.h>

#include "rtl837x_common.h"
#include "rtl837x_regs.h"
#include "rtl837x_qos.h"
#include "machine.h"
#include "hw_mock.h"
#include "support.h"

const struct machine machine = { .min_port = 0, .max_port = 7,
				 .log_to_phys_port = {5, 1, 2, 3, 4, 6, 7, 8, 9} };

char   out_buf[8192];
size_t out_len;
void out_reset(void) { out_len = 0; out_buf[0] = 0; }

void write_char(char c)
{
	if (out_len + 1 < sizeof(out_buf)) {
		out_buf[out_len++] = c;
		out_buf[out_len] = 0;
	}
}

void print_string(const char *p) { while (*p) write_char(*p++); }
void itoa(uint8_t v)
{
	if (v >= 100)
		write_char('0' + v / 100);
	if (v >= 10)
		write_char('0' + (v / 10) % 10);
	write_char('0' + v % 10);
}

void print_phys_port(uint8_t port) { write_char('0' + machine.log_to_phys_port[port]); }
void print_byte(uint8_t b) { (void)b; }
void print_short(uint16_t v) { (void)v; }
void print_long(uint32_t v) { (void)v; }
void print_reg(uint16_t r) { (void)r; }

#define QUEUE_CTRL(port, queue)	(RTL837X_SCHED_PORT_Q_CTRL + (port) * 1024 + ((queue) << 2))

static int fails;

static void check_u32(const char *what, uint32_t got, uint32_t want)
{
	if (got != want) {
		printf("FAIL %s: got %08x want %08x\n", what, got, want);
		fails++;
	}
}

/* The dot1q weight is the lowest five bits of PRI_WEIGHT; the other four
 * sources share the same word and have to survive the write. */
static void test_dot1q_weight(void)
{
	hw_reset();
	hw_reg_set(RTL837X_PRI_WEIGHT, 0x00123400);
	qos_dot1q_set(1);
	check_u32("dot1q on", hw_reg_get(RTL837X_PRI_WEIGHT), 0x00123401);
	qos_dot1q_set(0);
	check_u32("dot1q off", hw_reg_get(RTL837X_PRI_WEIGHT), 0x00123400);
}

/* PCP n sits in nibble n, counted from the least significant end. */
static void test_pcp_map(void)
{
	hw_reset();
	hw_reg_set(RTL837X_DOT1Q_PRI_REMAP, 0x76543210);
	qos_pcp_set(5, 7);
	check_u32("pcp 5 -> 7", hw_reg_get(RTL837X_DOT1Q_PRI_REMAP), 0x76743210);
	qos_pcp_set(0, 4);
	check_u32("pcp 0 -> 4", hw_reg_get(RTL837X_DOT1Q_PRI_REMAP), 0x76743214);
	qos_pcp_set(7, 1);
	check_u32("pcp 7 -> 1", hw_reg_get(RTL837X_DOT1Q_PRI_REMAP), 0x16743214);

	/* The field is three bits wide on a four-bit boundary, so the fourth bit
	 * of a nibble belongs to no field and has to come back untouched. */
	hw_reg_set(RTL837X_DOT1Q_PRI_REMAP, 0x88888888);
	qos_pcp_set(0, 5);
	check_u32("pcp 0 -> 5 keeps bit 3", hw_reg_get(RTL837X_DOT1Q_PRI_REMAP), 0x8888888d);
}

static void test_queue_ctrl(void)
{
	hw_reset();
	qos_queue_set(3, 6, 20);
	check_u32("port 3 queue 6", hw_reg_get(QUEUE_CTRL(3, 6)), 20);
	check_u32("port 3 queue 5 untouched", hw_reg_get(QUEUE_CTRL(3, 5)), 0);
	check_u32("port 2 queue 6 untouched", hw_reg_get(QUEUE_CTRL(2, 6)), 0);

	qos_queue_set(0, 0, SCHED_Q_STRICT);
	check_u32("port 0 queue 0 strict", hw_reg_get(QUEUE_CTRL(0, 0)), SCHED_Q_STRICT);

	qos_queue_set(9, 7, QOS_WEIGHT_MAX);
	check_u32("port 9 queue 7", hw_reg_get(QUEUE_CTRL(9, 7)), QOS_WEIGHT_MAX);
}

/* The identity table and equal weights have to read back as they are. */
static void test_show_factory(void)
{
	uint8_t port, queue;

	hw_reset();
	hw_reg_set(RTL837X_DOT1Q_PRI_REMAP, 0x76543210);
	for (port = 0; port < 10; port++)
		for (queue = 0; queue < QOS_QUEUES; queue++)
			hw_reg_set(QUEUE_CTRL(port, queue), 1);

	out_reset();
	qos_show();
	if (!t_strcmp(out_buf, "")) {
		printf("FAIL show: no output\n");
		fails++;
		return;
	}
	if (t_strcmp(out_buf,
		     "802.1p priority: off\npcp: 0 1 2 3 4 5 6 7\nqueue weights\n"
		     "port 5 1 1 1 1 1 1 1 1\n"
		     "port 1 1 1 1 1 1 1 1 1\n"
		     "port 2 1 1 1 1 1 1 1 1\n"
		     "port 3 1 1 1 1 1 1 1 1\n"
		     "port 4 1 1 1 1 1 1 1 1\n"
		     "port 6 1 1 1 1 1 1 1 1\n"
		     "port 7 1 1 1 1 1 1 1 1\n"
		     "port 8 1 1 1 1 1 1 1 1\n")) {
		printf("FAIL show, got:\n%s", out_buf);
		fails++;
	}
}

/* A queue switched to strict priority shows as sp, the rest keep their weight. */
static void test_show_strict(void)
{
	uint8_t queue;

	hw_reset();
	hw_reg_set(RTL837X_DOT1Q_PRI_REMAP, 0x76543210);
	hw_reg_set(RTL837X_PRI_WEIGHT, PRI_WEIGHT_DOT1Q);
	for (queue = 0; queue < QOS_QUEUES; queue++)
		hw_reg_set(QUEUE_CTRL(0, queue), 4);
	hw_reg_set(QUEUE_CTRL(0, 7), SCHED_Q_STRICT);

	out_reset();
	qos_show();
	if (t_strcmp(out_buf,
		     "802.1p priority: on\npcp: 0 1 2 3 4 5 6 7\nqueue weights\n"
		     "port 5 4 4 4 4 4 4 4 sp\n"
		     "port 1 0 0 0 0 0 0 0 0\n"
		     "port 2 0 0 0 0 0 0 0 0\n"
		     "port 3 0 0 0 0 0 0 0 0\n"
		     "port 4 0 0 0 0 0 0 0 0\n"
		     "port 6 0 0 0 0 0 0 0 0\n"
		     "port 7 0 0 0 0 0 0 0 0\n"
		     "port 8 0 0 0 0 0 0 0 0\n")) {
		printf("FAIL show strict, got:\n%s", out_buf);
		fails++;
	}
}

int main(void)
{
	test_dot1q_weight();
	test_pcp_map();
	test_queue_ctrl();
	test_show_factory();
	test_show_strict();
	printf(fails ? "test_qos: FAILED\n" : "test_qos: ok\n");
	return fails ? 1 : 0;
}
