# Lianguo 2G5F4_10G2 V1.01

An unmanaged 4 x 2.5G RJ45 + 2 x 10G SFP+ switch with PCB marking
`2G5F4_10G2_V1.01`. Reported Flash marking: T25S40; the stock programmer
backup is 512 KiB. The board has no reset button. This is distinct from
the Lianguo HYWS-SGT0108S 8+1 target.

## Build and startup

```sh
make CI=1 MACHINE=LIANGUO_2G5F4_10G2_V1_01
# Optional 125 MHz profile:
make CI=1 MACHINE=LIANGUO_2G5F4_10G2_V1_01_125MHZ
```

The default uses 20.8 MHz, single-IO Fast Read, the boot value of the
fast-SPI bit, and nominal 9600 8N1 UART. The optional profile uses 125 MHz
with the same Flash policy and UART baud rate. Timer2 remains at 200 Hz.
See [build profiles](../adapted_targets.md) for dependencies and outputs.

An initial 125 MHz build that also enabled fast SPI and Dual-SPI failed:
no LEDs or Ethernet link. Restoring the stock image restored operation.
The 20.8 MHz/SIO profile passed the owner's boot, 2.5G/10G link, LED,
configuration save, software restart and power-cycle checks. The later
125 MHz/SIO profile also boots. Its other functions and long-term stability
were not separately reported. The first failure's cause remains unisolated;
125 MHz alone is not established as the cause.

The 512 KiB Flash cannot stage Web firmware updates (minimum 1 MiB).
Use a programmer and the standard 512 KiB image at offset zero. Keep the
original dump for recovery. Default management settings follow `config.txt`.
UART settings are inferred and compiled, not verified with a serial adapter.

## Port and GPIO configuration

The stock 8051 firmware reads the following signals:

| SerDes | ModAbs | RX_LOS | I2C SDA | I2C SCL |
|---|---|---|---|---|
| SDS1 | GPIO30 | GPIO37 | GPIO39/SDA4 | GPIO40/SCL3 |
| SDS0 | GPIO50 | GPIO51 | GPIO41/SDA3 | GPIO40/SCL3 |

The profile follows the matching `SWGT024_V2_0_UNMANAGED` topology:
RJ45 ports 1-4 at MAC4-7, SFP5 at MAC8/SDS1, SFP6 at MAC3/SDS0.
Actual front-panel order and chip ID/N revision remain unverified; the
configuration assumes RTL8372. Reset and TX_DISABLE are GPIO_NA; software
transmitter control has not been established for this PCB.

LED SET0 reproduces `0x6548=0x0041017f`, `0x6544[15:0]=0x0044`, and
`0x6528[3:0]=0xf` from the stock initialization code. All ports use SET0,
following the apparent hardware default. The high LED mux enables LED29
and disables LED27. Unknown SYS/LED28 and LED3 are disabled; no custom mux
table is used. Basic LEDs work according to the owner; per-speed colors
and physical LED order have not been checked separately.

Reference stock image SHA-256:
`4031331975ec32e53ffe088e830a1a522622243952f7f2ebb584f836680d9e39`.
No stock image is included in the repository.
