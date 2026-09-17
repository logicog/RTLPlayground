/*
 * test_cmd_editor.c — host unit tests for cmd_editor.c (the serial line editor).
 *
 * Focus: finding C4 (08-findings.md) — a full input line both HANGS the editor
 * (continue without advancing the ring read pointer) and OVERFLOWS cmd_buffer by
 * one byte (guard off-by-one lets cmd_line_len reach CMD_BUF_SIZE).
 *
 * These tests are RED against the current source and GREEN after the fix, with
 * no hardware and no flash cycle. The hang is caught by a SIGALRM watchdog; the
 * overflow is caught by AddressSanitizer redzones on cmd_buffer[].
 */
#include "sdcc_shim.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "rtl837x_common.h"     /* CMD_BUF_SIZE */
#include "support.h"

/* editor state (globals in cmd_editor.c, external linkage) */
extern __xdata uint8_t cmd_line_len;
extern uint8_t cmd_buffer[];
extern uint8_t cmd_available;

static void test_basic_line(void) {
    printf("[test] basic line entry + Enter\n");
    env_reset();
    feed("vlan 199\n");
    CHECK(cmd_available == 1, "Enter marks a command available");
    CHECK(t_strcmp((char *)cmd_buffer, "vlan 199") == 0, "cmd_buffer holds the typed line");
}

static void test_backspace(void) {
    printf("[test] backspace edits the line\n");
    env_reset();
    feed("helo");
    feed("\b");            /* delete the 'o' -> wait, cursor at end: removes last char */
    feed("lo\n");
    CHECK(t_strcmp((char *)cmd_buffer, "hello") == 0, "backspace then retype yields 'hello'");
}

/* C4 part 1: a line that fills the buffer must not spin forever. */
static void test_full_line_no_hang(void) {
    char big[200];
    printf("[test] C4: overlong line does not hang the editor\n");
    env_reset();
    t_memset(big, 'a', sizeof(big));
    watchdog_arm(5, "C4 full-line hang (editor spun without advancing ring ptr)");
    feed_n(big, sizeof(big));      /* 200 printable chars, no newline */
    watchdog_disarm();
    /* Reaching here means no hang. With the fix the line caps one short of full. */
    CHECK(cmd_line_len == CMD_BUF_SIZE - 1,
          "line length caps at CMD_BUF_SIZE-1 (room reserved for NUL)");
}

/* C4 part 2: filling the buffer then Enter must not write cmd_buffer[CMD_BUF_SIZE]. */
static void test_full_line_no_overflow(void) {
    char full[CMD_BUF_SIZE];       /* 128 printable chars, exactly buffer size */
    printf("[test] C4: full line + Enter does not overflow cmd_buffer\n");
    env_reset();
    t_memset(full, 'b', sizeof(full));
    watchdog_arm(5, "C4 overflow-path hang");
    feed_n(full, sizeof(full));
    feed("\n");                    /* under the bug, writes cmd_buffer[128] -> ASan trap */
    watchdog_disarm();
    CHECK(1, "128-char line + Enter completes without ASan overflow");
}

int main(void) {
    printf("== cmd_editor host tests ==\n");
    test_basic_line();
    test_backspace();
    test_full_line_no_hang();
    test_full_line_no_overflow();
    printf("\n%d checks, %d failed\n", tests_run, tests_failed);
    return tests_failed ? 1 : 0;
}
