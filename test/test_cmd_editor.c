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
#include <test/support.h>

/* editor state (globals in cmd_editor.c, external linkage) */
extern __xdata uint8_t cmd_line_len;
extern uint8_t cmd_buffer[];
extern uint8_t cmd_available;
extern uint8_t cmd_history[];
extern uint16_t cmd_history_ptr;

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

static void test_backspace_full_line(void) {
    char full[CMD_BUF_SIZE - 1];
    printf("[test] backspace on a full line stays inside cmd_buffer\n");
    env_reset();
    t_memset(full, 'c', sizeof(full));
    feed_n(full, sizeof(full));
    feed("\b");
    CHECK(cmd_line_len == CMD_BUF_SIZE - 2, "the full line is one character shorter");
    feed("d\n");
    CHECK(cmd_buffer[CMD_BUF_SIZE - 3] == 'c' && cmd_buffer[CMD_BUF_SIZE - 2] == 'd'
          && cmd_buffer[CMD_BUF_SIZE - 1] == 0, "the line ends with the character typed after it");
}

static void test_second_line_waits(void) {
    printf("[test] a second line waits until the first is taken\n");
    env_reset();
    feed("ab\ncd\n");
    CHECK(cmd_available == 1 && t_strcmp((char *)cmd_buffer, "ab") == 0,
          "the first of two lines is left as it was typed");
    cmd_available = 0;
    feed("");
    CHECK(cmd_available == 1 && t_strcmp((char *)cmd_buffer, "cd") == 0,
          "the second line follows once the first is taken");
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

static void test_unknown_escape(const char *name, const char *seq) {
    printf("[test] %s does not hang the editor\n", name);
    env_reset();
    feed("ab");
    watchdog_arm(5, name);
    feed(seq);
    watchdog_disarm();
    feed("c\n");
    CHECK(cmd_available == 1, "the line after the sequence is accepted");
    CHECK(t_strcmp((char *)cmd_buffer, "abc") == 0, "the sequence leaves the line as it was");
}

static void test_escape_in_pieces(void) {
    printf("[test] Delete arriving in two pieces still deletes\n");
    env_reset();
    feed("abc\033[D");
    watchdog_arm(5, "split Delete sequence");
    feed("\033[3");
    feed("~");
    watchdog_disarm();
    feed("\n");
    CHECK(t_strcmp((char *)cmd_buffer, "ab") == 0, "cursor-left then a split Delete removes the 'c'");
}

static void test_escape_then_enter(void) {
    printf("[test] Enter inside an unfinished sequence is kept\n");
    env_reset();
    feed("ab");
    watchdog_arm(5, "unfinished sequence before Enter");
    feed("\033[1\n");
    watchdog_disarm();
    CHECK(cmd_available == 1, "Enter after an unfinished sequence still ends the line");
    CHECK(t_strcmp((char *)cmd_buffer, "ab") == 0, "the unfinished sequence adds nothing to the line");
}

static void test_history_keeps_typed_line(void) {
    static const char hist[] = "one\ntwo\n";
    printf("[test] the typed line comes back after a walk through the history\n");
    env_reset();
    for (uint8_t i = 0; i < sizeof(hist) - 1; i++)
        cmd_history[i] = hist[i];
    cmd_history_ptr = sizeof(hist) - 1;
    feed("abc");
    feed("\033[A");
    feed("\033[A");
    feed("\033[B");
    feed("\033[B");
    feed("\n");
    CHECK(t_strcmp((char *)cmd_buffer, "abc") == 0, "two up and two down give the typed line back");
}

int main(void) {
    printf("== cmd_editor host tests ==\n");
    test_basic_line();
    test_backspace();
    test_backspace_full_line();
    test_unknown_escape("Home (ESC [ H)", "\033[H");
    test_unknown_escape("Home (ESC [ 1 ~)", "\033[1~");
    test_unknown_escape("F1 (ESC O P)", "\033OP");
    test_unknown_escape("F5 (ESC [ 1 5 ~)", "\033[15~");
    test_unknown_escape("Ctrl-Right (ESC [ 1 ; 5 C)", "\033[1;5C");
    test_unknown_escape("a lone ESC", "\033");
    test_escape_in_pieces();
    test_escape_then_enter();
    test_full_line_no_hang();
    test_full_line_no_overflow();
    test_second_line_waits();
    test_history_keeps_typed_line();
    printf("\n%d checks, %d failed\n", tests_run, tests_failed);
    return tests_failed ? 1 : 0;
}
