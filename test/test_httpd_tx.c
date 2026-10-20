/*
 * Host-side bench for the httpd transmit path.
 *
 * Compiles the UNMODIFIED httpd/httpd.c and uip/uip.c and drives them through a
 * real handshake and a real GET from a client that moves its receive window.
 * Everything below the two modules - flash, console, JSON pages - is mocked
 * here, so what the bench observes is the byte stream the firmware would put on
 * the wire.
 *
 * The file served out of the simulated flash carries a position-dependent
 * pattern, so a duplicated or skipped range shows up as a mismatch at a known
 * offset instead of an anonymous "content differs".
 *
 * Run: make -C test    (exit code 0 = all scenarios pass)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Some hosts ship these in <sys/_endian.h>, and they assign to their argument;
 * uip.h refuses to be included next to them. The firmware definitions are the
 * ones this bench needs, so the host macros go first. */
#undef HTONS
#undef NTOHS

#include "httpd.h"
#include "uip.h"
#include "rtl837x_common.h"
#include "rtl837x_regs.h"
#include "rtl837x_flash.h"
#include "page_impl.h"
#include "html_data.h"

/* Private to uip.c, and the client needs them to build segments. */
#define TCP_SYN 0x02
#define TCP_PSH 0x08
#define TCP_ACK 0x10

#define TCPH		((struct uip_tcpip_hdr *)&uip_buf[UIP_LLH_LEN])

#define MSS_FULL	1460
#define SMALL_WINDOW	600

/* httpd.c serves this one file without a session, which keeps the bench clear
 * of the login machinery: what is under test is the transmit path. */
#define FILE_START	FDATA_START_login_html
#define FILE_NAME	"/login.html"
#define FILE_LEN	6000
#define PROTECTED_FILE_NAME	"/index.html"
#define PROTECTED_FILE_START	(FILE_START + FILE_LEN + 64)
#define PROTECTED_FILE_LEN	64
#define STREAM_MAX	16384

static int failures;
static int verbose;

#define CHECK(cond, name) do { \
	if (cond) printf("PASS  %s\n", name); \
	else { printf("FAIL  %s\n", name); failures++; } \
} while (0)

static uint8_t pattern(uint32_t addr)
{
	return (uint8_t)((addr * 7u) + (addr >> 8));
}

/* ---- firmware environment below httpd.c --------------------------------- */

volatile uint8_t sfr_data[4];
volatile uint32_t ticks;
uint8_t cmd_capture;
uint8_t err_status;
const uint8_t * const hex = (const uint8_t *)"0123456789abcdef";
uint16_t crc_value;
const uint8_t * const HTTP_RESPONCE_TXT = (const uint8_t *)"HTTP/1.1 200 OK\r\n\r\n";
uint32_t flash_size = 0x80000;
uint8_t flash_buf[FLASH_BUF_SIZE];
uint8_t rx_headers[16];
struct flash_region_t flash_region;

static uint8_t written_config[CONFIG_LEN];
static uint16_t written_config_len;
static uint32_t config_erase_addr, config_write_addr;
static int config_erases, config_writes;

const char * const mime_strings[] = { "text/html", "image/svg+xml", "image/x-icon",
			 "image/png", "text/javascript", "text/css", "text/plain" };

const struct f_data f_data[] = {
	{ FILE_NAME, FILE_START, FILE_LEN, mime_HTML, 0 },
	{ PROTECTED_FILE_NAME, PROTECTED_FILE_START, PROTECTED_FILE_LEN, mime_HTML, 0 },
	{ 0, 0, 0, mime_HTML, 0 },
};

void flash_read_bulk(uint8_t *dst)
{
	for (uint16_t i = 0; i < flash_region.len; i++)
		dst[i] = pattern(flash_region.addr + i);
}

void flash_init(uint8_t enable_dio) { (void)enable_dio; }
void flash_sector_erase(void)
{
	config_erases++;
	config_erase_addr = flash_region.addr;
}

void flash_write_bytes(uint8_t *ptr)
{
	config_writes++;
	config_write_addr = flash_region.addr;
	written_config_len = flash_region.len;
	if (written_config_len <= sizeof(written_config))
		memcpy(written_config, ptr, written_config_len);
}
const char *get_flash_size_str(void) { return "512 kB"; }
void crc16_bank1(uint8_t *v) { (void)v; }
static unsigned reset_calls;
void reset_chip(void) { reset_calls++; }
void delay(uint16_t t) { (void)t; }
void write_char(char c) { (void)c; }
void write_char_no_syslog(char c) { (void)c; }
void print_string(const char *p) { (void)p; }
void print_string_newline_no_syslog(const char *p) { (void)p; }
void set_sys_led_state(uint8_t state) { (void)state; }
void cmd_parser(void) { }
void execute_config(void) { }
void execute_commands(uint8_t *p) { (void)p; }
void clear_command_history(void) { }
void udp_callbacks(void) { }
void tcpip_output(void) { }
void get_random_32(void) { }
void read_reg_timer(uint32_t *tmr) { *tmr = 0; }

/* uip-conf.h asks uIP not to define the packet buffer: on the switch it lives
 * at a fixed XDATA address, so the bench provides the storage itself. */
u8_t uip_buf[UIP_CONF_BUFFER_ALLOC_SIZE];

uint16_t strlen_x(const char *s) { return (uint16_t)strlen(s); }

uint16_t strtox(uint8_t *dst, const char *s)
{
	uint16_t n = 0;

	while (s[n]) { dst[n] = (uint8_t)s[n]; n++; }
	return n;
}

void memcpyc(uint8_t *dst, const uint8_t *src, uint16_t len) { memcpy(dst, src, len); }

bool strstart(const uint8_t *a, const uint8_t *b)
{
	while (*b) { if (*a++ != *b++) return false; }
	return true;
}

bool strstart_x(const uint8_t *a, const uint8_t *b) { return strstart(a, b); }

bool send_counters(uint8_t phys_port) { (void)phys_port; return false; }
void send_status(void) { }
void send_vlan(uint16_t vlan) { (void)vlan; }
void send_basic_info(void) { }
void send_bandwidth(void) { }
void send_storm(void) { }
void send_isolation(void) { }
void send_eee(void) { }
void send_l2(uint16_t idx) { (void)idx; }
void l2_delete(uint16_t idx) { (void)idx; }
void send_mirror(void) { }
void send_mtu(void) { }
void send_config(void) { }
void send_cmd_log(void) { }
void send_lacp(void) { }
void send_lag(void) { }
void send_stp(void) { }
void send_stp_counters(void) { }
void send_vlanlist(void) { }
void send_lldp(void) {}

extern uint8_t authenticated;
extern char session_id[];
extern char passwd[21];

/* uip.c defines the listen table but no header declares it; the bench reads it
 * to confirm the port httpd_init() asked for is the one uIP is watching. */
extern u16_t uip_listenports[UIP_LISTENPORTS];

/* ---- the simulated client ----------------------------------------------- */

static uint32_t cli_seq;	/* next sequence number we send */
static uint32_t cli_rcv_nxt;	/* next sequence number we expect */
static uint16_t cli_window;	/* what we advertise */

static uint8_t stream[STREAM_MAX];	/* bytes accepted from the server */
static int stream_len;

static uint8_t last_tx[MSS_FULL + 64];	/* payload of the last server segment */
static int last_tx_len;

static uint16_t hton16(uint16_t v)
{
	return (uint16_t)((v << 8) | (v >> 8));
}

static void wr32(uint8_t *p, uint32_t v)
{
	p[0] = v >> 24; p[1] = v >> 16; p[2] = v >> 8; p[3] = v;
}

static uint32_t rd32(const uint8_t *p)
{
	return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
	       ((uint32_t)p[2] << 8) | p[3];
}

static void trace(const char *tag)
{
	if (!verbose)
		return;
	printf("      [%-8s] uip_len=%-5u flags=0x%02x seq=%-6u ack=%-6u | conn: state=0x%02x len=%-5u mss=%-5u tstate=%u\n",
	       tag, uip_len,
	       uip_len ? TCPH->flags : 0,
	       uip_len ? rd32(TCPH->seqno) : 0,
	       uip_len ? rd32(TCPH->ackno) : 0,
	       uip_conns[0].tcpstateflags, uip_conns[0].len, uip_conns[0].mss,
	       uip_conns[0].appstate.tstate);
}

/* Takes in one outgoing segment, if uIP produced one. */
static int harvest(void)
{
	uint8_t *payload;
	uint32_t seq;
	int plen, hlen;

	if (uip_len == 0)
		return -1;

	seq = rd32(TCPH->seqno);
	/* The SYNACK carries the MSS option, so the header is not always 20 B. */
	hlen = (TCPH->tcpoffset >> 4) * 4;
	payload = &uip_buf[UIP_LLH_LEN + 20 + hlen];
	plen = (int)uip_len - 20 - hlen;
	if (plen < 0)
		plen = 0;

	if (plen > 0) {
		last_tx_len = plen > (int)sizeof(last_tx) ? (int)sizeof(last_tx) : plen;
		memcpy(last_tx, payload, last_tx_len);

		/* A real client keeps what continues the stream and drops the
		 * rest, so bytes sent twice under new sequence numbers land in
		 * the file just as they would in a browser. */
		if (seq == cli_rcv_nxt) {
			if (stream_len + plen <= STREAM_MAX) {
				memcpy(stream + stream_len, payload, plen);
				stream_len += plen;
			}
			cli_rcv_nxt += plen;
		}
		if (verbose)
			printf("      server -> %d B, seq %u\n", plen, seq);
	}
	if (TCPH->flags & TCP_SYN)
		cli_rcv_nxt = seq + 1;

	uip_len = 0;
	return plen;
}

static void client_send(uint8_t flags, const char *payload, int plen)
{
	int hlen = 20;

	/* A SYN carries the MSS option, as every real client does: uIP takes the
	 * connection MSS from it, and without one the server has nothing to send
	 * with. */
	if (flags & TCP_SYN)
		hlen = 24;

	memset(uip_buf, 0, UIP_LLH_LEN + 20 + hlen + (plen > 0 ? plen : 0));
	TCPH->vhl = 0x45;
	TCPH->tos = 0;
	TCPH->len[0] = (uint8_t)((20 + hlen + plen) >> 8);
	TCPH->len[1] = (uint8_t)((20 + hlen + plen) & 0xff);
	TCPH->ttl = 64;
	TCPH->proto = UIP_PROTO_TCP;
	TCPH->ipchksum = 0;
	TCPH->srcipaddr[0] = hton16(0x0a00); TCPH->srcipaddr[1] = hton16(0x0002);
	TCPH->destipaddr[0] = hton16(0x0a00); TCPH->destipaddr[1] = hton16(0x0001);
	TCPH->srcport = hton16(40000);
	TCPH->destport = hton16(80);
	wr32(TCPH->seqno, cli_seq);
	wr32(TCPH->ackno, cli_rcv_nxt);
	TCPH->tcpoffset = (uint8_t)((hlen / 4) << 4);
	TCPH->flags = flags;
	TCPH->wnd[0] = (uint8_t)(cli_window >> 8);
	TCPH->wnd[1] = (uint8_t)(cli_window & 0xff);
	TCPH->tcpchksum = 0;

	if (hlen == 24) {
		uint8_t *opt = &uip_buf[UIP_LLH_LEN + 40];

		opt[0] = 2; opt[1] = 4;			/* kind = MSS, length 4 */
		opt[2] = MSS_FULL >> 8; opt[3] = MSS_FULL & 0xff;
	}
	if (plen > 0)
		memcpy(&uip_buf[UIP_LLH_LEN + 20 + hlen], payload, plen);

	uip_len = 20 + hlen + plen;
	uip_input();
	trace("input");
	cli_seq += plen;
	if (flags & TCP_SYN)
		cli_seq++;
	harvest();
}

static void client_ack(uint16_t window)
{
	cli_window = window;
	client_send(TCP_ACK, NULL, 0);
}

static void run_periodic(int rounds)
{
	for (int i = 0; i < rounds; i++) {
		uip_periodic(0);
		trace("timer");
		harvest();
	}
}

static void session_start(uint16_t window)
{
	uip_init();
	httpd_init();
	authenticated = 1;

	cli_seq = 1000;
	cli_rcv_nxt = 0;
	cli_window = window;
	stream_len = 0;
	last_tx_len = 0;

	client_send(TCP_SYN, NULL, 0);
	client_ack(window);
}

static void request_file(uint16_t window)
{
	static const char get[] = "GET " FILE_NAME " HTTP/1.1\r\nHost: sw\r\n\r\n";

	cli_window = window;
	client_send(TCP_ACK | TCP_PSH, get, (int)strlen(get));
}

/* Drains the response, acknowledging every segment with the given window. */
static void drain(uint16_t window)
{
	for (int i = 0; i < 20 && stream_len < STREAM_MAX; i++) {
		client_ack(window);
		run_periodic(1);
	}
}

static int body_offset(void)
{
	for (int i = 0; i + 4 <= stream_len; i++)
		if (!memcmp(stream + i, "\r\n\r\n", 4))
			return i + 4;
	return -1;
}

/* Offset of the first body byte that is not the one the file holds there, or
 * -1 when the body matches, or -2 when no header ever arrived. */
static int first_body_mismatch(void)
{
	int off = body_offset();

	if (off < 0)
		return -2;
	for (int i = 0; i < FILE_LEN && off + i < stream_len; i++)
		if (stream[off + i] != pattern(FILE_START + i))
			return i;
	return -1;
}

static int stream_contains(const char *needle)
{
	size_t n = strlen(needle);

	for (int i = 0; i + (int)n <= stream_len; i++)
		if (!memcmp(stream + i, needle, n))
			return 1;
	return 0;
}

static void login_session_start(void)
{
	session_start(MSS_FULL);
	strcpy(passwd, "1234");
}

static void report(const char *tag)
{
	if (verbose)
		printf("      %s: header %d B, stream %d B, first mismatch %d\n",
		       tag, body_offset(), stream_len, first_body_mismatch());
}

/* ---- scenarios ---------------------------------------------------------- */

/* Instrument check: with a window that never moves, the MSS at ACK time and the
 * length sent earlier are the same number, so the file must arrive intact. If
 * this one fails, the bench is wrong - not the firmware. */
static void scenario_steady_window(void)
{
	session_start(MSS_FULL);
	if (verbose)
		printf("      listen port=0x%04x, initial mss=%u\n",
		       uip_listenports[0], uip_conns[0].initialmss);
	request_file(MSS_FULL);
	drain(MSS_FULL);
	report("steady");

	CHECK(body_offset() >= 0, "control: response carries an HTTP header");
	CHECK(stream_len >= body_offset() + FILE_LEN,
	      "control: the whole file arrives");
	CHECK(first_body_mismatch() == -1,
	      "control: file content without duplicates or gaps");
}

/* The client stops draining, so the window in the ACK is smaller than the
 * segment that ACK covers. */
static void scenario_shrinking_window(void)
{
	session_start(MSS_FULL);
	request_file(MSS_FULL);
	drain(SMALL_WINDOW);
	report("shrinking");

	CHECK(body_offset() >= 0,
	      "shrinking window: response carries an HTTP header");
	CHECK(stream_len >= body_offset() + FILE_LEN,
	      "shrinking window: the whole file arrives");
	CHECK(first_body_mismatch() == -1,
	      "shrinking window: file content without duplicates or gaps");
}

/* The client catches up, so the window in the ACK is larger than the segment
 * that ACK covers. */
static void scenario_growing_window(void)
{
	session_start(SMALL_WINDOW);
	request_file(SMALL_WINDOW);
	drain(MSS_FULL);
	report("growing");

	CHECK(stream_len >= body_offset() + FILE_LEN,
	      "growing window: the whole file arrives");
	CHECK(first_body_mismatch() == -1,
	      "growing window: file content without duplicates or gaps");
}

/* A segment is lost, a window update shrinks the window while it is still
 * unacknowledged, and the retransmission timer fires. */
static void scenario_rexmit_after_shrink(void)
{
	uint8_t original[MSS_FULL + 64];
	int original_len, same = 1;

	session_start(MSS_FULL);
	request_file(MSS_FULL);

	original_len = last_tx_len;
	memcpy(original, last_tx, original_len);

	/* The segment never arrived: take it back out of the client's stream and
	 * send a pure window update, which acknowledges nothing. */
	if (original_len > 0 && stream_len >= original_len) {
		stream_len -= original_len;
		cli_rcv_nxt -= original_len;
	}
	cli_window = SMALL_WINDOW;
	client_send(TCP_ACK, NULL, 0);

	memset(&uip_buf[UIP_LLH_LEN + 40], 0xaa, MSS_FULL);

	last_tx_len = 0;
	run_periodic(UIP_RTO + 2);

	if (last_tx_len != original_len || memcmp(last_tx, original, original_len))
		same = 0;

	if (verbose)
		printf("      first %d B, retransmitted %d B\n",
		       original_len, last_tx_len);

	CHECK(original_len > 0, "retransmission: the first segment went out");
	CHECK(same, "retransmission: the repeat carries the same bytes");
}

static void scenario_bad_l4_checksum(void)
{
	uip_stats_t chkerr_before;
	uint32_t seq_before;
	int len_before;

	session_start(MSS_FULL);

	chkerr_before = uip_stat.tcp.chkerr;
	len_before = stream_len;
	seq_before = cli_seq;

	rx_headers[1] = RX_TAG_L4_CSUM_BAD;
	request_file(MSS_FULL);
	rx_headers[1] = 0;
	cli_seq = seq_before;

	CHECK(uip_stat.tcp.chkerr == chkerr_before + 1,
	      "bad checksum: the segment is counted as a checksum error");
	CHECK(stream_len == len_before,
	      "bad checksum: the request draws no reply");

	request_file(MSS_FULL);
	drain(MSS_FULL);
	report("bad checksum");

	CHECK(body_offset() >= 0,
	      "bad checksum control: the same request with a good flag is served");
	CHECK(first_body_mismatch() == -1,
	      "bad checksum control: the file that follows is intact");
}


/* Private HTTP lifecycle values; drive the real callback, not a model. */
enum { TSTATE_NONE = 0, TSTATE_TX = 1 };
extern uint16_t slen, cont_len, sent_len, bindex, crc_final, write_len;
extern uint8_t boundary[], verify_crc, outbuf[];
extern struct { uint8_t *p; uint16_t bptr, plen; } upload_settings;
extern uint8_t stream_upload(void);

static void scenario_upload_reset_owner(void)
{
	uint8_t byte = 0;
	uint8_t get[] = "GET " FILE_NAME " HTTP/1.1\r\nHost: sw\r\n\r\n";
	int initialized = 1;

	/* The firmware has one TCP slot. Its appstate can contain stale bytes. */
	uip_conn = &uip_conns[0];
	reset_calls = 0;
	for (unsigned state = 0; state <= 0xff; state++) {
		uip_conn->appstate.tstate = state;
		uip_conn->appstate.fw_reset_pending = 0xaa;
		uip_flags = UIP_CONNECTED;
		httpd_appcall();
		if (uip_conn->appstate.tstate != TSTATE_NONE
		    || uip_conn->appstate.fw_reset_pending != 0)
			initialized = 0;
	}
	CHECK(initialized, "connect initializes both fields for every stale state");
	CHECK(reset_calls == 0, "connection initialization does not reset");

	/* End-of-part verdict with flash/CRC hardware edges mocked. */
	boundary[0] = 0;
	bindex = 0;
	verify_crc = 1;
	crc_final = 0xb001;
	write_len = 0;
	upload_settings.p = &byte;
	upload_settings.bptr = 0;
	upload_settings.plen = 1;
	stream_upload();
	CHECK(uip_conn->appstate.fw_reset_pending == 1,
	      "verified upload records reset intent in its slot");

	uip_conn->appstate.tstate = TSTATE_TX;
	uip_conn->mss = MSS_FULL;
	slen = 10;
	sent_len = 5;
	cont_len = 0;
	uip_flags = UIP_ACKDATA;
	httpd_appcall();
	CHECK(reset_calls == 0 && slen == 5 && sent_len == 5,
	      "partial verdict ACK sends the remainder without resetting");
	uip_flags = UIP_ACKDATA;
	httpd_appcall();
	CHECK(reset_calls == 1 && slen == 0,
	      "final verdict ACK resets after all response bytes are acknowledged");

	/* No verdict ACK: a later connection reuses the same slot. */
	reset_calls = 0;
	uip_conn->appstate.tstate = TSTATE_TX;
	uip_conn->appstate.fw_reset_pending = 1;
	uip_flags = UIP_CONNECTED;
	httpd_appcall();
	CHECK(uip_conn->appstate.tstate == TSTATE_NONE
	      && uip_conn->appstate.fw_reset_pending == 0,
	      "slot reuse clears an unacknowledged upload's reset intent");
	uip_appdata = get;
	uip_len = strlen((char *)get);
	uip_flags = UIP_NEWDATA;
	httpd_appcall();
	CHECK(uip_conn->appstate.tstate == TSTATE_TX && slen > 0,
	      "reused slot serves a subsequent plain GET");
	for (unsigned i = 0; i < 20 && uip_conn->appstate.tstate == TSTATE_TX; i++) {
		uip_flags = UIP_ACKDATA;
		httpd_appcall();
	}
	CHECK(reset_calls == 0 && slen == 0 && cont_len == 0,
	      "subsequent GET response ACKs cannot inherit reset intent");

	for (unsigned i = 0; i < 2; i++) {
		uip_conn->appstate.tstate = TSTATE_TX;
		uip_conn->appstate.fw_reset_pending = 1;
		reset_calls = 0;
		uip_flags = i ? UIP_TIMEDOUT : UIP_ABORT;
		httpd_appcall();
		CHECK(reset_calls == 0, "abort/timeout does not reset");
		uip_flags = UIP_CONNECTED;
		httpd_appcall();
		CHECK(uip_conn->appstate.tstate == TSTATE_NONE
		      && uip_conn->appstate.fw_reset_pending == 0,
		      "connect after abort/timeout clears old upload intent");
		uip_conn->appstate.tstate = TSTATE_TX;
		slen = 0;
		cont_len = 0;
		sent_len = 0;
		uip_flags = UIP_ACKDATA;
		httpd_appcall();
		CHECK(reset_calls == 0, "new connection ACK cannot inherit reset");
	}

	crc_final = 0;
	bindex = 0;
	stream_upload();
	CHECK(uip_conn->appstate.fw_reset_pending == 0,
	      "failed CRC does not request reset");
	CHECK(strstr((char *)outbuf, "HTTP/1.1 400 Bad Request") != NULL,
	      "failed CRC returns an error verdict");
	uip_conn->appstate.tstate = TSTATE_TX;
	sent_len = slen;
	cont_len = 0;
	uip_flags = UIP_ACKDATA;
	httpd_appcall();
	CHECK(reset_calls == 0, "failed CRC verdict ACK does not reset");

}

static void scenario_fragmented_post_header(void)
{
	static const char first[] =
		"POST /login HTTP/1.1\r\n"
		"Host: sw\r\n"
		"Content-Len";
	static const char second[] =
		"gth: 8\r\n"
		"Content-Type: application/x-www-form-urlencoded\r\n"
		"\r\n"
		"pwd=1234";

	login_session_start();
	client_send(TCP_ACK | TCP_PSH, first, (int)strlen(first));
	CHECK(stream_len == 0,
	      "fragmented header: incomplete first fragment has no response");

	client_send(TCP_ACK | TCP_PSH, second, (int)strlen(second));
	CHECK(stream_contains("HTTP/1.1 302 Found\r\n"),
	      "fragmented header: split Content-Length is accepted");
	CHECK(stream_contains("Location: index.html\r\n"),
	      "fragmented header: login body is parsed after reassembly");
}

static void scenario_split_content_type(void)
{
	static const char first[] =
		"POST /login HTTP/1.1\r\n"
		"Content-Length: 8\r\n"
		"Content-Ty";
	static const char second[] =
		"pe: application/x-www-form-urlencoded\r\n"
		"\r\n"
		"pwd=1234";

	login_session_start();
	client_send(TCP_ACK | TCP_PSH, first, (int)strlen(first));
	CHECK(stream_len == 0,
	      "split Content-Type: incomplete header has no response");
	client_send(TCP_ACK | TCP_PSH, second, (int)strlen(second));
	CHECK(stream_contains("Location: index.html\r\n"),
	      "split Content-Type: field name can span receive events");
}

static void scenario_split_header_terminator(void)
{
	static const char first[] =
		"POST /login HTTP/1.1\r\n"
		"Content-Length: 8\r\n"
		"Content-Type: application/x-www-form-urlencoded\r\n"
		"\r";
	static const char second[] = "\npwd=1234";

	login_session_start();
	client_send(TCP_ACK | TCP_PSH, first, (int)strlen(first));
	CHECK(stream_len == 0,
	      "split terminator: CRLFCR fragment has no premature response");
	client_send(TCP_ACK | TCP_PSH, second, (int)strlen(second));
	CHECK(stream_contains("Location: index.html\r\n"),
	      "split terminator: CRLFCRLF can span receive events");
}

static void scenario_header_then_body(void)
{
	static const char header[] =
		"POST /login HTTP/1.1\r\n"
		"Content-Length: 8\r\n"
		"Content-Type: application/x-www-form-urlencoded\r\n"
		"\r\n";

	login_session_start();
	client_send(TCP_ACK | TCP_PSH, header, (int)strlen(header));
	CHECK(stream_len == 0,
	      "header-only segment: waits for the declared body");
	client_send(TCP_ACK | TCP_PSH, "pwd=1234", 8);
	CHECK(stream_contains("Location: index.html\r\n"),
	      "header-only segment: following body completes login");
}

static void scenario_header_with_partial_body(void)
{
	static const char first[] =
		"POST /login HTTP/1.1\r\n"
		"Content-Length: 8\r\n"
		"Content-Type: application/x-www-form-urlencoded\r\n"
		"\r\n"
		"pwd=";

	login_session_start();
	client_send(TCP_ACK | TCP_PSH, first, (int)strlen(first));
	CHECK(stream_len == 0,
	      "partial body: waits when final header segment has only part of body");
	client_send(TCP_ACK | TCP_PSH, "1234", 4);
	CHECK(stream_contains("Location: index.html\r\n"),
	      "partial body: existing body reassembly takes over correctly");
}

static void scenario_post_header_timeout(void)
{
	static const char partial[] =
		"POST /login HTTP/1.1\r\n"
		"Content-Length: 8\r\n"
		"Content-Type: application/x-www-form-urlencoded\r\n";

	login_session_start();
	ticks = 0;
	client_send(TCP_ACK | TCP_PSH, partial, (int)strlen(partial));
	CHECK(stream_len == 0,
	      "header timeout: incomplete header has no immediate response");

	/* The firmware timeout is 5 seconds; 2000 host ticks is safely beyond it. */
	ticks = 2000;
	run_periodic(1);
	CHECK(!uip_conn_active(0),
	      "header timeout: connection is aborted when the header never completes");
}

static void scenario_oversized_post_header(void)
{
	static const char start[] =
		"POST /login HTTP/1.1\r\n"
		"X-Fill: ";
	char fill[1200];

	login_session_start();
	memset(fill, 'a', sizeof(fill));
	client_send(TCP_ACK | TCP_PSH, start, (int)strlen(start));

	/* Keep extending a header with no CRLFCRLF until the fixed buffer refuses it. */
	for (int i = 0; i < 16 && !stream_contains("400 Bad Request"); i++)
		client_send(TCP_ACK | TCP_PSH, fill, (int)sizeof(fill));

	CHECK(stream_contains("HTTP/1.1 400 Bad Request\r\n"),
	      "oversized header: rejected before the header buffer overflows");
}

static void scenario_fragmented_missing_content_length(void)
{
	static const char first[] =
		"POST /login HTTP/1.1\r\n"
		"Content-Type: application/x-www-form-urlencoded\r\n";
	static const char second[] = "\r\npwd=1234";

	login_session_start();
	client_send(TCP_ACK | TCP_PSH, first, (int)strlen(first));
	client_send(TCP_ACK | TCP_PSH, second, (int)strlen(second));
	CHECK(stream_contains("HTTP/1.1 411 Length Required\r\n"),
	      "fragmented header: missing Content-Length still returns 411");
}


static void scenario_fragmented_get_cookie(void)
{
	static const char first[] =
		"GET " PROTECTED_FILE_NAME " HTTP/1.1\r\n"
		"Host: sw\r\n"
		"Cook";
	static const char second[] =
		"ie: session=0123456789ab\r\n"
		"\r\n";

	session_start(MSS_FULL);
	strcpy(session_id, "0123456789ab");

	client_send(TCP_ACK | TCP_PSH, first, (int)strlen(first));
	CHECK(stream_len == 0,
	      "fragmented GET cookie: no redirect before the header completes");

	client_send(TCP_ACK | TCP_PSH, second, (int)strlen(second));
	CHECK(stream_contains("HTTP/1.1 200 OK\r\n"),
	      "fragmented GET cookie: protected index is served");
	CHECK(!stream_contains("Location: login.html\r\n"),
	      "fragmented GET cookie: session is not lost on the redirected GET");
}

static void scenario_fragmented_get_cookie_value(void)
{
	static const char first[] =
		"GET " PROTECTED_FILE_NAME " HTTP/1.1\r\n"
		"Host: sw\r\n"
		"Cookie: session=012345";
	static const char second[] =
		"6789ab\r\n"
		"\r\n";

	session_start(MSS_FULL);
	strcpy(session_id, "0123456789ab");

	client_send(TCP_ACK | TCP_PSH, first, (int)strlen(first));
	CHECK(stream_len == 0,
	      "fragmented GET cookie value: waits for the rest of the header");
	client_send(TCP_ACK | TCP_PSH, second, (int)strlen(second));
	CHECK(stream_contains("HTTP/1.1 200 OK\r\n"),
	      "fragmented GET cookie value: split session value authenticates");
}

static void scenario_split_get_header_terminator(void)
{
	static const char first[] =
		"GET " PROTECTED_FILE_NAME " HTTP/1.1\r\n"
		"Cookie: session=0123456789ab\r\n"
		"\r";
	static const char second[] = "\n";

	session_start(MSS_FULL);
	strcpy(session_id, "0123456789ab");

	client_send(TCP_ACK | TCP_PSH, first, (int)strlen(first));
	CHECK(stream_len == 0,
	      "fragmented GET terminator: CRLFCR does not trigger parsing");
	client_send(TCP_ACK | TCP_PSH, second, 1);
	CHECK(stream_contains("HTTP/1.1 200 OK\r\n"),
	      "fragmented GET terminator: CRLFCRLF may span receive events");
}

static void scenario_fragmented_get_without_cookie(void)
{
	static const char first[] =
		"GET " PROTECTED_FILE_NAME " HTTP/1.1\r\n"
		"Host: sw\r\n";
	static const char second[] = "\r\n";

	session_start(MSS_FULL);
	strcpy(session_id, "0123456789ab");

	client_send(TCP_ACK | TCP_PSH, first, (int)strlen(first));
	client_send(TCP_ACK | TCP_PSH, second, (int)strlen(second));
	CHECK(stream_contains("HTTP/1.1 302 Found\r\n"),
	      "fragmented GET without cookie: protected file still redirects");
	CHECK(stream_contains("Location: login.html\r\n"),
	      "fragmented GET without cookie: authentication is not carried over");
}

static void scenario_get_header_timeout(void)
{
	static const char partial[] =
		"GET " PROTECTED_FILE_NAME " HTTP/1.1\r\n"
		"Host: sw\r\n"
		"Cookie: session=012345";

	session_start(MSS_FULL);
	strcpy(session_id, "0123456789ab");
	ticks = 0;

	client_send(TCP_ACK | TCP_PSH, partial, (int)strlen(partial));
	CHECK(stream_len == 0,
	      "fragmented GET timeout: incomplete header has no immediate response");

	ticks = 2000;
	run_periodic(1);
	CHECK(!uip_conn_active(0),
	      "fragmented GET timeout: incomplete request is eventually aborted");
}

static void scenario_oversized_get_header(void)
{
	static const char start[] =
		"GET " PROTECTED_FILE_NAME " HTTP/1.1\r\n"
		"X-Fill: ";
	char fill[1200];

	session_start(MSS_FULL);
	memset(fill, 'a', sizeof(fill));

	client_send(TCP_ACK | TCP_PSH, start, (int)strlen(start));
	for (int i = 0; i < 16 && !stream_contains("400 Bad Request"); i++)
		client_send(TCP_ACK | TCP_PSH, fill, (int)sizeof(fill));

	CHECK(stream_contains("HTTP/1.1 400 Bad Request\r\n"),
	      "oversized GET header: fixed header buffer is bounds checked");
}

static void scenario_split_method_names(void)
{
	static const char get[] =
		"GET " PROTECTED_FILE_NAME " HTTP/1.1\r\n"
		"Cookie: session=0123456789ab\r\n\r\n";
	static const char post[] =
		"POST /login HTTP/1.1\r\n"
		"Content-Length: 8\r\n"
		"Content-Type: application/x-www-form-urlencoded\r\n\r\n"
		"pwd=1234";
	int get_waits = 1, get_ok = 1, post_waits = 1, post_ok = 1;

	for (int split = 1; split < 3; split++) {
		session_start(MSS_FULL);
		strcpy(session_id, "0123456789ab");
		client_send(TCP_ACK | TCP_PSH, get, split);
		if (stream_len)
			get_waits = 0;
		client_send(TCP_ACK | TCP_PSH, get + split, (int)strlen(get) - split);
		if (!stream_contains("HTTP/1.1 200 OK\r\n"))
			get_ok = 0;
	}
	CHECK(get_waits, "split GET method: incomplete verb waits for the header");
	CHECK(get_ok, "split GET method: every split inside GET serves the file");

	for (int split = 1; split < 4; split++) {
		login_session_start();
		client_send(TCP_ACK | TCP_PSH, post, split);
		if (stream_len)
			post_waits = 0;
		client_send(TCP_ACK | TCP_PSH, post + split, (int)strlen(post) - split);
		if (!stream_contains("Location: index.html\r\n"))
			post_ok = 0;
	}
	CHECK(post_waits, "split POST method: incomplete verb waits for the header");
	CHECK(post_ok, "split POST method: every split inside POST completes login");
}

static void scenario_config_upload(void)
{
	static const char expected[] = "hostname upload-test\nip 10.0.0.1\n";
	static const char body[] =
		"--upload-boundary\r\n"
		"Content-Disposition: form-data; name=\"file\"; filename=\"config.txt\"\r\n"
		"Content-Type: text/plain\r\n\r\n"
		"hostname upload-test\nip 10.0.0.1\n"
		"\r\n--upload-boundary--\r\n";
	char request[768];
	int request_len, first_bad_response = -1, first_bad_flash = -1;

	request_len = snprintf(request, sizeof(request),
		"POST /config HTTP/1.1\r\n"
		"Host: sw\r\n"
		"Cookie: session=0123456789ab\r\n"
		"Content-Type: multipart/form-data; boundary=upload-boundary\r\n"
		"Content-Length: %zu\r\n\r\n%s", sizeof(body) - 1, body);
	if (request_len < 0 || request_len >= (int)sizeof(request)) {
		CHECK(0, "multipart/config: test request fits the client buffer");
		return;
	}

	/* The same well-formed request must work in one event and with a split
	 * anywhere in its HTTP header, first multipart delimiter, part headers,
	 * configuration, or closing delimiter. No extra CRLF is added to the body. */
	for (int split = 0; split <= request_len; split++) {
		session_start(MSS_FULL);
		strcpy(session_id, "0123456789ab");
		config_erases = config_writes = 0;
		written_config_len = 0;
		memset(written_config, 0xa5, sizeof(written_config));
		if (split)
			client_send(TCP_ACK | TCP_PSH, request, split);
		if (split < request_len)
			client_send(TCP_ACK | TCP_PSH, request + split, request_len - split);
		if (!stream_contains("HTTP/1.1 200 OK\r\n") && first_bad_response < 0)
			first_bad_response = split;
		if ((config_erases != 1 || config_writes != 1
		     || config_erase_addr != CONFIG_START || config_write_addr != CONFIG_START
		     || written_config_len != sizeof(expected)
		     || memcmp(written_config, expected, sizeof(expected))) && first_bad_flash < 0)
			first_bad_flash = split;
	}
	if (first_bad_response >= 0)
		printf("      multipart/config: first rejected request split at byte %d\n", first_bad_response);
	if (first_bad_flash >= 0)
		printf("      multipart/config: first incorrect flash write at split %d\n", first_bad_flash);
	CHECK(first_bad_response < 0,
	      "multipart/config: complete request and every two-event split return 200");
	CHECK(first_bad_flash < 0,
	      "multipart/config: every split writes the exact configuration and NUL to its sector");
}

int main(int argc, char **argv)
{
	if (argc > 1 && !strcmp(argv[1], "-v"))
		verbose = 1;

	printf("== httpd: accounting for the bytes actually sent ==\n");
	scenario_steady_window();
	scenario_shrinking_window();
	scenario_growing_window();
	scenario_rexmit_after_shrink();
	scenario_bad_l4_checksum();
	scenario_upload_reset_owner();
	scenario_fragmented_post_header();
	scenario_split_content_type();
	scenario_split_header_terminator();
	scenario_header_then_body();
	scenario_header_with_partial_body();
	scenario_post_header_timeout();
	scenario_oversized_post_header();
	scenario_fragmented_missing_content_length();
	scenario_fragmented_get_cookie();
	scenario_fragmented_get_cookie_value();
	scenario_split_get_header_terminator();
	scenario_fragmented_get_without_cookie();
	scenario_get_header_timeout();
	scenario_oversized_get_header();
	scenario_split_method_names();
	scenario_config_upload();

	printf("\n%s (%d failure%s)\n",
	       failures ? "BENCH: FAILURES" : "BENCH: ALL PASS",
	       failures, failures == 1 ? "" : "s");
	return failures ? 1 : 0;
}
