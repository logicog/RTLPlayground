# PCB-SWTG118AS-V2.1-17462

This is the documentation for the board marked as `SWTG118AS-V2.1-17462`.
It is a revision of the [PCB-SWTG118AS-V2.0-16029](PCB-SWTG118AS-V2.0-16029.md)
board with a different SFP detect pin.

## Brands

| Brand | Type | Managed | PCB | Flash | Chip RTL |
|---|---|---|---|---|---|
| XikeStor | SKS3200-8E1X | Yes | SWTG118AS-V2.1-17462 | 2MB (25Q16JVSIQ) | RTL8373 |
| XikeStor | SKS3200M-8GPY1XF | Yes | SWTG118AS-V2.1-17462 | 2MB (25Q16JVSIQ) | RTL8373 |

## Machine target

Select the machine target by the PCB silkscreen, not by the retail model name.
The `SWTG118AS-V2.1-17462` boards use the GPIO38 SFP detect pin, so they must
not use the V2.0-16029 definition, which expects GPIO30.

| Device | Machine target |
| --- | --- |
| XikeStor SKS3200-8E1X | `MACHINE=XIKESTOR_SKS3200_8E1X` |
| XikeStor SKS3200M-8GPY1XF | `MACHINE=XIKESTOR_SKS3200M_8GPY1XF` |

Both targets reuse `pcb/PCB_SWTG018AS_V2_1_0.c` and
`pcb/PCB_SWTG018AS_V2_1_0_init.c`.

Build example:

```
make MACHINE=XIKESTOR_SKS3200M_8GPY1XF
```

## Status

- Build verified for `XIKESTOR_SKS3200M_8GPY1XF` with `make ... CI=1`.
- SFP detection with the GPIO38 detect pin was confirmed for the SKS3200-8E1X
  in #781.

## PCB

PCB silkscreen: `SWTG118AS-V2.1-17462`.

## Photos

PCB (front):

<img src="photos/PCB-SWTG118AS-V2.1-17462/pcb_top.jpg" width="800" />

PCB silkscreen marking:

<img src="photos/PCB-SWTG118AS-V2.1-17462/pcb_label.jpg" width="600" />

Winbond `25Q16JVSIQ` flash chip:

<img src="photos/PCB-SWTG118AS-V2.1-17462/pcb_flash.jpg" width="600" />

Device label:

<img src="photos/PCB-SWTG118AS-V2.1-17462/case_bottom.jpg" width="800" />
