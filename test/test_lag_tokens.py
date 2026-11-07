#!/usr/bin/env python3
"""Exercise production LAG parsers; malformed commands must leave configuration intact."""
import os
import pathlib
import shlex
import subprocess

root = pathlib.Path(__file__).resolve().parent.parent
src = (root / 'cmd_parser.c').read_text()
functions = []
for signature in ('uint8_t cmd_compare(', 'uint8_t atoi_byte(', 'uint8_t cmd_parse_port(',
                  'uint8_t cmd_parse_port_separator(', '__bit cmd_is_space_or_nul(',
                  'static void cmd_error(', 'void parse_lag(', 'void parse_lag_hash(',
                  'void cmd_tokenize('):
    start = src.index(signature)
    functions.append(src[start:src.index('\n}', start) + 2])
harness = r'''
#include <stdio.h>
#include "cmd_parser.h"
#include "machine.h"
#include "rtl837x_port.h"
#include "rtl837x_regs.h"
#include <test/support.h>
#define N_WORDS 15
uint8_t cmd_buffer[CMD_BUF_SIZE], cmd_words_len, cmd_words_b[15];
uint8_t err_status, atoi_results_u8;
volatile uint8_t sfr_data[4];
const struct machine machine = {.phys_to_log_port = {0,1,2,3,4,5,6,7,8}};
int tests_run, tests_failed;
static unsigned writes, reads;
static uint16_t members, lacp;
static uint8_t hash;
void print_string(const char *s) {(void)s;}
void print_string_x(char *s) {(void)s;}
void print_byte(uint8_t p) {(void)p;}
void print_short(uint16_t p) {(void)p;}
void print_phys_port(uint8_t p) {(void)p;}
void write_char(char c) {(void)c;}
void reg_read_m(uint16_t r) {(void)r; reads++;}
uint16_t port_lag_members_get(uint8_t g) {(void)g; reads++; return 0;}
void port_lag_members_set(uint8_t g, uint16_t m) {(void)g; writes++; members=m;}
void port_lag_hash_set(uint8_t g, uint8_t h) {(void)g; writes++; hash=h;}
void lacp_lag_set(uint8_t g, uint16_t m) {(void)g; writes++; lacp=m;}
'''
tests = r'''
static void run(const char *command) {
    memset(cmd_buffer, 0, sizeof(cmd_buffer));
    strcpy((char *)cmd_buffer, command);
    members=0x55; lacp=0x33; hash=0x42; reads=writes=0;
    cmd_tokenize();
    if (err_status != ERR_OK) return;
    if (!strncmp(command,"laghash",7)) parse_lag_hash(); else parse_lag();
}
int main(void) {
    const char *invalid[]={
        "lag 1x 1 3", "lag 1x d", "lag 1r d", "lag 0 d", "lag 5 d", "lag 10 d",
        "lag 1 d 2", "lag show extra", "lag 1 lacp off 3", "lag 1.0 d",
        "laghash 1x smac", "laghash 1x dport", "laghash 1r smac", "laghash 0 smac",
        "laghash 5 smac", "laghash 10 smac", "laghash 1 smac junk"
    };
    for (unsigned i=0;i<sizeof(invalid)/sizeof(invalid[0]);i++) {
        run(invalid[i]);
        CHECK(err_status==ERR_INVALID_ARGUMENT && !writes && !reads && members==0x55 && lacp==0x33 && hash==0x42, invalid[i]);
    }
    run("lag 1 1 3");
    CHECK(err_status==ERR_OK && members==5 && lacp==0, "valid static LAG command still works");
    run("lag 1 d");
    CHECK(err_status==ERR_OK && members==0 && lacp==0, "valid deletion still works");
    run("lag 1 lacp off");
    CHECK(err_status==ERR_OK && lacp==0 && members==0x55, "valid LACP stop still works");
    run("laghash 1 smac dport");
    CHECK(err_status==ERR_OK && hash==0x42 && writes==1, "valid hash selection still works");
    run("lag show");
    CHECK(err_status==ERR_OK && !writes && reads==4, "valid status does not alter configuration");
    printf("%d checks, %d failures\n",tests_run,tests_failed);
    return tests_failed != 0;
}
'''
out = root / 'test/build'
out.mkdir(exist_ok=True)
source = out / 'lag_tokens.c'
source.write_text(harness + '\n\n'.join(functions) + tests)
binary = out / 'test_lag_tokens'
subprocess.run(shlex.split(os.environ.get('CC','gcc')) + [
    '-std=gnu11','-Wall','-Wextra','-Werror','-Wno-pointer-sign','-g','-DRTLP_HOST_TEST',
    '-fsanitize=address,undefined','-fno-omit-frame-pointer',
    '-include',str(root/'test/sdcc_shim.h'),'-I'+str(root),
    '-I'+str(root/'httpd'),'-I'+str(root/'uip'),str(source),'-o',str(binary)], check=True)
subprocess.run([str(binary)], check=True)
