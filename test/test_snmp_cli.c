/* Real SNMP packet/configuration engine. Hardware, NTP and CLI token edges mocked. */
#define SNMP_CONTROL_TEST
#define SNMP_TRANSPORT_TEST
#define SNMP_HORACO_CONTROL_CLOCK
#include "test_snmp.c"
#include "snmp_internal.h"
static char console[512];
static unsigned console_len;
void control_console_char(char c)
{
	if (console_len < sizeof(console) - 1) {
		console[console_len++] = c;
		console[console_len] = 0;
	}
}
void udp_callbacks(void)
{
	snmp_callback();
}
static bool command(const char *s)
{
	assert(strlen(s) < sizeof(cmd_buffer));
	strcpy((char *)cmd_buffer, s);
	cmd_words_len = 0;
	for (unsigned i = 0; cmd_buffer[i]; i++)
		if (cmd_buffer[i] != ' ' && (i == 0 || cmd_buffer[i - 1] == ' '))
			cmd_words_b[cmd_words_len++] = i;
	return snmp_command();
}
static uint16_t packet(const uint8_t *c, const uint8_t *src, uint8_t tag)
{
	query_oid(1, 5, 0);
	make_request(tag, 1);
	memcpy(req + 9, c, sizeof(comm) - 1);
	return snmp_process(req, req_len, src);
}
int main(void)
{
	static const uint8_t changed[] = "changed-secret";
	uint8_t status[10], prior[10];
	uip_init();
	snmp_init();
	assert(test_snmp_configure(comm, sizeof(comm) - 1, network, acl));
	save_cmd = 1;
	CHECK(packet(comm, source, 0xa0) > 0, "old community initially works");
	CHECK(command("snmp community changed-secret"), "live CLI community change");
	CHECK(!packet(comm, source, 0xa0), "old community stops immediately");
	CHECK(packet(changed, source, 0xa0) > 0 && packet(changed, source, 0xa1) > 0,
	      "new community GET/GETNEXT work immediately");
	CHECK(!packet(changed, source, 0xa3), "SET remains unsupported");
	CHECK(!command("snmp community") && !command("snmp community a b"),
	      "missing and extra community arguments rejected");
	uint8_t control[] = {1, 'x'}, longvalue[33];
	memset(longvalue, 'x', 33);
	CHECK(!snmp_set_community(control, 2) && !snmp_set_community(longvalue, 33) &&
		  !snmp_set_community(NULL, 1),
	      "invalid bytes/length/pointer rejected");
	CHECK(packet(changed, source, 0xa0) > 0, "invalid community preserves working value");
	CHECK(command("snmp manager 192.0.3.9/32"), "valid CLI manager change");
	uint8_t newsource[] = {192, 0, 3, 9};
	CHECK(!packet(changed, source, 0xa0) && packet(changed, newsource, 0xa0) > 0,
	      "ACL change blocks old manager and admits new manager");
	snmp_get_status(prior);
	const char *bad[] = {"192.0.3.256/32", "192.0.3.9/0", "192.0.3.9/33",
			     "192.0.3.9/32x",  "192.0.3/32",  "127.0.0.1/32",
			     "224.0.0.1/32",   "0.0.0.0/32",  "192.0.3.9/999"};
	for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++)
		assert(!snmp_set_manager((const uint8_t *)bad[i], strlen(bad[i])));
	snmp_get_status(status);
	CHECK(!memcmp(prior, status, 10), "all invalid IPv4/CIDR attempts preserve running state");
	CHECK(command("snmp manager 192.0.3.9/24"), "host CIDR canonicalized to network");
	snmp_get_status(status);
	CHECK(status[5] == 0 && status[9] == 0, "canonical network/mask exported");
	uint32_t prior_uptime = snmp_uptime;
	CHECK(command("snmp community timed-community"),
	      "second community change through same setter");
	struct uip_udp_conn *listener = snmp_listener;
	CHECK(command("snmp community timed-community") && snmp_listener == listener,
	      "same value does not reallocate listener");
	CHECK(command("snmp off") && command("snmp on") && snmp_uptime == prior_uptime,
	      "toggle preserves uptime");
	CHECK(!command("snmp changed 1735689700"), "removed timestamp command rejected live");
	save_cmd = 0;
	CHECK(command("snmp changed 1735689700"), "legacy timestamp ignored only on replay");
	save_cmd = 1;
	console_len = 0;
	console[0] = 0;
	snmp_show();
	CHECK(strstr(console, "Community: Configured") && !strstr(console, "timed-community") &&
		  !strstr(console, "Last Changed"),
	      "show has no secret or timestamp");
	unsigned printed = console_len;
	snmp_export_config();
	outbuf[slen] = 0;
	CHECK(console_len == printed && strstr((char *)outbuf, "Cache-Control: no-store") &&
		  strstr((char *)outbuf, "snmp community timed-community\n") &&
		  !strstr((char *)outbuf, "snmp changed"),
	      "snapshot exports current configuration without timestamp or console logging");
	uint16_t oldport = snmp_listener->lport;
	snmp_listener->lport = HTONS(162);
	CHECK(!snmp_set_community((const uint8_t *)"rejected", 8),
	      "invalid listener binding preserves community");
	snmp_listener->lport = oldport;
	assert(command("snmp community late-community"));
	scenario_information_auth();
	const char *requests[] = {
	    "GET /snmp_config HTTP/1.1\r\nHost: sw\r\n\r\n",
	    "GET /snmp_config HTTP/1.1\r\nHost: sw\r\nCookie: session=012345abcdef\r\n\r\n"};
	for (unsigned i = 0; i < 2; i++) {
		session_start(MSS_FULL);
		if (i)
			strcpy(session_id, "012345abcdef");
		else
			session_id[0] = 0;
		client_send(TCP_ACK | TCP_PSH, requests[i], strlen(requests[i]));
		drain(MSS_FULL);
		stream[stream_len] = 0;
		CHECK(strstr((char *)stream, i ? "200 OK" : "401 Unauthorized"),
		      "snapshot HTTP authentication");
		CHECK((strstr((char *)stream, "late-community") != NULL) == (i == 1),
		      "snapshot credential withheld before authentication");
	}
	return failures ? 1 : 0;
}
