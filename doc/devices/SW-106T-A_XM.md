# SW-106T-A_XM

| Field | Value |
|---|---|
| Board | SW-106T-A_XM |
| SoC | RTL8372N |
| Copper | 4 x 2.5G RJ45 |
| Uplink | 2 x SFP+ |
| Flash | 32 Mbit SPI, DIO |
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