# Code banks

The 8051 sees 64 KiB of code: the home bank at 0x0000-0x3fff, always there,
and a 48 KiB window at 0x4000-0xffff that `PSBANK` points at one bank of the
flash image. Bank n is linked at 0xn4000 and stored at flash
`(n - 1) * 0xc000 + 0x4000`.

## Placement is done by the build

No source names its bank. The build compiles every banked C file into an
area named `BANKED`, and `tools/place_banks.py` places the files in BANK1 to
BANK`$(CODE_BANKS)` (default 3) before the link, so the free space is shared
out between the banks. `$(BUILDDIR)/banks.txt` lists the result.

Files that have to share a bank are kept together:

- a file that calls a function in another file directly, i.e. one that isn't
  `__banked`;
- a file that reads another C file's `__code` data, since the read goes
  through the bank window;
- a file that takes the address of a function that isn't `__banked`.

Assembly modules name their own bank (`crc16.asm`'s tables in BANK1,
`chacha_8051.asm` in BANK3), and so does the generated `html_data.c`. They
pin the files that call them directly.

The files in `RESIDENT_SRCS` (Makefile) stay in the home bank: the interrupt
handlers and what they call, the flash and NIC code, and the board
definition.

## Writing code

- A function that is called from another file, and that isn't in the home bank,
  should be `__banked` unless the two files belong together anyway.
- `make` fails if the groups don't fit; raise `CODE_BANKS` to add a bank
  (up to 5, the 5th ends at the web UI at 0x40000).
- After each link, `tools/check_cross_bank.py` checks the linked image: a
  direct call into another bank, or a `__code` read across banks, fails the
  build. `make check-banks MACHINE=...` runs it on its own.
- `tools/check_linked.py` runs after each link too. It fails the build on an
  `__at` variable where the linker also places relocatable ones (it doesn't
  reserve the address), a bank fuller than its 48 KiB window, or code that
  writes `PSBANK` and returns without restoring the caller's bank.
