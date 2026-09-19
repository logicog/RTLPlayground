# Host unit-test harness

Compile and test individual firmware translation units on the build host with
**gcc + AddressSanitizer + UBSan** — no SDCC, no flashing, no hardware. This is
the fast inner loop for the logic-level bugs in `../../NOTES/08-findings.md`
(parser, line editor, HTTP header, DHCP): edit → `make` → see red/green in
seconds instead of a minutes-long flash-and-reboot cycle.

## Run
```
cd test
make            # build + run all tests (ASan/UBSan on)
make clean
```
Exit status is non-zero if any check fails, a sanitizer trips, or the watchdog
fires — so it drops straight into CI.

## How it works
- **`sdcc_shim.h`** (force-included) erases SDCC 8051 keywords (`__xdata`,
  `__code`, `__banked`, …) so firmware sources compile under host gcc. It changes
  no logic — the code under test is byte-for-byte the firmware source.
- **`hw_mock.c` / `hw_mock.h`** simulate the ASIC edge: a flat register file
  behind `reg_read`/`reg_write` and the `SFR_DATA_*` registers, with a table
  engine that performs a VLAN or L2 operation the moment `TBL_CTRL` is written,
  fetches a MIB counter on `STAT_GET` and drops entries on `L2_TBL_FLUSH_CTRL`.
  Nothing ever reports busy, so a polling loop runs once. The header states the
  VLAN and L2 entry layouts independently of the firmware, so a test can hold
  what `rtl837x_port.c` writes against what `page_impl.c` reads.
- **`env_tables.c`** carries the globals and leaf calls those two modules link
  against; **`stub/`** stands in for the two headers only a firmware build
  generates, so the harness needs no SDCC build first.
- **`support.c` / `support.h`** mock the hardware edges: the 16-byte serial ring
  (`sbuf`), the command/history buffers, and the character-output sink
  (`write_char` etc.). Buffers are sized **exactly** as on target, so ASan
  redzones catch the same off-by-one overflows the 8051 hits.
- Builds define **`RTLP_HOST_TEST`**, which hides the firmware's libc-named
  prototypes (`memset`/`strlen`/…) in `rtl837x_common.h` so they don't clash with
  glibc. Argument order matches libc, so on-host callers transparently use the C
  library. This guard is compiled out of normal firmware builds — zero on-target
  effect.
- A **SIGALRM watchdog** (`watchdog_arm`) turns an infinite-loop bug into a fast
  test failure instead of a hung runner.

## Current coverage
| Test binary | TU under test | Findings exercised |
|-------------|---------------|--------------------|
| `test_cmd_editor` | `cmd_editor.c` | **C4** — full-line hang + `cmd_buffer` 1-byte overflow; basic entry & backspace regressions |
| `test_port_tables` | `rtl837x_port.c` | VLAN entry layout and round trip, PVID register sharing, static multicast and management entries, per-port flush, trunk membership and hash seed |
| `test_page_json` | `httpd/page_impl.c` + `rtl837x_port.c` | `/vlan.json`, `/vlanlist`, `/l2.json` (walk, wrap marker, paging inside `outbuf`), 64-bit counters in `/status.json` and `/counters.json` |
| `test_httpd_tx` | `httpd/httpd.c` + `uip/uip.c` | a GET of a static file against a client that moves its receive window: ACK accounting, continuation chunks out of flash, retransmission |

## Adding a test for another module
1. Write `test_<module>.c` with `main()` driving the module's entry points and
   `CHECK(cond, msg)` assertions.
2. Add any missing mocked symbols to `support.c` (only what the linker asks for).
3. Add a target in the `Makefile` listing `test_<module>.c support.c ../<module>.c`.
4. If the module calls a firmware libc-name (`memcpy`/`strcpy`/…), it will use the
   C library on host (compatible arg order); provide a `fw_*`-style mock only if
   the semantics differ in a way the test cares about.

Good next targets (pure logic, high finding density): `cmd_parser.c` (C6/C8),
the HTTP header parser in `httpd/httpd.c` (S1), `dhcp.c` (S5).
