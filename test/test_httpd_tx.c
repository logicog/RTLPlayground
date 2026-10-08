/*
 * Host-side bench for httpd.
 *
 * The transmit path: compiles the UNMODIFIED httpd/httpd.c and uip/uip.c and
 * drives them through a real handshake and a real GET from a client that moves
 * its receive window. Everything below the two modules - flash, console, JSON
 * pages - is mocked here, so what the bench observes is the byte stream the
 * firmware would put on the wire.
 *
 * The firmware update: httpd/update_stage.c, update_pool.c and update_apply.c
 * are compiled in UNMODIFIED too, over the flash model in flash_mock.c, and the
 * upload is driven through the real multipart POST handler.
 *
 * The file served out of the simulated flash carries a position-dependent
 * pattern, so a duplicated or skipped range shows up as a mismatch at a known
 * offset instead of an anonymous "content differs".
 *
 * Run: make -C test    (exit code 0 = all scenarios pass)
 */
#include <setjmp.h>
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
#include "update_pool.h"
#include "update_stage.h"
#include "update_apply.h"
#include "page_impl.h"
#include "html_data.h"
#include "flash_mock.h"

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
const uint8_t * const hex = (const uint8_t *)"0123456789abcdef";
uint16_t crc_value;
const uint8_t * const HTTP_RESPONCE_TXT = (const uint8_t *)"HTTP/1.1 200 OK\r\n\r\n";
uint32_t flash_size = 0x80000;
uint8_t flash_buf[FLASH_BUF_SIZE];
uint8_t rx_headers[16];
struct flash_region_t flash_region;

const char * const mime_strings[] = { "text/html", "image/svg+xml", "image/x-icon",
			 "image/png", "text/javascript", "text/css", "text/plain" };

const struct f_data f_data[] = {
	{ FILE_NAME, FILE_START, FILE_LEN, mime_HTML, 0 },
	{ 0, 0, 0, mime_HTML, 0 },
};

/* The flash below the firmware is test/flash_mock.c; the file the bench serves
 * is written into it by session_start(). */

/* CRC16 as the firmware's crc16_bank1 and the browser implement it */
void crc16_bank1(uint8_t *p)
{
	crc_value ^= *p;
	for (int i = 0; i < 8; i++)
		crc_value = (crc_value & 1) ? (crc_value >> 1) ^ 0xA001
					    : crc_value >> 1;
}

/* The file the bench serves, in the flash model */
static void load_served_file(void)
{
	for (int i = 0; i < FILE_LEN; i++)
		flash_mock[FILE_START + i] = pattern(FILE_START + i);
}
static int reset_chip_calls;
void reset_chip(void) { reset_chip_calls++; }
void delay(uint16_t t) { (void)t; }
void write_char(char c) { (void)c; }
void write_char_no_syslog(char c) { (void)c; }

/* What the switch would print, so a scenario can look for it */
static char console[8192];
static int console_len;

void print_string(const char *p)
{
	int n = (int)strlen(p);

	if (console_len + n >= (int)sizeof(console))
		return;
	memcpy(console + console_len, p, n);
	console_len += n;
	console[console_len] = 0;
}

static void console_clear(void) { console_len = 0; console[0] = 0; }
static int console_has(const char *s) { return strstr(console, s) != 0; }

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

static void client_send(uint8_t flags, const void *payload, int plen)
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
	flash_mock_reset();
	load_served_file();
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

/* ---- firmware update ---------------------------------------------------- */

#define IMG_BYTES	524288u			/* one firmware image */
#define UI_BYTES	0x1000u			/* the "web UI" inside the image */
#define UI_END		(FILE_START + FILE_LEN)
#define POOL_FLOOR	((UI_END + FLASH_SECTOR_SIZE - 1) & ~(FLASH_SECTOR_SIZE - 1))
#define SESSION		"0123456789ab"
#define TOKEN		"RTLPbench"

extern char session_id[];

static uint8_t image[IMG_BYTES];		/* the image a scenario uploads */
static uint8_t cut_phase;			/* cut once this state flag is set */
static uint32_t cut_at;
static int cut_active;
static jmp_buf cut;

/* Called after every flash write; a scenario cuts the power here */
static void cut_after_write(void)
{
	if (!cut_active || flash_mock_writes < cut_at)
		return;
	if (cut_phase && !(update_state.flags & cut_phase))
		return;
	longjmp(cut, 1);
}

/* Appends the CRC the build puts on the end of an image: the complement of the
 * CRC of the rest, so that the whole image checksums to 0xb001, the verdict
 * both the upload and the apply look for. */
static void image_fix_crc(void)
{
	crc_value = 0;
	for (uint32_t i = 0; i < IMG_BYTES - 2; i++)
		crc16_bank1(&image[i]);
	crc_value ^= 0xffff;
	image[IMG_BYTES - 2] = (uint8_t)crc_value;
	image[IMG_BYTES - 1] = (uint8_t)(crc_value >> 8);
}

/* One image: code sectors at the bottom, a UI block, the default configuration
 * sector, zeros everywhere else. */
static void image_build(unsigned code_sectors)
{
	memset(image, 0, sizeof(image));
	image[0] = 0x00;			/* the prefetch header the next boot wants */
	image[1] = 0x40;
	image[2] = 0x02;			/* the LJMP it jumps to */
	for (uint32_t i = 3; i < (uint32_t)code_sectors * FLASH_SECTOR_SIZE; i++)
		image[i] = (uint8_t)(i * 7 + 3);
	for (uint32_t i = 0; i < UI_BYTES; i++)
		image[FILE_START + i] = (uint8_t)(i ^ 0x5a);
	image[DEFAULT_CONFIG_START] = 'i';
	image[DEFAULT_CONFIG_START + 1] = 'p';
	image_fix_crc();
}

static uint16_t image_crc(void)
{
	crc_value = 0;
	for (uint32_t i = 0; i < IMG_BYTES; i++)
		crc16_bank1(&image[i]);
	return crc_value;
}

/* CRC over the part of the image the apply writes */
static uint16_t image_apply_crc(void)
{
	crc_value = 0;
	for (uint32_t i = 0; i < CONFIG_START; i++)
		crc16_bank1(&image[i]);
	return crc_value;
}

/* The flash must hold the image below the configuration, the rest untouched */
static int image_in_place(void)
{
	for (uint32_t i = 0; i < CONFIG_START; i++)
		if (flash_mock[i] != image[i])
			return (int)i;
	return -1;
}

static int all_ff(uint32_t addr, uint32_t len)
{
	for (uint32_t i = 0; i < len; i++)
		if (flash_mock[addr + i] != 0xff)
			return 0;
	return 1;
}

/* Fresh flash with the served file in it, a session and an image to upload */
static void upload_setup(void)
{
	memcpy(session_id, SESSION, sizeof(SESSION));
	memset(&update_state, 0, sizeof(update_state));
	image_build(3);
	console_clear();
	reset_chip_calls = 0;
	cut_active = 0;
	session_start(MSS_FULL);
}

static const char upload_head[] =
	"POST /upload HTTP/1.1\r\nHost: sw\r\n"
	"Cookie: session=" SESSION "\r\n"
	"Content-Type: multipart/form-data; boundary=" TOKEN "\r\n\r\n";
/* Chrome labels a .bin file application/macbinary on macOS and Firefox
 * application/octet-stream: the part headers are found by position, never read,
 * so every type has to work */
static const char part_head[] =
	"--" TOKEN "\r\n"
	"Content-Disposition: form-data; name=\"uploadedfile\"; filename=\"img.bin\"\r\n"
	"Content-Type: application/macbinary\r\n\r\n";
static const char part_tail[] = "\r\n--" TOKEN "--\r\n";

/* POST /upload with `len` bytes of `img`, in the segments a browser would send,
 * and the reply drained. Returns the HTTP status, 0 when none arrived. */
static int upload_post(const uint8_t *img, uint32_t len, int send_tail)
{
	static uint8_t seg[MSS_FULL];
	uint32_t i, n;

	memcpy(seg, upload_head, sizeof(upload_head) - 1);
	n = sizeof(upload_head) - 1;
	memcpy(seg + n, part_head, sizeof(part_head) - 1);
	n += sizeof(part_head) - 1;
	i = len < MSS_FULL - n ? len : MSS_FULL - n;
	memcpy(seg + n, img, i);
	n += i;
	client_send(TCP_ACK | TCP_PSH, seg, (int)n);
	while (i < len) {
		uint32_t take = len - i < MSS_FULL ? len - i : MSS_FULL;

		client_send(TCP_ACK | TCP_PSH, img + i, (int)take);
		i += take;
	}
	if (send_tail)
		client_send(TCP_ACK | TCP_PSH, part_tail, (int)(sizeof(part_tail) - 1));
	drain(MSS_FULL);

	if (stream_len < 12 || memcmp(stream, "HTTP/1.1 ", 9))
		return 0;
	return atoi((const char *)stream + 9);
}

/* The reason the switch put in the reply body */
static const char *upload_reason(void)
{
	int off = body_offset();

	return off < 0 ? "" : (const char *)stream + off;
}

/* The pool layout the upload and the apply have to agree on */
static void scenario_pool_geometry(void)
{
	__xdata uint16_t b_slots = (uint16_t)((DEFAULT_CONFIG_START - POOL_FLOOR)
					      / FLASH_SECTOR_SIZE);
	__xdata uint16_t last = 24 + b_slots - 1;

	memset(&update_state, 0, sizeof(update_state));

	CHECK(update_pool_set_bottom(UI_END) == POOL_FLOOR,
	      "pool: the floor is rounded up to a sector");
	CHECK(update_pool_addr(0) == 0x3f000 && update_pool_addr(23) == 0x28000,
	      "pool: the first chunk is the dead space below the UI");
	CHECK(update_pool_addr(24) == 0x6e000 && update_pool_addr(last) == POOL_FLOOR,
	      "pool: the second chunk ends above the UI");
	CHECK(update_pool_addr(last + 1) == 0x7d000
	      && update_pool_addr(last + 13) == 0x71000,
	      "pool: the third chunk is the padding above the live configuration");
	CHECK(update_pool_addr(last + 14) == 0,
	      "pool: there is no room beyond the third chunk");
	CHECK(update_pool_set_bottom(0x4d457) == 0x4e000
	      && update_pool_addr(24) == 0x6e000 && update_pool_addr(56) == 0x4e000
	      && update_pool_addr(57) == 0x7d000,
	      "pool: a taller UI moves the floor and shrinks the second chunk");

	update_pool_set_bottom(UI_END);
	update_state.present[64 >> 3] = 1 << (64 & 7);
	update_state.staged = 1;
	CHECK(update_pool_index(64) == 0 && update_pool_index(65) == 1,
	      "pool: the manifest numbers the slots");
	CHECK(update_pool_conflict(0x3f000) == 1 && update_pool_conflict(0x20000) == 0,
	      "pool: a target inside the used pool is a conflict");
	memset(&update_state, 0, sizeof(update_state));
	update_pool_set_bottom(0);
}

/* Staging: the sectors that hold data land in the pool, the record is written,
 * the running image is untouched. */
static void scenario_update_stage(void)
{
	int st;

	upload_setup();
	st = upload_post(image, IMG_BYTES, 1);

	CHECK(image_crc() == IMAGE_CRC, "staging: the fixture image checksums to IMAGE_CRC");
	CHECK(st == 200, "staging: the upload is accepted");
	CHECK(update_state.magic == UPDATE_STATE_MAGIC
	      && update_state.flags == UPDATE_STATE_STAGING,
	      "staging: the update record is committed");
	CHECK(update_state.crc == IMAGE_CRC && update_state.crc_apply == image_apply_crc(),
	      "staging: the record carries the checksum of the applied part");
	CHECK(update_state.staged == 5, "staging: five image sectors hold data");
	CHECK(update_state.pool_bottom == POOL_FLOOR,
	      "staging: the pool starts above the web UI");
	CHECK(update_pool_addr(0) == 0x3f000 && update_pool_addr(4) == 0x3b000,
	      "staging: the slots are filled from the top downwards");
	CHECK(update_pool_index(64) == 3 && update_pool_index(111) == 4,
	      "staging: the manifest maps image sectors to slots");
	CHECK(!memcmp(flash_mock + 0x3f000, image, FLASH_SECTOR_SIZE)
	      && !memcmp(flash_mock + 0x3c000, image + 64 * FLASH_SECTOR_SIZE, FLASH_SECTOR_SIZE)
	      && !memcmp(flash_mock + 0x3b000, image + 111 * FLASH_SECTOR_SIZE, FLASH_SECTOR_SIZE),
	      "staging: the pool holds the staged sectors");
	CHECK(all_ff(0, 0x28000), "staging: the running image is untouched");
	CHECK(flash_mock[FILE_START] == pattern(FILE_START),
	      "staging: the running web UI is untouched");
}

/*
 * The third chunk is what makes an image of this size stageable at all. A board
 * whose running UI ends high up leaves only a few slots above it, so this drives
 * the staging directly with that floor: 39 sectors do not fit into the two
 * chunks below, they need the padding above the live configuration.
 */
static void scenario_update_stage_tail(void)
{
	__xdata uint16_t s, p;

	upload_setup();
	image_build(37);			/* 37 code + the UI + the configuration */
	CHECK(update_pool_set_bottom(0x61000) == 0x61000,
	      "tail: the floor is where a tall running UI ends");

	update_stage_begin(0x61000);
	for (s = 0; s < UPDATE_APPLY_SECTORS; s++) {
		for (p = 0; p < FLASH_SECTOR_SIZE / FLASH_PAGE_SIZE; p++) {
			memcpy(flash_buf, image + (uint32_t)s * FLASH_SECTOR_SIZE
			       + (uint32_t)p * FLASH_PAGE_SIZE, FLASH_PAGE_SIZE);
			if (update_stage_page()) {
				CHECK(0, "tail: the staging does not run out of room");
				return;
			}
		}
	}
	CHECK(update_stage_end() == UPDATE_STAGE_OK, "tail: the image is staged completely");
	CHECK(update_state.staged == 39, "tail: every sector that holds data is staged");
	CHECK(update_pool_addr(24) == 0x6e000 && update_pool_addr(37) == 0x61000,
	      "tail: the second chunk is the short one here");
	CHECK(update_pool_addr(38) == 0x7d000,
	      "tail: the slot that would not fit lands in the third chunk");
	CHECK(!memcmp(flash_mock + 0x7d000, image + DEFAULT_CONFIG_START, FLASH_SECTOR_SIZE),
	      "tail: it holds the sector that needed it");
}

/* An image that grew into the pool would be overwritten by its own copy back */
static void scenario_update_refuse_overlap(void)
{
	int st;

	upload_setup();
	memset(image + 0x3f000, 0x11, 0x100);
	image_fix_crc();
	st = upload_post(image, IMG_BYTES, 1);

	CHECK(st == 400, "refusal: an image that reaches into the pool is rejected");
	CHECK(strstr(upload_reason(), "overlaps its staging area") != 0,
	      "refusal: the reason names the overlap");
	CHECK(update_state.magic == 0, "refusal: no update record is left");
	CHECK(all_ff(0, 0x28000), "refusal: the running image is untouched");
}

/* A body that does not check out leaves nothing behind */
static void scenario_update_refuse_checksum(void)
{
	int st;

	upload_setup();
	image[0x100] ^= 0xff;
	st = upload_post(image, IMG_BYTES, 1);

	CHECK(st == 400, "refusal: a corrupted image is rejected");
	CHECK(strstr(upload_reason(), "checksum failed") != 0,
	      "refusal: the reason names the checksum");
	CHECK(update_state.magic == 0, "refusal: no update record is left");
}

/* A part that cannot hold the image at all is refused before the body */
static void scenario_update_refuse_small_flash(void)
{
	int st;

	upload_setup();
	flash_size = 0x40000;
	st = upload_post(image, 0, 0);
	flash_size = 0x80000;

	CHECK(st == 400, "refusal: a part smaller than the image is refused");
	CHECK(console_has("Flash too small"), "refusal: the console says why");
	CHECK(strstr(upload_reason(), "flash too small for a 512 KiB image") != 0,
	      "refusal: the reply carries a reason the web UI can show");
}

/* The apply: verify, copy the staged sectors, zero the rest, keep the
 * configuration, clear the record. */
static void scenario_update_apply(void)
{
	int resets;

	upload_setup();
	if (upload_post(image, IMG_BYTES, 1) != 200) {
		CHECK(0, "apply: the staging upload was accepted");
		return;
	}
	flash_mock[CONFIG_START + 5] = 0xaa;	/* the live configuration */
	console_clear();
	resets = reset_chip_calls;
	update_apply_staged();

	CHECK(image_in_place() == -1, "apply: the image below the configuration is in place");
	CHECK(flash_mock[CONFIG_START + 5] == 0xaa, "apply: the live configuration is kept");
	CHECK(update_state.magic == 0, "apply: the record is cleared");
	CHECK(reset_chip_calls == resets + 1, "apply: the switch is reset to boot the image");
	CHECK(console_has("Checking staged image") && console_has("Copying update")
	      && console_has("Writing zeros"),
	      "apply: it verifies, copies and zeroes");
	CHECK(flash_mock_nonzero(0x28000, 0x18000) == 0,
	      "apply: the pool and the space around it read zero again");
}

/* A reset in the middle of the copy: the next boot resumes it */
static void scenario_update_resume_copy(void)
{
	upload_setup();
	if (upload_post(image, IMG_BYTES, 1) != 200) {
		CHECK(0, "resume: the staging upload was accepted");
		return;
	}
	cut_at = flash_mock_writes + 4;	/* into the first staged sector */
	cut_phase = 0;
	cut_active = 1;
	if (setjmp(cut) == 0) {
		update_apply_staged();
		CHECK(0, "resume: the copy is interrupted");
	}
	cut_active = 0;

	CHECK(update_state.flags == (UPDATE_STATE_STAGING | UPDATE_STATE_APPLY),
	      "resume: the record says the copy started");
	CHECK(image_in_place() != -1, "resume: the copy stopped part way");

	console_clear();
	memset(&update_state, 0, sizeof(update_state));	/* a reset loses xdata */
	update_state_read();
	update_apply_staged();

	CHECK(image_in_place() == -1, "resume: the next boot completes the image");
	CHECK(console_has("resuming"), "resume: the boot says it resumes");
	CHECK(!console_has("Checking staged image"), "resume: the image is not verified again");
	CHECK(console_has("Copying update"), "resume: the copy is repeated");
}

/* A reset in the middle of the zeroing: the pool is already gone, so the next
 * boot must not go back to it. */
static void scenario_update_resume_zeroing(void)
{
	upload_setup();
	if (upload_post(image, IMG_BYTES, 1) != 200) {
		CHECK(0, "resume2: the staging upload was accepted");
		return;
	}
	cut_at = flash_mock_writes + 2;	/* the record write and the first zero */
	cut_phase = UPDATE_STATE_ZEROING;
	cut_active = 1;
	if (setjmp(cut) == 0) {
		update_apply_staged();
		CHECK(0, "resume2: the zeroing is interrupted");
	}
	cut_active = 0;

	CHECK(update_state.flags == (UPDATE_STATE_STAGING | UPDATE_STATE_APPLY
				     | UPDATE_STATE_ZEROING),
	      "resume2: the record says the copy is done");
	console_clear();
	memset(&update_state, 0, sizeof(update_state));
	update_state_read();
	update_apply_staged();

	CHECK(image_in_place() == -1, "resume2: the next boot finishes the image");
	CHECK(!console_has("Copying update"), "resume2: the staged sectors are not copied again");
	CHECK(console_has("Writing zeros"), "resume2: the zeroing is repeated");
}

/* A damaged staged image is refused, and the running one is left alone */
static void scenario_update_damaged(void)
{
	upload_setup();
	if (upload_post(image, IMG_BYTES, 1) != 200) {
		CHECK(0, "damaged: the staging upload was accepted");
		return;
	}
	flash_mock[0x3f000 + 0x40] ^= 0xff;	/* a sector that wore out */
	console_clear();
	update_apply_staged();

	CHECK(console_has("damaged"), "damaged: the staged image is rejected");
	CHECK(all_ff(0, 0x28000), "damaged: the running image is left alone");
	CHECK(update_state.magic == 0, "damaged: the record is cleared");
}

/* A request answered before its body was read: the rest of the body belongs to
 * that request and must not be parsed - and answered - on its own */
static void scenario_update_refused_body(void)
{
	static uint8_t seg[MSS_FULL];
	uint32_t head, n, i, sent, take;
	const char *p;
	int replies = 0;

	upload_setup();
	session_id[0] = 'x';		/* the cookie no longer matches */

	head = sizeof(upload_head) - 1 + sizeof(part_head) - 1;
	memcpy(seg, upload_head, sizeof(upload_head) - 1);
	memcpy(seg + sizeof(upload_head) - 1, part_head, sizeof(part_head) - 1);
	n = head;
	i = n < MSS_FULL ? MSS_FULL - n : 0;
	memcpy(seg + n, image, i);
	n += i;
	client_send(TCP_ACK | TCP_PSH, seg, (int)n);

	/* what the browser keeps sending while the reply is on its way */
	for (sent = i; sent < 6 * MSS_FULL; sent += take) {
		take = 6 * MSS_FULL - sent;
		if (take > MSS_FULL)
			take = MSS_FULL;
		client_send(TCP_ACK | TCP_PSH, image + sent, (int)take);
	}
	drain(MSS_FULL);

	for (p = (const char *)stream; (p = strstr(p, "HTTP/1.1 ")) != 0; p++)
		replies++;
	CHECK(replies == 1, "refused: the rest of the body is answered once");
	CHECK(stream_len >= 12 && atoi((const char *)stream + 9) == 401,
	      "refused: the one reply is the 401");
	CHECK(update_state.staged == 0 && update_state.magic == 0,
	      "refused: nothing is staged");
	CHECK(update_state.present[0] == 0, "refused: the pool manifest stays empty");
}

/* A part with room for a second complete image keeps the old upload path */
static void scenario_update_legacy(void)
{
	int st;

	upload_setup();
	flash_size = 0x100000;
	st = upload_post(image, IMG_BYTES, 1);
	flash_size = 0x80000;

	CHECK(st == 200, "legacy: the upload is accepted");
	CHECK(!memcmp(flash_mock + FIRMWARE_UPLOAD_START, image, IMG_BYTES),
	      "legacy: the image is written above the running one");
	CHECK(update_state.magic == 0, "legacy: the staging pool is not used");
	CHECK(flash_mock[0] == 0xff, "legacy: nothing is written at address 0");
}

int main(int argc, char **argv)
{
	if (argc > 1 && !strcmp(argv[1], "-v"))
		verbose = 1;

	flash_mock_after_write = cut_after_write;

	printf("== httpd: accounting for the bytes actually sent ==\n");
	scenario_steady_window();
	scenario_shrinking_window();
	scenario_growing_window();
	scenario_rexmit_after_shrink();
	scenario_bad_l4_checksum();

	printf("\n== httpd: firmware update ==\n");
	scenario_pool_geometry();
	scenario_update_stage();
	scenario_update_stage_tail();
	scenario_update_refuse_overlap();
	scenario_update_refuse_checksum();
	scenario_update_refuse_small_flash();
	scenario_update_refused_body();
	scenario_update_apply();
	scenario_update_resume_copy();
	scenario_update_resume_zeroing();
	scenario_update_damaged();
	scenario_update_legacy();

	printf("\n%s (%d failure%s)\n",
	       failures ? "BENCH: FAILURES" : "BENCH: ALL PASS",
	       failures, failures == 1 ? "" : "s");
	return failures ? 1 : 0;
}
