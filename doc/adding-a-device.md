# Adding a new device

A device is supported by adding one board header `.h` and board definition `.c` file
under `machine/`. 
the build selects the definition by name, and CI compiles every file it finds.

To compile for your added device, do:
```
make MACHINE=<NAME>
```

## What a new device consists of

| File | Purpose |
|------|---------|
| `machine/<NAME>.c` | the `struct machine machine` board definition |
| `machine/<NAME>.h` | `MACHINE_MODEL` (the model string shown in the UI and the CLI) and `MACHINE_CHIPSET` (`RTL8372` or `RTL8373`) |
| `doc/devices/<name>.md` + `doc/supported_devices.md` | (only if the board is verified and you want to support it publicly) user-facing documentation |

`<NAME>` is the machine symbol without its `MACHINE_` prefix, so `MACHINE` is
passed exactly as spelled on the command line: `make MACHINE=MY_BOARD_1_0`
needs `machine/MY_BOARD_1_0.c` and `machine/MY_BOARD_1_0.h`.

## Step 1: the board header

`machine/<NAME>.h` carries the product string and the SoC family. Use the
existing files as the template:

```c
#ifndef _MACHINE_MY_BOARD_1_0_H_
#define _MACHINE_MY_BOARD_1_0_H_

#define MACHINE_MODEL "Brand Model V1.0"
#define MACHINE_CHIPSET RTL8373

#endif
```

`MACHINE_CHIPSET` is `RTL8373` or `RTL8372`; a build without it will fail. It selects the default
logical port range: `min_port` is `0` for RTL8373 and `3` for RTL8372, while `max_port` is `8`,
the highest logical port index. A board with a different range defines
`MACHINE_MIN_PORT` and/or `MACHINE_MAX_PORT` in its header to override the
defaults.

The device product string is what the CLI or the Web-Interface will print to identify the device; `machine.h` includes it by name.

> [!NOTE]
> `machine_name` is a `char[30]` and `MACHINE_MODEL` is copied into it at
> compile time. A longer model string still builds, but SDCC emits
> `warning 147: excess elements in array of chars initializer` and the name is
> truncated in the UI. Keep it to 29 characters or less.

## Step 2: the board definition

`machine/<NAME>.c` starts with the shared prelude and then defines the single
`machine` object. Copy the closest existing board and adjust it — do not start
from a blank file, the field meanings are easier to see in a real example.
The best choice is usually a device that has the same number and type of Ethernet
ports and the same number of SFP+ cages.

```c
#include "machine_defs.h"

__code const struct machine machine = {
	MACHINE_COMMON_SETTING,
	.n_sfp = 1,
	.log_to_phys_port = {1, 2, 3, 4, 5, 6, 7, 8, 9},
	.phys_to_log_port = {0, 1, 2, 3, 4, 5, 6, 7, 8},
	.is_sfp = {0, 0, 0, 0, 0, 0, 0, 0, 1},

	.sfp_port[0].pin_detect = GPIO30_ACL_BIT3_EN,
	.sfp_port[0].pin_los = GPIO37,
	.sfp_port[0].pin_tx_disable = GPIO_NA,
	.sfp_port[0].sds = 1,
	.sfp_port[0].i2c = I2CBUS( GPIO39_I2C_SDA4, GPIO40_I2C_SCL3_MDC1 ),
	.reset_pin = GPIO_NA,

	.high_leds = { .mux = LED_27 | LED_29, .enable = LED_28_SYS | LED_29 },
	.port_led_set = { 0, 0, 0, 0, 0, 0, 0, 0, 1 },
	.led_sets = {
		{   /* RJ45 ports */
			LEDS_2G5 | LEDS_LINK,
			LEDS_2G5 | LEDS_1G | LEDS_100M | LEDS_10M | LEDS_LINK | LEDS_ACT,
			0,
			0
		}, { /* SFP port, single green LED */
			LEDS_10G | LEDS_2G5 | LEDS_1G | LEDS_100M | LEDS_10M | LEDS_LINK | LEDS_ACT,
			0,
			0,
			0
		}
	},
};
```

`MACHINE_COMMON_SETTING` (from `machine_defs.h`) initializes `machine_name`,
`isRTL8373`, `min_port` and `max_port` from the header's `MACHINE_MODEL` and
`MACHINE_CHIPSET`. Keep it as the first initializer and list the
board-specific fields after it.

### Fields

| Field | Meaning |
|-------|---------|
| `MACHINE_COMMON_SETTING` | expands to `machine_name` (`MACHINE_MODEL`), `isRTL8373` (`1` for `RTL8373`, `0` for `RTL8372`), `min_port` and `max_port`; all are derived from the header's `MACHINE_CHIPSET` (see Step 1) |
| `n_sfp` | number of SFP+ cages |
| `n_10g` | SerDes 10G capability used by the RTL837xN init: `0` none, `1` one, `2` two 10G-capable SerDes |
| `log_to_phys_port[9]` | logical port (0-based index) → physical front-panel port number |
| `phys_to_log_port[9]` | the inverse, physical port 1 is index 0, unused slots are 0 |
| `is_sfp[9]` | `0` for copper, `1` for left slot and `2` for right slot |
| `sfp_port[2]` | per-cage wiring, `sfp_port[0]` is the leftmost cage |
| `reset_pin` | GPIO of the reset button, `GPIO_NA` if unpopulated |
| `high_leds` | mux/enable for the LED pins 27–29 (`LED_27`, `LED_28_SYS`, `LED_29`) |
| `port_led_set[9]` | which LED set (0–3) each logical port uses |
| `led_sets[4][4]` | Each port can have up to 4 GPIO output pins assigned. 4 sets of LEDs with 4 LED-conditions per set; the conditions define which GPIO pins become active for a port given a port link-state. Which LEDs are then enabled depends on the wiring of the pins to up to 3 LEDs for that port, which can make a logical combination of the 4 LED output-pins. |
| `led_mux_custom` / `led_mux[28]` | `1` plus the 28 mux values when the stock LED muxing does not fit the board |
| `mac_flash_offset` | flash offset of the factory MAC address, omit or `0` if the board does not store one |

Each `struct sfp_port` entry holds `pin_detect`, `pin_los`, `pin_tx_disable`,
`sds` (which SerDes drives the cage) and `i2c` (built with `I2CBUS(sda, scl)`,
or `GPIO_NA`-based values where a cage has no such pin).

`I2CBUS(...)` only accepts the pin pairs listed in `machine_defs.h`. A wrong
pair is caught at compile time: SDCC reports
`error 176: sizeof applied to an incomplete type` pointing at the offending
`.i2c` line, because the macro deliberately turns an invalid bus number into an
array of size 0.

### Reusing an existing definition

If the new board is electrically identical to one that already exists — same
port mapping, same SFP wiring, same LEDs — do not copy the file. Make the new
`.c` a symlink and keep only a distinct `.h`, so the two boards cannot drift
apart:

```
ln -s KP_9000_6XHML_X2.c machine/MY_BOARD_1_0.c
```

The new board still needs `machine/MY_BOARD_1_0.h` (pick the version number on
the PCB) with its own `MACHINE_MODEL` and `MACHINE_CHIPSET`: `MACHINE_MODEL` is
what distinguishes the products, and `MACHINE_CHIPSET` must match the SoC of
the board definition being reused.
Existing examples: `machine/PCB_K0402WS_V3.c`, `machine/KP_9000_9XHML_X_V3_2.c`
and the six `KP_9000_*` revision variants.

Two files that differ only in whitespace or comments are duplicates and should
be collapsed this way.

## Step 3: build and verify

```
make MACHINE=MY_BOARD_1_0
```

The generated flash image appears as `output/rtlplayground-<version>-MY_BOARD_1_0.bin`. An
unknown or empty `MACHINE` fails immediately with a message naming the expected
file, before anything is compiled.

Then run the checks CI runs:

```
make machine_check    # compiles machine/<NAME>.c and machine_init.c for every machine
make -C test          # host-side unit tests
```

`make machine_check` discovers the definitions with
`$(wildcard machine/*.c)`, so a new file is covered without registering it
anywhere. CI (`.github/workflows/build.yml`) runs it on every push and pull
request, alongside full builds for a 4+2 and an 8+1 board.

A successful compile only proves the definition is syntactically valid. The
actual behaviour — port numbering, SFP detection, LEDs — has to be confirmed on
hardware, and the console commands (`port`, `stat`, `sfp`, `leds`) are the
tools for that.

## Step 4: documentation

Once the board has been tested on hardware:

1. Add a row to the table in [Supported devices](supported_devices.md).
2. Add `doc/devices/<name>.md` describing the board, the build target
   (`make MACHINE=MY_BOARD_1_0`), what works and what is unverified, plus
   photos and the PCB silkscreen marking.
3. If the board shares a PCB with other revisions, follow
   `doc/devices/KP-9000-6XH-X2.md` and document which `MACHINE` to pick for
   which silkscreen.

### Identifying the CPU variant

For RTL8373-based devices, use the read-only console command below to distinguish
the N and non-N CPU variants:

```text
regget 4
```

| Register 0x0004 value | CPU |
|---|---|
| `0x83730000` | RTL8373 (non-N) |
| `0x83737000` | RTL8373N |

Record the identified CPU in the device documentation. These values identify
only the CPU variant; document PHY and flash-chip models separately when verified.

