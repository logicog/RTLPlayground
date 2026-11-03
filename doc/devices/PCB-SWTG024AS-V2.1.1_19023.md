# Sodola SL902-SWTGW124AS

Use this profile for the board marked `PCB-SWTG024AS-V2.1.1_19023`.
It has an RTL8372N, 2 MiB SPI flash, four 2.5 GbE RJ45 ports and two SFP+ cages.

```sh
make MACHINE=SODOLA_SL902_SWTGW124AS
```

## Hardware status

- An earlier build booted after flashing with an external SPI programmer.
  UART output, factory MAC loading, LED register values and module insertion
  in both SFP+ cages were observed.
- The tested Free SFP module had intermittent EEPROM reads and did not
  complete initialization. Optical links are unverified.
- RJ45 traffic, visible LED behavior, the reset button and the OEM web
  upgrade are unverified. The updated build has not been tested on hardware.

The build produces a 512 KiB firmware image. It is not a complete 2 MiB
programmer image. The tested programmer image preserved the factory data,
including the MAC address at `0x1fc000`.

See the [original hardware notes](https://github.com/guilhem/RTLPlayground/pull/2).
