# iKuai IK-S3009MT

RTL8373N switch with 8 × 2.5G RJ45 ports and 1 × SFP+ port. 1 MiB SPI flash.

## Port mapping

| Front port | ASIC MAC | Interface |
| --- | --- | --- |
| 1–8 | 0–7 | RJ45 |
| 9 | 8 | SFP+ (SDS1) |

## Board signals

| Function | GPIO |
| --- | --- |
| SFP detect | 38 |
| SFP LOS | Not connected (`GPIO_NA`) |
| SFP TX disable | Not connected (`GPIO_NA`) |
| SFP I2C SDA / SCL | 39 / 40 |
| Reset | Not connected (`GPIO_NA`) |
| Flash mode | Single I/O (`flash_sio = 1`) |

## LEDs

| Port | Green LED | Amber LED |
| --- | --- | --- |
| 1–8 (RJ45) | 2.5G link/activity | 1000M / 100M / 10M link/activity |
| 9 (SFP+) | 10G link/activity | 1000M / 100M / 10M link/activity |

High LED pins 27, 28, and 29 are enabled for SFP LED driving and system status.

## Build

`make MACHINE=IKUAI_IK_S3009MT`
