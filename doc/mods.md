# Modifications

## SPI-Flash Memory

SPI-Flash memory can be replaced with an other type. Because the chip is defect, like #69 & #70 or you have an unmanaged-switch with a small flash size, and want to convert it to a managed-variant to run `RTLPlayground` software on it. Currently `RTLPlayground`-firmware expects `8 MBit / 2 MiB`.

### Size

The SOC (RTL837x network chip) used 24-bit address to access the device. This means that in theory, a memory size up to `2^24 x 8-bit = 16777216 x 8-bit = 128 MBit or 16 MiB` could be used, this is *untested*!

### Speed

The SOC (RTL837x network chip) is connected via an SPI-BUS to the flash memory. SPI-BUS frequency is `62.5 MHz`.
Look in the datasheet for `AC Electrical Characteristics` and lookup symbol `Fr`. Maximum value should be equal or higher than `62.5 MHz`.
A device that supports the highest possible clock speed is not a better device, nor is the SOC going to run faster. So pick one which have common frequency between `80 MHz` to `133 MHz`.
Memory speed is dictated by the SPI-BUS clock frequency which is `62.5 MHz`.

### SPI Operation

By default, SPI-BUS uses `CLK`, `CS`, `DI` and `DO`. To increase the data throughput without increasing the bus frequency, a single command can run in `DUAL SPI operation`. This means that for a specific command `DI` and `DO` are both used to transfer the data to/from the device. So it makes the data transfer twice as fast. Although the SOC datasheet doesn’t mention it, our software is making use of this mode.

### Package

Most use package are `SO8`-type may also called `SOIC8`-type. Which can also have different width. Like `150-mil`, `208-mil` or `300-mil`. Best to measure what you need and confirm the measurement with the device-datasheet.

### Specification

1. Size: At least `16 MBit / 4 MiB` (theoretic max. `128 MBit / 32 MiB`, but is not *tested*!)
2. Speed: `62.5 MHz` or better.
3. Support for `DUAL SPI operation`.

### Known Working

This list is incomplete.

| Brand      | Partnumber |
| ---------- |----------- |
| GigaDevice | GD25Q32E   |
| Winbond    | W25Q16JV   |
| Winbond    | W25Q32FV   |
| Winbond    | W25Q32JV   |
| Winbond    | W25Q16JL   |
| Fundan     | FM25Q16A   |

*NOTE*: Part numbers are incomplete. Part numbers may contain additional information such as package, temperature specifications, and even the number of devices on a reel. So always check the datasheet so that you have the right orderable partnumber.
