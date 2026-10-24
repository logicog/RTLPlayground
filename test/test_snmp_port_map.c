/* Real RTL8372 profile and non-identity mapping; no hardware access. */
#define SNMP_REAL_MACHINE
#define SNMP_TRANSPORT_TEST
#include "test_snmp.c"

void udp_callbacks(void)
{
	snmp_callback();
}

int main(void)
{
#ifdef SNMP_MAP_RESTRICTED
	/* Test-only upper bound excludes MAC7 and MAC8, hence logical rows 4/6. */
	static const uint8_t rows[] = {1, 2, 3, 5};
	static const uint8_t chips[] = {4, 5, 6, 3};
#else
	static const uint8_t rows[] = {1, 2, 3, 4, 5, 6};
	static const uint8_t chips[] = {4, 5, 6, 7, 3, 8};
#endif
	uip_init();
	snmp_init();
	assert(test_snmp_configure(comm, sizeof(comm) - 1, network, acl));
	query_oid(2, 0, 0);
	make_request(SNMP_PDU_GET, 1);
	decode_response(snmp_process(req, req_len, source));
	CHECK(!response_error && result_type[0] == 2 && result_data[0][0] == sizeof(rows),
	      "profile bounds and mapping determine ifNumber, not max_port+1");
	for (unsigned i = 0; i < sizeof(rows); ++i) {
		assert(MACHINE_PORT_VALID(chips[i]) && chips[i] != CPU_PORT);
		assert(machine.log_to_phys_port[chips[i]] == rows[i]);
		query_oid(31, 6, rows[i]);
		make_request(SNMP_PDU_GET, 1);
		decode_response(snmp_process(req, req_len, source));
		CHECK(!response_error && result_type[0] == 0x46 && last_port == chips[i],
		      "HC Counter64 row resolves to expected physical MAC");
	}
	query_oid(2, 2, 5);
	make_request(SNMP_PDU_GET, 1);
	decode_response(snmp_process(req, req_len, source));
	CHECK(result_type[0] == 4 && result_len[0] == 5 && !memcmp(result_data[0], "SFP 5", 5),
	      "non-identity RTL8372 mapping retains SFP row");
	for (unsigned i = 0; i < sizeof(rows); ++i) {
		query_oid(2, 1, i ? rows[i - 1] : 0);
		make_request(SNMP_PDU_GETNEXT, 1);
		decode_response(snmp_process(req, req_len, source));
		CHECK(!response_error && names[0][8] == 1 && names[0][9] == rows[i],
		      "GETNEXT orders logical ifIndex and skips unavailable MAC rows");
	}
	query_oid(2, 1, rows[sizeof(rows) - 1]);
	make_request(SNMP_PDU_GETNEXT, 1);
	decode_response(snmp_process(req, req_len, source));
	CHECK(names[0][8] == 2 && names[0][9] == 1,
	      "GETNEXT crosses column after last actual interface");
	for (uint8_t index = 0; index <= 10; ++index) {
		bool present = false;
		for (unsigned i = 0; i < sizeof(rows); ++i)
			present |= rows[i] == index;
		if (present)
			continue;
		unsigned before = hw_calls;
		query_oid(2, 8, index);
		make_request(SNMP_PDU_GET, 1);
		decode_response(snmp_process(req, req_len, source));
		CHECK(!response_error && result_type[0] == 0x81 && hw_calls == before,
		      "absent/zero/CPU indices return noSuchInstance without hardware read");
	}
	puts("PASS profile macros, RTL8372 mapping/SFP, sparse ordering and CPU exclusion");
	return failures ? 1 : 0;
}
