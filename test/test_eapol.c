/*
 * test_eapol.c - the 802.1X authenticator state machine on a stubbed RADIUS client.
 *
 * eapol.c and rtl837x_port.c are compiled unmodified against the register
 * mock; this file plays the supplicant, the RADIUS client and the link
 * status register, and checks the frames the authenticator sends, the
 * requests it hands to RADIUS and the port authorization it asks for.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "rtl837x_common.h"
#include "rtl837x_regs.h"
#include "rtl837x_trap.h"
#include "rtl837x_port.h"
#include "machine.h"
#include "uip.h"
#include "uip/uip.h"
#include "eapol.h"
#include "support.h"
#include "hw_mock.h"

uint8_t uip_buf[UIP_CONF_BUFFER_ALLOC_SIZE];
u16_t uip_len;
bool dot1x_enabled;
uint8_t dot1x_mode[DOT1X_PORTS];
uint8_t radius_eap[RADIUS_EAP_MAX];
uint16_t radius_eap_len;

static int frames;
static uint8_t frame[128];
static uint16_t frame_len;
static int auth_calls, auth_on, auth_port;
static int req_calls, pap_calls;
static bool req_ok = true;
static uint8_t req_port, req_mac[6], req_eap[64], req_eap_len, req_state[RADIUS_STATE_MAX], req_state_len;
static char req_identity[EAPOL_IDENTITY_MAX + 1];
static char pap_user[EAPOL_IDENTITY_MAX + 1];
static uint8_t pap_mac[6];

static const uint8_t supplicant[6] = { 0x00, 0x11, 0x22, 0x33, 0x44, 0x55 };

void memcpyc(uint8_t *dst, const uint8_t *src, uint16_t len) { memcpy(dst, src, len); }
void tcpip_output(void)
{
	frames++;
	frame_len = uip_len > sizeof(frame) ? sizeof(frame) : uip_len;
	memcpy(frame, uip_buf + RTL_FRAME_DESC_SIZE, frame_len);
}
void dot1x_authorize(uint8_t port, bool on) { auth_calls++; auth_port = port; auth_on = on; }
bool radius_request(uint8_t port, const char *identity, const uint8_t *mac, const uint8_t *eap, uint16_t eap_len, const uint8_t *state, uint8_t state_len)
{
	req_calls++; req_port = port; memcpy(req_mac, mac, 6);
	strncpy(req_identity, identity, EAPOL_IDENTITY_MAX); req_identity[EAPOL_IDENTITY_MAX] = 0;
	req_eap_len = eap_len > sizeof(req_eap) ? sizeof(req_eap) : eap_len; memcpy(req_eap, eap, req_eap_len);
	req_state_len = state_len; memcpy(req_state, state, state_len);
	return req_ok;
}
bool radius_request_pap(uint8_t port, const char *user, const uint8_t *mac)
{
	pap_calls++; req_port = port; memcpy(pap_mac, mac, 6);
	strncpy(pap_user, user, EAPOL_IDENTITY_MAX); pap_user[EAPOL_IDENTITY_MAX] = 0;
	return req_ok;
}

/* ---- the world around the module ---- */
static void links(uint16_t mask)
{
	hw_reg_set(RTL837X_REG_LINKS_STS, ((uint32_t)(mask & 0xff) << 16) | ((uint32_t)(mask >> 8) << 8));
}

static void setup(uint8_t port)
{
	hw_reset(); out_reset();
	frames = 0; auth_calls = 0; req_calls = 0; pap_calls = 0; req_ok = true;
	radius_eap_len = 0;
	memset(dot1x_mode, 0, sizeof(dot1x_mode));
	dot1x_enabled = true;
	dot1x_mode[port] = DOT1X_PORT_AUTO;
	eapol_init();
	links(1 << port);
}

static void ticks(int n) { while (n--) eapol_tick(); }

/* a frame as the switch hands it up: dst, src, the RTL tag naming the source port, then EAPOL */
static void rx(uint8_t port, uint8_t type, const uint8_t *body, uint16_t len)
{
	uint8_t *p = uip_buf;
	memcpy(p, EAPOL_MULTICAST_ADDR, 6); p += 6;
	memcpy(p, supplicant, 6); p += 6;
	*p++ = 0x88; *p++ = 0x99; *p++ = 0x04; *p++ = 0; *p++ = 0; *p++ = 0; *p++ = 0; *p++ = port;
	*p++ = 0x88; *p++ = 0x8e;
	*p++ = EAPOL_VERSION; *p++ = type; *p++ = len >> 8; *p++ = len;
	memcpy(p, body, len); p += len;
	uip_len = p - uip_buf;
	CHECK(eapol_in(), "the frame is taken as EAPOL");
}

static void rx_identity(uint8_t port, uint8_t id, const char *name)
{
	uint8_t eap[5 + EAPOL_IDENTITY_MAX + 8];
	uint16_t n = strlen(name);
	eap[0] = EAP_CODE_RESPONSE; eap[1] = id; eap[2] = (5 + n) >> 8; eap[3] = 5 + n; eap[4] = EAP_TYPE_IDENTITY;
	memcpy(eap + 5, name, n);
	rx(port, EAPOL_TYPE_EAP, eap, 5 + n);
}

static const uint8_t *eap_in_frame(void) { return frame + 12 + RTL_TAG_SIZE + 2 + EAPOL_HEADER_LENGTH; }

static void t_identity_request(void)
{
	printf("[test] link up on an auto port starts with a Request/Identity\n");
	setup(2);
	eapol_tick();
	CHECK(frames == 1, "one frame left the CPU");
	CHECK(memcmp(frame, EAPOL_MULTICAST_ADDR, 6) == 0, "to the PAE group address");
	CHECK(memcmp(frame + 6, uip_ethaddr.addr, 6) == 0, "from the switch");
	CHECK(frame[12] == 0x88 && frame[13] == 0x99, "with the RTL tag");
	CHECK(frame[18] == 0 && frame[19] == (1 << 2), "for port 2 only");
	CHECK(frame[20] == 0x88 && frame[21] == 0x8e, "EAPOL ethertype");
	CHECK(frame[22] == EAPOL_VERSION && frame[23] == EAPOL_TYPE_EAP, "EAPOL version 2, EAP packet");
	CHECK(eap_in_frame()[0] == EAP_CODE_REQUEST && eap_in_frame()[4] == EAP_TYPE_IDENTITY, "EAP Request/Identity");
	CHECK(frame_len == 12 + RTL_TAG_SIZE + 2 + 46, "padded to the minimum frame");
	CHECK(eapol_ports[2].state == EAPOL_STATE_CONNECTING, "port is connecting");
	CHECK(eapol_ports[2].tries == 1, "one request counted");
	ticks(EAPOL_TX_PERIOD_S - 1);
	CHECK(frames == 1, "no repeat before the period expires");
	eapol_tick();
	CHECK(frames == 2 && eapol_ports[2].tries == 2, "the request repeats after the period");
	CHECK(eap_in_frame()[1] != 0, "each request carries an identifier");
}

static void t_other_ports_quiet(void)
{
	printf("[test] ports that are off, down or forced stay silent\n");
	setup(2);
	dot1x_mode[3] = DOT1X_PORT_FORCE_AUTH;
	dot1x_mode[4] = DOT1X_PORT_AUTO;
	links((1 << 2) | (1 << 3));
	eapol_tick();
	CHECK(frames == 1 && frame[19] == (1 << 2), "only the auto port with a link talks");
	dot1x_enabled = false;
	eapol_tick();
	CHECK(eapol_ports[2].state == EAPOL_STATE_IDLE, "disabling 802.1X resets the port");
	CHECK(auth_calls == 1 && auth_on == 0, "and withdraws authorization");
}

static void t_full_exchange(void)
{
	uint8_t challenge[] = { EAP_CODE_REQUEST, 7, 0, 22, 4, 16, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 };
	uint8_t md5resp[] = { EAP_CODE_RESPONSE, 7, 0, 22, 4, 16, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9 };
	uint8_t state[] = { 0xaa, 0xbb, 0xcc };
	uint8_t id;

	printf("[test] Start, Identity, Challenge, Accept\n");
	setup(2);
	eapol_tick();
	id = eap_in_frame()[1];
	rx(2, EAPOL_TYPE_START, 0, 0);
	CHECK(frames == 2 && eap_in_frame()[1] == (uint8_t)(id + 1), "EAPOL-Start draws a fresh Request/Identity");
	CHECK(memcmp(eapol_ports[2].mac, supplicant, 6) == 0, "the supplicant address is remembered");
	rx_identity(2, id + 1, "alice");
	CHECK(req_calls == 1 && req_port == 2, "the identity goes to RADIUS");
	CHECK(strcmp(req_identity, "alice") == 0, "as User-Name");
	CHECK(memcmp(req_mac, supplicant, 6) == 0, "with the supplicant address");
	CHECK(req_eap_len == 10 && req_eap[0] == EAP_CODE_RESPONSE && req_eap[4] == EAP_TYPE_IDENTITY, "and the whole EAP packet");
	CHECK(req_state_len == 0, "no State on the first request");
	CHECK(eapol_ports[2].state == EAPOL_STATE_AUTHENTICATING, "port waits for the server");
	memcpy(radius_eap, challenge, sizeof(challenge)); radius_eap_len = sizeof(challenge);
	eapol_radius_result(2, RADIUS_CODE_ACCESS_CHALLENGE, state, sizeof(state));
	CHECK(frames == 2, "nothing is sent from inside the RADIUS callback");
	eapol_poll();
	CHECK(frames == 3 && memcmp(eap_in_frame(), challenge, sizeof(challenge)) == 0, "the main loop relays the server's EAP request");
	eapol_poll();
	CHECK(frames == 3, "and only once");
	rx(2, EAPOL_TYPE_EAP, md5resp, sizeof(md5resp));
	CHECK(req_calls == 2 && req_state_len == 3 && memcmp(req_state, state, 3) == 0, "the response echoes the server State");
	CHECK(strcmp(req_identity, "alice") == 0, "still as alice");
	eapol_radius_result(2, RADIUS_CODE_ACCESS_ACCEPT, 0, 0);
	CHECK(auth_calls == 1 && auth_port == 2 && auth_on == 1, "Accept authorizes the port");
	CHECK(eapol_ports[2].state == EAPOL_STATE_AUTHENTICATED, "port is authenticated");
	CHECK(eapol_ports[2].ok == 1 && eapol_ports[2].fail == 0, "one success counted");
	CHECK(eapol_ports[2].timer == EAPOL_REAUTH_S, "reauthentication timer armed");
	eapol_poll();
	CHECK(frames == 4 && eap_in_frame()[0] == EAP_CODE_SUCCESS && eap_in_frame()[1] == 7, "EAP Success with the identifier of the last response");
	CHECK(strstr(out_buf, "802.1X: port 3 authorized alice") != 0, "the console says who was authorized on the physical port");
	rx(2, EAPOL_TYPE_LOGOFF, 0, 0);
	CHECK(auth_calls == 2 && auth_on == 0, "Logoff withdraws authorization");
	CHECK(eapol_ports[2].state == EAPOL_STATE_IDLE && eapol_ports[2].identity[0] == 0, "and forgets the supplicant");
	CHECK(eapol_ports[2].ok == 1, "but keeps the counters");
	CHECK(strstr(out_buf, "logoff alice") != 0, "logoff is logged");
}

static void t_reject_and_timeout(void)
{
	printf("[test] Reject and a silent server hold the port\n");
	setup(2);
	eapol_tick();
	rx_identity(2, eap_in_frame()[1], "mallory");
	eapol_radius_result(2, RADIUS_CODE_ACCESS_REJECT, 0, 0);
	CHECK(auth_calls == 1 && auth_on == 0, "Reject withdraws authorization");
	CHECK(eapol_ports[2].state == EAPOL_STATE_HELD, "port is held");
	CHECK(eapol_ports[2].fail == 1, "one failure counted");
	eapol_poll();
	CHECK(eap_in_frame()[0] == EAP_CODE_FAILURE, "EAP Failure is sent");
	CHECK(strstr(out_buf, "rejected mallory") != 0, "rejection is logged");
	frames = 0;
	ticks(EAPOL_QUIET_S);
	CHECK(frames == 0 && eapol_ports[2].state == EAPOL_STATE_IDLE, "silence during the quiet period, then idle");
	eapol_tick();
	CHECK(frames == 1 && eapol_ports[2].state == EAPOL_STATE_CONNECTING, "a new round starts");
	rx_identity(2, eap_in_frame()[1], "bob");
	eapol_radius_result(2, RADIUS_CODE_TIMEOUT, 0, 0);
	CHECK(eapol_ports[2].state == EAPOL_STATE_HELD && eapol_ports[2].fail == 2, "server timeout counts as a failure");
	CHECK(strstr(out_buf, "no answer from the server for bob") != 0, "and is logged as such");
	setup(2);
	eapol_tick();
	rx_identity(2, eap_in_frame()[1], "carol");
	ticks(EAPOL_SERVER_TIMEOUT_S);
	CHECK(eapol_ports[2].state == EAPOL_STATE_HELD, "no RADIUS result at all also ends in held");
}

static void t_mab(void)
{
	static const uint8_t printer[6] = { 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0x0f };
	static const uint8_t mcast[6] = { 0x01, 0x00, 0x5e, 0x00, 0x00, 0xfb };
	static const uint8_t elsewhere[6] = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05 };
	static const uint8_t pinned[6] = { 0x00, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e };
	uint8_t found[6];

	printf("[test] MAC authentication bypass after the supplicant stays silent\n");
	setup(2);
	eapol_port_mab(2, true);
	hw_l2_put(3, mcast, 1, 1 << 2, false);
	hw_l2_put(5, elsewhere, 1, 4, false);
	hw_l2_put(7, pinned, 1, 2, true);
	CHECK(!port_l2_mac_on_port(2, found), "a multicast entry, a static entry and another port's entry are not candidates");
	ticks(EAPOL_TX_PERIOD_S * EAPOL_MAX_REQUESTS);
	CHECK(frames == EAPOL_MAX_REQUESTS, "three identity requests went out");
	eapol_tick();
	CHECK(pap_calls == 0 && frames == EAPOL_MAX_REQUESTS + 1, "with no learned address the port keeps asking");
	hw_l2_put(9, printer, 1, 2, false);
	CHECK(port_l2_mac_on_port(2, found) && memcmp(found, printer, 6) == 0, "a learned unicast entry on the port is found");
	ticks(EAPOL_TX_PERIOD_S * EAPOL_MAX_REQUESTS - 1);
	CHECK(pap_calls == 0, "the fallback waits for a full round of silence");
	eapol_tick();
	CHECK(pap_calls == 1 && req_port == 2, "then the learned address is tried");
	CHECK(strcmp(pap_user, "aabbccddee0f") == 0, "as a lower-case hex User-Name");
	CHECK(memcmp(pap_mac, printer, 6) == 0, "with the address as Calling-Station-Id");
	CHECK(memcmp(eapol_ports[2].mac, printer, 6) == 0, "the port remembers the address");
	CHECK(eapol_ports[2].state == EAPOL_STATE_AUTHENTICATING && eapol_ports[2].via_mab == 1, "port waits for the server, by address");
	frames = 0;
	eapol_radius_result(2, RADIUS_CODE_ACCESS_ACCEPT, 0, 0);
	eapol_poll();
	CHECK(auth_calls == 1 && auth_on == 1, "Accept authorizes the port");
	CHECK(frames == 0, "no EAP Success, nobody is listening");
	CHECK(eapol_ports[2].ok == 1, "counted as a success");
	CHECK(strstr(out_buf, "authorized by address aabbccddee0f") != 0, "logged as authorized by address");
	setup(2);
	eapol_port_mab(2, true);
	hw_l2_put(9, printer, 1, 2, false);
	ticks(EAPOL_TX_PERIOD_S * EAPOL_MAX_REQUESTS + 1);
	req_ok = true;
	eapol_radius_result(2, RADIUS_CODE_ACCESS_REJECT, 0, 0);
	eapol_poll();
	CHECK(eapol_ports[2].state == EAPOL_STATE_HELD && eapol_ports[2].fail == 1, "a rejected address holds the port");
	CHECK(frames == EAPOL_MAX_REQUESTS, "without an EAP Failure frame");
	setup(2);
	hw_l2_put(9, printer, 1, 2, false);
	ticks(EAPOL_TX_PERIOD_S * EAPOL_MAX_REQUESTS + 1);
	CHECK(pap_calls == 0, "no fallback unless mab is on for the port");
	eapol_port_reset(2);
	eapol_port_mab(2, true);
	eapol_port_reset(2);
	CHECK(eapol_ports[2].mab == 1, "the mab setting survives a port reset");
}

static void t_rx_guards(void)
{
	uint8_t eap[8] = { EAP_CODE_RESPONSE, 1, 0, 40, EAP_TYPE_IDENTITY, 'x', 'y', 'z' };
	char longname[EAPOL_IDENTITY_MAX + 8];

	printf("[test] malformed or misplaced frames are dropped\n");
	setup(2);
	eapol_tick();
	rx(2, EAPOL_TYPE_EAP, eap, sizeof(eap));
	CHECK(req_calls == 0, "an EAP length beyond the frame draws no request");
	eap[3] = 8;
	rx(5, EAPOL_TYPE_EAP, eap, sizeof(eap));
	CHECK(req_calls == 0, "a response on a port that is not auto is ignored");
	rx(2, 9, eap, sizeof(eap));
	CHECK(req_calls == 0, "an unknown EAPOL type is ignored");
	memset(longname, 'n', sizeof(longname) - 1); longname[sizeof(longname) - 1] = 0;
	rx_identity(2, 1, longname);
	CHECK(req_calls == 1 && strlen(req_identity) == EAPOL_IDENTITY_MAX, "a long identity is cut to the buffer");
	CHECK(eapol_ports[2].identity[EAPOL_IDENTITY_MAX] == 0, "and stays terminated");
	uip_buf[20] = 0x08; uip_buf[21] = 0x00;
	CHECK(!eapol_in(), "an IP frame is not EAPOL");
}

static void t_reauth_and_link(void)
{
	printf("[test] reauthentication and link loss\n");
	setup(2);
	eapol_tick();
	rx_identity(2, eap_in_frame()[1], "alice");
	eapol_radius_result(2, RADIUS_CODE_ACCESS_ACCEPT, 0, 0);
	eapol_poll();
	frames = 0;
	ticks(EAPOL_REAUTH_S);
	CHECK(frames == 1 && eap_in_frame()[0] == EAP_CODE_REQUEST, "the reauthentication asks for the identity again");
	CHECK(eapol_ports[2].state == EAPOL_STATE_AUTHENTICATED && auth_calls == 1, "the port stays authorized meanwhile");
	ticks(EAPOL_TX_PERIOD_S * (EAPOL_MAX_REQUESTS - 1));
	CHECK(frames == EAPOL_MAX_REQUESTS && auth_calls == 1, "three requests, still authorized");
	ticks(EAPOL_TX_PERIOD_S);
	CHECK(auth_calls == 2 && auth_on == 0 && eapol_ports[2].state == EAPOL_STATE_IDLE, "a supplicant silent on reauthentication is dropped");
	CHECK(strstr(out_buf, "silent on reauthentication alice") != 0, "and logged");
	setup(2);
	eapol_tick();
	rx_identity(2, eap_in_frame()[1], "alice");
	eapol_radius_result(2, RADIUS_CODE_ACCESS_ACCEPT, 0, 0);
	links(0);
	eapol_tick();
	CHECK(auth_calls == 2 && auth_on == 0 && eapol_ports[2].state == EAPOL_STATE_IDLE, "link down withdraws authorization");
	links(1 << 2);
	frames = 0;
	eapol_tick();
	CHECK(frames == 1 && eapol_ports[2].state == EAPOL_STATE_CONNECTING, "link up starts over");
}

int main(void)
{
	t_identity_request();
	t_other_ports_quiet();
	t_full_exchange();
	t_reject_and_timeout();
	t_mab();
	t_rx_guards();
	t_reauth_and_link();
	printf("%d checks, %d failed\n", tests_run, tests_failed);
	return tests_failed ? 1 : 0;
}
