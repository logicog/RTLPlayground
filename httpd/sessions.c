/*
 * Login sessions of the web interface.
 *
 * Up to SESSIONS logins at once, each with its own cookie and its own expiry.
 * With a single one, logging in from a second browser (or a phone) ended the
 * first browser's session at its next request.
 *
 * This sits in bank 3: the web server's own bank is full, so httpd.c only
 * calls in here. It shares three things with httpd.c: the cookie value it
 * parsed (session), the time read for the request (now) and the result
 * (authenticated); the reply is written to outbuf at slen.
 */

#pragma codeseg BANK3
#pragma constseg BANK3

#include <stdint.h>
#include "rtl837x_common.h"
#include "sessions.h"

extern volatile __xdata uint8_t sfr_data[4];
extern __xdata uint16_t session_timeout;
extern __xdata uint32_t now;
extern __xdata uint8_t authenticated;
extern __xdata uint8_t * __xdata session;
extern __xdata uint8_t outbuf[];
extern __xdata uint16_t slen;

__xdata char session_id[SESSIONS][SESSION_ID_LENGTH + 1];
__xdata uint32_t last_session_use[SESSIONS];

static __xdata uint8_t ss_slot, ss_i, ss_oldest, ss_c;
/* The slot whose cookie the last request came with; only meaningful while
 * authenticated is set, which only session_check() does */
static __xdata uint8_t ss_hit;

static __code const char ss_hex[] = "0123456789abcdef";
/* What may follow a cookie value: ';' separates cookies in a Cookie header */
static __code const char ss_end[] = " \t:?=\n\r;";

/* Leaf function, so the compiler keeps its 32-bit scratch in the overlay
 * rather than in internal RAM. */
static uint8_t ss_stale(void)
{
	if (now - last_session_use[ss_slot] > session_timeout)
		return 1;
	return 0;
}

/* Likewise a leaf: the 32-bit comparison needs no stack of its own this way */
static uint8_t ss_older(void)
{
	if (last_session_use[ss_slot] < last_session_use[ss_oldest])
		return 1;
	return 0;
}

/* Whether the cookie value is the id of slot ss_slot */
static uint8_t ss_match(void)
{
	for (ss_i = 0; ss_i < SESSION_ID_LENGTH; ss_i++) {
		if (session[ss_i] != session_id[ss_slot][ss_i])
			return 0;
	}
	ss_c = session[SESSION_ID_LENGTH];
	if (!ss_c)
		return 1;
	for (ss_i = 0; ss_end[ss_i]; ss_i++) {
		if (ss_c == ss_end[ss_i])
			return 1;
	}
	return 0;
}

void session_init(void) __banked
{
	for (ss_slot = 0; ss_slot < SESSIONS; ss_slot++)
		session_id[ss_slot][0] = NUL;
}

void session_check(void) __banked
{
	for (ss_slot = 0; ss_slot < SESSIONS; ss_slot++) {
		if (!session_id[ss_slot][0])
			continue;
		if (ss_stale()) {
			session_id[ss_slot][0] = NUL;
		} else if (ss_match()) {
			authenticated = 1;
			ss_hit = ss_slot;
			last_session_use[ss_slot] = now;
			return;
		}
	}
}

void session_new(void) __banked
{
	if (authenticated && session_id[ss_hit][0]) {
		/* The login came with a cookie that is still valid: its slot gets the
		 * new id. A second slot for the same browser would leave the old one
		 * looking fresh and push out someone else at the next login. */
		ss_slot = ss_hit;
	} else {
		/* A free or expired slot, else the least recently used */
		ss_oldest = 0;
		for (ss_slot = 0; ss_slot < SESSIONS; ss_slot++) {
			if (!session_id[ss_slot][0] || ss_stale())
				break;
			if (ss_older())
				ss_oldest = ss_slot;
		}
		if (ss_slot == SESSIONS)
			ss_slot = ss_oldest;
	}

	last_session_use[ss_slot] = now;
	for (ss_i = 0; ss_i < SESSION_ID_LENGTH; ss_i++) {
		if (!(ss_i & 7))
			get_random_32();
		ss_c = sfr_data[(ss_i >> 1) & 3];
		if (ss_i & 1)
			ss_c >>= 4;
		session_id[ss_slot][ss_i] = ss_hex[ss_c & 0xf];
		outbuf[slen++] = ss_hex[ss_c & 0xf];
	}
	session_id[ss_slot][SESSION_ID_LENGTH] = NUL;
}
