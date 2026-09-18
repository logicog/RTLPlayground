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
uint8_t *hex = (uint8_t *)"0123456789abcdef";
uint16_t crc_value;
uint8_t *HTTP_RESPONCE_TXT = (uint8_t *)"HTTP/1.1 200 OK\r\n\r\n";
uint32_t flash_size = 0x80000;
uint8_t flash_buf[FLASH_BUF_SIZE];
struct flash_region_t flash_region;

char *mime_strings[] = { "text/html", "image/svg+xml", "image/x-icon",
			 "image/png", "text/javascript", "text/css", "text/plain" };

struct f_data f_data[] = {
	{ FILE_NAME, FILE_START, FILE_LEN, mime_HTML, 0 },
	{ 0, 0, 0, mime_HTML, 0 },
};

void flash_read_bulk(uint8_t *dst)
{
	for (uint16_t i = 0; i < flash_region.len; i++)
		dst[i] = pattern(flash_region.addr + i);
}

void flash_init(uint8_t enable_dio) { (void)enable_dio; }
void flash_sector_erase(void) { }
void flash_write_bytes(uint8_t *ptr) { (void)ptr; }
char *get_flash_size_str(void) { return "512 kB"; }
void crc16_bank1(uint8_t *v) { (void)v; }
void reset_chip(void) { }
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
u8_t uip_buf[UIP_BUFSIZE + 2];

uint16_t strlen_x(const char *s) { return (uint16_t)strlen(s); }

uint16_t strtox(uint8_t *dst, const char *s)
{
	uint16_t n = 0;

	while (s[n]) { dst[n] = (uint8_t)s[n]; n++; }
	return n;
}

void memcpyc(uint8_t *dst, uint8_t *src, uint16_t len) { memcpy(dst, src, len); }

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
void send_eee(void) { }
void send_l2(uint16_t idx) { (void)idx; }
void l2_delete(uint16_t idx) { (void)idx; }
void send_mirror(void) { }
void send_mtu(void) { }
void send_config(void) { }
void send_cmd_log(void) { }
void send_lag(void) { }
void send_stp(void) { }
void send_stp_counters(void) { }
void send_vlanlist(void) { }

extern uint8_t authenticated;

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

int main(int argc, char **argv)
{
	if (argc > 1 && !strcmp(argv[1], "-v"))
		verbose = 1;

	printf("== httpd: accounting for the bytes actually sent ==\n");
	scenario_steady_window();
	scenario_shrinking_window();
	scenario_growing_window();
	scenario_rexmit_after_shrink();

	printf("\n%s (%d failure%s)\n",
	       failures ? "BENCH: FAILURES" : "BENCH: ALL PASS",
	       failures, failures == 1 ? "" : "s");
	return failures ? 1 : 0;
}
