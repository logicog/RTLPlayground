/* Real SNMP accessor + real information serializer; hardware/socket edges mocked. */
#define main existing_page_json_main
#include "test_page_json.c"
#undef main
#include "snmp.h"
#include "snmp_setup.h"
#include "snmp_internal.h"
#include "uip.h"
#include <assert.h>
bool EA = true;
extern uint16_t session_timeout;
extern volatile uint32_t snmp_uptime;
static struct uip_udp_conn socket_state;
static bool exhausted;
static unsigned allocations, initializations;
void snmp_hw_init(void)
{
	++initializations;
}
struct uip_udp_conn *uip_udp_new(uip_ipaddr_t *address, uint16_t port)
{
	(void)address;
	(void)port;
	++allocations;
	return exhausted ? NULL : &socket_state;
}
int main(void)
{
	uint8_t guarded[12], before[10];
	uint8_t n[] = {192, 0, 2, 99}, m[] = {255, 255, 255, 255};
	uint8_t secret[] = "PRIVATE-status-fixture";
	snmp_init();
	memset(guarded, 0xa5, sizeof(guarded));
	snmp_get_status(guarded + 1);
	CHECK(guarded[0] == 0xa5 && guarded[11] == 0xa5, "snapshot writes only ten bytes");
	const uint8_t zero[10] = {0};
	CHECK(!memcmp(guarded + 1, zero, 10), "unconfigured agent has no active ACL");
	send_basic_info();
	CHECK(has("\"snmp\":[0,0,0,0,0,0,0,0,0,0]"), "disabled JSON status");
	CHECK(test_snmp_configure(secret, sizeof(secret) - 1, n, m),
	      "runtime configuration for fixture");
	snmp_get_status(before);
	const uint8_t expected[] = {1, 1, 192, 0, 2, 99, 255, 255, 255, 255};
	CHECK(!memcmp(before, expected, 10),
	      "snapshot reports runtime values rather than lab defaults");
	socket_state.lport = 0;
	snmp_get_status(guarded + 1);
	CHECK(!guarded[1] && guarded[2] == 1,
	      "agent state checks actual UDP binding independently of credential presence");
	CHECK(!snmp_set_enabled(1), "cannot enable an incorrectly bound listener");
	socket_state.lport = HTONS(161);
	struct uip_udp_conn prior = socket_state;
	unsigned calls = allocations;
	snmp_uptime = 123456;
	for (unsigned i = 0; i < 1000; i++) {
		send_basic_info();
		assert(slen < TCP_OUTBUF_SIZE);
		assert(has("\"snmp\":[1,1,192,0,2,99,255,255,255,255]"));
		assert(!has((char *)secret) && !has("community_length") && balanced());
		snmp_get_status(guarded + 1);
		assert(!memcmp(before, guarded + 1, 10));
	}
	CHECK(allocations == calls && !memcmp(&prior, &socket_state, sizeof(prior)) &&
		  snmp_uptime == 123456 && initializations == 1,
	      "1000 reads preserve listener, ACL and uptime without configuration calls");
	CHECK(snmp_set_enabled(0), "runtime status agent toggle off");
	send_basic_info();
	CHECK(has("\"snmp\":[0,1,192,0,2,99,255,255,255,255]"),
	      "disabled agent JSON retains configured community and actual ACL");
	CHECK(!has((char *)secret) && allocations == calls && snmp_uptime == 123456,
	      "toggle does not allocate, expose community or reset uptime");
	CHECK(snmp_set_enabled(1), "runtime status agent toggle on");
	uint8_t other[] = "x";
	CHECK(test_snmp_configure(other, 1, n, m), "different credential length fixture");
	snmp_get_status(guarded + 1);
	CHECK(!memcmp(before, guarded + 1, 10),
	      "configured status independent of community value and length");
	CHECK(test_snmp_configure(NULL, 0, NULL, NULL), "disable fixture");
	snmp_get_status(guarded + 1);
	CHECK(!memcmp(guarded + 1, zero, 10), "disable clears reported active ACL");
	exhausted = true;
	CHECK(!test_snmp_configure(secret, sizeof(secret) - 1, n, m), "socket exhaustion fixture");
	snmp_get_status(guarded + 1);
	CHECK(!memcmp(guarded + 1, zero, 10), "failed listener allocation reports disabled");
	return tests_failed ? 1 : 0;
}
