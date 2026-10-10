# SEEKER RTL-4GT-2S+ V1.03 (experimental)

Owner-reported model: SEEKER SKS1200-4GPY2XF; PCB `RTL-4GT-2S+ V1.03`;
four 2.5G RJ45 ports and two 10G SFP+ slots. Flash marking: `25q16cs16`;
the programmer backup is 2 MiB. Neither new frequency profile has been
hardware tested. Do not identify this PCB solely by a flashed image's label.

## Build and startup

```sh
make CI=1 MACHINE=SEEKER_RTL_4GT_2S_PLUS_V1_03
# Optional 125 MHz profile:
make CI=1 MACHINE=SEEKER_RTL_4GT_2S_PLUS_V1_03_125MHZ
```

The unsuffixed target uses the working reference's runtime divider 3
(20.8 MHz); `_125MHZ` selects 125 MHz. Both use nominal 9600 8N1 UART,
a 200 Hz tick, single-IO Flash Fast Read, and the boot value of the fast-SPI
bit. The Flash policy follows the successful Lianguo trial; OEM Flash
read-mode control was not fully traced. See [build profiles](../adapted_targets.md).

## Reference evidence and wiring

The initial backup contains an older modified RTLPlayground image labeled
`PCB-K0402WS-V3.0`. It uses GPIO38/37 for module detection, no LOS pins,
and implicit GPIO0 for both TX_DISABLE fields. The owner reports working
RJ45 but no SFP link with that image. Those values do not establish PCB wiring.

The owner supplied `SR-S25G2206F_NOR.bin`, reporting that programmer flashing
it restores all functions except online updating. Its initialization code
provides these GPIO/I2C settings:

| SerDes | Module detection | RX_LOS | I2C SDA | I2C SCL |
|---|---|---|---|---|
| SDS1 | GPIO30 | GPIO37 | GPIO39/SDA4 | GPIO40/SCL3 |
| SDS0 | GPIO50 | GPIO51 | GPIO41/SDA3 | GPIO40/SCL3 |

The new profile assumes RJ45 ports 1-4 at MAC4-7, SFP5 at MAC8/SDS1,
and SFP6 at MAC3/SDS0. Actual panel order and chip ID/N revision remain
unverified; the configuration assumes RTL8372. Reset and TX_DISABLE are
GPIO_NA because their wiring was not established.

MAC3/8 use LED SET1, MAC4-7 use SET0. Recovered register values are
`0x6548=0x0041017f`, `0x6540=0x0040017f`, lower 16 bits of
`0x6544/0x653c=0x0044`, and extended fields `0x6528=0x001f000f`.
The custom hook preserves unrelated fields, clears LED_GLB_IO_EN[8:0],
and sets PIN_MUX_0[8:0]. LED29 is enabled, LED27 disabled, and unknown
SYS/LED28 disabled; no custom LED mux table is applied.

Working reference SHA-256:
`50097370a0d7f21430a7fbd05cc9f3a19bf37f8965576edddce2dc6f84034676`.
No OEM image or owner-specific full-chip image is included in the repository.

## Validation status

Both profiles build with SDCC 4.5 and CI=1. Generated 512 KiB images passed
bank, linked-code, Web-resource, configuration and CRC checks. This does not
establish hardware compatibility: boot, RJ45, both SFP slots, module EEPROM,
LEDs, saving and restart remain to be tested with these new images.

A 2 MiB Flash meets the project's Web-update capacity requirement; actual
Flash detection, uploading and reboot remain unverified. The OEM updater's
failure has not been diagnosed or claimed fixed by this board profile.
