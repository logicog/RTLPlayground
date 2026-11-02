# Chinese VLAN editing and cable diagnostics

This change makes the Chinese web UI usable without lossy VLAN names and brings
the existing PHY cable diagnostic into the web interface. It also fixes adjacent
configuration-save and firmware-update paths exercised during device testing.

## VLAN names and configuration

VLAN names are native UTF-8 tokens, up to 15 Unicode characters and 45 bytes.
They start with an ASCII letter or a non-ASCII character; ASCII suffixes permit
letters, digits, underscore and hyphen. Whitespace is not supported, and `d` and
`mgmt` remain reserved. `vlan <vid> - <members...>` explicitly clears a name
without deleting the VLAN. Existing ASCII names supported by the web UI remain
valid.

The parser validates UTF-8 and table capacity before changing the VLAN. It
matches the delete token exactly, so creating an empty VLAN named `devices`
does not execute a delete. JSON preserves validated UTF-8 names. A completeness
flag prevents edits based on a truncated VLAN list.

The editor presents one row per physical port, U/T membership labels, the
management VLAN, PVID changes and a confirmation before applying changes.
Read failures block creation, renaming and membership writes. Mobile layouts use
cards instead of compressing the port matrix.

## Cable diagnostics

`/cabletest.json` exposes the existing asynchronous cable-test state, supported
copper ports and four pair results. The page asks for confirmation because the
selected port temporarily loses link. SFP ports and powered-down PHYs are not
offered. This feature does not manage PoE power.

Distance is a TDR fault/reflection estimate, not a precision length measurement.
Normal and unknown pair results return `distance_dm: null`. An open far end is
needed when estimating the whole cable length. On one measured 1.5 m cable, the
tested unit returned roughly 0.9-1.1 m; no calibration constants were adjusted
to fit that single sample. The driver restores PHY settings after completion
or timeout, and length arithmetic uses a wide intermediate.

## Other fixes in this branch

- Preserve failed/unknown configuration state instead of reporting a successful
  save after a failed read, write or command-log clear.
- Gate web updates on actual flash capacity, bound the final staged page and
  avoid appending a string terminator beyond a binary image.
- Apply a verified update when the final response ACK arrives with FIN.
- Avoid out-of-range factory MAC reads on smaller flash variants.
- Clear obsolete 2.5G EEE advertisements when selecting 100M and correct the
  UI's 100M/2.5G bit labels.
- Preserve edited port controls during polling, validate numeric inputs, keep
  relevant configuration commands during merging and clamp reset counters.
- Default the UI and login page to Chinese while preserving language selection.

## Board profiles and validation

`MIMC_D4S2TXP` aliases the PCB-K0402W-U13-V2.0 layout and its existing board
initialization. The photographed D4S2TXP unit has a 2 MiB BY25Q16B flash.
`LIANGUO_LG_SG4T2` aliases PCB-SWTG024AS-A-V2.0.1_19650; the photographed unit
has a 512 KiB P25D40SH. The latter image has been built and checked, but has
not yet been programmed into that unit. `configs/` contains example static
management profiles; these are not factory firmware backups.

Build with the project's patched SDCC toolchain, for example:

```sh
make MACHINE=LIANGUO_LG_SG4T2 CONFIG=configs/LIANGUO_LG_SG4T2.txt CI=1
make MACHINE=MIMC_D4S2TXP CONFIG=configs/MIMC_D4S2TXP.txt CI=1
make -C test
node test/test_web_ui.cjs
```

The browser tests require Playwright. Set `PLAYWRIGHT_MODULE` to an alternative
module path and `BROWSER_EXE` to a browser executable if necessary; otherwise
they use the Playwright browser, or an installed Edge on Windows. Network
fixtures are used, so browser regression tests never contact a switch.

Validation completed: 12 ASan/UBSan host test executables, 110 browser checks,
both board builds with warnings as errors, image CRC/model/configuration/web
asset checks and code-bank capacity checks. On the MIMC unit, real-device tests
covered Chinese naming/clearing, malformed-name rejection, flash persistence
with a UTF-8 character split across a 256-byte read boundary, 15 web pages and
the open-ended cable test. Earlier hardware acceptance covered all copper/SFP
links, simple broadcast forwarding, an explicitly authorized short mirror test
and cold boot. This does not establish full throughput, PoE load behavior or
real STP/LACP partner interoperability.
