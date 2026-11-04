#!/usr/bin/env python3
"""Compile production mirror/tokenizer functions with only hardware/output edges stubbed."""
import os
import pathlib
import shlex
import subprocess

root = pathlib.Path(__file__).resolve().parent.parent
src = (root / 'cmd_parser.c').read_text()
functions = []
for signature in (
    'uint8_t cmd_compare(',
    'uint8_t cmd_parse_port(',
    'uint8_t cmd_parse_port_separator(',
    '__bit cmd_is_space_or_nul(',
    'static void cmd_error(',
    'void parse_mirror(',
    'void cmd_tokenize(',
):
    start = src.index(signature)
    end = src.index('\n}', start) + 2
    functions.append(src[start:end])

harness = r"""
#include <stdio.h>
#include "cmd_parser.h"
#include "machine.h"
#include "rtl837x_port.h"
#include "rtl837x_regs.h"
#include <test/support.h>

#define N_WORDS 15
uint8_t cmd_buffer[CMD_BUF_SIZE], cmd_words_len, cmd_words_b[N_WORDS];
uint8_t err_status, atoi_results_u8;
volatile uint8_t sfr_data[4];
const struct machine machine = {
    .phys_to_log_port = {
        MACHINE_MIN_PORT + 2, MACHINE_MIN_PORT, MACHINE_MIN_PORT + 1,
        MACHINE_MIN_PORT + 3, MACHINE_MIN_PORT + 4, MACHINE_MIN_PORT + 5,
        MACHINE_MIN_PORT + 6, MACHINE_MIN_PORT + 7, MACHINE_MIN_PORT + 8
    }
};
int tests_run, tests_failed;
static unsigned writes, reads;
static uint8_t monitor;
static uint16_t rx_mask, tx_mask;

void print_string(const char *s) { (void)s; }
void write_char(char c) { (void)c; }
void print_phys_port(uint8_t p) { (void)p; }
void print_short(uint16_t v) { (void)v; }
void reg_read_m(uint16_t reg) { (void)reg; reads++; }
void port_mirror_set(uint8_t p, uint16_t rx, uint16_t tx)
{
    writes++;
    monitor = p;
    rx_mask = rx;
    tx_mask = tx;
}
void port_mirror_del(void)
{
    writes++;
    monitor = 0;
    rx_mask = tx_mask = 0;
}
"""
tests = r"""
static void run(const char *command)
{
    memset(cmd_buffer, 0, sizeof(cmd_buffer));
    strcpy((char *)cmd_buffer, command);
    writes = reads = 0;
    monitor = 0xff;
    rx_mask = tx_mask = 0xffff;
    cmd_tokenize();
    if (err_status == ERR_OK)
        parse_mirror();
}

static void valid(const char *command, uint8_t p, uint16_t rx, uint16_t tx)
{
    run(command);
    CHECK(err_status == ERR_OK && writes == 1 && reads == 0 &&
          monitor == p && rx_mask == rx && tx_mask == tx, command);
}

int main(void)
{
    const char *invalid[] = {
        "mirror", "mirror 0 3", "mirror 10 3", "mirror 20 3", "mirror 99 3",
        "mirror 256 3", "mirror -1 3", "mirror 2r 3", "mirror 2t 3",
        "mirror 2junk 3", "mirror 2.0 3", "mirror p2 3",
        "mirror 2 0", "mirror 2 10", "mirror 2 30r", "mirror 2 256t",
        "mirror 2 -1", "mirror 2 3x", "mirror 2 3rr", "mirror 2 3rt",
        "mirror 2 3tr", "mirror 2 3tt", "mirror 2 3rx", "mirror 2 3tx",
        "mirror 2 3r0", "mirror 2 3t0", "mirror 2 3junk", "mirror 2 p3",
        "mirror 2 1r 3t junk", "mirror status extra", "mirror off extra",
        "mirror statusjunk", "mirror offjunk"
    };
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
        run(invalid[i]);
        CHECK(err_status == ERR_INVALID_ARGUMENT && writes == 0 && reads == 0 &&
              monitor == 0xff && rx_mask == 0xffff && tx_mask == 0xffff, invalid[i]);
    }

    uint8_t p1 = machine.phys_to_log_port[0];
    uint8_t p2 = machine.phys_to_log_port[1];
    uint8_t p3 = machine.phys_to_log_port[2];
    valid("mirror 2 3r", p2, PORT_BIT(p3), 0);
    valid("mirror 2 3t", p2, 0, PORT_BIT(p3));
    valid("mirror 2 3", p2, PORT_BIT(p3), PORT_BIT(p3));
    valid("mirror 2 1r 3t 2", p2, PORT_BIT(p1) | PORT_BIT(p2),
          PORT_BIT(p3) | PORT_BIT(p2));
    valid("mirror 2 3r 3t", p2, PORT_BIT(p3), PORT_BIT(p3));
    valid("mirror  2   3r   ", p2, PORT_BIT(p3), 0);
    valid("mirror 2", p2, 0, 0);
    valid("mirror off", 0, 0, 0);
    run("mirror status");
    CHECK(err_status == ERR_OK && writes == 0 && reads == 2, "mirror status");

    /* Every front-panel mapping: valid user ports, never the logical CPU port. */
    for (unsigned i = 0; i < 9; i++) {
        char command[32];
        uint8_t p = machine.phys_to_log_port[i];
        snprintf(command, sizeof(command), "mirror %u 1", i + 1);
        if (MACHINE_PORT_VALID(p)) {
            valid(command, p, PORT_BIT(p1), PORT_BIT(p1));
        } else {
            run(command);
            CHECK(err_status == ERR_INVALID_ARGUMENT && writes == 0, command);
        }
        snprintf(command, sizeof(command), "mirror 1 %ur", i + 1);
        if (MACHINE_PORT_VALID(p)) {
            valid(command, p1, PORT_BIT(p), 0);
        } else {
            run(command);
            CHECK(err_status == ERR_INVALID_ARGUMENT && writes == 0, command);
        }
    }
    printf("%d checks, %d failures\n", tests_run, tests_failed);
    return tests_failed != 0;
}
"""
out = root / 'test/build'
out.mkdir(exist_ok=True)
source = out / 'mirror.c'
source.write_text(harness + '\n\n'.join(functions) + tests)
cc = shlex.split(os.environ.get('CC', 'gcc'))
for name, limits in (
    ('full', []),
    ('restricted', ['-DMACHINE_MAX_PORT=7']),
    ('8372', ['-DMACHINE_MIN_PORT=3', '-DMACHINE_MAX_PORT=8']),
):
    binary = out / ('test_mirror_' + name)
    subprocess.run(cc + [
        '-std=gnu11', '-Wall', '-Wextra', '-Werror', '-Wno-pointer-sign', '-g',
        '-DRTLP_HOST_TEST', '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
        '-include', str(root / 'test/sdcc_shim.h'),
        '-I' + str(root),
        '-I' + str(root / 'httpd'), '-I' + str(root / 'uip'),
    ] + limits + [str(source), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
