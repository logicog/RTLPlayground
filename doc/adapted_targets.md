# Lianguo and SEEKER 4+2 build profiles

These PCB revisions have four 2.5G RJ45 ports and two 10G SFP+ slots.
Select the PCB revision rather than a similar brand or firmware label.

| PCB | MACHINE | CPU | Hardware validation |
|---|---|---|---|
| Lianguo 2G5F4_10G2 V1.01 | `LIANGUO_2G5F4_10G2_V1_01` | 20.8 MHz | Owner reports boot, 2.5G/10G links, LEDs, saving, software restart and power-cycle recovery |
| Same PCB | `LIANGUO_2G5F4_10G2_V1_01_125MHZ` | 125 MHz | Boot confirmed; other checks not separately reported |
| SEEKER RTL-4GT-2S+ V1.03 | `SEEKER_RTL_4GT_2S_PLUS_V1_03` | 20.8 MHz | Experimental; not hardware tested |
| Same PCB | `SEEKER_RTL_4GT_2S_PLUS_V1_03_125MHZ` | 125 MHz | Experimental; not hardware tested |

Both frequency profiles share the respective board's port, GPIO and LED
configuration. All four use single-IO Flash Fast Read (`0x0b`, eight dummy
cycles), preserve the boot value of the fast-SPI bit, and select nominal
9600 8N1 UART. Timer2 retains the 200 Hz system tick at either frequency.
The unsuffixed names select 20.8 MHz; `_125MHZ` selects 125 MHz. These
settings apply only to these targets; existing boards retain their defaults.

## Build

With the dependencies listed in the [README](../README.md), run:

```sh
make CI=1 MACHINE=LIANGUO_2G5F4_10G2_V1_01
make CI=1 MACHINE=LIANGUO_2G5F4_10G2_V1_01_125MHZ
make CI=1 MACHINE=SEEKER_RTL_4GT_2S_PLUS_V1_03
make CI=1 MACHINE=SEEKER_RTL_4GT_2S_PLUS_V1_03_125MHZ
```

No extra compiler define is needed. The `MACHINE` argument omits the
`MACHINE_` prefix used in C. SDCC 4.5 and GNU make 4.3 or newer are required;
on macOS use a suitable `gmake` or the project's Docker environment:

```sh
docker build -t rtlplayground-dev .
docker run --rm -v "$PWD:/workspace" rtlplayground-dev \
    make CI=1 MACHINE=SEEKER_RTL_4GT_2S_PLUS_V1_03_125MHZ
```

The generated firmware is the 512 KiB `.bin` in `output/<MACHINE>/`.
`output/rtlplayground.bin` points to the last generated target; use the
explicit target directory when switching between boards.

## Flashing and updating

Keep a complete original Flash backup and follow the project's programmer
instructions. The standard image starts at Flash offset zero. It is also
the format accepted by an existing RTLPlayground Web updater, subject to
Flash capacity; do not assume an OEM Web updater accepts it.

Lianguo's reported T25S40 Flash is 512 KiB, so firmware updates require a
programmer. RTLPlayground needs at least 1 MiB for Web update staging.
SEEKER's reported 25q16cs16 Flash has a 2 MiB programmer backup; actual
Flash detection, Web upload and reboot still require hardware validation.
The board target does not package an OEM backup into a full-chip image.

See the [Lianguo device notes](devices/LIANGUO_2G5F4_10G2_V1_01.md) and
[SEEKER device notes](devices/SEEKER_RTL_4GT_2S_PLUS_V1_03.md) for wiring,
reference evidence and unverified details.
