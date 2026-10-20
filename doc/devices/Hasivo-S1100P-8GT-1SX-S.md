# Hasivo S1100P-8GT-1SX-S

RTL8373N switch with 8 × 2.5G RJ45 ports and 1 × SFP port. GD25Q16 2 MiB flash.

## Port mapping

| Front port | ASIC MAC | Interface |
| --- | --- | --- |
| 1–8 | 0–7 | RJ45 |
| 9 | 8 | SFP (SDS1) |

## Board signals

| Function | GPIO |
| --- | --- |
| SFP detect | 38 |
| SFP LOS | 37 |
| SFP TX disable | Not connected (`GPIO_NA`) |
| SFP I2C SDA / SCL | 39 / 40 |
| Reset | 54 |

PoE is managed by an RTL8238B and a separate STC8H8K64U controller, not directly by the RTL8373N. RTLPlayground does not control PoE on this board.

## Build

`make MACHINE=HASIVO_S1100P_8GT_1SX_S CI=1`

## Photos

Front panel:

![Front panel](photos/Hasivo-S1100P-8GT-1SX-S/front.jpg)

PCB top:

![PCB top](photos/Hasivo-S1100P-8GT-1SX-S/pcb_top.jpg)

PCB bottom:

![PCB bottom](photos/Hasivo-S1100P-8GT-1SX-S/pcb_bottom.jpg)

