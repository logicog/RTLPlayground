/*
 * Minimal telnet server providing the same command line as the serial
 * console and the httpd /cmd endpoint. One session at a time; a second
 * connection attempt is aborted. Login uses the web/admin password.
 *
 * The server negotiates character-at-a-time mode (WILL ECHO + WILL SGA)
 * and does its own echo and line editing, so the password is not echoed.
 *
 * If a bind address is configured (telnet bind <ip>), a connection is
 * only accepted while the switch's own address matches it, so the
 * service follows a specific L3 address instead of whatever the device
 * currently has (relevant with DHCP).
 */
#include <8051.h>
#include "machine.h"
#include "telnetd.h"
#include "cmd_parser.h"
#include "rtl837x_common.h"
#include "sntp.h"
#include "totp.h"
#include "uip.h"

#pragma codeseg BANK3
#pragma constseg BANK3

extern volatile __xdata uint32_t ticks;
extern __xdata char passwd[21];

__xdata struct telnet_state_t telnet_state;
__xdata uint8_t telnet_outbuf[TELNET_OUTBUF];
__xdata uint16_t telnet_slen;
__xdata uint8_t telnet_capture;

/* Region of telnet_outbuf handed to uip_send() and not yet ACKed */
static __xdata uint16_t tn_oidx;
static __xdata uint16_t tn_sent;
static __xdata uint8_t tline[CMD_BUF_SIZE];
static __xdata uip_ipaddr_t tn_bindaddr;

#define tn telnet_state

/* Login window: password plus a TOTP code from a phone takes a while */
#define TN_LOGIN_TICKS  (120UL * SYS_TICK_HZ)
#define TN_IDLE_SECS_DEFAULT 600

/* `ticks` is 4 bytes and incremented in the timer ISR: an unguarded read
 * can tear mid-increment and yield a garbage delta, which made the idle
 * check close sessions at random. */
static __xdata uint32_t tick_snap;

static uint32_t ticks_now(void)
{
	EA = 0;
	tick_snap = ticks;
	EA = 1;
	return tick_snap;
}

#define TN_IDLE	0
#define TN_TX	1

/* IAC parser states */
#define IAC_NONE	0
#define IAC_SEEN	1
#define IAC_OPT		2
#define IAC_SB		3
#define IAC_SB_IAC	4

#define TN_IAC	255
#define TN_SE	240
#define TN_SB	250
#define TN_WILL	251
#define TN_DONT	254


static void tn_putc(uint8_t c)
{
	if (telnet_slen < TELNET_OUTBUF)
		telnet_outbuf[telnet_slen++] = c;
}


static void tn_puts(__code const char *p)
{
	while (*p)
		tn_putc(*p++);
}


static void tn_puts_x(__xdata char *p)
{
	while (*p)
		tn_putc(*p++);
}


void telnetd_init(void) __banked
{
	tn.enabled = 0;
	tn.idle_secs = TN_IDLE_SECS_DEFAULT;
	tn.idle_ticks = TN_IDLE_SECS_DEFAULT * (uint32_t)SYS_TICK_HZ;
	tn.bind[0] = 0; tn.bind[1] = 0; tn.bind[2] = 0; tn.bind[3] = 0;
	tn.conn = 0;
	telnet_slen = 0;
	telnet_capture = 0;
	tn_oidx = 0;
	tn_sent = 0;
}


void telnet_start(void) __banked
{
	if (tn.enabled) {
		print_string("Telnet is already enabled\n");
		return;
	}
	tn.enabled = 1;
	uip_listen(HTONS(TELNET_PORT));
	print_string("Telnet enabled\n");
}


void telnet_set_timeout(uint16_t secs) __banked
{
	tn.idle_secs = secs;
	tn.idle_ticks = (uint32_t)secs * SYS_TICK_HZ;
	print_string("Telnet idle timeout: ");
	itoa_short(secs);
	print_string(" seconds\n");
}


void telnet_stop(void) __banked
{
	if (!tn.enabled) {
		print_string("Telnet is not enabled\n");
		return;
	}
	tn.enabled = 0;
	uip_unlisten(HTONS(TELNET_PORT));
	/* An active session is closed from its own appcall context: with
	 * output pending after the final ACK, otherwise at the next poll. */
	if (tn.conn)
		tn.close_pending = 1;
	print_string("Telnet disabled\n");
}


static void tn_prompt(void)
{
	tn_puts_x(hostname);
	tn_puts("> ");
}


static uint8_t tn_pw_ok(void)
{
	__xdata uint8_t *a = tline;
	__xdata char *b = passwd;

	while (*a && *b) {
		if (*a != (uint8_t)*b)
			return 0;
		a++; b++;
	}
	return *a == 0 && *b == 0;
}


static uint8_t tn_is(__code const char *p)
{
	__xdata uint8_t *a = tline;

	while (*a && *p) {
		if (*a != (uint8_t)*p)
			return 0;
		a++; p++;
	}
	return *a == 0 && *p == 0;
}


static void tn_pump(void)
{
	__xdata struct httpd_state * __xdata s = &(uip_conn->appstate);

	if (s->tstate == TN_TX)
		return;
	if (tn_oidx >= telnet_slen) {
		if (tn.close_pending) {
			uip_close();
			tn.conn = 0;
		}
		return;
	}
	tn_sent = telnet_slen - tn_oidx;
	if (tn_sent > uip_mss())
		tn_sent = uip_mss();
	uip_send(telnet_outbuf + tn_oidx, tn_sent);
	s->tstate = TN_TX;
}


static void tn_denied(void)
{
	tn.tries++;
	if (tn.tries >= 3) {
		tn_puts("Access denied.\r\n");
		tn.close_pending = 1;
	}
}


static void tn_welcome(void)
{
	tn.authed = 2;
	tn_puts("\r\nRTLPlayground telnet console. Type 'exit' to leave.\r\n");
	tn_prompt();
}


static void tn_line_done(void)
{
	tline[tn.ll] = 0;
	tn.ll = 0;

	if (tn.authed == 0) {
		tn_puts("\r\n");
		if (tn_pw_ok()) {
			if (totp_enabled) {
				/* Fail closed: a second factor that cannot be
				 * checked must not fall back to password-only. */
				if (!totp_keylen || !sntp_unix_now()) {
					tn_puts("TOTP required but unavailable (time not synced). Access denied.\r\n");
					tn.close_pending = 1;
					return;
				}
				tn.authed = 1;
				tn_puts("Code: ");
			} else {
				tn_welcome();
			}
		} else {
			tn_denied();
			if (!tn.close_pending)
				tn_puts("Password: ");
		}
		return;
	}

	if (tn.authed == 1) {
		tn_puts("\r\n");
		if (totp_verify(tline)) {
			tn_welcome();
		} else {
			tn_denied();
			if (!tn.close_pending)
				tn_puts("Code: ");
		}
		return;
	}

	/* Echo the line break for the Enter keypress: with server-side echo
	 * the client shows nothing on its own, so without this the command
	 * output starts on the same line as the typed command. */
	tn_puts("\r\n");

	if (tline[0] == 0) {
		tn_prompt();
		return;
	}

	if (tn_is("exit") || tn_is("quit") || tn_is("logout")) {
		tn_puts("Bye.\r\n");
		tn.close_pending = 1;
		return;
	}

	telnet_capture = 1;
	execute_commands(tline);
	if (telnet_capture == 2)
		tn_puts("\r\n[output truncated]\r\n");
	telnet_capture = 0;
	tn_prompt();
}


static void tn_input(uint8_t c)
{
	/* Strip telnet option negotiation */
	if (tn.iac == IAC_SEEN) {
		if (c >= TN_WILL && c <= TN_DONT)
			tn.iac = IAC_OPT;
		else if (c == TN_SB)
			tn.iac = IAC_SB;
		else
			tn.iac = IAC_NONE;
		return;
	}
	if (tn.iac == IAC_OPT) {
		tn.iac = IAC_NONE;
		return;
	}
	if (tn.iac == IAC_SB) {
		if (c == TN_IAC)
			tn.iac = IAC_SB_IAC;
		return;
	}
	if (tn.iac == IAC_SB_IAC) {
		tn.iac = (c == TN_SE) ? IAC_NONE : IAC_SB;
		return;
	}
	if (c == TN_IAC) {
		tn.iac = IAC_SEEN;
		return;
	}

	if (tn.crseen) {
		tn.crseen = 0;
		if (c == '\n' || c == 0)
			return;
	}

	if (c == '\r' || c == '\n') {
		if (c == '\r')
			tn.crseen = 1;
		tn_line_done();
		return;
	}

	if (c == 0x08 || c == 0x7f) {
		if (tn.ll) {
			tn.ll--;
			if (tn.authed)
				tn_puts("\b \b");
		}
		return;
	}

	if (c < 0x20)	/* other control characters, including ^C */
		return;

	if (tn.ll < CMD_BUF_SIZE - 1) {
		tline[tn.ll++] = c;
		if (tn.authed)
			tn_putc(c);
	}
}


void telnetd_appcall(void) __banked
{
	__xdata struct httpd_state * __xdata s = &(uip_conn->appstate);

	if (uip_connected()) {
		if (!tn.enabled || tn.conn) {
			uip_abort();
			return;
		}
		if (tn.bind[0] | tn.bind[1] | tn.bind[2] | tn.bind[3]) {
			uip_ipaddr(&tn_bindaddr, tn.bind[0], tn.bind[1], tn.bind[2], tn.bind[3]);
			if (!uip_ipaddr_cmp(uip_hostaddr, tn_bindaddr)) {
				uip_abort();
				return;
			}
		}
		tn.conn = uip_conn;
		tn.authed = 0;
		tn.tries = 0;
		tn.iac = IAC_NONE;
		tn.crseen = 0;
		tn.close_pending = 0;
		tn.ll = 0;
		tn.last_rx = ticks_now();
		telnet_slen = 0;
		tn_oidx = 0;
		tn_sent = 0;
		s->tstate = TN_IDLE;
		/* IAC WILL ECHO, IAC WILL SGA: character mode, we echo */
		tn_putc(TN_IAC); tn_putc(TN_WILL); tn_putc(1);
		tn_putc(TN_IAC); tn_putc(TN_WILL); tn_putc(3);
		tn_puts("Password: ");
		tn_pump();
		return;
	}

	if (uip_conn != tn.conn)
		return;

	if (uip_closed() || uip_aborted() || uip_timedout()) {
		tn.conn = 0;
		return;
	}

	if (uip_acked() && s->tstate == TN_TX) {
		tn_oidx += tn_sent;
		tn_sent = 0;
		s->tstate = TN_IDLE;
		if (tn_oidx >= telnet_slen) {
			tn_oidx = 0;
			telnet_slen = 0;
			if (tn.close_pending) {
				uip_close();
				tn.conn = 0;
				return;
			}
		}
	}

	if (uip_rexmit()) {
		if (tn_sent)
			uip_send(telnet_outbuf + tn_oidx, tn_sent);
		return;
	}

	if (uip_newdata()) {
		__xdata uint8_t *p = uip_appdata;
		__xdata uint16_t n = uip_len;

		tn.last_rx = ticks_now();
		while (n--)
			tn_input(*p++);
	}

	if (uip_poll()) {
		if (!tn.enabled)
			tn.close_pending = 1;
		if (!tn.close_pending
		    && ticks_now() - tn.last_rx > (tn.authed == 2 ? tn.idle_ticks : TN_LOGIN_TICKS)) {
			uip_close();
			tn.conn = 0;
			return;
		}
	}

	tn_pump();
}
