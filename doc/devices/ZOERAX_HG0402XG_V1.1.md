### HG0402XG V1.1

## Brands
|Brand|Type|Managed|PCB|Flash|Chip RTL|
|---|---|---|---|---|---|
| ZoeRax | HG0402XG | No|  HG0402XG V1.1 | 1M (Winbond 25Q) | |

### Label specifications

- **Name**: ZOERAX 2.5G 2SFP 4-Port Gigabit Switch
- **Model**: HG0402XG-EU
- **Ports**:
  - 4 × RJ45: 10/100/1000/2500 Mbps
  - 2 × SFP+: 1000 / 2500 / 10000 Mbps
<img src="photos/HG0402XG_V1_1/HG0402XG_V1_1_bottom.jpg" width="300" />

## PCB

<img src="photos/HG0402XG_V1_1/HG0402XG_V1_1_top.jpg" width="300" />
<img src="photos/HG0402XG_V1_1/HG0402XG_V1_1_bottom.jpg" width="300" />

Be aware that the UART header is metric, the pitch is 2mm. The JST PH2.0mm header shown was addeded after purchase

The slider switch controls microcontroller U4 in order to configure the VLAN on/off settings. U4 can reset the SoC
and also write into its registers to configure VLAN. With RTLPlayground the slider cannot be used for anything else
than triggering reset. Any change in the position will trigger this reset.

### Installation

Using SPI clamp on-board is the only method for initial installation.

### What works

- All four 2.5GBASE-T RJ45 ports at 10/100/1000/2500 Mbps
- SFP ports with various 1G/10G Fiber and Ethernet modules
- LEDs
