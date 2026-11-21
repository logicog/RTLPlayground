# PoE with the TI TPS23880

`poe/tps23880.c` drives a TI TPS23880 PSE: 8 channels as two I2C devices of
4 channels each, on one of the switch's I2C buses. It implements the
chip-agnostic interface of `poe/poe.h` (as proposed in PRs #265 and #712),
which the `poe` command, `/poe.json` and the web interface's PoE page use.

## Enabling it for a machine

In `machine/<MACHINE>.h`:

    #define MACHINE_POE_CHIP    POE_TPS23880
    #define POE_TPS_ADDR        0x20                  // channels 1-4; 5-8 at +1
    #define POE_I2C_BUS         I2CBUS(sda, scl)      // setup_i2c() muxes it
    #define POE_RESET_GPIO      <GPIO driving RESET>
    #define POE_CHANNEL_PORTS   { 1, 2, 3, 4, 7, 9, 8, 0 }  // port per channel 1-8, 0 = none

Ports are the numbers the console and web interface show. `NO_POE=1` builds
the machine without PoE. Machines without `MACHINE_POE_CHIP` compile none of
it, and their web interface hides the PoE page.

## Reference board: REM-8x

The REM-8x (two RTL8372) has a TPS23880 (U2201) for PoE on seven copper
ports, controlled by its Low switch chip.

| Signal | Connection |
|---|---|
| I2C | SDA_B/SCL_B: Low's SDA0/SCL0 (GPIO47/46), shared with the High chip's slave interface (0x5c). The ESP32's IO13/IO14 only through R1005/R1007, not fitted |
| Address | 0x20 channels 1-4, 0x21 channels 5-8 (A1-A4 low) |
| RESET | POE_RST from Low's GPIO30 (pin 11, R902), pulled up |
| INT | P_INT, pulled up only: the driver polls |

Each port uses one channel (2-pair PoE):

| Channel | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 |
|---|---|---|---|---|---|---|---|---|
| Port | 1 | 2 | 3 | 4 | 7 | 9 | 8 | - |
| Switch | Low 4 | Low 5 | Low 6 | Low 7 | High 4 | High 6 | High 5 | not wired |

## Firmware image

The TPS23880 runs its port state machines from SRAM that is lost at power
off. TI publishes the image in "TPS23880 PoE MSP430 Reference Code for Basic
Applications" (SLVC709) as `SRAMCode[0x4000]` and `ParityCode[0x800]`. It is
not part of this repository:

1. Download `slvc709c.zip` from <https://www.ti.com/tool/download/SLVC709>
   into `stock_fw/` (see `stock_fw/README.md`).
2. Building a TPS23880 machine runs `tools/poe/extract_tps23880.py`,
   which writes `poe/firmware/tps23880.bin` (git-ignored) and puts it into the
   image at `POE_IMAGE_LOCATION` (0x60000). The web pages then have to end
   below it.

The tool also takes `sram_code.c` itself. It checks the sizes and knows the
SHA-256 of the 01.00.00.0C image (SRAM version 1). The file starts with a
16-byte header ("TPSI", lengths, version, sum), which the driver checks before
it sends anything.

## Bring-up

`poe/tps23880.c`, following TI's `tps_UpdateSRAMCode()` and the datasheet:
reset pulse, 100 ms, then on device 0x20:

    0x60 = 0x01, 0x62 = 0x00, 0x63 = 0x80    address pointer to 0x8000
    0x60 = 0xc4                               CPU in reset, parity write
    0x1d = 0xbc, 0xd7 = 0x02, 0x91 = 0, 0x90 = 0, 0xd7 = 0, 0x1d = 0
    2048 parity bytes to 0x61
    0x60 = 0xc5, 0x62 = 0x00, 0x63 = 0x80
    0x60 = 0xc0                               CPU in reset, SRAM write
    the same six preparation writes
    16384 SRAM bytes to 0x61
    0x60 = 0x18                               run from SRAM, parity on

Register 0x41 then reads the image's version. One byte per I2C transfer at
I2C_CTRL2 speed 2: about 2 s on the REM-8x. Then both devices get 2-pair
30 W allocation (0x29 = 0x33), DC disconnect on (0x13) and the enabled
channels auto mode (0x12) with detection and classification (0x14).

At boot every port starts **off**, also when the chip still runs from before a
switch reboot (then without a new download). Only the config or the console
enables ports, so nothing gets power that nobody chose: an uplink, or a PC
whose NIC can look like a powered device. For example the config can have

    poe port 4 on

## Use

| Command | |
|---|---|
| `poe` | firmware version, input voltage, temperature, per port: enabled, powered, class, voltage, current |
| `poe port <n> on\|off` | enable or disable one port |
| `poe global on\|off` | all PoE ports |
| `poe load` | reset the chip, download again, keep the port settings |
| `poe reg <0\|1> <reg> [value]` | raw register access on device 0x20/0x21 (hex) |

`GET /poe.json`:

    {"chip":"TPS23880","fw":1,"vin":47884,"temp":30,
     "ports":[{"port":4,"admin":1,"on":1,"class":0,"mv":47668,"ma":88}, ...]}

`vin` and `mv` in mV, `ma` in mA, class 255 = none. The web interface can
change ports through `/cmd`.

## Registers used

| Reg | Use |
|---|---|
| 0x0c-0x0f | channel status: 7:4 class (1-4, 6 = class 0, 8-b = 5-8), 3:0 detection (4 = valid) |
| 0x10 | 3:0 power enabled, 7:4 power good |
| 0x12 | operating mode, 2 bits per channel, 11 = auto |
| 0x13 | DC disconnect enable |
| 0x14 | 7:4 classification, 3:0 detection enable |
| 0x18 | restart detection/classification |
| 0x19 | 3:0 power on, 7:4 power off |
| 0x29 | power allocation; 0x33 = 2-pair 30 W on both pairs |
| 0x2c | temperature: x * 0.652 - 20 C |
| 0x2e | input voltage, 14 bits, 3.662 mV |
| 0x30 + 4n | channel current, 14 bits, 61.039 uA. Read 2 bytes: a 4-byte read does not continue into the voltage |
| 0x32 + 4n | channel voltage, 14 bits, 3.662 mV |
| 0x41 | firmware (SRAM) version |
| 0x60-0x63 | SRAM control, data, address |

## Tests

`test/test_poe.c` runs the driver against a simulated TPS23880 (the register
mock's I2C hook, a synthetic image): download order and content, the checks
on a bad image, ports off at boot, port control and the status decoding.
