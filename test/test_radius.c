/*
 * test_radius.c - the RADIUS client on a stubbed uIP.
 *
 * radius.c and md5.c are compiled unmodified; this file plays the UDP stack
 * and the EAPOL side, builds the server's replies with the same md5 and
 * checks what the client sends and accepts.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "rtl837x_common.h"
#include "machine.h"
#include "uip.h"
#include "uip/uip.h"
#include "md5.h"
#include "radius.h"
#include "support.h"

const struct machine machine = { .machine_name = "HOSTTEST",
	.log_to_phys_port = { 1, 2, 3, 4, 5, 6, 7, 8, 9 } };
uint8_t sfr_data[4];
uint8_t uip_buf[UIP_CONF_BUFFER_ALLOC_SIZE];
void *uip_appdata = uip_buf + 54;
u16_t uip_len, uip_slen;
u8_t uip_flags;
uip_ipaddr_t uip_hostaddr;
static struct uip_udp_conn conn;
static uint8_t sent[RADIUS_REQUEST_MAX];
static uint16_t sent_len;
static int results;
static uint8_t result_port, result_code, result_state[RADIUS_STATE_MAX], result_state_len;
static uint8_t rnd;

struct uip_udp_conn *uip_udp_new(uip_ipaddr_t *ripaddr, u16_t rport) { conn.lport = HTONS(49152); conn.rport = rport; return &conn; }
void uip_send(const void *data, uint16_t len) { memcpy(sent, data, len); sent_len = len; uip_slen = len; }
void get_random_32(void) { for (int i = 0; i < 4; i++) sfr_data[i] = rnd++; }
void memcpyc(uint8_t *dst, const uint8_t *src, uint16_t len) { memcpy(dst, src, len); }
uint16_t strlen_x(const char *s) { return strlen(s); }
void eapol_radius_result(uint8_t port, uint8_t code, const uint8_t *state, uint8_t state_len)
{
	results++; result_port = port; result_code = code; result_state_len = state_len; memcpy(result_state, state, state_len);
}

static void poll(void) { uip_flags = UIP_POLL; sent_len = 0; radius_callback(conn.lport); uip_flags = 0; }
static void deliver(const uint8_t *pkt, uint16_t len) { memcpy(uip_appdata, pkt, len); uip_len = len; uip_flags = UIP_NEWDATA; radius_callback(conn.lport); uip_flags = 0; }

static const uint8_t *attr(const uint8_t *pkt, uint16_t len, uint8_t type)
{
	for (const uint8_t *a = pkt + 20; a + 2 <= pkt + len; a += a[1]) if (a[0] == type) return a;
	return 0;
}

static struct md5_ctx ctx;
static uint8_t digest[16];
static uint8_t reply[600];
static uint16_t reply_len;

/* Access-Challenge/Accept/Reject as a server would build it; authenticators from the request on the wire. */
static void build_reply(uint8_t code, const char *secret, const uint8_t *state, uint8_t state_len, const uint8_t *eap, uint16_t eap_len)
{
	uint8_t *p = reply, *ma;
	*p++ = code; *p++ = sent[1]; p += 2;
	memcpy(p, sent + 4, 16); p += 16;
	if (state_len) { *p++ = RADIUS_ATTR_STATE; *p++ = state_len + 2; memcpy(p, state, state_len); p += state_len; }
	while (eap_len) {
		uint8_t n = eap_len > 253 ? 253 : eap_len;
		*p++ = RADIUS_ATTR_EAP_MESSAGE; *p++ = n + 2; memcpy(p, eap, n); p += n; eap += n; eap_len -= n;
	}
	ma = p; *p++ = RADIUS_ATTR_MESSAGE_AUTHENTICATOR; *p++ = 18; memset(p, 0, 16); p += 16;
	reply_len = p - reply; reply[2] = reply_len >> 8; reply[3] = reply_len;
	hmac_md5((const uint8_t *)secret, strlen(secret), reply, reply_len, ma + 2);
	md5_init(&ctx); md5_update(&ctx, reply, 4); md5_update(&ctx, sent + 4, 16);
	md5_update(&ctx, reply + 20, reply_len - 20); md5_update(&ctx, (const uint8_t *)secret, strlen(secret));
	md5_final(&ctx, reply + 4);
}

static void setup(void)
{
	uint8_t ip[4] = { 10, 0, 2, 220 };
	radius_init();
	radius_server(ip, 1812);
	strcpy(radius_cfg.secret, "testing123");
	results = 0; rnd = 0x40;
}

static void t_request(void)
{
	uint8_t mac[6] = { 0xc0, 0x74, 0x2b, 0xfb, 0x16, 0xbc };
	uint8_t eap[] = { 2, 7, 0, 10, 1, 'a', 'l', 'i', 'c', 'e' };
	const uint8_t *a;
	uint8_t saved[16];

	setup();
	CHECK(radius_request(3, "alice", mac, eap, sizeof(eap), 0, 0), "a request is accepted when idle");
	CHECK(!radius_request(4, "bob", mac, eap, sizeof(eap), 0, 0), "a second request is refused while one is in flight");
	poll();
	CHECK(sent_len > 20 && sent[0] == RADIUS_CODE_ACCESS_REQUEST, "Access-Request goes out on the poll");
	CHECK(((sent[2] << 8) | sent[3]) == sent_len, "length field matches the datagram");
	a = attr(sent, sent_len, RADIUS_ATTR_USER_NAME);
	CHECK(a && a[1] == 7 && memcmp(a + 2, "alice", 5) == 0, "User-Name from the identity");
	a = attr(sent, sent_len, RADIUS_ATTR_NAS_PORT);
	CHECK(a && a[5] == 4, "NAS-Port is the physical port number");
	a = attr(sent, sent_len, RADIUS_ATTR_NAS_PORT_TYPE);
	CHECK(a && a[5] == RADIUS_NAS_PORT_TYPE_ETHERNET, "NAS-Port-Type Ethernet");
	a = attr(sent, sent_len, RADIUS_ATTR_CALLING_STATION_ID);
	CHECK(a && a[1] == 19 && memcmp(a + 2, "c0-74-2b-fb-16-bc", 17) == 0, "Calling-Station-Id is the supplicant MAC");
	a = attr(sent, sent_len, RADIUS_ATTR_EAP_MESSAGE);
	CHECK(a && a[1] == 12 && memcmp(a + 2, eap, 10) == 0, "EAP-Message carries the response");
	CHECK(attr(sent, sent_len, RADIUS_ATTR_STATE) == 0, "no State on the first request");
	a = attr(sent, sent_len, RADIUS_ATTR_MESSAGE_AUTHENTICATOR);
	CHECK(a && a[1] == 18 && a + 18 == sent + sent_len, "Message-Authenticator is the last attribute");
	memcpy(saved, a + 2, 16);
	memset((uint8_t *)a + 2, 0, 16);
	hmac_md5((const uint8_t *)"testing123", 10, sent, sent_len, digest);
	CHECK(memcmp(saved, digest, 16) == 0, "Message-Authenticator is HMAC-MD5 over the request with the field zeroed");
	memcpy((uint8_t *)a + 2, saved, 16);
}

static void t_challenge(void)
{
	uint8_t mac[6] = { 1, 2, 3, 4, 5, 6 };
	uint8_t eap[] = { 2, 1, 0, 8, 1, 'b', 'o', 'b' };
	uint8_t state[] = { 0xde, 0xad, 0xbe, 0xef, 1, 2, 3 };
	uint8_t big[300];
	int i;

	setup();
	for (i = 0; i < 300; i++) big[i] = i;
	big[0] = 1; big[1] = 2; big[2] = 300 >> 8; big[3] = 300 & 0xff;
	radius_request(2, "bob", mac, eap, sizeof(eap), 0, 0);
	poll();
	build_reply(RADIUS_CODE_ACCESS_CHALLENGE, "testing123", state, sizeof(state), big, sizeof(big));
	deliver(reply, reply_len);
	CHECK(results == 1 && result_code == RADIUS_CODE_ACCESS_CHALLENGE && result_port == 2, "a valid Access-Challenge reaches the authenticator");
	CHECK(result_state_len == sizeof(state) && memcmp(result_state, state, sizeof(state)) == 0, "State is passed up");
	CHECK(radius_eap_len == 300 && memcmp(radius_eap, big, 300) == 0, "EAP-Message split over two attributes is reassembled");

	CHECK(radius_request(2, "bob", mac, eap, sizeof(eap), state, sizeof(state)), "the next request is accepted after the reply");
	poll();
	CHECK(attr(sent, sent_len, RADIUS_ATTR_STATE) != 0, "the State is echoed in the next request");
	build_reply(RADIUS_CODE_ACCESS_ACCEPT, "testing123", 0, 0, (const uint8_t *)"\x03\x02\x00\x04", 4);
	deliver(reply, reply_len);
	CHECK(results == 2 && result_code == RADIUS_CODE_ACCESS_ACCEPT, "Access-Accept reaches the authenticator");
}

static void t_rejected_replies(void)
{
	uint8_t mac[6] = { 1, 2, 3, 4, 5, 6 };
	uint8_t eap[] = { 2, 1, 0, 8, 1, 'b', 'o', 'b' };

	setup();
	radius_request(2, "bob", mac, eap, sizeof(eap), 0, 0);
	poll();
	build_reply(RADIUS_CODE_ACCESS_ACCEPT, "wrongsecret", 0, 0, (const uint8_t *)"\x03\x02\x00\x04", 4);
	deliver(reply, reply_len);
	CHECK(results == 0, "a reply signed with another secret is ignored");
	build_reply(RADIUS_CODE_ACCESS_ACCEPT, "testing123", 0, 0, (const uint8_t *)"\x03\x02\x00\x04", 4);
	reply[1] ^= 1;
	deliver(reply, reply_len);
	CHECK(results == 0, "a reply with another identifier is ignored");
	build_reply(RADIUS_CODE_ACCESS_ACCEPT, "testing123", 0, 0, (const uint8_t *)"\x03\x02\x00\x04", 4);
	reply[reply_len - 1] ^= 1;
	deliver(reply, reply_len);
	CHECK(results == 0, "a reply with a bad Message-Authenticator is ignored");
	build_reply(RADIUS_CODE_ACCESS_ACCEPT, "testing123", 0, 0, (const uint8_t *)"\x03\x02\x00\x04", 4);
	deliver(reply, reply_len);
	CHECK(results == 1 && result_code == RADIUS_CODE_ACCESS_ACCEPT, "the genuine reply still goes through");
}

static void t_timeout(void)
{
	uint8_t mac[6] = { 1, 2, 3, 4, 5, 6 };
	uint8_t eap[] = { 2, 1, 0, 8, 1, 'b', 'o', 'b' };
	int polls = 0, s;

	setup();
	radius_request(5, "bob", mac, eap, sizeof(eap), 0, 0);
	for (s = 0; s < RADIUS_RETRIES * RADIUS_TIMEOUT_S + 2; s++) {
		poll();
		if (sent_len) polls++;
		radius_tick();
	}
	CHECK(polls == RADIUS_RETRIES, "the request is sent once per timeout, RADIUS_RETRIES times");
	CHECK(results == 1 && result_code == RADIUS_CODE_TIMEOUT && result_port == 5, "then the authenticator hears a timeout");
	CHECK(radius_request(5, "bob", mac, eap, sizeof(eap), 0, 0), "and the client is free again");
}

static void t_pap(void)
{
	uint8_t mac[6] = { 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0x0f };
	const uint8_t *a;
	uint8_t clear[16];
	int i;

	setup();
	CHECK(radius_request_pap(3, "aabbccddee0f", mac), "a MAC request is accepted when idle");
	poll();
	a = attr(sent, sent_len, RADIUS_ATTR_USER_NAME);
	CHECK(a && a[1] == 14 && memcmp(a + 2, "aabbccddee0f", 12) == 0, "User-Name is the address");
	a = attr(sent, sent_len, RADIUS_ATTR_SERVICE_TYPE);
	CHECK(a && a[5] == RADIUS_SERVICE_TYPE_CALL_CHECK, "Service-Type Call-Check marks a MAC check");
	CHECK(attr(sent, sent_len, RADIUS_ATTR_EAP_MESSAGE) == 0, "no EAP-Message");
	a = attr(sent, sent_len, RADIUS_ATTR_USER_PASSWORD);
	CHECK(a && a[1] == 18, "User-Password is one 16-octet block");
	md5_init(&ctx); md5_update(&ctx, (const uint8_t *)"testing123", 10); md5_update(&ctx, sent + 4, 16); md5_final(&ctx, digest);
	for (i = 0; i < 16; i++) clear[i] = a[2 + i] ^ digest[i];
	CHECK(memcmp(clear, "aabbccddee0f\0\0\0\0", 16) == 0, "User-Password is the address under MD5(secret + authenticator)");
	a = attr(sent, sent_len, RADIUS_ATTR_CALLING_STATION_ID);
	CHECK(a && memcmp(a + 2, "aa-bb-cc-dd-ee-0f", 17) == 0, "Calling-Station-Id is the address");
	build_reply(RADIUS_CODE_ACCESS_ACCEPT, "testing123", 0, 0, 0, 0);
	deliver(reply, reply_len);
	CHECK(results == 1 && result_code == RADIUS_CODE_ACCESS_ACCEPT && result_port == 3, "Access-Accept without EAP reaches the authenticator");
}

static void t_unconfigured(void)
{
	uint8_t mac[6] = { 1, 2, 3, 4, 5, 6 };
	uint8_t eap[] = { 2, 1, 0, 8, 1, 'b', 'o', 'b' };

	radius_init();
	CHECK(!radius_request(1, "x", mac, eap, sizeof(eap), 0, 0), "no server, no request");
}

int main(void)
{
	printf("== radius.c tests ==\n");
	t_request();
	t_challenge();
	t_rejected_replies();
	t_timeout();
	t_pap();
	t_unconfigured();
	printf("\n%d checks, %d failed\n", tests_run, tests_failed);
	return tests_failed ? 1 : 0;
}
