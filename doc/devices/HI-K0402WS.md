# Hisource Hi-KO4022WS

Following is documentation for unmanaged switch marked as `Hi-KO4022WS`.

Original software is running UART on 9600 baud rate. 

Using SPI clamp in-board is the only method for initial installation.

The board has two flash chips `BY25Q16BS` with 16M-bit size. The front switch, switches between the two flash chips.
These can be programed independently by using said switch - so it is e.g. possible to run the original and new firmware in parallel.

### Label specifications

- **Name**: 2.5G Ethernet Switch  
- **Model**: Hi-K0402WS  
- **Ports**:  
  - 4 × RJ45: 10/100/1000/2500 Mbps  
  - 2 × SFP: 1000 / 2500 / 10000 Mbps  

### What works (expected from label + similar devices)

- All four 2.5GBASE-T RJ45 ports at 10/100/1000/2500 Mbps  
- Both SFP ports supporting 1G, 2.5G and 10G modules 
- LEDs

### PCB overview

**Board markings**  
- Top silkscreen: PCB-KO4022W-V3.0 / DIP-KO4022WS-V3.0  

Top side

<img src="photos/K0402W-V3.0-unmanaged\PCB-top.jpg" width="300" />

Bottom

<img src="photos/K0402W-V3.0-unmanaged\PCB-bottom.jpg" width="300" />

### T2, serial console

| `J2` pin | Signal      |
| -------- | ----------- |
| 1        | 3V3         |
| 2        | RX (Input) |
| 3        | TX (Output)  |
| 4        | GND         |


## Power supply

Input power is delivered via barell plug, `12V 1A` adapter was provided.
Board has two supply rails. `0.95` and `3.3` volt.

### `0.95` Core Voltage

Voltage is made by a `Techcode TD1720` .

### `3.3` Voltage

Voltage is created by chip marked as `Techcode TD1720`.
There seems to have been a miscalculation when choosing the inductor and the device is ~25% more efficient with an 5V power supply.
