/*
 * support.c — mock hardware edges + test environment for the host harness.
 *
 * Everything the firmware expects the platform/other-TUs to provide, but which
 * has no business touching real hardware in a unit test, lives here: the serial
 * ring, the command/history buffers, the character-output sink, and stubs for
 * the print helpers. The buffers are sized EXACTLY as on target so that
 * AddressSanitizer redzones catch the same off-by-one overflows the 8051 hits.
 */
#include "sdcc_shim.h"
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <signal.h>
/* Declared by hand instead of <unistd.h>, whose sleep() prototype clashes with
 * the firmware's own sleep() declaration in rtl837x_common.h. */
extern unsigned int alarm(unsigned int);
extern void _exit(int);
#include "rtl837x_common.h"     /* SBUF_SIZE, CMD_BUF_SIZE, CMD_HISTORY_SIZE, decls */
#include "support.h"

/* --- symbols the editor/parser reference (extern in the firmware headers) --- */
__xdata volatile uint8_t sbuf_ptr;
__xdata uint8_t          sbuf[SBUF_SIZE];
uint8_t                  cmd_buffer[CMD_BUF_SIZE];   /* ASan-instrumented: OOB caught */
uint8_t                  cmd_available;
uint8_t                  err_status;
uint8_t                  cmd_history[CMD_HISTORY_SIZE];
uint16_t                 cmd_history_ptr;

/* editor-internal read pointer (defined in cmd_editor.c, external linkage) */
extern __xdata uint8_t l;

/* the editor entry points under test */
void cmd_editor_init(void);
void cmd_edit(void);

/* ------------------------------- output sink ------------------------------- */
char   out_buf[8192];
size_t out_len;

void out_reset(void) { out_len = 0; out_buf[0] = 0; }

static void out_putc(char c) {
    if (out_len < sizeof(out_buf) - 1) { out_buf[out_len++] = c; out_buf[out_len] = 0; }
}

/* --- mocked print helpers (signatures match rtl837x_common.h post-shim) ---- */
void write_char(char c)            { out_putc(c); }
void print_string(char *p)         { while (*p) out_putc(*p++); }
void print_string_x(char *p)       { while (*p) out_putc(*p++); }
void print_cmd_prompt(void)        { print_string((char *)"> "); }
void itoa(uint8_t v) {                       /* firmware prints a decimal number */
    char t[4]; int n = 0;
    if (!v) { out_putc('0'); return; }
    while (v) { t[n++] = '0' + v % 10; v /= 10; }
    while (n) out_putc(t[--n]);
}

/* ------------------------------ environment -------------------------------- */
void env_reset(void) {
    sbuf_ptr = 0;
    t_memset(sbuf, 0, sizeof(sbuf));
    t_memset(cmd_buffer, 0, sizeof(cmd_buffer));
    t_memset(cmd_history, 0, sizeof(cmd_history));
    cmd_history_ptr = 0;
    cmd_available = 0;
    err_status = 0;
    out_reset();
    cmd_editor_init();          /* sets l = sbuf_ptr, clears cursor/len */
}

/* Push one byte into the ring, draining first if it is full (leave 1 slot). */
static void ring_push(uint8_t b) {
    while (((sbuf_ptr + 1) & SBUF_MASK) == l)
        cmd_edit();                          /* drain; under the C4 bug this spins */
    sbuf[sbuf_ptr] = b;
    sbuf_ptr = (sbuf_ptr + 1) & SBUF_MASK;
}

void feed_n(const char *s, size_t n) {
    for (size_t i = 0; i < n; i++) ring_push((uint8_t)s[i]);
    cmd_edit();
}
void feed(const char *s) { feed_n(s, t_strlen(s)); }

/* -------------------------------- watchdog --------------------------------- */
static const char *wd_what;
static void wd_fire(int sig) {
    (void)sig;
    /* async-signal-unsafe write is acceptable: we abort immediately after. */
    fprintf(stderr, "\n  HANG DETECTED (watchdog): %s\n", wd_what ? wd_what : "?");
    _exit(2);
}
void watchdog_arm(unsigned seconds, const char *what) {
    wd_what = what;
    signal(SIGALRM, wd_fire);
    alarm(seconds);
}
void watchdog_disarm(void) { alarm(0); }

/* -------------------------------- framework -------------------------------- */
int tests_run, tests_failed;
