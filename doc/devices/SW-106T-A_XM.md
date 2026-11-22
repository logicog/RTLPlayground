Xiaomi SW-106T-A

RTL8372N-based 4×2.5G + 2×SFP+ managed switch with a Web interface.

The PCB is marked `SW-106T-A_XM`; the vendor/model name is only on the case
label.

### Label specifications

- **Manufacturer**: Xiaomi
- **Model**: SW-106T-A
- **Ports**:
  - 4 × RJ45: 10/100/1000/2500 Mbps
  - 2 × SFP+: 1G / 2.5G / 10G
- **Power**: 12 V DC

### What works

- All four 2.5GBASE-T RJ45 ports, including 2.5G link
- Both SFP+ cages with 10G and 10G copper (10GBASE-T) modules
- Copper LEDs: one LED per port, follows link/activity, all LEDs off without
  link
- SFP+ LEDs: one LED per cage, follows link/activity
- Web interface, VLANs, port statistics, configuration save
- Flash is accessed in DIO mode (`flash_sio` stays 0); the 32 Mbit chip is not
  fully used by the 512 KB image

### Not verified

- The reset button footprint is unpopulated and the case has no opening for
  it, so `reset_pin` stays `GPIO_NA`.

### Board specifics

- **Main switch**: RTL8372N under a heatsink
- **Flash**: 32 Mbit SPI NOR (SOP-8)
- Copper MDI pair polarity must be set to `0x03` on register `0xa90`, otherwise
  physical ports 1 and 2 do not link. This is expressed with `.mdi_reverse` in
  the board definition.
- Physical ports 1-4 map to logical ports 4-7. The left SFP+ cage is physical
  port 5 / logical port 8 / SDS1 with detect on GPIO38; the right cage is
  physical port 6 / logical port 3 / SDS0 with detect on GPIO37. Neither cage
  has a LOS or TX-disable pin wired.
- The LED wiring is not the stock mux; `.led_mux` is set explicitly in the
  board definition.

### Installation

The stock firmware has a Web interface but its firmware images are signed, so
RTLPlayground has to be written with a programmer / SPI clamp the first time.
After that the Web interface can be used for updates, as with any other board.

Back up the stock flash before the first write.