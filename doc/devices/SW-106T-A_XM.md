# SW-106T-A_XM

| Field | Value |
|---|---|
| Board | SW-106T-A_XM (provisional, see PCB marking) |
| SoC | RTL8372N |
| Copper | 4 x 2.5G RJ45 |
| Uplink | 2 x SFP+ |
| Flash | 8 Mbit / 1 MB (stock); the development unit has a 4 MB part fitted by the maintainer |
| Final firmware | V0.2.6 |

## Ports

```text
physical 1-4 -> logical 4-7
SFP5 -> logical 8, SDS1, GPIO38
SFP6 -> logical 3, SDS0, GPIO37
MDI reverse 0xa90 = 0x03
```

## LEDs

```text
no link        all off
port 1         1 green blink
port 2         2 green blink
port 3         3 green blink
port 4         4 amber blink
SFP5           5 blue blink
SFP6           6 blue blink
```

Port 4 is amber on this unit; measured behaviour, not a fault.

## Key registers

```text
LED_PORT_SET_SEL = 0x00030ac0
LED_GLB_IO_EN    = 0xffffffff
LED_GLB_ACTIVE   = 0x3fffffff
led_mux          custom, pad based
```

## Build

```sh
make MACHINE=SW-106T-A_XM CONFIG=config/default.txt VERSION=0.2.6
```

## Notes

- 512 KB image, no 4 MB padding
- Web firmware update preserves the configuration sector
- A configured image needs `factory-reset`, restore-defaults, or a full
  programmer write

## PCB marking

The case label only gives the vendor and model (Xiaomi SW-106T-A). The PCB
silkscreen has **not been photographed or confirmed** yet, so the identifier
`SW-106T-A_XM` used for the PCB source and for this document is derived from the
model name and is provisional.

If the silkscreen turns out to be different, this document and
`machine/pcb/SW_106T_A_XM.c` get renamed; a *different board revision* would
instead get its own `machines.json` entry (like the Horaco HC-SWTGW218AS V2.0 /
V2.1 pair) rather than changing this one.

## Flash

Retail units carry a cFeon Q80C-104HIP (8 Mbit / 1 MB) on the back side, and the
board has a second, unpopulated flash footprint. The unit used for development had
its flash replaced with a 4 MB part; **the 512 KB image fits both** (one half holds
the image, the other half stages the update).

A stock 1 MB device is therefore the reference configuration; anything with a
different flash layout gets its own `machines.json` entry.

This is an unmanaged device: the stock firmware stores no factory MAC, so
`mac_flash_offset = 0` and the address is derived from the chip UUID (locally
administered, `06:` prefix), which is stable per unit.
