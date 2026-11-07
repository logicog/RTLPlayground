#!/usr/bin/env python3
"""Run the production global LACP dispatcher against two negotiated groups."""
import os
import pathlib
import shlex
import subprocess

root = pathlib.Path(__file__).resolve().parent.parent
src = (root / 'cmd_parser.c').read_text()
functions = []
for signature in ('uint8_t cmd_compare(', 'static void cmd_error(', 'void cmd_tokenize('):
    start = src.index(signature)
    functions.append(src[start:src.index('\n}', start) + 2])
start = src.index('if (cmd_compare(0, "lacp"))')
end = src.index('} else if (cmd_compare(0, "pvid"))', start)
dispatch = src[start:end + 1]
harness = r'''
#define main lacp_fsm_scenarios
#include <test/test_lacp.c>
#undef main
#include "cmd_parser.h"
#define N_WORDS 15
uint8_t cmd_buffer[CMD_BUF_SIZE], cmd_words_len, cmd_words_b[N_WORDS], err_status;
'''
tests = r'''
static void command(const char *text) {
    memset(cmd_buffer, 0, sizeof(cmd_buffer));
    strcpy((char *)cmd_buffer, text);
    cmd_tokenize();
    if (err_status == ERR_OK) dispatch();
}
static void negotiate(void) {
    lacp_off();
    lacp_init();
    lacp_lag_set(0, 0x0003);
    lacp_lag_set(1, 0x0030);
    step(8);
    for (int round = 0; round < 3; round++) {
        partner_frame(0, SYS_A, P_ACT | P_AGG | P_SYNC, 1);
        partner_frame(1, SYS_A, P_ACT | P_AGG | P_SYNC, 1);
        partner_frame(4, SYS_B, P_ACT | P_AGG | P_SYNC, 1);
        partner_frame(5, SYS_B, P_ACT | P_AGG | P_SYNC, 1);
        step(8);
    }
    assert(hw_members_lag[0] == 0x0003 && hw_members_lag[1] == 0x0030);
}
int main(void) {
    const char *invalid[] = {"lacp off 1", "lacp off extra", "lacp show 1", "lacp", "lacp offx", "lacp showx"};
    for (unsigned i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
        negotiate();
        int writes = hw_set_calls;
        command(invalid[i]);
        CHECK(err_status == ERR_INVALID_ARGUMENT && lacpEnabled
              && lacp_lag_ports[0] == 0x0003 && lacp_lag_ports[1] == 0x0030
              && hw_members_lag[0] == 0x0003 && hw_members_lag[1] == 0x0030
              && hw_set_calls == writes, invalid[i]);
    }
    negotiate();
    command("lacp show");
    CHECK(err_status == ERR_OK && lacpEnabled && hw_members_lag[0] == 0x0003
          && hw_members_lag[1] == 0x0030, "valid show preserves both negotiated groups");
    command("lacp off");
    CHECK(err_status == ERR_OK && !lacpEnabled && !lacp_lag_ports[0] && !lacp_lag_ports[1]
          && !hw_members_lag[0] && !hw_members_lag[1], "valid off disables both groups");
    printf("LACP CLI: %d failures\n", failures);
    return failures != 0;
}
'''
out = root / 'test/build'
out.mkdir(exist_ok=True)
source = out / 'lacp_cli.c'
source.write_text(harness + '\n'.join(functions) + '\nstatic void dispatch(void) {\n' + dispatch + '\n}\n' + tests)
binary = out / 'test_lacp_cli'
sources = [str(source), str(root / 'rtl837x_lacp.c'), str(root / 'timer.c')]
subprocess.run(shlex.split(os.environ.get('CC', 'gcc')) + [
    '-std=gnu11', '-Wall', '-Wextra', '-Werror', '-Wno-unused-parameter', '-Wno-pointer-sign',
    '-Wno-unknown-pragmas', '-DRTLP_HOST_TEST', '-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
    '-include', str(root / 'test/sdcc_shim.h'), '-I' + str(root),
    '-I' + str(root / 'uip'), '-I' + str(root / 'httpd'), *sources, '-o', str(binary)], check=True)
subprocess.run([str(binary)], check=True)
