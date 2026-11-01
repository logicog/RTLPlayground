# Adding a new device

A device is supported by adding one board header `.h` and board definition `.c` file
under `machine/`. 
the build selects the definition by name, and CI compiles every file it finds.

To compile for your added device, do:
```
make MACHINE=<MACHINENAME>
```
## Choosing a name
Names are derived from the board, not invented.

* <PCB silkscreen> is the name used for documentation. Take the silkscreen
as printed and replace `<space>` with `_`. It names `doc/devices/<PCB silkscreen>.md`
and its photo folder `doc/devices/photos/<PCB silkscreen>/`. So for example `S25G2206FP V1.1` becomes
`S25G2206FP_V1.1`; `PCB-K0402W-V3.0` stays as `PCB-K0402W-V3.0`.
* <PCB model> is the name used for machine/pcb/ file names and needs to be more tool friendly.
It starts from the same silkscreen and does the further conversion: <space> and all
punctuation become `_`. So for example machine/pcb/<PCB model>.c for `PCB-K0402W-V3.0` is
`machine/pcb/PCB_K0402W_V3_0.c`. Vendor names are not part of it. A board with
no identifying PCB marking is called `NONAME_<model_name>`.
When differently wired boards share one PCB model, the vendor is appended to disambiguate
(<PCB model>_<vendor>).
* <machine> is <vendor>_<model name>. If the same <vendor>_<model name> already exists
for a different PCB, append the PCB model i.e.: <vendor>_<model name>_<PCB model>.
* `MACHINE_MODEL` is the human-readable "<Vendor Name> <model_name>".
When several boards share one configuration, one PCB is the baseline and the
others symlink to it. The baseline is the lowest revision of the same major
model, or alphabetically earliest major model when they differ. Boards that
are electrically different but carry the same PCB model are distinguished in
the baseline name by <PCB model>_<vendor>.

In addition to the PCB and model, devices can have "plugins" (usually in a literal sense)
and are usually additional boards that add functionality to the base model. They are
also used to clarify differences in the Uplink/10GBE port population on boards.
These are represented by the following suffixes, appended onto the <PCB model> with
two `_` (underscores). Additional plugins are appended with a single underscore (`_`)

The current known list of plugins is:
* POE: POE
* Reset/VLAN-switch: _VLANSW
* Special Case[^1]: _UNMANAGED

Uplink port configurations:
* 1xSFP+1xRJ45: _1S1C
* 2xRJ45: _2C
* For the 8x1-variant: _1C

For example for `MY_BOARD_V1.0.c` the 1SFP, 1RJ, POE variant would be: `MY_BOARD_V1.0__1S1C_POE.c`

[^1]: This is only used in the situation where it is necessary to disambiguate between a
managed and unmanaged device that share the same PCB but do not share the same pin map. In all cases
the managed switch assumes the base name and the unmanaged has _UNMANAGED appended.

## Discovering LED's
With the stock firmware still on the device, you can use the UART console to discover the pinout:
```
$ ./tools/led_gpio_utils/dec_leds_from_rtl837x_uart.py  --port=/dev/ttyUSB0 --baudrate=57600
Reading 22 registers from /dev/ttyUSB0 at 57600 baud, about 17 s ...
Decoding ports 0..8 (from model 0x83737000)
LED pad Configuration:
00 01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f 10 11 12 13 14 15 16 17 18 19 1a 1b
00 01 04 05 08 09 0c 09 0d 10 11 0e 14 11 12 15 15 16 18 19 1a 19 1d 1e 1c 1d 20 21
.led_mux = { 0x00, 0x01, 0x04, 0x05, 0x08, 0x09, 0x0c, 0x09, 0x0d, 0x10, 0x11, 0x0e, 0x14, 0x11, 0x12, 0x15, 0x15, 0x16, 0x18, 0x19, 0x1a, 0x19, 0x1d, 0x1e, 0x1c, 0x1d, 0x20, 0x21 },

LED-set Configuration:
LED-ID         0               1               2               3
SET 0:     00141           00174           01000           00141
SET 1:     00155           10140           00041           01800
SET 2:     00170           00144           40141           10140
SET 3:     00400           00200           f013f           f007f
Port 0: SET 0: ( 2G5 LINK ACT), ( 1G 100M 10M LINK ACT), ( DUPLEX), ( 2G5 LINK ACT)
Port 1: SET 0: ( 2G5 LINK ACT), ( 1G 100M 10M LINK ACT), ( DUPLEX), ( 2G5 LINK ACT)
Port 2: SET 0: ( 2G5 LINK ACT), ( 1G 100M 10M LINK ACT), ( DUPLEX), ( 2G5 LINK ACT)
Port 3: SET 0: ( 2G5 LINK ACT), ( 1G 100M 10M LINK ACT), ( DUPLEX), ( 2G5 LINK ACT)
Port 4: SET 0: ( 2G5 LINK ACT), ( 1G 100M 10M LINK ACT), ( DUPLEX), ( 2G5 LINK ACT)
Port 5: SET 0: ( 2G5 LINK ACT), ( 1G 100M 10M LINK ACT), ( DUPLEX), ( 2G5 LINK ACT)
Port 6: SET 0: ( 2G5 LINK ACT), ( 1G 100M 10M LINK ACT), ( DUPLEX), ( 2G5 LINK ACT)
Port 7: SET 0: ( 2G5 LINK ACT), ( 1G 100M 10M LINK ACT), ( DUPLEX), ( 2G5 LINK ACT)
Port 8: SET 1: ( 2G5 1G 100M LINK ACT), ( 10G LINK ACT), ( 2G5 LINK), ( COL DUPLEX)


        .led_sets = { { LEDS_2G5 | LEDS_LINK | LEDS_ACT,
                        LEDS_1G | LEDS_100M | LEDS_10M | LEDS_LINK | LEDS_ACT,
                        LEDS_DUPLEX,
                        LEDS_2G5 | LEDS_LINK | LEDS_ACT
                      },
                      { LEDS_2G5 | LEDS_1G | LEDS_100M | LEDS_LINK | LEDS_ACT,
                        LEDS_10G | LEDS_LINK | LEDS_ACT,
                        LEDS_2G5 | LEDS_LINK,
                        LEDS_COL | LEDS_DUPLEX
                      },
                      { LEDS_100M | LEDS_10M | LEDS_LINK | LEDS_ACT,
                        LEDS_1G | LEDS_LINK | LEDS_ACT,
                        LEDS_5G | LEDS_2G5 | LEDS_LINK | LEDS_ACT,
                        LEDS_10G | LEDS_LINK | LEDS_ACT
                      },
                      { LEDS_TX,
                        LEDS_RX,
                        LEDS_10G | LEDS_TWO_PAIR_5G | LEDS_5G | LEDS_TWO_PAIR_2G5 | LEDS_2G5 | LEDS_TWO_PAIR_1G | LEDS_1G | LEDS_500M | LEDS_100M | LEDS_10M | LEDS_ACT,
                        LEDS_10G | LEDS_TWO_PAIR_5G | LEDS_5G | LEDS_TWO_PAIR_2G5 | LEDS_2G5 | LEDS_TWO_PAIR_1G | LEDS_1G | LEDS_500M | LEDS_100M | LEDS_10M | LEDS_LINK
                      }
                },

Pin MUX Configuration:
PIN_MUX_0 (0x7f8c) = 0x20db6880   pads 0-29: 0 = LED function, 1 = GPIO
PIN_MUX_1 (0x7f90) = 0x40000041   one or two bits per pin, see below
PIN_MUX_2 (0x7f94) = 0x00000000   pins 30, 52-54: 0 = GPIO, 1 = ACL function
PIN_GLB_IO_EN (0x65dc) = 0x7f24977f

Pin      Field             Value    Function
GPIO0    PIN_MUX_0[0]      0        LED0
GPIO1    PIN_MUX_0[1]      0        LED1
GPIO2    PIN_MUX_0[2]      0        LED2
GPIO3    PIN_MUX_0[3]      0        LED3
GPIO4    PIN_MUX_0[4]      0        LED4
GPIO5    PIN_MUX_0[5]      0        LED5
GPIO6    PIN_MUX_0[6]      0        LED6
GPIO7    PIN_MUX_0[7]      1        GPIO
GPIO8    PIN_MUX_0[8]      0        LED8
GPIO9    PIN_MUX_0[9]      0        LED9
GPIO10   PIN_MUX_0[10]     0        LED10
GPIO11   PIN_MUX_0[11]     1        GPIO
GPIO12   PIN_MUX_0[12]     0        LED12
GPIO13   PIN_MUX_0[13]     1        GPIO
GPIO14   PIN_MUX_0[14]     1        GPIO
GPIO15   PIN_MUX_0[15]     0        LED15
GPIO16   PIN_MUX_0[16]     1        GPIO
GPIO17   PIN_MUX_0[17]     1        GPIO
GPIO18   PIN_MUX_0[18]     0        LED18
GPIO19   PIN_MUX_0[19]     1        GPIO
GPIO20   PIN_MUX_0[20]     1        GPIO
GPIO21   PIN_MUX_0[21]     0        LED21
GPIO22   PIN_MUX_0[22]     1        GPIO
GPIO23   PIN_MUX_0[23]     1        GPIO
GPIO24   PIN_MUX_0[24]     0        LED24
GPIO25   PIN_MUX_0[25]     0        LED25
GPIO26   PIN_MUX_0[26]     0        LED26
GPIO27   PIN_MUX_0[27]     0        LED27
GPIO28   PIN_MUX_0[28]     0        SYS_LED
GPIO29   PIN_MUX_0[29]     1        GPIO
GPIO30   PIN_MUX_2[3]      0        GPIO
GPIO31   PIN_MUX_1[0]      1        UART0_TX
GPIO32   PIN_MUX_1[0]      1        UART0_RX
GPIO33   PIN_MUX_1[2]      0        GPIO_INT
GPIO34   PIN_MUX_1[3]      0        MDC0
GPIO35   PIN_MUX_1[4]      0        MDIO0
GPIO36   PIN_MUX_1[30]     1        GPIO
GPIO37   -                 -        GPIO (no mux field)
GPIO38   -                 -        GPIO (no mux field)
GPIO39   PIN_MUX_1[29]     0        I2C_SDA4
GPIO40   PIN_MUX_1[6:5]    0b10     I2C_SCL3
GPIO41   PIN_MUX_1[6:5]    0b10     I2C_SDA3
GPIO42   -                 -        SPI flash (no mux field)
GPIO43   -                 -        SPI flash (no mux field)
GPIO44   -                 -        SPI flash (no mux field)
GPIO45   -                 -        SPI flash (no mux field)
GPIO46   PIN_MUX_1[8:7]    0b00     GPIO
GPIO47   PIN_MUX_1[10:9]   0b00     GPIO
GPIO48   PIN_MUX_1[12:11]  0b00     GPIO
GPIO49   PIN_MUX_1[14:13]  0b00     GPIO
GPIO50   PIN_MUX_1[16:15]  0b00     GPIO
GPIO51   PIN_MUX_1[18:17]  0b00     GPIO
GPIO52   PIN_MUX_2[0]      0        GPIO
GPIO53   PIN_MUX_2[1]      0        GPIO
GPIO54   PIN_MUX_2[2]      0        GPIO
GPIO55   PIN_MUX_1[19]     0        PTP_CLK125M_IN
GPIO56   PIN_MUX_1[20]     0        PTP_CLK_OUT
GPIO57   PIN_MUX_1[21]     0        PTP_TOD_OUT
GPIO58   PIN_MUX_1[22]     0        PTP_PPS_OUT
GPIO59   PIN_MUX_1[23]     0        PTP_TOD_IN
GPIO60   PIN_MUX_1[24]     0        PTP_PPS_IN
GPIO61   PIN_MUX_1[27]     0        SYNCELOCK0
GPIO62   PIN_MUX_1[28]     0        SYNCELOCK1
GPIO63   -                 -        no mux field of its own
```
Further details can be found in [led_gpio_utils/README.md].

## What a new device consists of

| File | Purpose |
|------|---------|
| `machine/pcb/<PCB model>.c` | the `struct machine machine` board definition |
| `machine/pcb/<PCB model>_init.c` | optional `machine_custom_init()` boot hook, for registers the machine definition cannot express |
| `machine/<machine>.h` | `MACHINE_MODEL` (the model string shown in the UI and the CLI) and `MACHINE_CHIPSET` (`RTL8372` or `RTL8373`, note: in the case of RTL837xN the N is omitted) |
| `machine/<machine>.c` | a symlink to the pcb/<PCBNAME>.c file
| `doc/devices/<PCB silkscreen>.md` + `doc/supported_devices.md` | user-facing documentation |

`<machine>` is the machine symbol without its `MACHINE_` prefix, so `MACHINE` is
passed exactly as spelled on the command line: `make MACHINE=MY_BOARD_1_0`
needs `machine/MY_BOARD_1_0.c` and `machine/MY_BOARD_1_0.h`.

## Step 1: the board header

`machine/<machine>.h` carries the product string and the SoC family. Use the
existing files as the template:

```c
#ifndef _MACHINE_MY_BOARD_1_0_H_
#define _MACHINE_MY_BOARD_1_0_H_

#define MACHINE_MODEL "Brand Model V1.0"
#define MACHINE_CHIPSET RTL8373

#endif
```

`MACHINE_CHIPSET` is `RTL8373` or `RTL8372`; a build without it will fail. It selects the default
logical port range: `MACHINE_MIN_PORT` is `0` for RTL8373 and `3` for RTL8372, while
`MACHINE_MAX_PORT` is `8`, the highest logical port index. A board with a different range defines
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

The struct should contain important comments such as the LED colour and
port-type as shown in the following example and nothing else:

```c
#include "machine_defs.h"

__code const struct machine machine = {
	MACHINE_COMMON_SETTING,
	.n_sfp = 2,
	.log_to_phys_port = {0, 0, 0, 6, 1, 2, 3, 4, 5},
	.phys_to_log_port = {4, 5, 6, 7, 8, 3, 0, 0, 0},
	.is_sfp= {0, 0, 0, 2, 0, 0, 0, 0, 1},
	// Left SFP port (J4)
	.sfp_port[0].pin_detect = GPIO30_ACL_BIT3_EN,
	.sfp_port[0].pin_los = GPIO_NA,
	.sfp_port[0].pin_tx_disable = GPIO_NA,
	.sfp_port[0].sds = 1,
	.sfp_port[0].i2c = I2CBUS( GPIO39_I2C_SDA4, GPIO40_I2C_SCL3_MDC1 ),
	// Right SFP port (J2)
	.sfp_port[1].pin_detect = GPIO50_I2C_SCL2_UART1_TX,
	.sfp_port[1].pin_los = GPIO_NA,
	.sfp_port[1].pin_tx_disable = GPIO_NA,
	.sfp_port[1].sds = 0,
	.sfp_port[1].i2c = I2CBUS( GPIO41_I2C_SDA3_MDIO1, GPIO40_I2C_SCL3_MDC1 ),
	.reset_pin = GPIO54_ACL_BIT2_EN,
	.high_leds = { .mux = LED_27 | LED_29, .enable = LED_28_SYS | LED_29 },
	.port_led_set = { 0, 0, 0, 1, 0, 0, 0, 0, 1},
	.led_sets = {
		{   /* RJ45: Green LED */
			 LEDS_10G | LEDS_1G | LEDS_100M | LEDS_10M | LEDS_ACT | LEDS_LINK,
			0,
			/* Amber LED */
			LEDS_2G5 | LEDS_LINK | LEDS_ACT,
			0
		}, { /* SFP PORT: SINGLE GREEN LED */
			LEDS_10G | LEDS_2G5 | LEDS_1G | LEDS_100M | LEDS_10M | LEDS_ACT | LEDS_LINK,
			0,
			0,
			0,
		},
	},
};
```

`MACHINE_COMMON_SETTING` (from `machine_defs.h`) initializes `machine_name` and
`isRTL8373` from the header's `MACHINE_MODEL` and `MACHINE_CHIPSET`. The logical
port range comes from `MACHINE_MIN_PORT` and `MACHINE_MAX_PORT` in `machine.h`.
Keep it as the first initializer and list the board-specific fields after it.

### Fields

| Field | Meaning |
|-------|---------|
| `MACHINE_COMMON_SETTING` | expands to `machine_name` (`MACHINE_MODEL`) and `isRTL8373` (`1` for `RTL8373`, `0` for `RTL8372`); both are derived from the header's `MACHINE_CHIPSET` (see Step 1) |
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

### Optional custom init hook

If the board needs custom initialization, add
`machine/pcb/<PCBNAME>_init.c` defining `machine_custom_init()`. The Makefile picks the
file up by name and compiles it.

Boards that need the same hook share one file: keep the code in a single
`machine/pcb/<PCBNAME>_init.c` whose `machine/pcb/<PCBNAME>.c` is a real file (not a symlink),
and make the other boards' `_init.c` symlinks to it, for example:

```
ln -s KP_9000_6XH_X2_init.c machine/pcb/MY_BOARD_1_0_init.c
```

## Step 3: build and verify

```
make MACHINE=MY_BOARD_1_0 CI=1
```

The generated flash image appears as `output/rtlplayground-<version>-MY_BOARD_1_0.bin`. An
unknown or empty `MACHINE` fails immediately with a message naming the expected
file, before anything is compiled.

Then run the checks CI runs:

```
make machine_check    # compiles machine/<NAME>.c and its optional _init.c for every machine
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
2. Add `doc/devices/<MACHINENAME>.md` describing the board, the build target
   (`make MACHINE=MY_BOARD_1_0`), what works and what is unverified, plus
   photos and the PCB silkscreen marking.
   The photos should consist of: the front and reverse of the PCB and should be
   as straight as possible and croppped to show just the PCB; front of the device.
   The rear of the device is only necessary if it has a switch or power-port, other
   photos such as the label are not required.
   If the device has an internal power supply, it is critical that a warning is
   added to the documentation regarding the danger such as:
   ```
   [!CAUTION]
   This device contains a mains voltage power supply, which is a potentially lethal hazard. Even after the device is unplugged, high-voltage capacitors can remain charged afterward and may still shock you. Do not attempt to open/disassemble a mains powered device unless you understand how to do so safely.
   ```

3. If the board shares a PCB with other revisions, follow
   `doc/devices/KP-9000-6XH-X2.md` and document which `MACHINE` to pick for
   which silkscreen.

### Identifying the CPU variant

Use the read-only console command below to distinguish the CPU variants:

```text
regget 4
```

| Register 0x0004 value | CPU |
|---|---|
| `0x83700000` | RTL8370 |
| `0x83720000` | RTL8372 |
| `0x83727000` | RTL8372N |
| `0x83730000` | RTL8373 |
| `0x83737000` | RTL8373N |

Record the identified CPU in the device documentation. These values identify
only the CPU variant; document PHY and flash-chip models separately when verified.

Note:
The RTL8372 is always paired with the RTL8224 whereas
the RTL8372N is always paired with the RTL8224N
