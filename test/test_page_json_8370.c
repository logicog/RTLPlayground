/*
 * test_page_json_8370.c - /status.json as the RTL836x (SOC_FAMILY == RTL836X)
 * branches of httpd/page_impl.c emit it.
 *
 * This is the only host binary built with -DMACHINE_NAME=SODOLA_SL_SG008W, so it is the
 * only place those paths are exercised at all. What it guards is the SHAPE of the
 * port object rather than the counter values: the RTL836x branch ends each
 * object with an unterminated string value, because the shared code below
 * appends the terminating quote together with "}". A field that terminates
 * itself (a literal ,"foo":1) therefore yields ..."foo":1"} - one stray
 * quote that makes the whole page unparseable, and the browser then blanks
 * every table that reads it (dashboard and statistics alike). Every field is
 * emitted with its opening quote only, including the per-port `macs` count
 * (read from the RTL8367B learn-count register at 0x0a87 + port), and the
 * ends_with() CHECK below pins that convention down.
 *
 * The counter values are all zero here: the mock models an RTL8372/3 register
 * file, so the MIB read fails rather than counting. JSON shape is the subject.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "rtl837x_common.h"
#include "rtlos_regs.h"
#include "rtl837x_port.h"
#include "machine.h"
#include "page_impl.h"
#include "support.h"
#include "hw_mock.h"

extern uint8_t  outbuf[TCP_OUTBUF_SIZE];
extern uint16_t slen;

void delay(uint16_t ms) { (void)ms; }

/* The response body: what follows the blank line after the header. */
static const char *body(void)
{
	outbuf[slen < TCP_OUTBUF_SIZE ? slen : TCP_OUTBUF_SIZE - 1] = 0;
	const char *p = strstr((const char *)outbuf, "\r\n\r\n");
	return p ? p + 4 : (const char *)outbuf;
}

static int has(const char *needle)
{
	return strstr(body(), needle) != 0;
}

static int count(const char *needle)
{
	int n = 0;
	for (const char *p = body(); (p = strstr(p, needle)); p += strlen(needle))
		n++;
	return n;
}

/* Brackets and braces balance outside strings, quotes pair up. */
static int balanced(void)
{
	int depth = 0, instr = 0;
	for (const char *p = body(); *p; p++) {
		if (*p == '"') { instr = !instr; continue; }
		if (instr) continue;
		if (*p == '{' || *p == '[') depth++;
		if (*p == '}' || *p == ']') depth--;
		if (depth < 0) return 0;
	}
	return depth == 0 && !instr;
}

static int ends_with(const char *tail)
{
	size_t l = strlen(body()), t = strlen(tail);
	if (l < t) return 0;
	return strcmp(body() + l - t, tail) == 0;
}

/* /status.json emits the front ports only; this board has 8 of them. */
#define STATUS_PORTS (MACHINE_MAX_PORT - MACHINE_MIN_PORT + 1)

static void t_status_8370(void)
{
	printf("[test] /status.json from the RTL836x branch is well-formed\n");
	hw_reset();
	hw_reg_set(RTL836x_LUT_PORT_LEARN_CNT_REG(1), 0x21);
	out_reset();
	send_status();

	CHECK(balanced(), "valid JSON: quotes pair up, braces balance");
	CHECK(ends_with("\"macs\":\"0x0\"}]"),
	      "the port object ends on an OPEN string value closed by the shared \"}");
	CHECK(count("\"txG\":\"0x") == STATUS_PORTS
	      && count("\"rxG\":\"0x") == STATUS_PORTS
	      && count("\"txB\":\"0x") == STATUS_PORTS
	      && count("\"rxB\":\"0x") == STATUS_PORTS,
	      "all four counter fields are present on every port");
	CHECK(has("\"macs\":\"0x21\""), "port 1 reports the learned count from 0x0a88");
	CHECK(count("\"macs\":\"0x") == STATUS_PORTS,
	      "every port reports a learned count");
	CHECK(count("\"link\":") == STATUS_PORTS,
	      "every port reports a link state");
	CHECK(strstr(body(), "[{") == body(), "the body is a JSON array of objects");
	CHECK(slen <= TCP_OUTBUF_SIZE, "fits the TCP output buffer");
}

int main(void)
{
	printf("== httpd/page_impl.c RTL836x /status.json tests ==\n");
	t_status_8370();
	printf("\n%d checks, %d failed\n", tests_run, tests_failed);
	return tests_failed ? 1 : 0;
}
