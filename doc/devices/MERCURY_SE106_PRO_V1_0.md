# Mercury SE106 Pro V1.0

Mercury SE106 Pro, a managed 4x 2.5G + 2x 10G SFP+ switch. The stock firmware
(1.0.0 Build 20240812 Rel.65021) has a TP-Link style web interface.

| | |
|---|---|
| SoC | RTL8372N |
| Flash | XMC XM25QH32DHIG, 4 MB (flashrom: `XM25QH32C/XM25QH32D`) |
| PCB marking | `2088501190`, no version on the board; the label says Ver 1.0 |
| Build target | `make MACHINE=MERCURY_SE106_PRO_V1_0` |

## Board specifics

- The copper ports need `phy_mdi_reverse = PHY_MDI_REVERSE(0x3)`: ports 1/2
  with reversed MDI pairs, ports 3/4 without. With the default `0xc` none of
  them links and the PHYs see no link partner at all.
- SFP module detect, TX disable and the reset button sit on a PCA9555-style
  I2C GPIO expander at 0x21 on SCL1/SDA1 (GPIO48/49), see `gpio_exp`:
  P0.3 port 5 module present, P0.1 port 6 module present, P0.4 reset button,
  P0.2/P0.0 TX disable of port 5/6. The TX disable assignment is inferred: the
  stock firmware drives these low once the SerDes of the port is up.
- Port 5 (left cage) is on SerDes 1 with its EEPROM on SDA3/SCL3, port 6 on
  SerDes 0 with its EEPROM on SDA0/SCL0.
- GPIO39 is most likely the board reset line: the stock firmware drives it high
  at boot and low to reboot. Do not use it, also not as SDA4.
- The factory MAC is stored at flash offset 0x1FC000.
- The LED settings are the stock firmware's register writes; the init hook
  reproduces its `LED_GLB_ACTIVE`/`LED_GLB_IO_EN` values and TX polarity 0x596a.

## Status

Working: copper ports 1-4 (1G, and 2.5G tested on port 4), port LEDs (yellow
up to 1G, green 2.5G), both SFP+ cages, reset button, factory MAC, web
interface and firmware update through it.

## Installation

Install with a SOIC-8 clip or another SPI flash programmer. The stock web
updater of firmware 1.8.0 Build 20260129 Rel.40575 rejects
`rtlplayground_oem_upgrade.bin` with "The image has an error. Please refresh
the page and re-upload the image."; other stock versions, including the 1.0.0
Build 20240812 Rel.65021 this board shipped with, are untested. The stock
firmware has a TP-Link style web interface, not the Realtek SDK one the
installer targets. Dump the flash first: Mercury publishes no firmware
download for this model. Write the RTLPlayground image to the first 512 KB and
keep the rest of the flash, in particular the factory pages at
0x1FB000-0x1FFFFF. Later updates work through the web interface.

## Serial console

The unpopulated 4-hole header between the left SFP cage and the SoC carries
the console at 115200 8N1.

| Pin | Signal |
|-----|--------|
| 1 (square pad) | TX |
| 2 | RX |
| 3 | GND |
| 4 | 3.3 V |
