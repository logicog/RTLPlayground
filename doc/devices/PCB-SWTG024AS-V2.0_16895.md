### PCB-SWTG024AS-V2.0_16895

## Brands
|Brand|Type|Managed|PCB|Flash|Chip RTL|
|---|---|---|---|---|---|
| hongyavision | LG-SG5T1 | No | PCB-SWTG024AS-V2.0_16895 | 25Q40 | 8372 |

### Machine target

```text
make MACHINE=SWTG024AS_V2_0 CI=1
```

This target uses `machine/pcb/PCB_SWTG024AS_V2_0_16895.c` and its custom init hook.


### Label specifications

- **Name**: LG-SG5T1
- **Ports**:
  - 5 × RJ45: 10/100/1000/2500 Mbps
  - 1 × SFP+: 1000 / 2500 / 10000 Mbps
- **Power**: 12V DC, 1A  5525 connector 

<img src="photos/PCB-SWTG024AS-V2.0_16895/label.jpg" width="300" />

### What works
- ALL 2.5GBASE-T RJ45 ports work at 10/100/1000/2500 Mbps
- The SFP+ port supports 1G, 2.5G and 10G modules 
- LEDs work with the same indications as the OEM firmware

### Limitations and unverified items

- Online update does not work with 512 KiB flash; use an external flash programmer.

### Front panel

<img src="photos/PCB-SWTG024AS-V2.0_16895/front.jpg" width="600" />

### PCB overview

**Board markings**
- Top silkscreen: PCB-SWTG024AS-V2.0_16895

Top side

<img src="photos/PCB-SWTG024AS-V2.0_16895/pcb_top.jpg" width="300" />

Bottom

<img src="photos/PCB-SWTG024AS-V2.0_16895/pcb_bottom.jpg" width="300" />

### J1, serial console

| `J1` pin | Signal      |
| -------- | ----------- |
| 1        | GND         |
| 2        | RX (Input)  |
| 3        | TX (Output) |

Note,`R52`、`R53`may not be installed.You need to bridge them using either solder or resistors.

## Power supply

Input power is delivered via barell plug, `12V 1A` adapter was provided.
