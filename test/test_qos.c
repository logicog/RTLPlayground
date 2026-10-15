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

const struct machine machine = {
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
#define Q_MAX(port, queue)	(RTL837X_EGBW_PORT_Q_MAX + (port) * 1024 + ((queue) << 3))
#define Q_MIN(port, queue)	(RTL837X_EGBW_PORT_Q_ASSURED + (port) * 1024 + ((queue) << 2))
#define DSCP_WORD(dscp)		(RTL837X_DSCP_PRI_REMAP + ((dscp) / QOS_DSCP_PER_WORD) * 4)

/* Ten values per line, four on the last one: the table ends at DSCP 63. */
#define SHOW_DSCP_EMPTY \
	"\ndscp: off" \
	"\n0: 0 0 0 0 0 0 0 0 0 0" \
	"\n10: 0 0 0 0 0 0 0 0 0 0" \
	"\n20: 0 0 0 0 0 0 0 0 0 0" \
	"\n30: 0 0 0 0 0 0 0 0 0 0" \
	"\n40: 0 0 0 0 0 0 0 0 0 0" \
	"\n50: 0 0 0 0 0 0 0 0 0 0" \
	"\n60: 0 0 0 0"

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
		     "802.1p priority: off\npcp: 0 1 2 3 4 5 6 7"
		     SHOW_DSCP_EMPTY
		     "\nqueue weights\n"
		     "port 5 1 1 1 1 1 1 1 1\n"
		     "port 1 1 1 1 1 1 1 1 1\n"
		     "port 2 1 1 1 1 1 1 1 1\n"
		     "port 3 1 1 1 1 1 1 1 1\n"
		     "port 4 1 1 1 1 1 1 1 1\n"
		     "port 6 1 1 1 1 1 1 1 1\n"
		     "port 7 1 1 1 1 1 1 1 1\n"
		     "port 8 1 1 1 1 1 1 1 1\n"
		     "queue limits\n")) {
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
		     "802.1p priority: on\npcp: 0 1 2 3 4 5 6 7"
		     SHOW_DSCP_EMPTY
		     "\nqueue weights\n"
		     "port 5 4 4 4 4 4 4 4 sp\n"
		     "port 1 0 0 0 0 0 0 0 0\n"
		     "port 2 0 0 0 0 0 0 0 0\n"
		     "port 3 0 0 0 0 0 0 0 0\n"
		     "port 4 0 0 0 0 0 0 0 0\n"
		     "port 6 0 0 0 0 0 0 0 0\n"
		     "port 7 0 0 0 0 0 0 0 0\n"
		     "port 8 0 0 0 0 0 0 0 0\n"
		     "queue limits\n")) {
		printf("FAIL show strict, got:\n%s", out_buf);
		fails++;
	}
}

/* The DSCP weight is bits 10 to 14 of PRI_WEIGHT. */
static void test_dscp_weight(void)
{
	hw_reset();
	hw_reg_set(RTL837X_PRI_WEIGHT, 0x01ff83ff);
	qos_dscp_weight_set(1);
	check_u32("dscp weight on", hw_reg_get(RTL837X_PRI_WEIGHT), 0x01ff87ff);
	qos_dscp_weight_set(0);
	check_u32("dscp weight off", hw_reg_get(RTL837X_PRI_WEIGHT), 0x01ff83ff);

	hw_reset();
	qos_dot1q_set(1);
	qos_dscp_weight_set(1);
	check_u32("both sources", hw_reg_get(RTL837X_PRI_WEIGHT), 0x00000401);
}

/* Ten entries of three bits share a word, so two of them cross a byte boundary:
 * entry 2 covers bits 6 to 8 and entry 5 covers bits 15 to 17. */
static void test_dscp_map(void)
{
	hw_reset();
	qos_dscp_set(0, 7);
	check_u32("dscp 0", hw_reg_get(DSCP_WORD(0)), 0x00000007);

	hw_reset();
	qos_dscp_set(2, 7);
	check_u32("dscp 2 crosses byte 0 and 1", hw_reg_get(DSCP_WORD(2)), 0x000001c0);

	hw_reset();
	qos_dscp_set(5, 7);
	check_u32("dscp 5 crosses byte 1 and 2", hw_reg_get(DSCP_WORD(5)), 0x00038000);

	hw_reset();
	qos_dscp_set(9, 7);
	check_u32("dscp 9", hw_reg_get(DSCP_WORD(9)), 0x38000000);

	hw_reset();
	qos_dscp_set(10, 3);
	check_u32("dscp 10 is the second word", hw_reg_get(DSCP_WORD(10)), 0x00000003);
	check_u32("dscp 10 leaves the first word", hw_reg_get(DSCP_WORD(0)), 0);
	qos_dscp_set(63, 5);
	check_u32("dscp 63 is the last word", hw_reg_get(DSCP_WORD(63)), 0x00000a00);

	hw_reset();
	hw_reg_set(DSCP_WORD(2), 0xffffffff);
	qos_dscp_set(2, 0);
	check_u32("dscp 2 clears only its own bits", hw_reg_get(DSCP_WORD(2)), 0xfffffe3f);
}

#define DSCP_WORDS	((QOS_DSCP_VALUES + QOS_DSCP_PER_WORD - 1) / QOS_DSCP_PER_WORD)
#define DSCP_ALL_SEVEN	0x3fffffff

/* Entry n of a word occupies bits n * 3 to n * 3 + 2, counted from the least
 * significant end. */
static uint8_t dscp_field_of(uint32_t word, uint8_t entry)
{
	return (word >> (entry * 3)) & 7;
}

static void test_dscp_map_exhaustive(void)
{
	uint8_t dscp, pri, w, i, want;
	uint32_t v;

	for (dscp = 0; dscp < QOS_DSCP_VALUES; dscp++) {
		for (pri = 0; pri < QOS_PRIORITIES; pri++) {
			hw_reset();
			for (w = 0; w < DSCP_WORDS; w++)
				hw_reg_set(RTL837X_DSCP_PRI_REMAP + w * 4, DSCP_ALL_SEVEN);

			qos_dscp_set(dscp, pri);

			for (w = 0; w < DSCP_WORDS; w++) {
				v = hw_reg_get(RTL837X_DSCP_PRI_REMAP + w * 4);
				for (i = 0; i < QOS_DSCP_PER_WORD; i++) {
					want = 7;
					if (w == dscp / QOS_DSCP_PER_WORD
					    && i == dscp % QOS_DSCP_PER_WORD)
						want = pri;
					if (dscp_field_of(v, i) == want)
						continue;
					printf("FAIL dscp %u -> %u: word %u entry %u is %u, want %u\n",
					       dscp, pri, w, i, dscp_field_of(v, i), want);
					fails++;
					return;
				}
			}
		}
	}
}

/* Word 0 holds DSCP 0 to 9, word 4 holds 40 to 49, word 6 holds 60 to 63. */
static void test_dscp_defaults(void)
{
	hw_reset();
	qos_defaults();
	check_u32("default word 0", hw_reg_get(DSCP_WORD(0)), 0x09000000);
	check_u32("default word 4", hw_reg_get(DSCP_WORD(40)), 0x36b6db6d);
	check_u32("default word 6", hw_reg_get(DSCP_WORD(60)), 0x00000fff);
	check_u32("nothing past the table", hw_reg_get(DSCP_WORD(60) + 4), 0);
}

static void test_show_dscp_defaults(void)
{
	hw_reset();
	hw_reg_set(RTL837X_DOT1Q_PRI_REMAP, 0x76543210);
	hw_reg_set(RTL837X_PRI_WEIGHT, 0x00000400);
	qos_defaults();

	out_reset();
	qos_show();
	if (t_strcmp(out_buf,
		     "802.1p priority: off\npcp: 0 1 2 3 4 5 6 7"
		     "\ndscp: on"
		     "\n0: 0 0 0 0 0 0 0 0 1 1"
		     "\n10: 1 1 1 1 1 1 2 2 2 2"
		     "\n20: 2 2 2 2 3 3 3 3 3 3"
		     "\n30: 3 3 4 4 4 4 4 4 4 4"
		     "\n40: 5 5 5 5 5 5 5 5 6 6"
		     "\n50: 6 6 6 6 6 6 7 7 7 7"
		     "\n60: 7 7 7 7"
		     "\nqueue weights\n"
		     "port 5 0 0 0 0 0 0 0 0\n"
		     "port 1 0 0 0 0 0 0 0 0\n"
		     "port 2 0 0 0 0 0 0 0 0\n"
		     "port 3 0 0 0 0 0 0 0 0\n"
		     "port 4 0 0 0 0 0 0 0 0\n"
		     "port 6 0 0 0 0 0 0 0 0\n"
		     "port 7 0 0 0 0 0 0 0 0\n"
		     "port 8 0 0 0 0 0 0 0 0\n"
		     "queue limits\n")) {
		printf("FAIL show dscp defaults, got:\n%s", out_buf);
		fails++;
	}
}

/* A rate holds Kbit/s divided by 16 in the lowest 20 bits, BW_LB_EN in bit 20.
 * The maximum takes two words per queue, the second one its burst; the assured
 * rate takes one. */
static void test_queue_rate(void)
{
	hw_reset();
	qos_queue_max_set(3, 5, 2000);
	check_u32("max of port 3 queue 5", hw_reg_get(Q_MAX(3, 5)), 0x0010007d);
	check_u32("max leaves queue 4", hw_reg_get(Q_MAX(3, 4)), 0);
	check_u32("max leaves its own burst word", hw_reg_get(Q_MAX(3, 5) + 4), 0);
	check_u32("max leaves the assured rate", hw_reg_get(Q_MIN(3, 5)), 0);
	check_u32("max leaves port 2", hw_reg_get(Q_MAX(2, 5)), 0);

	qos_queue_max_set(3, 5, 0);
	check_u32("max off", hw_reg_get(Q_MAX(3, 5)), BW_RATE_MAX);

	hw_reset();
	qos_queue_min_set(3, 5, 2000);
	check_u32("min of port 3 queue 5", hw_reg_get(Q_MIN(3, 5)), 0x0010007d);
	check_u32("min leaves queue 4", hw_reg_get(Q_MIN(3, 4)), 0);
	check_u32("min leaves the maximum", hw_reg_get(Q_MAX(3, 5)), 0);
	qos_queue_min_set(3, 5, 0);
	check_u32("min off", hw_reg_get(Q_MIN(3, 5)), BW_RATE_MAX);

	/* The widest rate the field holds, and the port shaper for comparison. */
	hw_reset();
	qos_queue_max_set(0, 0, 0xfffff0);
	check_u32("widest rate", hw_reg_get(Q_MAX(0, 0)), 0x001fffff);
}

int main(void)
{
	test_dot1q_weight();
	test_dscp_weight();
	test_pcp_map();
	test_dscp_map();
	test_dscp_map_exhaustive();
	test_dscp_defaults();
	test_queue_ctrl();
	test_queue_rate();
	test_show_factory();
	test_show_strict();
	test_show_dscp_defaults();
	printf(fails ? "test_qos: FAILED\n" : "test_qos: ok\n");
	return fails ? 1 : 0;
}
