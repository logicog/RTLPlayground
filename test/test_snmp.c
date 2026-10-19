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
char port_names[9][PORT_NAME_SIZE];
static uint8_t admin_state = 1;
extern volatile uint32_t snmp_uptime;
static unsigned hw_calls;
static bool hw_failed;
static unsigned hw_fail_at;
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
	return !hw_failed && (!hw_fail_at || hw_calls != hw_fail_at);
}
bool snmp_hw_admin(uint8_t port, uint8_t *state)
{
	hw_calls++;
	last_port = port;
	*state = admin_state;
	return !hw_failed;
}
bool snmp_hw_counter32(uint8_t port, uint8_t selector, bool high, struct snmp_counter *v)
{
	uint32_t sample = high ? 0x80000005UL : 0x11223344UL;
	hw_calls++;
	last_port = port;
	last_selector = selector;
	memset(v->bytes, 0, 4);
	for (unsigned i = 0; i < 4; i++)
		v->bytes[i + 4] = sample >> ((3 - i) * 8);
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
	return !hw_failed && (!hw_fail_at || hw_calls != hw_fail_at);
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
#ifndef SNMP_PDU_GETBULK
#define SNMP_PDU_GETBULK 0xa5
#endif
/* A 484-byte response can hold more than SNMP_MAX_VARBINDS short GETBULK varbinds. */
#define MAX_RESULTS 80
static uint8_t names[MAX_RESULTS][100], name_len[MAX_RESULTS], result_type[MAX_RESULTS],
    result_data[MAX_RESULTS][100], result_len[MAX_RESULTS];
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
		assert(i < MAX_RESULTS);
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
/* Independent BER writer for requests the fixed-shape make_request() cannot express. */
static size_t hdr(size_t n)
{
	return n < 128 ? 2 : n < 256 ? 3 : 4;
}
static size_t put_tlv(uint8_t *d, uint8_t tag, const uint8_t *c, size_t n)
{
	size_t p = 0;
	d[p++] = tag;
	if (n >= 256) {
		d[p++] = 0x82;
		d[p++] = n >> 8;
	} else if (n >= 128)
		d[p++] = 0x81;
	d[p++] = n;
	memcpy(d + p, c, n);
	return p + n;
}
static size_t put_int(uint8_t *d, int32_t v)
{
	uint8_t b[4] = {(uint32_t)v >> 24, (uint32_t)v >> 16, (uint32_t)v >> 8, (uint32_t)v};
	unsigned s = 0;
	while (s < 3 && ((!b[s] && !(b[s + 1] & 128)) || (b[s] == 255 && (b[s + 1] & 128))))
		s++;
	return put_tlv(d, ASN_INTEGER, b + s, 4 - s);
}
static uint8_t bulk_oid[8][12], bulk_oid_len[8];
static unsigned bulk_count;
static size_t bulk_comm, bulk_pdu, bulk_first;
static void bulk_add_oid(const uint8_t *o, uint8_t n)
{
	assert(bulk_count < 8 && n <= 12);
	memcpy(bulk_oid[bulk_count], o, n);
	bulk_oid_len[bulk_count++] = n;
}
static void bulk_add(uint8_t group, uint8_t column, uint8_t index)
{
	query_oid(group, column, index);
	bulk_add_oid(input_oid, input_oid_len);
}
/* Request with NULL bindings from bulk_oid[]; the two INTEGERs are non-repeaters/max-repetitions
 * for GETBULK and ignored error fields for other PDUs. Records offsets for mutation tests. */
static void make_bulk(uint8_t pdu_type, int32_t nonrep, int32_t maxrep)
{
	uint8_t list[256], pdu[256], outer[300];
	size_t n = 0, ints, m, o, ohdr;
	for (unsigned i = 0; i < bulk_count; i++) {
		uint8_t vb[32];
		size_t v = put_tlv(vb, ASN_OBJECT_ID, bulk_oid[i], bulk_oid_len[i]);
		vb[v++] = ASN_NULL;
		vb[v++] = 0;
		n += put_tlv(list + n, ASN_SEQUENCE, vb, v);
	}
	m = put_int(pdu, INT32_MIN);
	m += put_int(pdu + m, nonrep);
	m += put_int(pdu + m, maxrep);
	ints = m;
	m += put_tlv(pdu + m, ASN_SEQUENCE, list, n);
	o = put_int(outer, SNMP_VERSION_2C);
	o += put_tlv(outer + o, ASN_OCTET_STRING, comm, sizeof(comm) - 1);
	ohdr = hdr(o + hdr(m) + m);
	bulk_comm = ohdr + 3 + 2;
	bulk_pdu = bulk_comm + sizeof(comm) - 1;
	bulk_first = bulk_pdu + hdr(m) + ints + hdr(n);
	o += put_tlv(outer + o, pdu_type, pdu, m);
	req_len = put_tlv(req, ASN_SEQUENCE, outer, o);
	assert(req_len <= sizeof(req) && ohdr == req_len - o);
}
/* Grow the three enclosing long-form lengths of make_request() around extra list bytes. */
static void append_list(const uint8_t *t, size_t n)
{
	const size_t offsets[] = {1, 9 + sizeof(comm), 25 + sizeof(comm)};
	assert(req_len + n <= sizeof(req));
	for (unsigned i = 0; i < 3; i++) {
		size_t o = offsets[i], v;
		assert(req[o] == 0x82);
		v = ((size_t)req[o + 1] << 8 | req[o + 2]) + n;
		req[o + 1] = v >> 8;
		req[o + 2] = v;
	}
	memcpy(req + req_len, t, n);
	req_len += n;
}
/* Whole-MIB reference built from single GETNEXT requests. */
#define REF_MAX 300
static uint8_t ref_name[REF_MAX][12], ref_name_len[REF_MAX], ref_type[REF_MAX],
    ref_val[REF_MAX][96], ref_val_len[REF_MAX];
static unsigned ref_count;
static void build_reference(void)
{
	input_oid[0] = 43;
	input_oid[1] = 6;
	input_oid[2] = 1;
	input_oid[3] = 2;
	input_oid[4] = 1;
	input_oid_len = 5;
	for (ref_count = 0;;) {
		make_request(SNMP_PDU_GETNEXT, 1);
		execute();
		assert(!response_error);
		if (result_type[0] == SNMP_END_OF_MIB)
			break;
		assert(ref_count < REF_MAX && name_len[0] <= 12 && result_len[0] < 96);
		memcpy(ref_name[ref_count], names[0], name_len[0]);
		ref_name_len[ref_count] = name_len[0];
		ref_type[ref_count] = result_type[0];
		memcpy(ref_val[ref_count], result_data[0], result_len[0]);
		ref_val_len[ref_count] = result_len[0];
		memcpy(input_oid, names[0], name_len[0]);
		input_oid_len = name_len[0];
		ref_count++;
	}
}
static int ref_find(uint8_t group, uint8_t column, uint8_t index)
{
	query_oid(group, column, index);
	for (unsigned i = 0; i < ref_count; i++)
		if (ref_name_len[i] == input_oid_len && !memcmp(ref_name[i], input_oid, input_oid_len))
			return i;
	assert(!"reference row exists");
	return -1;
}
/* Start of a chain: a reference row, or -1 for the 1.3.6.1.2.1 prefix before the first row. */
static void bulk_add_ref(int row)
{
	static const uint8_t root[] = {43, 6, 1, 2, 1};
	if (row < 0)
		bulk_add_oid(root, sizeof(root));
	else
		bulk_add_oid(ref_name[row], ref_name_len[row]);
}
static bool row_is(unsigned i, unsigned r)
{
	return name_len[i] == ref_name_len[r] && !memcmp(names[i], ref_name[r], name_len[i]) &&
	       result_type[i] == ref_type[r] && result_len[i] == ref_val_len[r] &&
	       !memcmp(result_data[i], ref_val[r], result_len[i]);
}
/* Step number `row`+1 from `start`: a later MIB row, else endOfMibView echoing the last row. */
static bool expect_elem(unsigned i, int start, unsigned row)
{
	unsigned idx = start + 1 + row;
	if (idx < ref_count)
		return row_is(i, idx);
	return result_type[i] == SNMP_END_OF_MIB && !result_len[i] &&
	       name_len[i] == ref_name_len[ref_count - 1] &&
	       !memcmp(names[i], ref_name[ref_count - 1], name_len[i]);
}
/* Every decoded varbind equals the RFC 3416 ordering: N GETNEXTs, then row-major repeaters. */
static bool bulk_prefix(unsigned nonrep, unsigned vars, const int *start)
{
	unsigned reps = vars - nonrep;
	if (result_count > nonrep && !reps)
		return false;
	for (unsigned i = 0; i < result_count; i++) {
		unsigned k = i - nonrep;
		if (i < nonrep ? !expect_elem(i, start[i], 0)
			       : !expect_elem(i, start[nonrep + k % reps], k / reps))
			return false;
	}
	return true;
}
static size_t vb_size(size_t name, size_t value)
{
	size_t inner = hdr(name) + name + hdr(value) + value;
	return hdr(inner) + inner;
}
static size_t message_size(size_t list)
{
	size_t pdu = 6 + 3 + 3 + hdr(list) + list;
	size_t outer = 3 + hdr(sizeof(comm) - 1) + sizeof(comm) - 1 + hdr(pdu) + pdu;
	return hdr(outer) + outer;
}
static size_t decoded_list_size(void)
{
	size_t n = 0;
	for (unsigned i = 0; i < result_count; i++)
		n += vb_size(name_len[i], result_len[i]);
	return n;
}
/* Size of the next ordered varbind a longer response would have contained. */
static size_t next_vb_size(unsigned nonrep, unsigned vars, const int *start)
{
	unsigned j = result_count, k = j - nonrep, reps = vars - nonrep;
	int s = j < nonrep ? start[j] : start[nonrep + k % reps];
	unsigned idx = s + 1 + (j < nonrep ? 0 : k / reps);
	if (idx < ref_count)
		return vb_size(ref_name_len[idx], ref_val_len[idx]);
	return vb_size(ref_name_len[ref_count - 1], 0);
}
static size_t response_len;
static bool bulk_exec(int32_t nonrep, int32_t maxrep, unsigned *hw)
{
	unsigned before = hw_calls;
	make_bulk(SNMP_PDU_GETBULK, nonrep, maxrep);
	response_len = snmp_process(req, req_len, source);
	*hw = hw_calls - before;
	assert(response_len <= SNMP_MAX_MESSAGE);
	if (response_len)
		decode_response(response_len);
	return response_len != 0;
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
	query_oid(31, 18, machine.max_port + 1);
	make_request(0xa1, 1);
	execute();
	CHECK(result_type[0] == 0x82, "GETNEXT ends after last interface alias");
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
	CHECK(!response_error && result_type[0] == ASN_INTEGER && result_data[0][0] == 1,
	      "ifAdminStatus is administratively up");
	link_up = false;
	execute();
	CHECK(!response_error && result_type[0] == ASN_INTEGER && result_len[0] == 1 &&
	      result_data[0][0] == 1, "a down link does not imply admin down");
	admin_state = 2;
	execute();
	CHECK(!response_error && result_type[0] == ASN_INTEGER && result_len[0] == 1 &&
	      result_data[0][0] == 2 && last_port == machine.min_port,
	      "ifAdminStatus follows the mapped port power state");
	hw_failed = true;
	execute();
	CHECK(response_error == SNMP_ERR_GEN_ERR && response_index == 1,
	      "admin hardware failure returns genErr");
	hw_failed = false;
	admin_state = 1;
	link_up = true;
	strcpy(port_names[machine.min_port], "uplink");
	query_oid(31, 18, 8);
	make_request(0xa0, 1);
	execute();
	CHECK(result_type[0] == ASN_OCTET_STRING && result_len[0] == 6 &&
	      !memcmp(result_data[0], "uplink", 6), "ifAlias follows logical-to-front-panel mapping");
	memset(port_names[machine.min_port], 'z', PORT_NAME_SIZE - 1);
	port_names[machine.min_port][PORT_NAME_SIZE - 1] = 0;
	execute();
	CHECK(result_len[0] == PORT_NAME_SIZE - 1, "ifAlias retains the full supported name");
	memset(port_names, 0, sizeof port_names);
	execute();
	CHECK(!response_error && result_type[0] == ASN_OCTET_STRING && result_len[0] == 0,
	      "unnamed port has an empty alias");
	for (unsigned col = 14; col <= 20; col += 6) {
		static const uint8_t rx[] = {0, 0x80, 0, 0, 5}, tx[] = {0x11, 0x22, 0x33, 0x44};
		query_oid(2, col, 8);
		make_request(0xa0, 1);
		execute();
		CHECK(result_type[0] == ASN_COUNTER32 && last_selector == 48 &&
		      result_len[0] == (col == 14 ? sizeof rx : sizeof tx) &&
		      !memcmp(result_data[0], col == 14 ? rx : tx, result_len[0]),
		      "ifErrors uses the correct 32-bit snapshot half with unsigned BER padding");
		hw_failed = true;
		execute();
		CHECK(response_error == SNMP_ERR_GEN_ERR, "error counter failure is not fabricated zero");
		hw_failed = false;
	}
	for (unsigned col = 2; col <= 5; col++) {
		query_oid(31, col, 8);
		make_request(0xa0, 1);
		execute();
		CHECK(result_type[0] == ASN_COUNTER32 &&
		      last_selector == (col < 4 ? col + 1 : col + 2),
		      "32-bit multicast and broadcast counters use native selectors");
	}
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
	query_oid(31, 18, machine.max_port + 1);
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
	for (unsigned pdu_type = SNMP_PDU_GET; pdu_type <= SNMP_PDU_GETNEXT; pdu_type++)
		for (unsigned n = 9; n <= 30; n += 21) {
			make_request(pdu_type, n);
			unsigned len = snmp_process(req, req_len, source);
			if (len)
				decode_response(len);
			CHECK(len && response_error == SNMP_ERR_TOO_BIG && !response_index &&
				  !result_count,
			      "authenticated well-formed oversize request returns empty tooBig");
		}
	make_request(0xa0, 9);
	req[9] ^= 1;
	CHECK(!snmp_process(req, req_len, source), "wrong-community oversize request dropped");
	req[9] ^= 1;
	source[2] = 3;
	CHECK(!snmp_process(req, req_len, source), "denied-source oversize request dropped");
	source[2] = 2;
	{
		/* Malformed content after eight bindings is not an oversize list: silent drop. */
		static const uint8_t stray[] = {0};
		static const uint8_t no_value[] = {0x30, 4, 6, 2, 43, 6};
		static const uint8_t unterminated[] = {0x30, 6, 6, 2, 43, 0x80, 5, 0};
		static const uint8_t bad_value[] = {0x30, 6, 6, 2, 43, 6, 0x50, 0};
		static const uint8_t truncated[] = {0x30, 0x20};
		static const struct {
			const uint8_t *data;
			size_t len;
		} tails[] = {{stray, sizeof(stray)},
			     {no_value, sizeof(no_value)},
			     {unterminated, sizeof(unterminated)},
			     {bad_value, sizeof(bad_value)},
			     {truncated, sizeof(truncated)}};
		bool dropped = true;
		for (unsigned pdu_type = SNMP_PDU_GET; pdu_type <= SNMP_PDU_GETNEXT; pdu_type++)
			for (unsigned t = 0; t < sizeof(tails) / sizeof(*tails); t++) {
				make_request(pdu_type, SNMP_MAX_VARBINDS);
				append_list(tails[t].data, tails[t].len);
				dropped &= !snmp_process(req, req_len, source);
			}
		CHECK(dropped, "malformed TLV after eight varbinds dropped, not tooBig");
	}
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
	{
		/* Denied GETBULK stays silent and never reaches hardware. */
		unsigned before_denied = hw_calls;
		bulk_count = 0;
		bulk_add(2, 10, 1);
		make_bulk(SNMP_PDU_GETBULK, 0, 1);
		req[bulk_comm] ^= 1;
		CHECK(!snmp_process(req, req_len, source), "GETBULK wrong community dropped");
		req[bulk_comm] ^= 1;
		source[2] = 3;
		CHECK(!snmp_process(req, req_len, source), "GETBULK source ACL enforced");
		source[2] = 2;
		CHECK(hw_calls == before_denied, "denied GETBULK does not read hardware");
	}
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
	/* GETBULK through the real UDP callback is truncated to the 484-byte payload bound. */
	bulk_count = 0;
	bulk_add_oid((const uint8_t[]){43, 6, 1, 2, 1}, 5);
	make_bulk(SNMP_PDU_GETBULK, 0, INT32_MAX);
	udp_input_test(40001, 7);
	CHECK(uip_len - 28 <= SNMP_MAX_MESSAGE && uip_len - 28 > 400 &&
		  !memcmp(uip_buf + UIP_LLH_LEN + 28, snmp_response, uip_len - 28),
	      "UDP GETBULK response truncated within SNMP_MAX_MESSAGE");
	query_oid(1, 5, 0);
	make_request(0xa0, 1);
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
	for (; walked < REF_MAX; walked++) {
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
	CHECK(walked == 6u + 27u * (machine.max_port + 1u) && result_type[0] == 0x82,
	      "complete MIB walk and endOfMibView unchanged by status reads");
	/* RFC 3416 GETBULK against the actual MIB order. */
	build_reference();
	CHECK(ref_count == 6u + 27u * (machine.max_port + 1u), "GETBULK reference covers whole MIB");
	{
		int s[3];
		unsigned hw;
		bool ok;
		s[0] = ref_find(1, 1, 0);
		s[1] = ref_find(2, 1, 1);
		s[2] = ref_find(2, 3, 1);
		bulk_count = 0;
		for (unsigned i = 0; i < 3; i++)
			bulk_add_ref(s[i]);
		ok = bulk_exec(1, 2, &hw);
		CHECK(ok && !response_error && !response_index && result_count == 5 &&
			  bulk_prefix(1, 3, s) && hw <= result_count,
		      "GETBULK non-repeater then two repeater columns, row-major");
		ok = bulk_exec(INT32_MAX, INT32_MAX, &hw);
		CHECK(ok && !response_error && result_count == 3 && bulk_prefix(3, 3, s) &&
			  hw <= result_count,
		      "GETBULK non-repeaters clamp to varbind count, no repetitions");
		ok = bulk_exec(0, 0, &hw);
		CHECK(ok && !response_error && !response_index && !result_count && !hw,
		      "GETBULK zero max-repetitions returns empty list");
		ok = bulk_exec(1, 0, &hw);
		CHECK(ok && !response_error && result_count == 1 && bulk_prefix(1, 3, s) &&
			  hw <= 1,
		      "GETBULK zero max-repetitions keeps only non-repeaters");
		ok = bulk_exec(-1, -1, &hw);
		CHECK(ok && !response_error && !result_count && !hw,
		      "GETBULK negative fields treated as zero");
		ok = bulk_exec(INT32_MIN, INT32_MIN, &hw);
		CHECK(ok && !response_error && !result_count && !hw,
		      "GETBULK INT32_MIN fields treated as zero");
		ok = bulk_exec(0, -1, &hw);
		CHECK(ok && !response_error && !result_count && !hw,
		      "GETBULK negative max-repetitions with no non-repeaters");
		ok = bulk_exec(-3, 2, &hw);
		CHECK(ok && !response_error && result_count == 6 && bulk_prefix(0, 3, s) &&
			  hw <= result_count,
		      "GETBULK negative non-repeaters treated as zero");
		ok = bulk_exec(2, INT32_MIN, &hw);
		CHECK(ok && !response_error && result_count == 2 && bulk_prefix(2, 3, s) &&
			  hw <= result_count,
		      "GETBULK negative max-repetitions keeps non-repeaters");
	}
	{
		int s[2];
		unsigned hw;
		bool ok;
		s[0] = -1;
		bulk_count = 0;
		bulk_add_ref(s[0]);
		ok = bulk_exec(0, SNMP_MAX_VARBINDS + 1, &hw);
		CHECK(ok && !response_error && result_count == SNMP_MAX_VARBINDS + 1 &&
			  bulk_prefix(0, 1, s) && hw <= result_count,
		      "GETBULK repetitions are not limited to eight varbinds");
		ok = bulk_exec(0, INT32_MAX, &hw);
		CHECK(ok && !response_error && !response_index &&
			  result_count > SNMP_MAX_VARBINDS + 1 && result_count < ref_count &&
			  bulk_prefix(0, 1, s) && response_len <= SNMP_MAX_MESSAGE &&
			  response_len == message_size(decoded_list_size()) &&
			  message_size(decoded_list_size() + next_vb_size(0, 1, s)) >
			      SNMP_MAX_MESSAGE &&
			  hw <= result_count,
		      "GETBULK huge repetitions: maximal ordered prefix within 484 bytes");
		ok = bulk_exec(0, 300, &hw);
		CHECK(ok && !response_error && result_count > SNMP_MAX_VARBINDS + 1 &&
			  result_count < 300 && bulk_prefix(0, 1, s) &&
			  response_len == message_size(decoded_list_size()) && hw <= result_count,
		      "GETBULK two-byte max-repetitions truncates to ordered prefix");
		s[0] = ref_find(2, 1, 1);
		s[1] = ref_find(2, 3, 1);
		bulk_count = 0;
		bulk_add_ref(s[0]);
		bulk_add_ref(s[1]);
		ok = bulk_exec(0, INT32_MAX, &hw);
		CHECK(ok && !response_error && result_count > SNMP_MAX_VARBINDS + 1 &&
			  bulk_prefix(0, 2, s) &&
			  response_len == message_size(decoded_list_size()) &&
			  message_size(decoded_list_size() + next_vb_size(0, 2, s)) >
			      SNMP_MAX_MESSAGE &&
			  hw <= result_count,
		      "GETBULK huge repetitions keep row-major order across repeaters");
		s[0] = ref_find(2, 3, 6);
		bulk_count = 0;
		bulk_add_ref(s[0]);
		ok = bulk_exec(0, INT32_MAX, &hw);
		CHECK(ok && !response_error && result_count > SNMP_MAX_VARBINDS + 1 &&
			  bulk_prefix(0, 1, s) &&
			  response_len == message_size(decoded_list_size()) &&
			  message_size(decoded_list_size() + next_vb_size(0, 1, s)) >
			      SNMP_MAX_MESSAGE &&
			  hw && hw <= result_count + 1,
		      "GETBULK hardware work is bounded by output plus one speculative candidate");
		/* One repeater at the end reports endOfMibView per row; the other continues. */
		s[0] = (int)ref_count - 1;
		s[1] = ref_find(1, 1, 0);
		bulk_count = 0;
		bulk_add_ref(s[0]);
		bulk_add_ref(s[1]);
		ok = bulk_exec(0, 3, &hw);
		CHECK(ok && !response_error && result_count == 6 && bulk_prefix(0, 2, s) &&
			  result_type[0] == SNMP_END_OF_MIB && result_type[2] == SNMP_END_OF_MIB &&
			  result_type[4] == SNMP_END_OF_MIB && hw <= result_count,
		      "GETBULK endOfMibView per repeater while others continue");
		ok = bulk_exec(1, 2, &hw);
		CHECK(ok && !response_error && result_count == 3 && bulk_prefix(1, 2, s) &&
			  result_type[0] == SNMP_END_OF_MIB && hw <= result_count,
		      "GETBULK endOfMibView non-repeater and live repeater");
		/* All repeaters exhausted: stopping after the first all-exception row is allowed. */
		s[1] = (int)ref_count - 2;
		bulk_count = 0;
		bulk_add_ref(s[0]);
		bulk_add_ref(s[1]);
		ok = bulk_exec(0, 4, &hw);
		CHECK(ok && !response_error && result_count >= 2 && result_count <= 8 &&
			  !(result_count & 1) && bulk_prefix(0, 2, s) && hw <= result_count &&
			  result_type[0] == SNMP_END_OF_MIB && result_type[1] != SNMP_END_OF_MIB,
		      "GETBULK all repeaters endOfMibView bounded by max-repetitions");
	}
	{
		/* A later repetition fails after output has already been byte-reversed. */
		static const uint8_t columns[] = {5, 10};
		for (unsigned c = 0; c < sizeof columns; c++) {
			uint8_t list[128], vb[32], pd[192], outer[256];
			static const uint8_t first_value[] = {'a', 'b'}, second_value[] = {7};
			size_t n = 0, m, o, v;
			bulk_count = 0;
			bulk_add(2, 2, 0);
			bulk_add(2, columns[c], 0);
			for (unsigned i = 0; i < 2; i++) {
				v = put_tlv(vb, ASN_OBJECT_ID, bulk_oid[i], bulk_oid_len[i]);
				v += put_tlv(vb + v, i ? ASN_INTEGER : ASN_OCTET_STRING,
					     i ? second_value : first_value, i ? 1 : 2);
				n += put_tlv(list + n, ASN_SEQUENCE, vb, v);
			}
			m = put_int(pd, INT32_MIN);
			m += put_int(pd + m, 0);
			m += put_int(pd + m, 3);
			m += put_tlv(pd + m, ASN_SEQUENCE, list, n);
			o = put_int(outer, SNMP_VERSION_2C);
			o += put_tlv(outer + o, ASN_OCTET_STRING, comm, sizeof(comm) - 1);
			o += put_tlv(outer + o, SNMP_PDU_GETBULK, pd, m);
			req_len = put_tlv(req, ASN_SEQUENCE, outer, o);
			hw_fail_at = hw_calls + 2;
			execute();
			hw_fail_at = 0;
			CHECK(response_error == SNMP_ERR_GEN_ERR && response_index == 2 &&
			      result_count == 2 && name_len[0] == bulk_oid_len[0] &&
			      name_len[1] == bulk_oid_len[1] &&
			      !memcmp(names[0], bulk_oid[0], name_len[0]) &&
			      !memcmp(names[1], bulk_oid[1], name_len[1]) &&
			      result_type[0] == ASN_OCTET_STRING && result_len[0] == 2 &&
			      !memcmp(result_data[0], first_value, 2) &&
			      result_type[1] == ASN_INTEGER && result_len[1] == 1 &&
			      result_data[1][0] == 7,
			      "GETBULK later hardware failure restores original list and input error-index");
		}
	}
	{
		/* A child TLV may not extend past its immediate parent even if the packet has room. */
		static const uint8_t types[] = {SNMP_PDU_GET, SNMP_PDU_GETNEXT, SNMP_PDU_GETBULK};
		static const char *const labels[] = {
		    "GET child TLV bounds checked against immediate parent",
		    "GETNEXT child TLV bounds checked against immediate parent",
		    "GETBULK child TLV bounds checked against immediate parent"};
		for (unsigned t = 0; t < 3; t++) {
			bool ok;
			bulk_count = 0;
			bulk_add(1, 5, 0);
			bulk_add(1, 1, 0);
			make_bulk(types[t], 0, 1);
			ok = snmp_process(req, req_len, source) != 0;
			/* OID overruns its varbind into the next one. */
			make_bulk(types[t], 0, 1);
			req[bulk_first + 3] += 3;
			ok &= !snmp_process(req, req_len, source);
			/* Value claims bytes owned by the next varbind. */
			make_bulk(types[t], 0, 1);
			req[bulk_first + 4 + bulk_oid_len[0]] = ASN_OCTET_STRING;
			req[bulk_first + 5 + bulk_oid_len[0]] = 6;
			ok &= !snmp_process(req, req_len, source);
			/* Bytes after the outer SEQUENCE. */
			make_bulk(types[t], 0, 1);
			req[req_len++] = 0;
			ok &= !snmp_process(req, req_len, source);
			/* PDU and outer both grown over a trailing byte the varbind list does not cover. */
			make_bulk(types[t], 0, 1);
			assert(req[1] < 127 && req[bulk_pdu + 1] < 127);
			req[1]++;
			req[bulk_pdu + 1]++;
			req[req_len++] = 0;
			ok &= !snmp_process(req, req_len, source);
			CHECK(ok, labels[t]);
		}
	}
	query_oid(1, 5, 0);
	make_request(0xa0, 1);
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
	/* Boot RAM may still contain a complete enabled configuration: init restores default deny. */
	uip_init();
	query_oid(1, 5, 0);
	make_request(0xa0, 1);
	snmp_agent_enabled = 1;
	snmp_community_length = sizeof(comm) - 1;
	memcpy(snmp_community, comm, sizeof(comm) - 1);
	memcpy(snmp_network, network, 4);
	memcpy(snmp_mask, acl, 4);
	uip_udp_conns[0].lport = HTONS(161);
	snmp_listener = &uip_udp_conns[0];
	CHECK(snmp_process(req, req_len, source) > 0, "dirty enabled fixture would answer");
	unsigned boot_hw = hw_initializations;
	snmp_init();
	uint8_t boot_status[SNMP_STATUS_SIZE];
	memset(boot_status, 0xa5, sizeof(boot_status));
	snmp_get_status(boot_status);
	bool boot_clean = true;
	for (unsigned i = 0; i < sizeof(boot_status); i++)
		boot_clean &= !boot_status[i];
	CHECK(hw_initializations == boot_hw + 1 && boot_clean,
	      "boot clears dirty enabled/community/ACL status");
	CHECK(!snmp_process(req, req_len, source), "boot default deny despite dirty enabled RAM");
	CHECK(!snmp_set_enabled(1), "boot cannot enable without fresh credentials");
	uip_init();
	CHECK(test_snmp_configure(comm, sizeof(comm) - 1, network, acl),
	      "fresh configuration after dirty boot");
	execute();
	CHECK(!response_error && result_type[0] == ASN_OCTET_STRING, "fresh configuration answers");
	return failures ? 1 : 0;
}
