/*
 * support.h — test harness API for off-target firmware unit tests.
 *
 * Provides the mock environment (serial ring, command buffers, output capture)
 * plus a tiny assertion framework. The firmware sources under test are compiled
 * unmodified against this; only the hardware edges are mocked here.
 */
#ifndef TEST_SUPPORT_H
#define TEST_SUPPORT_H

#include <stdint.h>
#include <stddef.h>

/* Distinct-named string helpers so host code needn't include <string.h>, which
 * would clash with the firmware's own memset/strlen/strcpy prototypes in
 * rtl837x_common.h (different 8051 signatures). */
static inline void  t_memset(void *d, int c, size_t n) {
    unsigned char *p = d; while (n--) *p++ = (unsigned char)c;
}
static inline size_t t_strlen(const char *s) { const char *p = s; while (*p) p++; return (size_t)(p - s); }
static inline int    t_strcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; } return (unsigned char)*a - (unsigned char)*b;
}

/* ---- captured terminal output (what the firmware would send over serial) ---- */
extern char     out_buf[8192];
extern size_t   out_len;
void            out_reset(void);

/* ---- editor/parser environment reset ---- */
void env_reset(void);           /* clears ring, buffers, editor state */

/* ---- drive the serial input ring exactly like the RX ISR would ----
 * Pushes bytes into the 16-byte sbuf ring, draining via cmd_edit() whenever the
 * ring would overflow, then a final drain. Mirrors real ISR→editor handoff.
 */
void feed(const char *s);                 /* NUL-terminated */
void feed_n(const char *s, size_t n);     /* explicit length (for embedded NULs) */

/* ---- watchdog: arm before calling into code that could spin (C4 hang) ---- */
void watchdog_arm(unsigned seconds, const char *what);
void watchdog_disarm(void);

/* ---- minimal test framework ---- */
extern int tests_run, tests_failed;
#define CHECK(cond, msg) do {                                            \
        tests_run++;                                                     \
        if (!(cond)) { tests_failed++;                                   \
            printf("  FAIL: %s  (%s:%d)\n", (msg), __FILE__, __LINE__);  \
        } else { printf("  ok:   %s\n", (msg)); }                        \
    } while (0)

#endif /* TEST_SUPPORT_H */
