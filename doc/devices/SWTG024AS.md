### SWTG024AS
SWTG024AS has at least 4 variants that look the same.

Variants are `managed` and a `unmanaged` version.
But both have pcb version `v1.0` and `v2.0`. 
Also the RJ45 connectors can be all plastic/non-shielded or with metal shielding.

## Brands
|Brand|Type|Managed|PCB|PCB Label|Flash|Chip RTL|
|---|---|---|---|---|---|---|
| LIANGUO |SWTG024AS |No|  SWTG024AS-v2.0 | CM-23-11-2336 023-17453| 512kB| 8272 |
| Haraco |ZX-SWTG124AS | Yes |  SWTG024AS-v2.0 | ??? | ??? | 8272 |
| Xikestore |SKS3200M-4GPY2XF | Yes |  SWTG024AS-v1.0 | CM-23-08-2043 023-16721 | ??? | 8272 |

# SWTG024AS-v2.0 managed vs unmanged
Changes I found with my board vs [Managed version](https://github.com/up-n-atom/SWTG118AS/tree/main/photos/SWGT024AS-v2.0) of the PCB.

### Bottom
* R105: Installed, goes to R10-PullDown SFP2 -> TX DISABLE
* R85: Not Installed (Connected to K1 Reset Button)
* R90: Not installed (System Led)
* LED3: Not installed (System Led)
### Top
* K1: Not installed (Reset Button)
* R95: Installed (SFP2 signal RX-LOS), means that the managed-version can´t use the RX-LOS function.
* R270: Installed (SFP1 signal RX-LOS), same here as above.
* R268: Installed (SFP2 signal TX_DISABLE, but R262 200R pull-down is to high to drive by the SOC, needs mod!)
* U5: Flash is only 512kB instead of 2/4 MBit.

### Notes
* `TX Disable`-SFP2 and Button `K1` share the same GPIO pin via `R105` and `R85`.
  But via `R88`, `TX Disable`-SFP2 can be mapped to `GPIO36`.
* `TX Disable` pull-down resistos on both SFP are to low to drive by the SOC.
  We need to make a `Best`-BOM variant so we can use all the featues.

# Connectors
|`J4` SFP1 PINs | Signal | Component | GPIO | Notes |
|---|---|---|---|---|
|2|	TX_FAULT	        | B-R262	       | --- | | 
|3|	TX_DISABLE	      | B-R263, T-R268 | GPIO38 | R262 = Pull-down 200R|
|4|	MODDEF2 – SDA     |	B-R261, T-R266 | GPIO39 | |
|5|	MODDEF1 – SCL     |	B-R260, T-R267 | GPIO40	| Shared with both SFP |
|6|	MODDEF0 – PRESENT |	B-R259, T-R296 | GPIO30 | |
|7|	RATE SEL          |	B-R257	       | ---    | |
|8|	LOS	              | B-R258, T-R270 | GPIO37 | | 
|9|	TO?               | B-R256	       | ---    | |

|`J2` SFP2 PINs | Signal | Component | GPIO | Notes |
|---|---|---|---|---|
|2|	TX_FAULT	        | B-R70	         | ---	  | |
|3|	TX_DISABLE	      | B-R10, B-R105-R, T-R88-L | GPIO54	| R10 = Pull-down 200R |
|4|	MODDEF2 – SDA     |	B-R26, T-R85   | GPIO41 | |
|5|	MODDEF1 – SCL     |	B-R15, T-R87   | GPIO40	| Shared with both SFP |
|6|	MODDEF0 – PRESENT |	B-R14, T-R89   | GPIO50	| |
|7|	RATE SEL          |	B-R12	         | ---	  | | 
|8|	LOS	              | B-R13, T-R95   | GPIO51	| | 
|9|	TO?               | B-R11	         | ---	  | | 

Note: component numbering `<L>-<REFDES>-<SIDE>`
* L: Layer, T=Top, B=Bottom
* REFDEES: full silkscreen like `R123`
* SIDE: Side of the component. when the rj45 are facing towards you are you can read the silkscreen normal.
  L = Left, R=right, B=bottom, T=top or P with a pin number.

### T3
This connector seems to go to U4 and U10.
I thing is used to connect a external CPU to controlle the SOC.
Even to program the flash via the SOC.
Signals are based on that `U4` is likely a I2C-EEPROM, `U10` is likely other SPI-chip.
|`T3` pin|what|Signal|
|---|---|---|
| 1 | U4-P6, 33R U10-P6| I2C-SCL, SPI-CLK |
|2| GND | --- |
|3| U4-P5, U10-P5  | I2C-SDA, SPI-DI/DO |
|4| VCC  | 
|5| 33R -> U10-P2 | SPI-DO/D1 | 
|6| U10-P1 | SPI-CS |

### T5, serial console
|`T5` pin|GPIO|Signal|
|---|---|---|
| 1 | GPIO31 | U0TXD (Output) |
| 2 | GND | |
| 3 | GPIO32 | U0RXD (Input) |
| 4 | 3V3 | |

### T8
|`T8` pin|what|Signal|
|---|---|---|
| 1 | GPIO46 | |
| 2 | GND    | |
| 3 | GPIO48 | |
| 4 | 3V3    | |
| 5 | GPIO47 | |
| 6 | GPIO49 | |

# Reset ciruit
| Cmp | Function |
|---|---|
| T-R78  | 33k PullUp      |
| T-D3   | Discharge Diode |
| T-C187 | RC-Delay        |

Reset-line found at `T-D3-D` active-low.

# GPIO

| HEX VAL. | GPIO   | Component | What |  | GPIO | Component | What |
| -------- | ------ |  ---- | ---- | ---- | ---- | ---- | ---- |
| 00000001 | GPIO00 | T-C151T, T-R28T, T-R29T |?    | | GPIO32 | T-R143-R | U0RXD |
| 00000002 | GPIO01 | T-C152T                 |?    | | GPIO33 |  |  |
| 00000004 | GPIO02 | T-C153T                 |?    | | GPIO34 |  |  |
| 00000008 | GPIO03 | T-R33T                  |?    | | GPIO35 |  |  |
| 00000010 | GPIO04 | B-C155                  |?    | | GPIO36 | T-R88L | Optional SFP-TX-DIS[^2] |
| 00000020 | GPIO05 | B-C156                  |?    | | GPIO37 | SFP1-8, R270 | SFP-LOS |
| 00000040 | GPIO06 | T-C157T                 |?    | | GPIO38 | SFP1-3, R268 | SFP-TX-DIS[^2] |
| 00000080 | GPIO07 | T-C158T, R165           |?    | | GPIO39 | SFP1-4, R266 | I2C-SDA4 |
| 00000100 | GPIO08 |                         |     |  | GPIO40 | SFP2-5, T-R87; SFP1-5, T-R267; | I2C-SCL |
| 00000200 | GPIO09 | SFP2-LED?               |LEDx[^1] | | GPIO41 | SFP2-4, R85 | I2C-SDA |
| 00000400 | GPIO10 |                         |     | | GPIO42 |  |  |
| 00000800 | GPIO11 | SFP2-LED?               |LEDx[^1] | | GPIO43 |  |  |
| 00001000 | GPIO12 | PORT1-LED-COMMON        |LEDx[^1] | | GPIO44 |  |  |
| 00002000 | GPIO13 | PORT1-LED-GREEN         |LEDx[^1] | | GPIO45 |  |  |
| 00004000 | GPIO14 | PORT1-LED-YELLOW        |LEDx[^1] | | GPIO46 | T8-1 | ? |
| 00008000 | GPIO15 | PORT2-LED-COMMON        |LEDx[^1] | | GPIO47 | T8-5 | ? |
| 00010000 | GPIO16 | PORT2-LED-GREEN         |LEDx[^1] | | GPIO48 | T8-3 | ? |
| 00020000 | GPIO17 | PORT2-LED-YELLOW        |LEDx[^1] | | GPIO49 | T8-6 | ? |
| 00040000 | GPIO18 | PORT3-LED-COMMON        |LEDx[^1] | | GPIO50 | SFP2-6, R89 | SFP-DETECT |
| 00080000 | GPIO19 | PORT3-LED-GREEN         |LEDx[^1] | | GPIO51 | SFP2-8, R95 | SFP-LOS |
| 00100000 | GPIO20 | PORT3-LED-YELLOW        |LEDx[^1] | | GPIO52 |  |  |
| 00200000 | GPIO21 | PORT4-LED-COMMON        |LEDx[^1] | | GPIO53 |  |  |
| 00400000 | GPIO22 | PORT4-LED-GREEN         |LEDx[^1] | | GPIO54 | SFP2-3, T-R105L | SFP-TX-DIS[^2] or via T-R85 to RESET[^3] |
| 00800000 | GPIO23 | PORT4-LED-YELLOW        |LEDx[^1] | | GPIO55 | | |
| 01000000 | GPIO24 | SFP1-LED?               |LEDx | | GPIO56 | | |
| 02000000 | GPIO25 |                         |     | | GPIO57 | | |
| 04000000 | GPIO26 | SFP1-LED?               |LEDx | | GPIO58 | | |
| 08000000 | GPIO27 | R44L                    |?    | | GPIO59 | | |
| 10000000 | GPIO28 | R50                     |?    | | GPIO60 | | |
| 20000000 | GPIO29 |                         |     | | GPIO61 | | |
| 40000000 | GPIO30 | SFP1-6, R269            |SFP-DETECT | | GPIO62 | | |
| 80000000 | GPIO31 | T-R144-R                |U0TXD| | GPIO63 | | |

# Power supply

Board has two supply rails.
`0.95` and `3.3` volt.

## `0.95` Core Voltage.

Voltage is made by a `Richtek RT8120A` Buck converter.
0.95V must be within 3%.

## `3.3` Voltage

Voltage is crated by a `TMI3244T` Buck converter.
3.3V must be within 4.5%.
Chip can deliver up to 4A and the sweetspot is at 1A.
So higher power SFP-modules should work.


[^1]: LEDs are found by just plugin a RJ45 connector and see with cmd `gpio` the status change. But the bit pattern for port 1,2 are diffrent from port 3,4.
[^2]: Only on the unmanaged verions are `R10` and `R268` placed. But the very low pull-down resistor `R10` & `R262` prevent to SOC to drive does pins. A mod is needed.
[^3]: GPIO54 is used for the reset-button. `T-R85` is placed.
