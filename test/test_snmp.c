/* Network environment and TCP regressions are shared with the existing bench. */
#define udp_callbacks snmp_test_unused_udp_callbacks
#define main snmp_test_existing_tcp_main
#include "test_httpd_tx.c"
#undef main
#undef udp_callbacks
#include "machine.h"
#include "snmp.h"
#include <assert.h>
#include "snmp_hw.h"
#include "snmp_setup.h"
bool EA = true;
#ifndef SNMP_REAL_MACHINE
const struct machine machine = {
#ifdef SNMP_TRANSPORT_TEST
    .machine_name = "host switch long name 1234567",
#else
    .machine_name = "host switch",
#endif
    .min_port = 0,
#ifdef SNMP_TEST_NINE
    .max_port = 8,
    .log_to_phys_port = {8, 7, 6, 5, 4, 3, 2, 1, 9},
#else
    .max_port = 7,
    .log_to_phys_port = {8, 7, 6, 5, 4, 3, 2, 1},
#endif
    .is_sfp = {1}};
#else
extern const struct machine machine;
#endif
char hostname[24] = "host-agent";
extern volatile uint32_t snmp_uptime;
static unsigned hw_calls;
static bool hw_failed;
static bool link_up = true;
static unsigned hw_initializations;
void snmp_hw_init(void)
{
	++hw_initializations;
}
static uint64_t hw_value = UINT64_C(0x80000000ffffffff);
static uint8_t last_port, last_selector;
bool snmp_hw_counter(uint8_t port, uint8_t selector, struct snmp_counter *v)
{
	hw_calls++;
	last_port = port;
	last_selector = selector;
	for (unsigned i = 0; i < 8; i++)
		v->bytes[i] = (uint8_t)(hw_value >> ((7 - i) * 8));
	return !hw_failed;
}
bool snmp_hw_link(uint8_t port, uint32_t *v)
{
	hw_calls++;
	*v = link_up ? 1UL << port : 0;
	return !hw_failed;
}
static uint16_t measured_speed = 10000;
bool snmp_hw_speed(uint8_t port, uint16_t *v)
{
	hw_calls++;
	last_port = port;
	*v = measured_speed;
	return !hw_failed;
}
#ifndef SNMP_TRANSPORT_TEST
void udp_callbacks(void)
{
	snmp_callback();
}
#endif
static uint8_t source[4] = {192, 0, 2, 7}, network[4] = {192, 0, 2, 0}, acl[4] = {255, 255, 255, 0};
static uint8_t comm[] = "host-test-only";
static uint8_t req[484];
static size_t req_len;
static uint8_t names[8][100], name_len[8], result_type[8], result_data[8][100], result_len[8];
static unsigned result_count, response_error, response_index;
static uint8_t input_oid[80], input_oid_len;
static void query_oid(uint8_t group, uint8_t column, uint8_t index)
{
	uint8_t base[] = {43, 6, 1, 2, 1};
	memcpy(input_oid, base, 5);
	input_oid[5] = group;
	if (group == 1) {
		input_oid[6] = column;
		input_oid[7] = index;
		input_oid_len = 8;
	} else if (!column) {
		input_oid[6] = 1;
		input_oid[7] = 0;
		input_oid_len = 8;
	} else if (group == 31) {
		input_oid[6] = 1;
		input_oid[7] = 1;
		input_oid[8] = 1;
		input_oid[9] = column;
		input_oid[10] = index;
		input_oid_len = 11;
	} else {
		input_oid[6] = 2;
		input_oid[7] = 1;
		input_oid[8] = column;
		input_oid[9] = index;
		input_oid_len = 10;
	}
}
static void make_request(uint8_t pdu_type, unsigned bindings)
{
	size_t p = 0, outer, pd, list;
	req[p++] = 0x30;
	outer = p;
	p += 3;
	req[p++] = 2;
	req[p++] = 1;
	req[p++] = 1;
	req[p++] = 4;
	req[p++] = sizeof(comm) - 1;
	memcpy(req + p, comm, sizeof(comm) - 1);
	p += sizeof(comm) - 1;
	req[p++] = pdu_type;
	pd = p;
	p += 3;
	/* Signed request-id boundary: INT32_MIN. */
	req[p++] = 2;
	req[p++] = 4;
	req[p++] = 0x80;
	req[p++] = 0;
	req[p++] = 0;
	req[p++] = 0;
	req[p++] = 2;
	req[p++] = 1;
	req[p++] = 0;
	req[p++] = 2;
	req[p++] = 1;
	req[p++] = 0;
	req[p++] = 0x30;
	list = p;
	p += 3;
	for (unsigned i = 0; i < bindings; i++) {
		req[p++] = 0x30;
		req[p++] = input_oid_len + 4;
		req[p++] = 6;
		req[p++] = input_oid_len;
		memcpy(req + p, input_oid, input_oid_len);
		p += input_oid_len;
		req[p++] = 5;
		req[p++] = 0;
	}
	assert(p < sizeof(req));
	req[outer] = req[pd] = req[list] = 0x82;
	req[outer + 1] = (p - outer - 3) >> 8;
	req[outer + 2] = p - outer - 3;
	req[pd + 1] = (p - pd - 3) >> 8;
	req[pd + 2] = p - pd - 3;
	req[list + 1] = (p - list - 3) >> 8;
	req[list + 2] = p - list - 3;
	req_len = p;
}
static unsigned decode_len(const uint8_t *b, size_t *p, size_t end)
{
	assert(*p < end);
	unsigned n = b[(*p)++], v = 0;
	if (!(n & 128))
		return n;
	n &= 127;
	assert(n && n <= 2 && *p + n <= end);
	while (n--)
		v = (v << 8) | b[(*p)++];
	return v;
}
static size_t field(const uint8_t *b, size_t *p, size_t end, uint8_t tag)
{
	assert(*p < end && b[(*p)++] == tag);
	unsigned n = decode_len(b, p, end);
	assert(*p + n <= end);
	return *p + n;
}
static void decode_response(size_t n)
{
	size_t p = 0, e, t, list, bind;
	const uint8_t *b = snmp_response;
	e = field(b, &p, n, 0x30);
	assert(e == n);
	t = field(b, &p, e, 2);
	assert(t - p == 1 && b[p] == 1);
	p = t;
	t = field(b, &p, e, 4);
	assert(t - p == sizeof(comm) - 1 && !memcmp(b + p, comm, t - p));
	p = t;
	t = field(b, &p, e, 0xa2);
	assert(t == e);
	t = field(b, &p, e, 2);
	assert(t - p == 4 && b[p] == 0x80 && !b[p + 1] && !b[p + 2] && !b[p + 3]);
	p = t;
	t = field(b, &p, e, 2);
	assert(t - p == 1);
	response_error = b[p];
	p = t;
	t = field(b, &p, e, 2);
	assert(t - p == 1);
	response_index = b[p];
	p = t;
	list = field(b, &p, e, 0x30);
	assert(list == e);
	result_count = 0;
	while (p < list) {
		unsigned i = result_count++;
		assert(i < 8);
		bind = field(b, &p, list, 0x30);
		t = field(b, &p, bind, 6);
		assert(t - p < 100);
		name_len[i] = t - p;
		memcpy(names[i], b + p, t - p);
		p = t;
		result_type[i] = b[p++];
		unsigned len = decode_len(b, &p, bind);
		assert(len < 100 && p + len == bind);
		result_len[i] = len;
		memcpy(result_data[i], b + p, len);
		p = bind;
	}
}
static void execute(void)
{
	unsigned n = snmp_process(req, req_len, source);
	assert(n);
	decode_response(n);
}
static void udp_input_test(uint16_t port, uint8_t host)
{
	struct uip_udpip_hdr *h = (void *)(uip_buf + UIP_LLH_LEN);
	memset(uip_buf, 0, sizeof(uip_buf));
	h->vhl = 0x45;
	h->ttl = 64;
	h->proto = UIP_PROTO_UDP;
	h->len[0] = (req_len + 28) >> 8;
	h->len[1] = req_len + 28;
	uint8_t s[4] = {192, 0, 2, host};
	memcpy(h->srcipaddr, s, 4);
	memcpy(h->destipaddr, uip_hostaddr, 4);
	h->srcport = HTONS(port);
	h->destport = HTONS(161);
	h->udplen = HTONS(req_len + 8);
	memcpy(uip_buf + UIP_LLH_LEN + 28, req, req_len);
	uip_len = req_len + 28;
	uip_input();
	CHECK(uip_len > 28 && h->destport == HTONS(port) && !memcmp(h->destipaddr, s, 4) &&
		  h->srcport == HTONS(161) && !memcmp(h->srcipaddr, uip_hostaddr, 4),
	      "UDP response destination and source");
	CHECK(uip_udp_conn && uip_udp_conn->rport == 0 &&
		  !memcmp(uip_udp_conn->ripaddr, "\0\0\0\0", 4),
	      "listener remains wildcard");
}
#ifdef SNMP_TRANSPORT_TEST
int snmp_unit_main(void)
#else
int main(void)
#endif
{
	setvbuf(stdout, NULL, _IONBF, 0);
	uip_init();
	uip_ipaddr(uip_hostaddr, 192, 0, 2, 1);
	snmp_init();
	CHECK(hw_initializations == 1, "hardware quarantine initialized at agent startup");
	query_oid(1, 5, 0);
	make_request(0xa0, 1);
	CHECK(!snmp_process(req, req_len, source), "default deny without community");
	CHECK(!snmp_set_enabled(1), "cannot enable unconfigured agent");
	CHECK(test_snmp_configure(comm, sizeof(comm) - 1, network, acl),
	      "explicit community and CIDR ACL");
	execute();
	CHECK(result_type[0] == 4 && result_len[0] == 10 &&
		  !memcmp(result_data[0], "host-agent", 10),
	      "sysName OCTET STRING");
	for (unsigned col = 4; col <= 6; col += 2) {
		query_oid(1, col, 0);
		make_request(0xa0, 1);
		execute();
		CHECK(result_type[0] == 4 && result_len[0] == 0,
		      "sysContact/sysLocation empty OCTET STRING");
	}
	query_oid(1, 2, 0);
	make_request(0xa0, 1);
	execute();
	CHECK(result_type[0] == 0x80, "sysObjectID pending authoritative upstream PEN");
	query_oid(1, 3, 0);
	make_request(0xa0, 1);
	snmp_uptime = 0x80000000;
	execute();
	CHECK(result_type[0] == 0x43 && result_len[0] == 5 && !result_data[0][0],
	      "TimeTicks unsigned sign padding");
	snmp_uptime = 0xffffffff;
	execute();
	CHECK(result_len[0] == 5 && result_data[0][4] == 255, "TimeTicks rollover boundary");
	snmp_uptime++;
	execute();
	CHECK(result_type[0] == ASN_TIMETICKS && result_len[0] == 1 && !result_data[0][0],
	      "TimeTicks wraps at 2^32 centiseconds");
	EA = false;
	snmp_uptime = 0x80000000UL;
	ticks = 0;
	execute();
	CHECK(!EA && result_len[0] == 5 && result_data[0][1] == 0x80,
	      "atomic uptime read restores disabled interrupts and survives raw tick wrap");
	EA = true;
	query_oid(2, 10, 8);
	make_request(0xa0, 1);
	execute();
	CHECK(result_type[0] == 0x41 && result_len[0] == 5 && !result_data[0][0] &&
		  last_port == 0 && last_selector == 0,
	      "Counter32 native low word and front-panel mapping");
	hw_value++;
	execute();
	CHECK(result_len[0] == 1 && !result_data[0][0],
	      "Counter32 wraps independently of native high word");
	query_oid(31, 6, 8);
	make_request(0xa0, 1);
	execute();
	const uint8_t hc_oid[] = {43, 6, 1, 2, 1, 31, 1, 1, 1, 6, 8};
	CHECK(name_len[0] == sizeof(hc_oid) && !memcmp(names[0], hc_oid, sizeof(hc_oid)),
	      "standard ifXTable.ifXEntry OID");
	CHECK(result_type[0] == 0x46 && result_len[0] == 9 && result_data[0][1] == 0x80 &&
		  result_data[0][4] == 1 && !result_data[0][8],
	      "Counter64 native carry and sign padding");
	for (unsigned c = 6; c <= 13; c++) {
		static const uint8_t selectors[] = {0, 2, 3, 4, 1, 5, 6, 7};
		query_oid(31, c, 8);
		make_request(0xa0, 1);
		execute();
		CHECK(last_selector == selectors[c - 6], "IF-MIB native selector");
	}
	query_oid(2, 5, 8);
	make_request(0xa0, 1);
	execute();
	CHECK(result_type[0] == 0x42 && result_len[0] == 5 && result_data[0][1] == 255,
	      "ifSpeed Gauge32 saturates at 10G");
	query_oid(31, 15, 8);
	make_request(0xa0, 1);
	execute();
	CHECK(result_type[0] == 0x42 && result_len[0] == 2 && result_data[0][0] == 39 &&
		  result_data[0][1] == 16,
	      "ifHighSpeed Gauge32 Mbps");
	measured_speed = 0;
	execute();
	CHECK(result_type[0] == 0x42 && result_len[0] == 1 && !result_data[0][0],
	      "unknown ifHighSpeed remains Gauge32 zero");
	query_oid(2, 5, 8);
	make_request(0xa0, 1);
	execute();
	CHECK(!response_error && result_type[0] == 0x42 && result_len[0] == 1 && !result_data[0][0],
	      "link-down ifSpeed row remains Gauge32 zero");
	query_oid(2, 5, 1);
	make_request(0xa1, 1);
	execute();
	CHECK(!response_error && names[0][8] == 5 && names[0][9] == 2 && result_type[0] == 0x42 &&
		  !result_data[0][0],
	      "GETNEXT retains zero-speed rows in numeric order");
	query_oid(31, 15, machine.max_port + 1);
	make_request(0xa1, 1);
	execute();
	CHECK(result_type[0] == 0x82, "GETNEXT ends after last retained speed row");
	measured_speed = 2500;
	query_oid(2, 5, 8);
	make_request(0xa0, 1);
	execute();
	CHECK(result_type[0] == 0x42 && result_len[0] == 5 && result_data[0][1] == 0x95,
	      "2.5G unsigned Gauge32 sign padding");
	for (unsigned s = 0; s < 3; s++) {
		static const uint16_t speeds[] = {100, 1000, 2500};
		static const uint8_t encoded[][5] = {
		    {5, 0xf5, 0xe1, 0}, {0x3b, 0x9a, 0xca, 0}, {0, 0x95, 2, 0xf9, 0}};
		measured_speed = speeds[s];
		query_oid(2, 5, 8);
		make_request(0xa0, 1);
		execute();
		CHECK(!response_error && result_type[0] == 0x42 &&
			  result_len[0] == (s == 2 ? 5 : 4) &&
			  !memcmp(result_data[0], encoded[s], result_len[0]),
		      "ifSpeed BER Gauge32 100M/1G/2.5G");
	}
	query_oid(2, 8, 8);
	make_request(0xa0, 1);
	execute();
	CHECK(!response_error && result_type[0] == 2 && result_len[0] == 1 &&
		  result_data[0][0] == 1,
	      "ifOperStatus link up INTEGER up(1)");
	link_up = false;
	execute();
	CHECK(!response_error && result_type[0] == 2 && result_len[0] == 1 &&
		  result_data[0][0] == 2,
	      "ifOperStatus link down INTEGER down(2)");
	link_up = true;
	hw_failed = true;
	execute();
	CHECK(response_error == 5 && response_index == 1 && result_type[0] == 5,
	      "ifOperStatus API failure genErr echoes NULL");
	query_oid(2, 5, 8);
	make_request(0xa0, 1);
	execute();
	CHECK(response_error == 5 && response_index == 1 && result_type[0] == 5,
	      "ifSpeed API failure genErr echoes NULL");
	hw_failed = false;
	query_oid(2, 7, 8);
	make_request(0xa0, 1);
	execute();
	CHECK(!response_error && result_type[0] == 0x80,
	      "ifAdminStatus explicitly unsupported, never inferred from link");
	for (unsigned col = 4; col <= 6; col += 2) {
		query_oid(2, col, 8);
		make_request(0xa0, 1);
		execute();
		CHECK(!response_error && result_type[0] == 0x80,
		      "ifMtu/ifPhysAddress unsupported measurements");
	}
	query_oid(2, 3, 8);
	make_request(0xa0, 1);
	execute();
	CHECK(result_type[0] == 2 && result_data[0][0] == 6, "ifType ethernetCsmacd INTEGER");
	measured_speed = 10000;
	unsigned before_invalid = hw_calls;
	for (unsigned col = 5; col <= 8; col += 3) {
		query_oid(2, col, 10);
		make_request(0xa0, 1);
		execute();
		CHECK(!response_error && result_type[0] == 0x81,
		      "invalid ifSpeed/ifOperStatus row noSuchInstance");
	}
	CHECK(hw_calls == before_invalid, "invalid interface indexes never access hardware");
	query_oid(2, 1, machine.max_port + 2);
	make_request(0xa0, 1);
	execute();
	CHECK(result_type[0] == 0x81, "CPU not enumerated as max_port+1");
	query_oid(2, 4, 1);
	make_request(0xa0, 1);
	execute();
	CHECK(result_type[0] == 0x80, "unsupported measurement noSuchObject");
	query_oid(1, 5, 1);
	make_request(0xa0, 1);
	execute();
	CHECK(result_type[0] == 0x81, "scalar noSuchInstance");
	query_oid(31, 15, machine.max_port + 1);
	make_request(0xa1, 1);
	execute();
	CHECK(result_type[0] == 0x82, "endOfMibView preserves requested OID");
	CHECK(name_len[0] == input_oid_len && !memcmp(names[0], input_oid, input_oid_len),
	      "exception original OID");
	query_oid(1, 1, 0);
	make_request(0xa1, 1);
	execute();
	CHECK(names[0][6] == 3, "GETNEXT scalar ordering");
	query_oid(2, 1, machine.max_port + 1);
	make_request(0xa1, 1);
	execute();
	CHECK(names[0][8] == 2 && names[0][9] == 1, "GETNEXT column ordering");
	input_oid[0] = 43;
	input_oid[1] = 6;
	input_oid[2] = 1;
	input_oid[3] = 2;
	input_oid[4] = 1;
	input_oid[5] = 2;
	input_oid[6] = 2;
	input_oid[7] = 1;
	input_oid[8] = 0x81;
	input_oid[9] = 0;
	input_oid[10] = 1;
	input_oid_len = 11;
	make_request(0xa1, 1);
	execute();
	CHECK(names[0][5] == 31, "OID numeric rather than BER byte ordering");
	query_oid(1, 5, 0);
	make_request(0xa0, 1);
	query_oid(2, 2, 8);
	make_request(0xa0, 1);
	execute();
	CHECK(result_type[0] == 4 && result_len[0] == 5 && !memcmp(result_data[0], "SFP 8", 5),
	      "SFP is a physical interface, CPU excluded");
	query_oid(2, 0, 0);
	make_request(0xa0, 1);
	execute();
	CHECK(result_type[0] == 2 && result_data[0][0] == machine.max_port + 1,
	      "actual interface count for eight/nine port profile");
	query_oid(1, 5, 0);
	make_request(0xa0, 8);
	execute();
	CHECK(result_count == 8, "eight bounded varbinds and long-form BER lengths");
	make_request(0xa0, 9);
	CHECK(!snmp_process(req, req_len, source), "excessive varbind count");
	make_request(0xa0, 0);
	execute();
	CHECK(!result_count, "empty request list");
	make_request(0xa0, 1);
	req[req_len - 2] = 4;
	execute();
	CHECK(result_type[0] == 4, "valid non-NULL request value ignored");
	input_oid[0] = 0x90;
	input_oid[1] = 0x80;
	input_oid[2] = 0x80;
	input_oid[3] = 0x80;
	input_oid[4] = 0x4f;
	input_oid_len = 5;
	make_request(0xa0, 1);
	execute();
	CHECK(result_type[0] == 0x80, "maximum second OID arc with root two");
	req[req_len - 3] = 0x50;
	CHECK(!snmp_process(req, req_len, source), "combined first OID subidentifier overflow");
	input_oid_len = 33;
	input_oid[0] = 43;
	memset(input_oid + 1, 0, 32);
	make_request(0xa0, 1);
	CHECK(!snmp_process(req, req_len, source), "OID arc cap before hardware access");
	query_oid(1, 5, 0);
	make_request(0xa0, 1);
	unsigned before = hw_calls;
	req[9] ^= 1;
	CHECK(!snmp_process(req, req_len, source), "wrong community silent drop");
	req[9] ^= 1;
	source[2] = 3;
	CHECK(!snmp_process(req, req_len, source), "source IP denied");
	source[2] = 2;
	source[3] = 255;
	CHECK(!snmp_process(req, req_len, source), "broadcast source rejected");
	source[3] = 0;
	CHECK(!snmp_process(req, req_len, source), "subnet source rejected");
	source[3] = 7;
	CHECK(before == hw_calls, "denied requests do not read hardware");
	make_request(0xa3, 1);
	CHECK(!snmp_process(req, req_len, source), "SET denied");
	make_request(0xa5, 1);
	CHECK(!snmp_process(req, req_len, source), "GETBULK deferred");
	make_request(0xa0, 1);
	for (size_t n = 0; n < req_len; n++) {
		uint8_t *b = malloc(n ? n : 1);
		memcpy(b, req, n);
		assert(!snmp_process(b, n, source));
		free(b);
	}
	CHECK(1, "all truncations with exact-size allocations");
	req[1] = 0x80;
	CHECK(!snmp_process(req, req_len, source), "indefinite BER rejected");
	make_request(0xa0, 1);
	req[req_len - 3] = 0x80;
	CHECK(!snmp_process(req, req_len, source), "unterminated OID rejected");
	make_request(0xa0, 1);
	req[1] = 255;
	CHECK(!snmp_process(req, req_len, source), "length overflow rejected");
	uint32_t rng = 1;
	for (unsigned round = 0; round < 30000; round++) {
		size_t n = round % 485;
		uint8_t *b = malloc(n ? n : 1);
		for (size_t j = 0; j < n; j++) {
			rng = rng * 1664525u + 1013904223u;
			b[j] = rng >> 24;
		}
		snmp_process(b, n, source);
		free(b);
	}
	CHECK(1, "30000 malformed packets under sanitizers");
	query_oid(2, 10, 1);
	make_request(0xa0, 1);
	hw_failed = true;
	execute();
	CHECK(response_error == 5 && response_index == 1 && result_type[0] == 5,
	      "hardware failure genErr with original binding");
	hw_failed = false;
	query_oid(1, 5, 0);
	make_request(0xa0, 1);
	udp_input_test(40001, 7);
	udp_input_test(50123, 8);
	udp_input_test(40001, 7);
	struct uip_udpip_hdr *h = (void *)(uip_buf + UIP_LLH_LEN);
	h->udplen = HTONS(7);
	uip_len = 28;
	h->len[0] = 0;
	h->len[1] = 28;
	uip_input();
	CHECK(!uip_len, "malformed UDP length dropped");
	/* Control does not reload credentials, allocate sockets or reset uptime. */
	uint8_t toggle_before[10], toggle_after[10];
	snmp_get_status(toggle_before);
	uint32_t toggle_uptime = snmp_uptime;
	unsigned toggle_hw = hw_calls;
	CHECK(snmp_set_enabled(0), "disable configured agent");
	snmp_get_status(toggle_after);
	CHECK(!toggle_after[0] && !memcmp(toggle_before + 1, toggle_after + 1, 9),
	      "disable preserves credential presence and ACL");
	query_oid(1, 5, 0);
	make_request(0xa0, 1);
	CHECK(!snmp_process(req, req_len, source) && hw_calls == toggle_hw,
	      "disabled GET drops before hardware access");
	make_request(0xa1, 1);
	CHECK(!snmp_process(req, req_len, source), "disabled GETNEXT drops");
	CHECK(!snmp_set_enabled(2), "invalid enable value rejected");
	CHECK(snmp_set_enabled(1) && snmp_uptime == toggle_uptime, "reenable preserves uptime");
	snmp_get_status(toggle_after);
	CHECK(!memcmp(toggle_before, toggle_after, 10),
	      "reenable restores actual state without configuration reload");
	make_request(0xa0, 1);
	execute();
	CHECK(!response_error, "GET works after reenable");
	uint8_t denied_source[4] = {192, 0, 3, 7};
	CHECK(!snmp_process(req, req_len, denied_source), "ACL preserved after toggle");
	req[9] ^= 1;
	CHECK(!snmp_process(req, req_len, source),
	      "community authentication preserved after toggle");
	req[9] ^= 1;
	/* Walk the complete implemented MIB while interleaving status reads. */
	input_oid[0] = 43;
	input_oid[1] = 6;
	input_oid[2] = 1;
	input_oid[3] = 2;
	input_oid[4] = 1;
	input_oid_len = 5;
	uint8_t status_before[10], status_after[10];
	snmp_get_status(status_before);
	unsigned walked = 0;
	for (; walked < 200; walked++) {
		make_request(0xa1, 1);
		execute();
		assert(!response_error);
		snmp_get_status(status_after);
		assert(!memcmp(status_before, status_after, 10));
		if (result_type[0] == 0x82)
			break;
		memcpy(input_oid, names[0], name_len[0]);
		input_oid_len = name_len[0];
	}
	CHECK(walked == 6u + 19u * (machine.max_port + 1u) && result_type[0] == 0x82,
	      "complete MIB walk and endOfMibView unchanged by status reads");
	/* Exercise the production CIDR parser across every allowed prefix. */
	for (unsigned prefix = 1; prefix <= 32; ++prefix) {
		char cidr[19];
		snprintf(cidr, sizeof(cidr), "192.0.2.1/%u", prefix);
		CHECK(snmp_set_manager((const uint8_t *)cidr, strlen(cidr)),
		      "every CIDR prefix accepted through runtime parser");
	}
	CHECK(!snmp_set_manager((const uint8_t *)"192.0.2.1/0", 11), "unrestricted ACL rejected");
	snmp_listener = 0;
	memset(uip_udp_conns, 0xff, sizeof(uip_udp_conns));
	CHECK(!snmp_set_community((const uint8_t *)"socket-fail", 11),
	      "socket exhaustion fails closed");
	return failures ? 1 : 0;
}
