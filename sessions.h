#ifndef _SESSIONS_H_
#define _SESSIONS_H_

#include <stdint.h>

#define SESSIONS 4
#define SESSION_ID_LENGTH 12

extern __xdata char session_id[SESSIONS][SESSION_ID_LENGTH + 1];
extern __xdata uint32_t last_session_use[SESSIONS];

/* No session at boot */
void session_init(void) __banked;
/* Sets authenticated if the cookie in session belongs to a live session */
void session_check(void) __banked;
/* Starts a session at now and writes its id to outbuf at slen */
void session_new(void) __banked;

#endif
