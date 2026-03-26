### ZX-SWTGW215AS

## Brands
|Brand|Type|Managed|PCB|Flash|Chip RTL|
|---|---|---|---|---|---|
| Lianguo | ZX-SWTGW215AS | Yes | PCB-SWTG115AS-V2.0-16030 | 2MB (FM25Q16A) | RTL8372 family |

## Overview

This device is a managed 5-port plus 1 SFP switch supported by RTLPlayground.

- 5 x RJ45 ports
- 1 x SFP port
- Managed web firmware
- Original SPI flash chip: FM25Q16A

## RTLPlayground target

Use machine target `MACHINE_LIANGUO_ZX_SWTGW215AS` for this device.

## Notes

- The original unit was fitted with an `FM25Q16A` flash chip.
- In the current codebase, this device is treated as a 5+1 managed variant in the RTL8372 family.
- There is a second SOIC8/SOP8 footprint (label U10) beside the flash chip. Likely
- Uart follows standard pinout scheme for these units. 
- T8 Connector seems to be a JTAG port for main CPU
- T3 connector is wired to second(unpopulated) SPI flash footprint
