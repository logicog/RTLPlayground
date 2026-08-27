# MokerLink POE-2G080110GS

The `POE-2G080110GS` is an unmanaged 8x2.5G RJ45 and 1xSFP+ PoE switch sold by Mokerlink. There is also a `POE-2G080110GSM` managed version of the switch which may be similar but this has not been verified on actual hardware.

## Brands
| Brand  | Type             |Managed| PCB              | Flash           | Chip RTL      |
|--------|------------------|-------|------------------|-----------------|---------------|
| Mokerlink | POE-2G080110GS | No | 2M-PCB51-ML-V1.1 | 4MB (W25Q32JV) | 8373N + 8224N |

## Hardware overview
Front

<img src="photos/POE-2G080110GS/POE-2G080110GS-front.jpg" width="300" />

Label

<img src="photos/POE-2G080110GS/POE-2G080110GS-label.jpg" width="300" />

The stock firmware from a POE-2G080110GS device had a sha256sum of `4c280853465eaad80e1772075c0f2d29c11fb8c561547f06353d3a5443c388e9`.

### PCB

Top silkscreen is marked `2M-PCB51-ML-V1.1`. PoE is provided by RTL8238C. Flash chip is 4 MB Winbond 25Q32JV (U8).

<img src="photos/POE-2G080110GS/POE-2G080110GS-pcb.jpg" width="300" />

## Flashing process

This example uses `flashrom` and a CH341A programmer [modded for 3.3V](https://flashrom.org/supported_hw/supported_prog/ch341ab.html).

```
# compile RTLPlayground
make MACHINE=POE_2G080110GS

# for unmanaged POE-2G080110GS with 4MB Winbond: extend firmware to 4MB with 0xFF (or else flashrom gets mad)
objcopy -I binary -O binary --gap-fill=0xff --pad-to=0x400000 output/rtlplayground.bin output/rtlplayground-4MiB.bin

# read stock firmware for backup
sudo flashrom -p ch341a_spi -c W25Q32JV -r mokerlink-stock.bin

# write RTLPlayground firmware
sudo flashrom -p ch341a_spi -c W25Q32JV -w output/rtlplayground-4MiB.bin
```

## Notes

The stock firmware blinks both the 2.5G and 1G LEDs on activity, despite the front of the device indicating that each LED should be independent. The `POE_2G080110GS` machine config behaves as the front of the device indicates (and what seems more logical) -- each port LED turns on and blinks independently.
