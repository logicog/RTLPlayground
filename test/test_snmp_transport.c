/* Real uIP + UDP dispatcher + DHCP/DNS/NTP/syslog + ARP. No switch access. */
#define SNMP_TRANSPORT_TEST
#include "test_snmp.c"
#include "uip/uip_arp.h"
#include "uip/uip_rx.h"
#include "snmp_wire_fixtures.h"
extern struct dhcp_state dhcp_state;
struct uip_eth_addr uip_ethaddr;
void print_byte(uint8_t v)
{
	(void)v;
}
void itoa(uint8_t v)
{
	(void)v;
}
static const uint8_t manager_mac[6] = {2, 0, 0, 0, 0, 7};
static const uint8_t local_mac[6] = {2, 0, 0, 0, 0, 1};
static uint8_t packet[UIP_BUFSIZE];
static size_t packet_len;
static uint32_t mock_seconds = 123;
void print_string_x(char *p)
{
	(void)p;
}
void print_ip(uint8_t *p)
{
	(void)p;
}
void print_long(uint32_t n)
{
	(void)n;
}
void print_short(uint16_t n)
{
	(void)n;
}
void itoa_short(uint16_t n)
{
	(void)n;
}
void reg_read_m(uint16_t reg)
{
	assert(reg == RTL837X_REG_SEC_COUNTER);
	sfr_data[0] = mock_seconds >> 24;
	sfr_data[1] = mock_seconds >> 16;
	sfr_data[2] = mock_seconds >> 8;
	sfr_data[3] = mock_seconds;
}
static uint16_t be16(const uint8_t *p)
{
	return ((uint16_t)p[0] << 8) | p[1];
}
static void set16(uint8_t *p, uint16_t n)
{
	p[0] = n >> 8;
	p[1] = n;
}
static uint32_t sum_bytes(uint32_t sum, const uint8_t *p, size_t n)
{
	while (n > 1) {
		sum += be16(p);
		p += 2;
		n -= 2;
	}
	if (n)
		sum += (uint16_t)p[0] << 8;
	while (sum >> 16)
		sum = (sum & 65535) + (sum >> 16);
	return sum;
}
static uint16_t ip_sum(const uint8_t *p)
{
	return (uint16_t)sum_bytes(0, p, 20);
}
static uint16_t udp_sum(const uint8_t *p)
{
	size_t n = be16(p + 24);
	assert(n >= 8 && n <= UIP_BUFSIZE - 20);
	return (uint16_t)sum_bytes(sum_bytes(17 + n, p + 12, 8), p + 20, n);
}
static void seal(uint8_t *p)
{
	p[10] = p[11] = p[26] = p[27] = 0;
	set16(p + 10, (uint16_t)~ip_sum(p));
	uint16_t n = (uint16_t)~udp_sum(p);
	set16(p + 26, n ? n : 65535);
}
static void raw_payload(const uint8_t *p, size_t n, uint16_t sport, uint16_t dport,
			const uint8_t *src)
{
	assert(n + 28 <= sizeof(packet));
	memset(packet, 0, sizeof(packet));
	packet_len = n + 28;
	packet[0] = 0x45;
	packet[8] = 64;
	packet[9] = 17;
	set16(packet + 2, packet_len);
	memcpy(packet + 12, src, 4);
	memcpy(packet + 16, uip_hostaddr, 4);
	set16(packet + 20, sport);
	set16(packet + 22, dport);
	set16(packet + 24, n + 8);
	memcpy(packet + 28, p, n);
	seal(packet);
}
static void load(const uint8_t *ip, size_t n)
{
	assert(n + UIP_LLH_LEN <= sizeof(uip_buf));
	memset(uip_buf, 0xcc, sizeof(uip_buf));
	memcpy(uip_buf, local_mac, 6);
	memcpy(uip_buf + 6, manager_mac, 6);
	uip_buf[11] = ip[15];
	set16(uip_buf + 12, 0x8899);
	uip_buf[14] = 4;
	uip_buf[15] = 0;
	memset(uip_buf + 16, 0, 8);
	set16(uip_buf + 24, 0x0800);
	memcpy(uip_buf + UIP_LLH_LEN, ip, n);
	uip_len = n + UIP_LLH_LEN;
	memset(rx_headers, 0, sizeof(rx_headers));
}
/* Same Ethernet/IP sequence as handle_rx, including the NIC's full-frame length. */
static void input(void)
{
	uip_rx_ip_input();
}
static void response(const uint8_t *src, uint16_t sport)
{
	uint8_t *p = uip_buf + UIP_LLH_LEN;
	size_t n = uip_len;
	uint8_t peer_mac[6] = {2, 0, 0, 0, 0, src[3]};
	CHECK(n >= 28 && n <= 512 && be16(p + 2) == n && be16(p + 24) == n - 20,
	      "response IP/UDP lengths bounded and consistent");
	CHECK(p[0] == 0x45 && p[9] == 17 && !memcmp(p + 12, uip_hostaddr, 4) &&
		  !memcmp(p + 16, src, 4) && be16(p + 20) == 161 && be16(p + 22) == sport,
	      "response IP and ports reversed correctly");
	CHECK(!be16(p + 10) && !be16(p + 26),
	      "pre-NIC TX checksum fields zero for hardware offload");
	CHECK(n >= 28 && !memcmp(p + 28, snmp_response, n - 28),
	      "response payload owned by uip_buf before callback returns");
	decode_response(n - 28);
	/* Independent ASIC model; this does not execute NIC firmware or prove offload. */
	memcpy(packet, p, n);
	seal(packet);
	CHECK(ip_sum(packet) == 65535 && udp_sum(packet) == 65535,
	      "independent modeled TX offload yields valid checksums");
	uip_arp_out();
	struct uip_eth_hdr *eth = (void *)(uip_buf + RTL_FRAME_DESC_SIZE);
	CHECK(uip_len == n + 14 && eth->type == HTONS(0x0800) &&
		  !memcmp(eth->dest.addr, peer_mac, 6) && !memcmp(eth->src.addr, local_mac, 6),
	      "ARP hit preserves IP packet and selects manager MAC");
	CHECK(!memcmp(uip_buf + UIP_LLH_LEN + 28, snmp_response, n - 28),
	      "ARP hit leaves response payload intact");
}
static void fixture(const uint8_t *ip, size_t n)
{
	CHECK(ip_sum(ip) == 65535 && udp_sum(ip) == 65535,
	      "frozen request IPv4 and UDP checksums valid");
	load(ip, n);
	input();
	response(ip + 12, be16(ip + 20));
}
static void drop(const char *name)
{
	unsigned before = hw_calls;
	input();
	CHECK(!uip_len, name);
	CHECK(before == hw_calls, "rejected wire request does not read hardware");
}
static void poll(struct uip_udp_conn *c)
{
	assert(c);
	uip_udp_periodic(c - uip_udp_conns);
}
static void service_response(struct uip_udp_conn *c, const uint8_t *addr, uint16_t port)
{
	const uint8_t *p = uip_buf + UIP_LLH_LEN;
	CHECK(uip_len >= 28 && be16(p + 2) == uip_len && be16(p + 24) == uip_len - 20 &&
		  be16(p + 20) == NTOHS(c->lport) && be16(p + 22) == port &&
		  !memcmp(p + 16, addr, 4),
	      "non-SNMP UDP send retains its socket destination and lengths");
}
static void interleave(void)
{
	uint8_t saved[SNMP_MAX_MESSAGE];
	memcpy(saved, snmp_response, sizeof(saved));
	static const uint8_t service_ip[] = {192, 0, 2, 53};
	uip_ipaddr_t addr;
	memcpy(addr, service_ip, 4);
	syslog_init();
	syslog_state.syslog_conn = uip_udp_new(&addr, HTONS(514));
	assert(syslog_state.syslog_conn);
	syslog_state.enabled = 1;
	memcpy(logbuf, "transport\n", 10);
	syslog_state.readptr = 0;
	syslog_state.writeptr = 10;
	syslog_state.line_available = 1;
	fixture(wire_get, sizeof(wire_get));
	CHECK(syslog_state.line_available && syslog_state.readptr == 0,
	      "SNMP dispatcher does not consume pending syslog");
	memcpy(saved, snmp_response, sizeof(saved));
	poll(syslog_state.syslog_conn);
	service_response(syslog_state.syslog_conn, service_ip, 514);
	CHECK(!memcmp(uip_buf + UIP_LLH_LEN + 28, "<14>host-agent transport", 23),
	      "real syslog callback sends queued line");
	CHECK(!memcmp(saved, snmp_response, sizeof(saved)),
	      "other UDP buffer writes leave SNMP workspace intact");
	fixture(wire_getnext, sizeof(wire_getnext));

	dns_init();
	memcpy(dns_state.server[0], service_ip, 4);
	strcpy(dns_state.name, "lab.example");
	dns_lookup();
	assert(dns_state.conn);
	poll(dns_state.conn);
	service_response(dns_state.conn, service_ip, 53);
	uint16_t lport = NTOHS(dns_state.conn->lport), id = dns_state.id;
	CHECK(uip_buf[UIP_LLH_LEN + 28 + 2] == 1, "real DNS callback sends query");
	fixture(wire_get, sizeof(wire_get));
	uint8_t answer[] = {0,	  0, 0x81, 0x80, 0,   1,   0,	1,   0,	  0, 0, 0,   3, 'l', 'a',
			    'b',  7, 'e',  'x',	 'a', 'm', 'p', 'l', 'e', 0, 0, 1,   0, 1,   0xc0,
			    0x0c, 0, 1,	   0,	 1,   0,   0,	0,   60,  0, 4, 192, 0, 2,   123};
	set16(answer, id);
	raw_payload(answer, sizeof(answer), 53, lport, service_ip);
	load(packet, packet_len);
	input();
	CHECK(!uip_len && dns_state.status == DNS_DONE &&
		  !memcmp(dns_state.addr, "\xc0\x00\x02\x7b", 4),
	      "real DNS response completes lookup between SNMP requests");
	fixture(wire_getnext, sizeof(wire_getnext));

	ntp_init();
	memcpy(ntp_state.addr, service_ip, 4);
	ntp_state.conn = uip_udp_new(&addr, HTONS(123));
	ntp_state.enabled = 1;
	ntp_state.phase = 2;
	poll(ntp_state.conn);
	service_response(ntp_state.conn, service_ip, 123);
	CHECK(uip_len == 76 && uip_buf[UIP_LLH_LEN + 28] == 0x23, "real NTP request emitted");
	lport = NTOHS(ntp_state.conn->lport);
	fixture(wire_get, sizeof(wire_get));
	uint8_t ntp[48] = {0x24, 2};
	ntp[40] = 0x83;
	ntp[41] = 0xaa;
	ntp[42] = 0x7e;
	ntp[43] = 0x85;
	raw_payload(ntp, sizeof(ntp), 123, lport, service_ip);
	load(packet, packet_len);
	input();
	CHECK(!uip_len && ntp_state.synced && ntp_state.utc == 5 && ntp_state.at == 123,
	      "real NTP response updates time between SNMP requests");

	memset(&dhcp_state, 0, sizeof(dhcp_state));
	uip_ipaddr(addr, 255, 255, 255, 255);
	dhcp_state.conn = uip_udp_new(&addr, HTONS(67));
	assert(dhcp_state.conn);
	uip_udp_bind(dhcp_state.conn, HTONS(68));
	dhcp_state.state = DHCP_START;
	poll(dhcp_state.conn);
	static const uint8_t broadcast[] = {255, 255, 255, 255};
	service_response(dhcp_state.conn, broadcast, 67);
	CHECK(dhcp_state.state == DHCP_DISCOVER_SENT && uip_len > 268,
	      "real DHCP discover emitted");
	fixture(wire_get, sizeof(wire_get));
	uint8_t dhcp[300] = {0};
	memcpy(dhcp + 4, &dhcp_state.transaction_id, 4);
	memcpy(dhcp + 16, "\xc0\x00\x02\x01", 4);
	memcpy(dhcp + 236, "\x63\x82\x53\x63", 4);
	uint8_t options[] = {53,  1, 5, 1,   4,	 255, 255, 255, 0, 3,  4,
			     192, 0, 2, 254, 51, 4,   0,   0,	1, 44, 255};
	memcpy(dhcp + 240, options, sizeof(options));
	memcpy(dhcp_state.current_ip, network, 4);
	dhcp_state.current_ip[3] = 1;
	dhcp_state.state = DHCP_REQUEST_SENT;
	raw_payload(dhcp, sizeof(dhcp), 67, 68, service_ip);
	load(packet, packet_len);
	input();
	CHECK(!uip_len && dhcp_state.state == DHCP_LEASING && dhcp_state.dhcp_timer == 150,
	      "real DHCP ACK accepted without replacing SNMP listener");
	fixture(wire_getnext, sizeof(wire_getnext));
	for (unsigned i = 0; i < UIP_UDP_CONNS; i++)
		if (uip_udp_conns[i].lport == HTONS(161)) {
			poll(&uip_udp_conns[i]);
			CHECK(!uip_len, "SNMP periodic poll does not replay cached response");
			CHECK(!uip_udp_conns[i].rport &&
				  !memcmp(uip_udp_conns[i].ripaddr, "\0\0\0\0", 4),
			      "SNMP socket remains wildcard after service interleaving");
		}
}
int main(void)
{
	setvbuf(stdout, NULL, _IONBF, 0);
	uip_init();
	uip_arp_init();
	uip_ipaddr(uip_hostaddr, 192, 0, 2, 1);
	uip_ipaddr(uip_netmask, 255, 255, 255, 0);
	uip_ipaddr(uip_draddr, 192, 0, 2, 254);
	memcpy(uip_ethaddr.addr, local_mac, 6);
	/* Exhaust the 16-bit descriptor domain using the production DMA guard.
	 * Host arithmetic is independent and does not issue a hardware transfer. */
	for (uint32_t n = 0; n <= 65535; n++)
		assert(!!uip_rx_dma_fits(n) == (((n + 7) / 8) * 8 <= sizeof(uip_buf)));
	CHECK(uip_rx_dma_fits(1552) && !uip_rx_dma_fits(1553) && !uip_rx_dma_fits(1556) &&
		  !uip_rx_dma_fits(65535),
	      "pre-DMA guard rejects every rounded transfer beyond packet storage");
	snmp_init();
	assert(test_snmp_configure(comm, sizeof(comm) - 1, network, acl));
	fixture(wire_get, sizeof(wire_get));
	CHECK(result_type[0] == 4 && result_len[0] == 10 &&
		  !memcmp(result_data[0], "host-agent", 10),
	      "wire GET returns real sysName");
	snmp_uptime = 0x80000000;
	fixture(wire_getnext, sizeof(wire_getnext));
	CHECK(result_type[0] == 0x43 && result_len[0] == 5 && names[0][6] == 3,
	      "wire GETNEXT advances to TimeTicks");
	/* Leave a complete old packet in storage, but report a shorter new frame. */
	load(wire_get, sizeof(wire_get));
	uip_len--;
	drop("truncated frame cannot reuse stale trailing BER byte");
	for (size_t n = 0; n < sizeof(wire_get); n++) {
		load(wire_get, sizeof(wire_get));
		uip_len = UIP_LLH_LEN + n;
		input();
		CHECK(!uip_len, "every wire truncation rejected with stale valid tail");
	}
	uint8_t malformed[sizeof(wire_get)];
	memcpy(malformed, wire_get, sizeof(malformed));
	malformed[29] = 0x80;
	seal(malformed);
	load(malformed, sizeof(malformed));
	drop("wire indefinite BER dropped");
	memcpy(malformed, wire_get, sizeof(malformed));
	malformed[38] ^= 1;
	seal(malformed);
	load(malformed, sizeof(malformed));
	drop("wire wrong community dropped");
	uint8_t denied[][4] = {{192, 0, 3, 7}, {192, 0, 2, 0}, {192, 0, 2, 255},
			       {0, 0, 0, 0},   {127, 0, 0, 1}, {224, 0, 0, 1}};
	for (unsigned i = 0; i < sizeof(denied) / sizeof(denied[0]); i++) {
		memcpy(malformed, wire_get, sizeof(malformed));
		memcpy(malformed + 12, denied[i], 4);
		seal(malformed);
		load(malformed, sizeof(malformed));
		drop("wire source rejected");
	}
	memcpy(malformed, wire_get, sizeof(malformed));
	memset(malformed + 16, 255, 4);
	seal(malformed);
	load(malformed, sizeof(malformed));
	drop("SNMP broadcast destination dropped");
	memcpy(malformed, wire_get, sizeof(malformed));
	malformed[19] = 2;
	seal(malformed);
	load(malformed, sizeof(malformed));
	drop("foreign destination IP dropped");
	memcpy(malformed, wire_get, sizeof(malformed));
	set16(malformed + 20, 0);
	seal(malformed);
	load(malformed, sizeof(malformed));
	drop("source UDP port zero dropped");
	memcpy(malformed, wire_get, sizeof(malformed));
	set16(malformed + 22, 162);
	seal(malformed);
	load(malformed, sizeof(malformed));
	drop("unsupported destination port dropped");
	memcpy(malformed, wire_get, sizeof(malformed));
	set16(malformed + 24, sizeof(malformed) - 21);
	load(malformed, sizeof(malformed));
	drop("UDP/IP length disagreement dropped");
	memcpy(malformed, wire_get, sizeof(malformed));
	malformed[6] = 0x20;
	seal(malformed);
	load(malformed, sizeof(malformed));
	drop("fragmented UDP dropped");
	load(wire_get, sizeof(wire_get));
	rx_headers[1] = RX_TAG_L4_CSUM_BAD;
	drop("NIC bad UDP checksum descriptor dropped");
	memcpy(malformed, wire_get, sizeof(malformed));
	malformed[26] ^= 1;
	CHECK(udp_sum(malformed) != 65535, "independent checksum oracle detects UDP corruption");
	load(malformed, sizeof(malformed));
	rx_headers[1] = RX_TAG_L4_CSUM_BAD;
	drop("bad encoded UDP checksum dropped through offload status");
	memcpy(malformed, wire_get, sizeof(malformed));
	malformed[10] ^= 1;
	CHECK(ip_sum(malformed) != 65535, "independent checksum oracle detects IPv4 corruption");
	load(malformed, sizeof(malformed));
	input();
	CHECK(uip_len > 28, "documented dependency: uIP alone does not verify IPv4 checksum");
	uint8_t huge[SNMP_MAX_MESSAGE + 1] = {0};
	raw_payload(huge, sizeof(huge), 40001, 161, source);
	load(packet, packet_len);
	drop("485-byte SNMP datagram dropped");
	query_oid(2, 4, 1);
	make_request(0xa0, 1);
	raw_payload(req, req_len, 40001, 161, source);
	load(packet, packet_len);
	input();
	response(source, 40001);
	CHECK(result_type[0] == 0x80, "wire unsupported OID returns noSuchObject");
	query_oid(1, 5, 1);
	make_request(0xa0, 1);
	raw_payload(req, req_len, 40001, 161, source);
	load(packet, packet_len);
	input();
	response(source, 40001);
	CHECK(result_type[0] == 0x81, "wire unsupported instance returns noSuchInstance");
	query_oid(1, 1, 0);
	make_request(0xa0, 8);
	raw_payload(req, req_len, 40001, 161, source);
	load(packet, packet_len);
	input();
	response(source, 40001);
	CHECK(response_error == 1 && !response_index && !result_count,
	      "wire tooBig uses empty bindings and preserves request-id");
	query_oid(2, 10, 8);
	make_request(0xa0, 1);
	raw_payload(req, req_len, 40001, 161, source);
	load(packet, packet_len);
	input();
	response(source, 40001);
	CHECK(result_type[0] == 0x41 && result_len[0] == 5,
	      "wire Counter32 has unsigned sign padding");
	hw_value++;
	raw_payload(req, req_len, 40001, 161, source);
	load(packet, packet_len);
	input();
	response(source, 40001);
	CHECK(result_type[0] == 0x41 && result_len[0] == 1 && !result_data[0][0],
	      "wire Counter32 wraps on native low-word rollover");
	query_oid(31, 6, 8);
	make_request(0xa0, 1);
	raw_payload(req, req_len, 40001, 161, source);
	load(packet, packet_len);
	input();
	response(source, 40001);
	CHECK(result_type[0] == 0x46 && result_len[0] == 9 && result_data[0][1] == 0x80 &&
		  result_data[0][4] == 1,
	      "wire Counter64 preserves carry and sign padding");
	query_oid(2, 5, 8);
	make_request(0xa0, 1);
	raw_payload(req, req_len, 40001, 161, source);
	load(packet, packet_len);
	input();
	response(source, 40001);
	CHECK(result_type[0] == 0x42 && result_len[0] == 5 && result_data[0][1] == 255,
	      "wire Gauge32 saturates 10G ifSpeed");
	query_oid(2, 10, 1);
	make_request(0xa0, 1);
	hw_failed = true;
	raw_payload(req, req_len, 40001, 161, source);
	load(packet, packet_len);
	input();
	response(source, 40001);
	hw_failed = false;
	CHECK(response_error == 5 && response_index == 1 && result_type[0] == 5,
	      "wire hardware failure returns genErr with original binding");
	query_oid(31, 18, machine.max_port + 1);
	make_request(0xa1, 1);
	raw_payload(req, req_len, 40001, 161, source);
	load(packet, packet_len);
	input();
	response(source, 40001);
	CHECK(result_type[0] == 0x82 && name_len[0] == input_oid_len &&
		  !memcmp(names[0], input_oid, input_oid_len),
	      "wire GETNEXT endOfMibView preserves original OID");
	fixture(wire_max_get, sizeof(wire_max_get));
	CHECK(result_type[0] == 4 && result_len[0] == 10,
	      "484-byte accepted datagram validates ignored Opaque request value");
	memset(packet, 0xcc, sizeof(packet));
	memcpy(packet, wire_get, sizeof(wire_get));
	load(packet, sizeof(wire_get) + 12);
	input();
	response(source, 40001);
	CHECK(result_type[0] == 4, "Ethernet padding excluded from BER payload");
	memcpy(malformed, wire_get, sizeof(malformed));
	malformed[0] = 0x46;
	load(malformed, sizeof(malformed));
	drop("IPv4 options unsupported and dropped");
	memcpy(malformed, wire_get, sizeof(malformed));
	set16(malformed + 24, 7);
	load(malformed, sizeof(malformed));
	drop("UDP length below header size dropped");
	memcpy(malformed, wire_get, sizeof(malformed));
	set16(malformed + 26, 0);
	load(malformed, sizeof(malformed));
	input();
	response(source, 40001);
	CHECK(result_type[0] == 4, "IPv4 zero UDP checksum accepted with clean NIC status");
	query_oid(1, 5, 0);
	make_request(0xa3, 1);
	raw_payload(req, req_len, 40001, 161, source);
	load(packet, packet_len);
	drop("wire SET dropped");
	query_oid(1, 5, 0);
	make_request(0xa5, 1);
	raw_payload(req, req_len, 40001, 161, source);
	load(packet, packet_len);
	input();
	int answered = uip_len > 28;
	if (answered)
		response(source, 40001);
	CHECK(answered && !response_error && !response_index && !result_count,
	      "wire GETBULK with zero max-repetitions returns an empty response");
	uint8_t large[UIP_BUFSIZE - UIP_LLH_LEN - 28] = {0};
	raw_payload(large, sizeof(large), 40001, 161, source);
	load(packet, packet_len);
	drop("maximum buffer-sized oversized SNMP datagram dropped");
	load(wire_get, sizeof(wire_get));
	uip_len = UIP_BUFSIZE + 1;
	drop("driver length beyond uIP buffer rejected");
	uint32_t rng = 7;
	for (unsigned round = 0; round < 2000; round++) {
		size_t n = round % 485;
		uint8_t fuzz[484];
		for (size_t j = 0; j < n; j++) {
			rng = rng * 1664525u + 1013904223u;
			fuzz[j] = rng >> 24;
		}
		if (n)
			fuzz[0] = 0; /* always malformed BER; valid IPv4/UDP envelope */
		raw_payload(fuzz, n, 40001, 161, source);
		load(packet, packet_len);
		input();
		assert(!uip_len);
	}
	CHECK(1, "2000 valid-checksum UDP envelopes with malformed BER rejected");
	CHECK(snmp_set_enabled(0), "wire agent disable succeeds");
	load(wire_get, sizeof(wire_get));
	drop("disabled agent drops valid GET with correct UDP checksum");
	CHECK(snmp_set_enabled(1), "wire agent reenable succeeds");
	load(wire_get, sizeof(wire_get));
	input();
	response(source, 40001);
	CHECK(result_type[0] == 4,
	      "reenabled listener replies to the original manager and UDP port");
	interleave();
	/* Routed manager: source ACL stays explicit; no VLAN assumption. */
	uip_ipaddr(uip_hostaddr, 192, 0, 1, 1);
	uip_ipaddr(uip_draddr, 192, 0, 1, 254);
	uip_arp_init();
	memcpy(malformed, wire_get, sizeof(malformed));
	memcpy(malformed + 16, uip_hostaddr, 4);
	seal(malformed);
	load(malformed, sizeof(malformed));
	input();
	CHECK(uip_len > 28 && !memcmp(uip_buf + UIP_LLH_LEN + 16, source, 4),
	      "routed response keeps manager IP before ARP");
	uip_arp_out();
	CHECK(be16(uip_buf + 24) == 0x0806 && be16(uip_buf + 32) == 1 &&
		  !memcmp(uip_buf + 50, uip_draddr, 4),
	      "ARP miss replaces response with gateway ARP request");
	uint8_t gateway_mac[] = {2, 0, 0, 0, 0, 254};
	memset(uip_buf, 0, sizeof(uip_buf));
	memcpy(uip_buf, local_mac, 6);
	memcpy(uip_buf + 6, gateway_mac, 6);
	set16(uip_buf + 24, 0x0806);
	uint8_t *arp = uip_buf + 26;
	set16(arp, 1);
	set16(arp + 2, 0x0800);
	arp[4] = 6;
	arp[5] = 4;
	set16(arp + 6, 2);
	memcpy(arp + 8, gateway_mac, 6);
	memcpy(arp + 14, uip_draddr, 4);
	memcpy(arp + 18, local_mac, 6);
	memcpy(arp + 24, uip_hostaddr, 4);
	uip_len = 54;
	uip_arp_arpin();
	CHECK(!uip_len, "gateway ARP reply updates cache without stale SNMP transmission");
	load(malformed, sizeof(malformed));
	input();
	size_t ip_length = uip_len;
	uip_arp_out();
	CHECK(
	    uip_len == ip_length + 14 && be16(uip_buf + 24) == 0x0800 &&
		!memcmp(uip_buf + 12, gateway_mac, 6) &&
		!memcmp(uip_buf + UIP_LLH_LEN + 16, source, 4) &&
		be16(uip_buf + UIP_LLH_LEN + 22) == 40001,
	    "manager retry after ARP resolution replies through gateway with correct IP and port");

	return failures ? 1 : 0;
}
