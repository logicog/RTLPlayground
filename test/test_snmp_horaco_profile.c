/* Actual machine/SWTGW218AS.c profile; hardware edge stays mocked. */
#define SNMP_HORACO_CONTROL_CLOCK
#define SNMP_REAL_MACHINE
#define SNMP_TRANSPORT_TEST
#define SNMP_TEST_NINE
#include "test_snmp.c"
#include "rtl837x_pins.h"
void udp_callbacks(void)
{
	snmp_callback();
}
int main(void)
{
	assert(machine.isRTL8373 && MACHINE_MIN_PORT == 0 && MACHINE_MAX_PORT == 8);
	assert(machine.mac_flash_offset == 0x1fc000 && machine.n_sfp == 1);
	assert(machine.sfp_port[0].pin_detect == GPIO30_ACL_BIT3_EN);
	assert(machine.sfp_port[0].pin_los == GPIO37 && machine.reset_pin == GPIO54_ACL_BIT2_EN);
	uip_init();
	uip_ipaddr(uip_hostaddr, 192, 0, 2, 1);
	snmp_init();
	assert(test_snmp_configure(comm, sizeof(comm) - 1, network, acl));
	query_oid(2, 0, 0);
	make_request(0xa0, 1);
	decode_response(snmp_process(req, req_len, source));
	CHECK(result_type[0] == 2 && result_len[0] == 1 && result_data[0][0] == 9,
	      "real HORACO ifNumber is nine, excluding CPU");
	for (uint8_t index = 1; index <= 9; index++) {
		assert(machine.log_to_phys_port[index - 1] == index &&
		       machine.phys_to_log_port[index - 1] == index - 1);
		query_oid(31, 6, index);
		make_request(0xa0, 1);
		decode_response(snmp_process(req, req_len, source));
		CHECK(result_type[0] == 0x46 && last_port == index - 1 && last_selector == 0,
		      "real HORACO HC row maps to physical MAC");
	}
	query_oid(2, 2, 9);
	make_request(0xa0, 1);
	decode_response(snmp_process(req, req_len, source));
	CHECK(result_type[0] == 4 && result_len[0] == 5 && !memcmp(result_data[0], "SFP 9", 5),
	      "real HORACO SFP is front-panel row nine");
	query_oid(2, 1, 10);
	make_request(0xa0, 1);
	decode_response(snmp_process(req, req_len, source));
	CHECK(result_type[0] == 0x81, "real HORACO CPU MAC9 has no synthesized row ten");
	query_oid(1, 1, 0);
	make_request(0xa0, 1);
	decode_response(snmp_process(req, req_len, source));
	CHECK(result_type[0] == 4 &&
		  !memcmp(result_data[0], machine.machine_name, strlen(machine.machine_name)),
	      "sysDescr uses actual HORACO machine name");
	query_oid(2, 2, 9);
	make_request(0xa1, 1);
	decode_response(snmp_process(req, req_len, source));
	CHECK(result_type[0] == 2 && names[0][8] == 3 && names[0][9] == 1,
	      "real HORACO GETNEXT crosses column in numeric order");
	return failures ? 1 : 0;
}
