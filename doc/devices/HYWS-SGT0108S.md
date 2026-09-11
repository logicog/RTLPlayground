# Lianguo HYWS-SGT0108S

Following is documentation for unmanaged switch marked as `HYWS-SGT0108S`.

Original software is running UART on 9600 baud rate. Output is very minimal.

Using SPI clamp in-board is the only method for initial installation and update.

Stock flash chip is 512KB in size. Consider replacing for update through Web UI.

- Header for uart is clearly identified.
- The red LED act as a powered-on LED.
- There is no SYS LED.
- There is a footprint for a Reset button. Unpopulated. Unclear if routed back to RTL8373.

### Label specifications

- **Name**: 2.5G Ethernet Switch 8+1
- **Model**: HYWS-SGT0108S
- **Ports**:  
  - 8 × RJ45: 10/100/1000/2500 Mbps  
  - 1 × SFP: 1000 / 2500 / 10000 Mbps  

### What works

- All eight 2.5GBASE-T RJ45 ports at 10/100/1000/2500 Mbps  
- SFP port supporting 1G, 2.5G and 10G modules 
- RJ45 LEDs (Blue+White)
- SFP Single LED (Blue)

### Assembled unit

Top side

<img src="photos/HYWS-SGT0108S-unmanaged\Assembled-top.jpg" width="300" />

Front

<img src="photos/HYWS-SGT0108S-unmanaged\Assembled-front.jpg" width="300" />

Bottom

<img src="photos/HYWS-SGT0108S-unmanaged\Assembled-bottom.jpg" width="300" />

### PCB overview

**Board markings**  
- Top silkscreen: 2G5F_20G_V1.01 / 2023-09-28  

Top side

<img src="photos/HYWS-SGT0108S-unmanaged\PCB-top.jpg" width="300" />

Bottom

<img src="photos/HYWS-SGT0108S-unmanaged\PCB-bot.jpg" width="300" />

Original SPI Flash Chip

<img src="photos/HYWS-SGT0108S-unmanaged\og_spi_flash.jpg" width="300" />

## Power supply

Input power is delivered via barell plug, `12V 1A` adapter was provided.
