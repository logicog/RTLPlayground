<!--
  TEMPLATE — copy this file to doc/devices/<PCB-marking>.md and fill in the
  sections marked with <PLACEHOLDERS>. Delete sections that do not apply and
  any HTML comments once the page is finished.

  Conventions used by the existing pages:
  * File name = the PCB silkscreen marking, e.g. PCB-SWTG115AS-V2.1-17443.md.
  * Photos live in doc/devices/photos/<same-name>/ and are referenced with
    relative paths. Either <img src="..." width="300" /> or ![alt](...).
  * Add the finished board to doc/supported_devices.md.
  * Only document what has been verified; list the rest under "Unverified".
-->

# <Brand> <Model> (<PCB marking>)

<!-- One short paragraph: SoC variant, PHY(s), port count, flash size, PoE. -->

<Intro: e.g. "RTL8372N with an RTL8221B 2.5G PHY: 4 x 2.5G RJ45 + 2 x SFP+,
2 MB flash, no PoE.">

> [!CAUTION]
> Only keep this callout on boards with an internal PSU or PoE fed from mains
> voltage. Remove it otherwise.
> This device operates on mains voltage, which can be a dangerous or potentially
> lethal hazard. Even after the device is unplugged, high-voltage capacitors can
> remain charged. Do not open a mains powered device unless you understand how to
> do so safely. Opening the device may void your warranty.

## Brands

<!-- One row per retail variant known to use this PCB. State "???" where a
     value is not verified instead of guessing. -->

| Brand | Type | Managed | Variant | PCB | PCB Label | Flash | Chip RTL |
|-------|------|---------|---------|-----|-----------|-------|----------|
| <Brand> | <Model> | <Yes/No> | <4C_2SFP / 5C_1SFP / -> | <PCB marking> | <label sticker marking or ???> | <512 KiB / 2 MiB (W25Q16JV) / ???> | <8272 / 8372N / 8373 + 8224N> |
| <Rebrand> | <Model> | <Yes/No> | | | | | |

<!-- If the PCB ships in several electrical variants, explain here which
     MACHINE target belongs to which silkscreen, because the front panel
     labelling alone does not identify the wiring. -->

## Label specifications

| Item | Value |
|------|-------|
| Manufacturer | <name as printed> |
| Model | <model as printed> |
| PCB marking | `<silkscreen text>` |
| Ports | <n> x RJ45 10/100/1000/2500 Mbps<, with PoE>, <n> x SFP+ 1G/2.5G/10G |
| Power input | <12 V DC, 1.5 A barrel / internal AC PSU 100-240 V, DC 53.5 V 2.8 A> |
| Stock firmware | <version> |
| Stock defaults | IP <192.168.x.x>, user `<user>`, password `<pass>` |
| Serial number | <if printed on the label> |
| MAC address | <value, or "not printed on the label"> |

<!-- Note label vs. silkscreen mismatches, e.g. a label that names a managed
     model while the PCB is the unmanaged one. Also note rejected MAC values. -->

## CPU, PHY and flash

| Part | Identification |
|------|----------------|
| SoC | <RTL8372N: `regget 4` returns `0x83727000`, boot console prints `Detecting CPU: ...`> |
| PHY | <RTL8221B / RTL8224, reported as `RTL8224 ID: 0x82240000`, or "none, internal PHYs only"> |
| Flash | <vendor/part (U<n>), size, JEDEC id `ef 40 15`> |
| PoE controller | <RTL8238C / HS104PTI at I2C 0x1A and 0x2A / none> |

## Build target

```
make MACHINE=<NAME>
```

<Optional: `CI=1` for a reproducible image. Note whether the board file is a
real definition or a symlink to a sibling board, and what differs (name only,
LED mux, GPIO).>

The image is written to `output/rtlplayground-<version>-<NAME>.bin`.

## Installation

<!-- Keep the methods that actually work for this board; drop the rest. -->

**From the stock web interface**

1. <Upload `installer/output/rtlplayground_oem_upgrade.bin` on the firmware page.>
2. <Device reboots into RTLPlayground.>

**Serial console / bootloader**

1. <Baud rate of the stock firmware vs. RTLPlayground (115200).>
2. <Keys to interrupt the loader, upload command.>

**Flash programmer (SPI)**

1. <SOIC-8 clip or desoldered chip; flashprog chip name to use.>
2. <If in-circuit reading fails (ID reads `ef ff ff`), explain the workaround.>

> [!IMPORTANT]
> Back up the whole flash before writing: the factory MAC address and other
> device-specific data live in it (see "MAC address" below).

## What works

- <All <n> 2.5GBASE-T RJ45 ports at 10/100/1000/2500 Mbps>
- <SFP+ ports with 1G / 2.5G / 10G modules, including DDM data where available>
- <LEDs behave as on the stock firmware>
- <Reset button: short press reboots, >10 s restores the default config>
- <Module insert/remove detection and loss-of-signal reporting>

## Unverified / known issues

- <SFP lamp pads not mapped yet, so an inserted module lights nothing.>
- <UART footprint unpopulated, baud rate unknown.>
- <PoE is not configurable from RTLPlayground.>
- <2.5G SFP modules untested, none was available.>

## Photos

<!-- Photos belong in doc/devices/photos/<same-name>/. -->

Front panel:

<img src="photos/<PCB marking>/front.jpg" width="600" />

Label:

<img src="photos/<PCB marking>/label.jpg" width="600" />

## PCB overview

**Board markings**

- Top silkscreen: `<text>`
- Alternative marking on the same board: `<text>`

**Key components**

- U<n>: <SPI NOR flash, 2 MiB>
- U<n>: <PHY / PoE controller>
- J<n>: <unpopulated serial header>
- SW<n>: <reset button>

Top side:

<img src="photos/<PCB marking>/pcb_top.jpg" width="600" />

Bottom side:

<img src="photos/<PCB marking>/pcb_bottom.jpg" width="600" />

## Variants

<!-- Only for PCBs that ship in more than one electrical variant. Repeat the
     block below per variant and state the difference in population, wiring or
     GPIO use, e.g. "4C_2SFP vs. 5C_1SFP". -->

### <PCB marking>_<variant>

- Difference to the other variant: <e.g. SFP2 TX-disable shares GPIO54 with the reset button>
- Population: <R105 installed / R85 not installed>
- Build with `make MACHINE=<NAME>_<VARIANT>`

## Connectors

### SFP cages

<!-- One row per cage. pin_tx_disable is GPIO_NA on boards that leave it
     unconnected or that the SoC cannot drive. -->

| SFP | logical port | pin_detect (ModAbs) | pin_los | pin_tx_disable | SerDes | I2C SDA | I2C SCL |
|-----|--------------|---------------------|---------|----------------|--------|---------|---------|
| <J4, left> | <8> | <GPIO30_ACL_BIT3_EN> | <GPIO37> | <GPIO_NA> | <SDS1> | <GPIO39_I2C_SDA4> | <GPIO40_I2C_SCL3_MDC1> |
| <J2, right> | <3> | <GPIO50_I2C_SCL2_UART1_TX> | <GPIO51_I2C_SDA2_UART1_RX> | <GPIO_NA> | <SDS0> | <GPIO41_I2C_SDA3_MDIO1> | <GPIO40_I2C_SCL3_MDC1> |

<!-- Component-level pinout of a cage, when it was traced. -->

| `<J4>` pin | Signal | Component | GPIO | Notes |
|------------|--------|-----------|------|-------|
| 2 | TX_FAULT | <T-R262> | --- | |
| 3 | TX_DISABLE | <T-R268> | <GPIO38> | <Pull-down 200R too strong for the SoC> |
| 4 | MODDEF2 - SDA | <T-R266> | <GPIO39> | |
| 5 | MODDEF1 - SCL | <T-R267> | <GPIO40> | <Shared with both cages> |
| 6 | MODDEF0 - PRESENT | <T-R296> | <GPIO30> | <Low when a module is inserted> |
| 7 | RATE SEL | <B-R257> | --- | |
| 8 | LOS | <T-R270> | <GPIO37> | <High without light> |
| 9 | TO? | <B-R256> | --- | |

Component numbering `<L>-<REFDES>-<SIDE>`: `T` = top, `B` = bottom, side letter
for the resistor orientation.

### J<n>, serial console

| `<J1>` pin | GPIO | Signal |
|------------|------|--------|
| 1 | | <3V3 / TX (Output)> |
| 2 | | <GND> |
| 3 | | <RX (Input)> |
| 4 | <GPIO31> | <TX (Output) / 3V3> |

- Settings: <115200 baud, 8N1, 3.3 V TTL>; stock firmware uses <57600>.
- <Header unpopulated, holes filled — clear them with a 1.2 mm drill or
  desoldering wick.>

### T<n>, slave interface

<!-- Optional: only on boards that expose the RTL837x slave port. -->

This connector goes to U<n> `I2C EEPROM` and U<n> `SPI FLASH`. Depending on the
`IF_SEL` bootstraps the port is `I2C`, `SPI` or `SMI`; on this board it is <I2C
on address 0x5c>.

| `T<n>` pin | what | Signal |
|------------|------|--------|
| 1 | <U4-P6, 33R U10-P6> | <I2C-SCL, SPI-CLK, Slave SCK/SCL/MDC/EE_SCL> |
| 2 | <GND> | --- |
| 3 | <U4-P5, U10-P5> | <I2C-SDA, SPI-DI/DO, Slave SDI/SDA/MDIO/EE_SDA> |
| 4 | <VCC> | |
| 5 | <33R -> U10-P2> | <SPI-DO/D1> |
| 6 | <U10-P1> | <SPI-CS> |

Register access: read `<Dev-ADDR><RegAddr15:8><RegAddr7:0>` then
`<DevAddr><Data7:0><Data15:8><Data23:16><Data31:24>`; register `0x0004` returns
the chip id (<`0x83720000`>).

## Board signals

<!-- Summary table of the pins a reader cares about before touching the machine
     definition. Detailed wiring goes into the GPIO map below. -->

| Function | GPIO | Notes |
|----------|------|-------|
| SFP detect (ModAbs) | <30> | <low with a module inserted> |
| SFP LOS | <37> | <high without light> |
| SFP TX disable | <GPIO_NA> | <not connected> |
| Reset button | <54> | <active low, `GPIO54_ACL_BIT2_EN`> |
| System LED | <28> | <low active> |
| UART0 TX / RX | <31 / 32> | |

## Reset

| Function | GPIO | Notes |
|----------|------|-------|
| Reset button | <54> | <active low> |

| Hold time while running | Effect |
|-------------------------|--------|
| <less than 3 s> | <no action> |
| <3-10 s> | <switch chip reset, ~20 s downtime> |
| <more than 10 s> | <restore default configuration> |

<!-- Reset circuit detail, if traced: -->
<Reset line found at `T-D3-D`, active low. `T-R78` 33k pull-up, `T-C187` RC
delay.>

## LEDs

<!-- Per-port lamp wiring. Name the GPIO, the port it belongs to and the LED set
     used in the machine definition. -->

| NAME | GPIO | Port(s) | Function | Notes |
|------|------|---------|----------|-------|
| <PORT1 LED GREEN> | <GPIO13> | <5> | <Activity> | <LEDS_2G5, LEDS_LINK, LEDS_ACT> |
| <PORT1 LED ORANGE> | <GPIO14> | <5> | <Speed> | <LEDS_1G, LEDS_100M, LEDS_10M, LEDS_LINK, LEDS_ACT> |
| <SFP LED GREEN> | <GPIO24> | <6 (SFP J4)> | <Multi-speed> | <LEDS_10G, LEDS_5G, LEDS_2G5, LEDS_1G, LEDS_LINK, LEDS_ACT> |
| <LED-SYSTEM> | <GPIO28> | --- | <System status> | <active low> |

<!-- LED sets as configured in machine/<NAME>.c. -->

Copper ports use LED SET0, SFP ports use LED SET1.

| SET | LED pin 0 | LED pin 1 | LED pin 2 | LED pin 3 |
|-----|-----------|-----------|-----------|-----------|
| SET0 | <Green - 2.5G link with activity> | <Amber - 1G/100M/10M link with activity> | <unpopulated> | <unpopulated> |
| SET1 | <All speeds - any link with activity> | <10G link with activity> | <unpopulated> | <unpopulated> |

<!-- If the stock mux does not fit, say so explicitly: pin n belongs to port
     n+1, function m; which pads are not wired; that `led_mux_custom = 1` is
     needed and which table entry differs from the default. -->

## MAC address

<Factory MAC is stored at `0x1FC000`, six bytes followed by `0xFF` padding. The
stock value is a locally administered address which the firmware rejects, so the
device falls back to a UUID-derived MAC.>

## Flash layout

| Offset | Content |
|--------|---------|
| `0x000000` | <RTLPlayground image> |
| `0x1FC000` | <factory MAC address> |
| `0x1FF000` | <device-specific data / configuration> |

## Power supply

<Board has two rails, `0.95 V` core and `3.3 V`. The core rail is made by a
`Richtek RT8120A` buck converter (0.95 V within 3 %), `3.3 V` by a `TMI3244T`
(4 A capable, 3.3 V within 4.5 %), so higher power SFP modules work.>

## PoE

<!-- Remove if the board has no PoE. -->

The <n> copper ports support PoE, powered by <RTL8238C / internal 150 W PSU,
DC 53.5 V 2.8 A>. RTLPlayground does not provide PoE management for this board;
the PoE hardware runs with its own configuration, exactly as on the stock
firmware.

## Machine definition notes

<!-- Optional: only the details a reader cannot infer from the tables above. -->

- `reset_pin`: <GPIO54_ACL_BIT2_EN>
- `sfp_port[0]`: <pin_detect GPIO30, pin_los GPIO37, pin_tx_disable GPIO_NA,
  sds 1, I2CBUS(GPIO39_I2C_SDA4, GPIO40_I2C_SCL3_MDC1)>
- `high_leds`: <mux = LED_27 | LED_28_SYS | LED_29, enable = LED_28_SYS | LED_29>
- `mac_flash_offset`: <0x1FC000>
- Custom init: <`machine_custom_init()` writes `RTL837X_REG_LED_GLB_IO_EN = 0x7f24977f`.>

## Notes

<!-- Caveats, cross-references, known firmware quirks, footnotes. -->

- <The stock firmware blinks both speed LEDs on activity; RTLPlayground follows
  the front panel labelling.>
- <Values copied from a sibling board are marked as such and are not verified
  on this hardware.>

## References

- [Adding a new device](../adding-a-device.md)
- [Supported devices](../supported_devices.md)
- <Vendor page, forum thread or teardown used as a source.>

<!--
CHECKLIST before committing:
[ ] `make MACHINE=<NAME>` builds and `make machine_check` passes
[ ] every port, SFP cage, LED and the reset button tested on hardware
[ ] unverified items listed instead of omitted
[ ] row added to doc/supported_devices.md, link uses the file name above
[ ] photos placed in doc/devices/photos/<same-name>/ and paths check out
[ ] no leftover <PLACEHOLDERS> or guidance comments
-->
