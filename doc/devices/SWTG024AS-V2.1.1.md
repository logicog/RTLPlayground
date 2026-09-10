# Sodola SL902-SWTGW124AS / SWTG024AS-V2.1.1_19023

Build with `make MACHINE=PCB_SWTG024AS_V2_1_1`.
This profile is based on an OEM flash dump, OEM register reads, and insertion
measurements on the exact PCB. **Experimental support, partially tested on
hardware.** A complete 2 MiB SPI image has been flashed with an external programmer. Boot, UART, factory MAC
loading and the LED configuration registers have been checked. Ethernet traffic,
optical links, physical LED behavior, reset-button operation and OEM web upgrade
acceptance remain unverified. The tested Free SFP has intermittent EEPROM read
failures; this profile does not claim to fix them.

## Evidence and target selection

The [original board report](https://github.com/up-n-atom/SWTG118AS/issues/26)
identifies this model and PCB with OEM V200.2.4. The supplied 2 MiB flash dump
has SHA256 `3f370767a178af79a5e97fb5520bd61480b40bfca5e42c03bbc1c965ce49d3b8`
and contains the exact model at `0x1fd000`. Its embedded version string at
`0x5f211` is V200.1.11, not V200.2.4; that discrepancy is unresolved. Both OEM
runtime header and segmented payload checksums validate. The live LED registers
agree with the dump's initialization. Flashrom identifies the SPI flash as
Winbond W25Q16.V, 2 MiB;
RTLPlayground reports JEDEC bytes EF 40 15 and the same capacity.

The A-V2.0.1_19650 4+2 profile supplies the matching port/SFP/LED tables. It is
shared rather than duplicated. This revision differs in reset GPIO48, the OEM
MAC offset, and global LED mux/enable settings. The older
`SWGT024_V2_0_MANAGED` profile has different presence pins, reversed SFP numbering,
reset GPIO54, and different LEDs; it is not the appropriate base.

## Port and GPIO mapping

| Physical port, viewed from front | MAC/logical | SDS | Presence, active low | I2C SDA/SCL |
|---|---:|---:|---:|---|
| RJ45 1–4 | 4–7 | — | — | — |
| Left SFP, port 5 | 3 | 0 | GPIO37 | GPIO41/GPIO40 |
| Right SFP, port 6 | 8 | 1 | GPIO38 | GPIO39/GPIO40 |

RJ45 numbering retains the established family mapping; individual RJ45 ports
were not tested. The OEM runtime associates UI port5 with SDS0/SDA41 and UI
port6 with SDS1/SDA39. Insertion of the Free module in the right cage also changes
the reported SDS1 mode to `0x04` under the OEM firmware, corroborating that side. Under RTLPlayground,
insertion is detected in both cages. The left cage was also checked directly over
UART: GPIO37 returns high on removal. The module sometimes identifies as
`EZconn Corp. ETB43315-7S34-FR`, but EEPROM reads fail repeatedly and complete
SFP initialization has not been demonstrated. The OEM `fiber` command ignores
I2C errors, so its printed data cannot establish successful EEPROM reads.

OEM `regget 4` returns `0x83727000`, confirming RTL8372N (`isRTL8373=0`).
Module insertion readings, with no optical fiber connected:

| Module position | Register 0x44 | Register 0x48 | GPIO37 | GPIO38 |
|---|---|---|---:|---:|
| Absent | 0x6ffb6dff | 0x0a7fdbeb | 1 | 1 |
| Left only | 0x6ffb6dff | 0x0a7fdbcf | 0 | 1 |
| Right only | 0x6ffb6dff | 0x0a7fdbaf | 1 | 0 |

GPIO30 and GPIO50 stay high in all three samples. GPIO34 also changes with
insertion, but its role was not established; it is not used for detection.
The OEM diagnostic labels GPIO37 as LOS, but the insertion evidence and matching
A-V2.0.1 wiring support using it for presence here. **LOS and TX_DISABLE remain
GPIO_NA**: no usable optical LOS or TX-disable wiring was established.

The dump's reset poller at flash `0x32a86` reads **GPIO48**, active low, and
branches to factory reset after its counter threshold. The CLI prints `gpio54`
but passes `0x30` (decimal48) to the direct GPIO reader at flash `0x756c9`.
Physical button operation under RTLPlayground remains untested.
The board hook clears PIN_MUX_1 (`0x7f90`) bits11/12 to select GPIO48 rather than
I2C SCL1, matching the OEM setup path at flash `0x973aa…0x97426`.

## LEDs

Live OEM register values:

```text
7f8c: 20db68bf   65dc: 7f249740   6520: 0023e430
6528: 00100000   653c: 18000041   6540: 01400155
6544: 01411000   6548: 01740141   654c: 00010040
65e0: 08144040   65e4: 1037f309   65e8: 12454391
65ec: 19616555   65f0: 1c79d65a   65f4: 0002181d
```

These decode to the shared A-V2.0.1 LED mux and sets0/1. MAC3 and MAC8 select
set1; copper ports select set0. High LED mux mask is LED29 only (4), with
LED27/28/29 enabled (7). The board hook restores the observed global pad mux and
enable registers after generic LED setup; the shared mode-bit updates produce
`0x0023e430` before the normal system-LED state changes. No constant port-link
speed is forced by this profile.

## Flash and installation boundary

The dump contains an OEM unicast MAC at `0x1fc000`, so the existing MAC reader
uses that offset. The address is not hardcoded into the firmware. Model and
OEM configuration occupy `0x1fd000` and `0x1fe000` respectively.

The normal image is 512 KiB: HTML at `0x40000`, default configuration at
`0x6f000`, active configuration at `0x70000`. RTLPlayground upload staging
starts at `0x80000`. The unchanged OEM installer embeds the normal image and
copies 120 sectors into `[0,0x78000)`, replacing the OEM boot region. Its input
layout is compatible with the format observed in the dump, but OEM web-upgrade
acceptance and recovery from a failed update have not been exercised. The normal
512 KiB image is not a complete image for a 2 MiB SPI programmer write. The tested
external-programmer image combined that firmware with the supplied OEM dump
above offset `0x80000`, preserving the factory MAC sector. No OEM dump or
device-specific full-flash image is distributed with this profile.

No support claim for other SL902 products or SWTG024AS revisions is implied.
