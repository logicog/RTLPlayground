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
Changes I found with my own board vs [Managed version](https://github.com/up-n-atom/SWTG118AS/tree/main/photos/SWGT024AS-v2.0) of the PCB.

### Bottom
*  R105: Installed, goes to R10-PullDown SFP2 -> TX DISABLE
*  R85: Not Installed (Connected to K1)
*  R90: Not installed (PowerLed) 
*  LED3: Not installed (PowerLed)
### Top
* K1: Not installed (Reset butten)
* R95: Installed (SFP2 signal RX-LOS), mean that the manage version can use RX-LOS function.
* R270: Installed (SFP1 signal RX-LOS)
* R268: Installed (SFP2 signal TX_DISABLE, but R262 200R pull-down is to high to drive by the SOC, needs mod!)
* U5: Flash is only 512kB instead of 2/4 MBit.

### Notes
* `TX Disable`-SFP2 and Button `K1` share the same GPIO pin via `R105` and `R85`.
  But with `R88`, `TX Disable`-SFP2 can be mapped to `C4`.
* `TX Disable` pull-down resistos on both SFP are to low to drive by the SOC.
  We need to make a `Best`-BOM variant so we can use all the featues.

# Connectors
|`J4` SFP1 PINs | Signal | Component | GPIO | Notes |
|---|---|---|---|---|
|2|	TX_FAULT	      | B-R262	       | --- | | 
|3|	TX_DISABLE	      | B-R263, T-R268 | C6	 | R262 = Pull-down 200R|
|4|	MODDEF2 – SDA     |	B-R261	       | C7	 | |
|5|	MODDEF1 – SCL     |	B-R260, T-R267 |---	 | |
|6|	MODDEF0 – PRESENT |	B-R259, T-R296 | B30 | |
|7|	RATE SEL          |	B-R257	       | --- | |
|8|	LOS	              | B-R258, T-R270 | C5	 | | 
|9|	TO?               | B-R256	       | --- | |

|`J2` SFP2 PINs | Signal | Component | GPIO | Notes |
|---|---|---|---|---|
|2|	TX_FAULT	      | B-R70	      | ---	| |
|3|	TX_DISABLE	      | B-R10, T-R88-L | C22	| R10 = Pull-down 200R |
|4|	MODDEF2 – SDA     |	B-R26         | C9	| |
|5|	MODDEF1 – SCL     |	B-R15, T-R87  |	---	| |
|6|	MODDEF0 – PRESENT |	B-R14, T-R89  | C18	| |
|7|	RATE SEL          |	B-R12	      | ---	| | 
|8|	LOS	              | B-R13, T-R95  |	C19	| | 
|9|	TO?               | B-R11	      | ---	| | 

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

### T8
|`T8` pin|what|Signal|
|---|---|---|
| 1 | C14 | |
| 2 | GND | |
| 3 | C16 | |
| 4 | 3V3 | |
| 5 | C15 | |
| 6 | C17 | |

# Reset ciruit
| Cmp | Function |
|---|---|
| R78 | 33k PullUp|
| D3 | Discharge Diode |
| C187 | RC-Delay |

Reset-line found at `T-D3-D` active-low.

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

