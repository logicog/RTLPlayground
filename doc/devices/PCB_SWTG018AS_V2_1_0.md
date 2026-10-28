### SWTG024AS-V2.1

## Brands
|Brand|Type|Managed|PCB|Flash|Chip RTL|
|---|---|---|---|---|---|
| Sodola   | SL902-SWTGW218AS | Yes | PCB_SWTG018AS_V2_1_0 |   2M     | 8372N |
| Sodola   | SL902-SWTG018AS | No | PCB_SWTG018AS_V2_1_0 |    2M    | 8372N |

### Label specifications

- **Ports**:
  - 8 × RJ45: 10/100/1000/2500 Mbps
  - 1 × SFP+: 1000/2500/10000 Mbps
- **Power**: 1A @ 12V DC

### What works
- ALL 2.5GBASE-T RJ45 ports work at 10/100/1000/2500 Mbps
- The SFP+ ports supports 1G, 2.5G and 10G modules.
- LEDs work with the same indiciations as the OEM firmware.

### PCB overview

**Board markings**
- Top silkscreen: PCB-SWTG018AS-V2.1.0 19030

Top side

<img src="photos/SWTG024AS-V2.1.0/pcb-top.jpg" width="300" />

Bottom

<img src="photos/SWTG024AS-V2.1.0/pcb-bottom.jpg" width="300" />

### Serial console

The four-pin through-hole serial header can be seen to the right of the CPU heatsink in the photo above. It is labelled J1, with pin 1 (Indicated by a triangle on the silkscren) at the bottom. the pinout is also on the silkscreen:

| Pin | Label  |
|-----|--------|
| 1   | TX     |
| 2   | RX     |
| 3   | GND    |
| 4   | 3V3    |

Adding a header is a few moments work with a soldering iron. Only pins 1-3 are required for serial port use, and the stock firmware outputs at 57600bps at boot.
