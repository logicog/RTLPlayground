# RTL8372/RTL8373 switch registers

Realtek publishes no datasheet for these chips. Everything here was worked out
from the stock firmware (register traces, Ghidra, see [ghidra.md](ghidra.md)),
from Realtek SDK register names seen on stock firmware, from the similar
RTL838x/RTL8367 families, and from experiments with this firmware. The names
are those of [rtl837x_regs.h](../rtl837x_regs.h).

Each entry says how sure we are:

| Mark | Meaning |
|---|---|
| **confirmed** | A working feature depends on it, or it was measured on hardware |
| **trace** | Copied from the stock firmware's register writes; the effect is not understood |
| **comment** | Only a code comment or another doc says so |
| **guess** | Inferred, e.g. from the name or neighbouring registers |

Bits not listed are unknown. Code that changes part of a register keeps the
other bits (read-modify-write) unless it says otherwise.

## Access

The 8051 core reaches the 0x10000 switch registers (16-bit address, 32-bit
value) through SFRs:

- `reg_read_m(reg)` puts bits 31:24 in `sfr_data[0]` … bits 7:0 in
  `sfr_data[3]`; `reg_read(reg)` leaves them in `SFR_DATA_24/16/8/0`.
- `reg_write_m(reg)` writes `sfr_data[]`, `REG_SET(reg, v32)` a value,
  `REG_WRITE(reg, b31_24, b23_16, b15_8, b7_0)` four bytes.
- `reg_bit_set/clear/test(reg, bit)` take a bit **number**.
- `sfr_mask_data(n, mask, set)` counts bytes from the LSB: n=0 is bits 7:0,
  which is `sfr_data[3]`.

Per-port registers come with different strides: `+ 4 * port`, `+ (port << 8)`
(the per-port MAC block at 0x12xx/0x1cxx) or two bits per port in one word.
"Port" is always the chip's logical port; the CPU is logical port 9.
`machine.log_to_phys_port[]` maps to front-panel numbers.

Not switch registers, but reached the same way through SFRs: MDIO to PHYs
(`phy_read/phy_write`, SFRs 0xc2-0xc5, clause 45, writes take a PHY mask) and the
SoC SerDes (`sds_read/sds_write_v`, SFR 0x93 = reg, 0x94 = page << 1 | sds,
only SerDes 0 and 1 are addressable). See [hardware.md](hardware.md).

## Contents

- [Chip, reset, temperature, clock](#chip-reset-temperature-clock)
- [LEDs](#leds)
- [GPIO and pin mux](#gpio-and-pin-mux)
- [I2C master](#i2c-master)
- [SMI (PHY polling)](#smi-phy-polling)
- [SerDes](#serdes)
- [PHY configuration](#phy-configuration)
- [Link and port status](#link-and-port-status)
- [Interrupts](#interrupts)
- [NIC (CPU port DMA)](#nic-cpu-port-dma)
- [MIB counters](#mib-counters)
- [Table access (VLAN and L2)](#table-access-vlan-and-l2)
- [L2 learning and flushing](#l2-learning-and-flushing)
- [VLAN](#vlan)
- [Mirroring](#mirroring)
- [Link aggregation](#link-aggregation)
- [Port isolation](#port-isolation)
- [IGMP / multicast](#igmp--multicast)
- [STP and reserved multicast (RMA) trapping](#stp-and-reserved-multicast-rma-trapping)
- [Bandwidth, storm control and meters](#bandwidth-storm-control-and-meters)
- [EEE and MTU](#eee-and-mtu)
- [Random numbers](#random-numbers)
- [Registers known only by address](#registers-known-only-by-address)
- [Known problems](#known-problems)

---

## Chip, reset, temperature, clock

| Address | Name | Fields | |
|---|---|---|---|
| 0x0004 | `REG_CHIP_ID` | 31:16 chip, 0x8372/0x8373 (0x8224 on the RTL8224 over MDIO MMD30). 15:8 = 0x70 on "N" parts | confirmed |
| 0x000c | `REG_CHIP_INFO` | Write 0xa to 19:16 to unlock the version, read, then clear. Stock: `00200000 → 002a0000 → 202a6818`. Fields not understood | trace |
| 0x0010 | `REG_CHIP_UUID` | Per-chip value, XOR-folded into a local MAC when flash has none | comment |
| 0x0014 | `REG_CHIP_LOT_NO` | As above | comment |
| 0x0024 | `REG_RESET` | bit 0 whole chip reset (`reset_chip()` writes 1). bit 2 NIC reset, self-clearing | confirmed |
| 0x0338 | `TM_CTRL2` | bit 2 latch: set at reset, freezes the temperature at its power-on sample; `tm_init()` clears it | confirmed |
| 0x0340 | `TM_RESULT` | Temperature. Bits 15:0 as int16: °C = v >> 7, fraction (v & 0x7f)/128. Bits 18:16 are 3 more fraction bits, ignored | confirmed |
| 0x0350 | `TM_RESULT_POWERON` | Same format, sampled at power-on (`temp` command) | confirmed |
| 0x06f4 | `REG_SEC_COUNTER` | Not a counter: a 32-bit store the firmware increments every second (uptime, sessions, NTP). Only holds a value while SerDes 0 is on | confirmed |
| 0x06f8 | `REG_SEC_COUNTER2` | Unused | — |
| 0x6040 | `REG_HW_CONF` | 5:4 CPU clock divider from 125 MHz (0 = 125, 1 = 62.5, 2 = 31.25, 3 = 20.8 MHz; stock unmanaged firmware runs at 3). 8 fast SPI. 12 NIC enable. 0 set after port setup, copied from stock | 5:4 = 0 and bit 12 confirmed, rest trace |

## LEDs

Realtek SDK names (seen with `regget` on stock firmware, [devices/2M-PCB23-V2.2.md](devices/2M-PCB23-V2.2.md))
in brackets. Board setup is described in [howto_leds.md](howto_leds.md).

| Address | Name | Fields | |
|---|---|---|---|
| 0x6520 | `REG_LED_MODE` (LED_GLB_CTRL) | 17:16 system LED: 0 off, 1 fast, 2 slow, 3 on (`SYS_LED_*`). 23:21 blink rate. 8:5 and 12:9 two 4-bit mode fields, meaning unknown (0x0021e6b0 copied from stock) | 17:16 confirmed, rest trace |
| 0x6524 | `REG_LED3_0_SET3` (LED3_0_SET3_2_CTRL1) | High-speed conditions for LED sets 2 and 3: one nibble per LED, bit 0 10G, 1 two-pair 5G, 2 5G, 3 two-pair 2.5G. Bytes: set2 LED1:0, set2 LED3:2, set3 LED1:0, set3 LED3:2 | confirmed |
| 0x6528 | `REG_LED3_0_SET1` (LED3_0_SET1_0_CTRL1) | Same for sets 0 and 1 | confirmed |
| 0x652c … 0x6548 | `REG_LED1_0_SET0..3`, `REG_LED3_2_SET0..3` | LED1_0_SETn = 0x6548 − 8n, LED3_2_SETn = that − 4. Two LEDs per register (15:0, 31:16), each a mask of conditions: 0 2.5G, 1 two-pair 1G, 2 1G, 3 500M, 4 100M, 5 10M, 6 link, 7 link flash, 8 activity, 9 RX, 10 TX, 11 collision, 12 duplex, 13 training, 14 master (`LEDS_*`) | confirmed |
| 0x654c | `LED_PORT_SET_SEL` | 2 bits per port: which LED set (0-3) the port uses | confirmed |
| 0x65d8 | `REG_LED_GLB_ACTIVE` | Stock 0x3ffbedff; the clear bits 9, 12, 18 are the pads with pull-downs, so probably per-pad polarity | guess |
| 0x65dc | `REG_LED_GLB_IO_EN` | bit n enables LED output on pad n (0-29). Bit 6 also seems needed for link/speed conditions on some boards | confirmed, partly understood |
| 0x65e0 … 0x65f4 | `REG_LED_GLB_MUX_1..6` | Pad → LED signal mux, 6 bits per pad, 5 pads per register (bits 0/6/12/18/24). 0x3f unused, otherwise apparently port × 4 + LED | layout confirmed, values guess |
| 0x65f8 | `REG_LED_RLDP_1` (LED_RLDP_CTRL_1) | bits 1:0 loop-detection LED indication; firmware clears them, stock sets them | comment |
| 0x65fc | `REG_LED_RLDP_2` | One nibble per port 0-7: port LED takes part in the loop indication | trace |
| 0x6600 | `REG_LED_RLDP_3` | bits 3:0 port 8 | trace |

## GPIO and pin mux

GPIO n is bit n of the 00_31 register, GPIO 32+n bit n of the 32_63 register.
See [gpio.md](gpio.md) for which pin carries which GPIO.

| Address | Name | Fields | |
|---|---|---|---|
| 0x003c / 0x0040 | `REG_GPIO_00_31_OUTPUT` / `_32_63_OUTPUT` | Output level. GPIO36 (0x40 bit 4) resets the RTL8224 on RTL8373 boards | confirmed |
| 0x0044 / 0x0048 | `REG_GPIO_00_31_INPUT` / `_32_63_INPUT` | Pin level (SFP detect/LOS, reset button) | confirmed |
| 0x004c / 0x0050 | `REG_GPIO_00_31_DIRECTION` / `_32_63_` | 1 = output | confirmed |
| 0x7f9c / 0x7fa0 | `REG_GPIO_*_PULL_EN` | Pull resistor enable per pin | comment |
| 0x7fa4 / 0x7fa8 | `REG_GPIO_*_PULL_UP` | Pull-up select | comment |
| 0x7fac / 0x7fb0 | `REG_GPIO_*_PULL_DOWN` | Pull-down select | comment |
| 0x7f8c | `PIN_MUX_0` (IO_MUX_SEL_0) | bit n (0-29): pad n is an LED output instead of GPIO; bits 27-29 the "high" LEDs. The polarity of the low bits differs between boards | bits 10, 27-29 confirmed |
| 0x7f90 | `PIN_MUX_1` (IO_MUX_SEL_1) | bit 0 UART0 TX. I2C routing, 01 = I2C, 00 = GPIO: SCL0 8:7, SDA0 10:9, SCL1 12:11, SDA1 14:13, SCL2 16:15, SDA2 18:17; SCL3+SDA3 6:5 = 10; SDA4 bit 29 = 0. bit 30 GPIO36 as GPIO | I2C buses 0, 3, 4 confirmed (REM-8x), 1-2 from doc |
| 0x7f94 | `PIN_MUX_2` (IO_MUX_SEL_2) | bits 3:0 ACL pins on GPIO52/53/54/30; clear = GPIO | confirmed |

## I2C master

One master serves all buses; the bus is chosen per transfer. Used for SFP
EEPROMs (`sfp_read_block()`) and on REM-8x for the second chip (`peer.c`).

| Address | Name | Fields | |
|---|---|---|---|
| 0x0414 | `REG_I2C_MST_IF_CTRL` | Written 0 at setup | guess |
| 0x0418 | `REG_I2C_CTRL` | bit 0 GO, reads 1 while busy. bit 1 FAIL (NACK). bit 2 WRITE (data from `I2C_OUT`). 9:3 7-bit device address. 12:10 SDA bus 0-4, 14:13 SCL bus 0-3 (bits 15:8 = `machine.*.i2c`, see `I2CBUS()`). 19:16 length − 1 (up to 16 bytes). 21:20 register address width in bytes (1 or 2) | confirmed |
| 0x041c | `REG_I2C_CTRL2` | 1:0 clock. Measured on REM-8x, 1.1 kB: 0 (default) 0.82 s, 1 0.48 s, 2 0.28 s, 3 faster but corrupts bits | confirmed |
| 0x0420 | `REG_I2C_IN` | The register address sent to the device (despite the name). Two-byte width: MSB first | confirmed |
| 0x0424 … 0x0430 | `REG_I2C_OUT` | 16-byte data buffer for both directions, LSB first in each word: byte i at word `I2C_OUT + (i & ~3)`, bits 8(i & 3)+7 : 8(i & 3) | confirmed |

`I2C_SCL_SHIFT`/`SDA_SHIFT`/`MASK` are not registers. They encode the bus byte
that is written into `I2C_CTRL` bits 15:8.

## SMI (PHY polling)

Set once in `init_smi()`.

| Address | Name | Fields | |
|---|---|---|---|
| 0x6330 | `REG_SMI_MAC_TYPE` | 2 bits per port: 01 PHY polled over SMI, 00 none (SFP) | confirmed |
| 0x6334 | `REG_SMI_PORT_POLLING` | bit n: the MAC polls port n's PHY for link | confirmed |
| 0x644c | `REG_SMI_PORT0_5_ADDR` | MDIO address per port, 5 bits each (port n at 5n+4:5n) | values trace, layout guess |
| 0x6450 | `REG_SMI_PORT6_9_ADDR` | Ports 6-9, same layout | as above |
| 0x6454 | `REG_SMI_CTRL` | 14:12 MDC enable for SMI buses 0-2 | confirmed |

## SerDes

| Address | Name | Fields | |
|---|---|---|---|
| 0x7b20 | `REG_SDS_MODES` | 4:0 SerDes 0 mode, 9:5 SerDes 1 mode, 15:10 SerDes 2 mode (RTL8373, to the RTL8224). Written by `sds_config_mac()` | SDS0/1 confirmed |
| 0x03f8 | `SDS_INDACS_CMD` | Indirect SerDes access **of the RTL8224**, reached over MDIO (MMD30), not used on the SoC: 0 sds, 6:1 page, 11:7 reg, 14 write, 15 execute/busy | confirmed |
| 0x0400 | `SDS_INDACS_WRITE_DATA` | 16-bit data for that write | confirmed |

SerDes modes (`SDS_*`): 0x02 SGMII, 0x04 1000BASE-X, 0x05 100BASE-FX (untested),
0x0d QXGMII (also 10G to the RTL8224/RTL8261), 0x12 HISGMII (2.5G to an
RTL8221), 0x16 HSG (2.5G SFP), 0x1a 10GBASE-R, 0x1f off.

After setting the mode, `sds_config()` writes a page for the speed (0x24 SGMII,
0x26 100FX, 0x28 2.5G, 0x2e 10G) and fixed values in pages 0x06, 0x07, 0x1f,
0x20, 0x21 and 0x36 through the SFR path. Board files can add overrides
(`.sfp_port[].tune`), and so can the `sdstune` command.

## PHY configuration

| Address | Name | Fields | |
|---|---|---|---|
| 0x0a90 | `CFG_PHY_MDI_REVERSE` | 3:0 set to 0xc. Probably MDI pair order. Also written on the RTL8224 over MDIO | trace |
| 0x0a94 | `CFG_PHY_TX_POLARITY_SWAP` | 15:0 = 0x596a on "N" parts. Probably TX polarity per lane | comment |

## Link and port status

| Address | Name | Fields | |
|---|---|---|---|
| 0x63e8 | `REG_LINKS_STS` | Link up, bit n = port n (0-9, 9 = CPU), in 9:0 and again in 25:16. The code reads ports 0-7 at 16 + p and ports 8/9 at bits 8/9; both copies agree. Example: 0x030f030f = ports 0-3, 8 and CPU | confirmed (RTL8373) |
| 0x63f0 | `REG_LINKS` | Speed, one nibble per port 0-7 (port n at 4n+3:4n): 0 10M, 1 100M, 2 1G, 3 500M (only a label), 4 10G, 5 2.5G, 6 5G. Valid only with link | confirmed |
| 0x63f4 | `REG_LINKS_89` | Same for port 8 (3:0) and 9 (7:4). The CPU port reads 2 (1G): 0x24 = port 8 at 10G, CPU at 1G | confirmed |
| 0x63fc | `MAC_TX_PAUSE_STS` | bit n: port n sends pause | comment |
| 0x6400 | `MAC_RX_PAUSE_STS` | bit n: port n honours pause | comment |
| 0x6404 | `MAC_EEE_ABLTY` | Unused; probably a per-port bitmap like its neighbours | guess |
| 0x6408 | `MAC_MSTR_SLV_STS` | bit n: 1000BASE-T master | comment |
| 0x6344 | `REG_MAC_FORCE_MODE` | + 4 × port. bits 1:0 force link (set for the CPU port, 0x6368); default 0x194, other bits probably speed/duplex/pause | comment |

## Interrupts

| Address | Name | Fields | |
|---|---|---|---|
| 0x5f34 | `IMR_INT_PORT_LINK_STS_CHG` | Link change interrupt mask, bit n = port n (set to 0x3ff) | confirmed |
| 0x5f84 | `ISR_SW_INT_MODE` | Written 0x42, meaning unknown (probably routes the interrupt to EX0) | trace |
| 0x5f88 | `ISR_INT_PORT_LINK_CHG` | Link changed, bit n = port n, write 1 to clear. Read and cleared in `idle()`, then `check_links()` reads the links | confirmed |
| 0x7854 | `NIC_INT_STS` | bit 1 RX packet (`NIC_INT_RXIS`), write 1 to clear | confirmed |
| 0x7858 | `NIC_INT_MSK` | bit 1 RX interrupt enable, to EX1 | confirmed |

## NIC (CPU port DMA)

Frames move between the CPU and an 8-byte-slot ring through SFRs 0xb3-0xb7, see
[CpuPort.md](CpuPort.md).

| Address | Name | Fields | |
|---|---|---|---|
| 0x6720 | `REG_CPU_TAG` | bit 0 insert the CPU tag on frames to the CPU; 9:8 tag mode, 0 = all frames | comment |
| 0x603c | `REG_CPU_TAG_AWARE_PMASK` | Ports that understand CPU tags: 0x200 = port 9 | trace |
| 0x7844 | `REG_NIC_BUFFSIZE_TX` | 0x7fe, probably the last TX slot | trace |
| 0x7848 | `REG_NIC_RXBUFF_RX` | Last RX slot, inclusive: 0x4ff = 0x2800-byte ring | confirmed |
| 0x784c | `REG_NIC_RXCMD` | Write 1: release the current RX frame | confirmed |
| 0x7850 | `REG_NIC_TXCMD` | Write 1: send the frame just copied | confirmed |
| 0x785c | `REG_RX_CTRL` | bit 0 RX enable, bit 2 accept bad CRC (cleared). 31:24 = 0x04, 17:16 cleared, unknown | bits 0, 2 confirmed |
| 0x7860 | `REG_TX_CTRL` | bit 0 TX enable | confirmed |
| 0x7874 | `REG_NIC_RX_BUFF_DATA` | Bytes waiting in the RX ring (15:0) | confirmed |
| 0x787c | `REG_CPU_RX_CURR_PKT` | Slot of the current RX frame (× 8 = byte offset) | confirmed |
| 0x7880 | — | Not used by this firmware. The stock managed firmware busy-waits on it until a frame arrives. Probably the RX write slot, where the hardware puts the next frame (the counterpart of 0x787c) | busy-wait trace; meaning guess |
| 0x7884 | `REG_NIC_TX_CURR_PKT` | Read after TX, value unused | comment |
| 0x7890 | `REG_CPU_TX_CURR_PKT` | TX write slot | confirmed |

RX descriptor (8 bytes before each frame): byte 0 sequence, byte 1 flags
(bit 2 `RX_TAG_L4_CSUM_BAD`), byte 3 bits 3:0 source port, bytes 4-5 length
(little endian). TX descriptor: byte 0 sequence, byte 1 = 0x07 (checksum
offloads), bytes 4-5 length.

Frames to the CPU normally carry an 802.1Q tag after the RTL tag. Trapped IGMP
is the exception: it keeps its ingress tagging (see [IGMP](#igmp--multicast)).

To confirm 0x7880: with RX idle, read 0x787c, 0x7880 and 0x7874. If 0x7880 is
the write slot, it equals 0x787c when the ring is empty. With frames waiting,
(0x7880 − 0x787c) × 8 should match the byte count in 0x7874, modulo the ring
size.

## MIB counters

| Address | Name | Fields | |
|---|---|---|---|
| 0x0f60 | `STAT_GET` | Command: bit 0 execute/busy, 4:1 port, from bit 5 counter index (`STAT_GET(cnt, port)` macro) | confirmed |
| 0x0f64 | `STAT_V_HIGH` | Bits 63:32 of the selected counter slot | confirmed |
| 0x0f68 | `STAT_V_LOW` | Bits 31:0 | confirmed |

There are 0x37 slots of 64 bits. A slot is one 64-bit counter or two 32-bit
counters (first in the high half). Names are the `MIB` list in
`html/app.js`; the firmware names 46 TX packets, 47 RX packets, 48 RX/TX
errors (`STAT_COUNTER_*`).

## Table access (VLAN and L2)

Like the RTL8366/RTL8367: fill the input words, write the command, poll until the
execute bit clears, read the output words.

| Address | Name | Fields | |
|---|---|---|---|
| 0x5cac | `TBL_CTRL` | 27:16 entry index or VLAN ID. 15:8 table: 3 VLAN (`TBL_VLAN`), 4 the whole L2 table incl. multicast (`TBL_L2_UNICAST`). bit 1 write, bit 0 execute/busy | confirmed |
| 0x5cb0 | `TBL_DATA_0` | 11:0 index found. 12 hit. 13 miss: set by a method-0 lookup that misses, cleared by a hit or a write. 16:14 L2 read method (`TBL_LUTREAD_*`): 0 by key; 1 and 2 index reads that also return IPv4 multicast (L3MC) entries; 3 next entry, skipping L3MC; 4-7 unused. 18 delete the entry (with write): not cleared by the chip, stays set until written to 0 | confirmed (multicast parts on RTL8372 too) |
| 0x5cb8 / 0x5cbc / 0x5cc0 | `TBL_DATA_IN_A/B/C` | Entry to write or key to look up | confirmed |
| 0x5ccc / 0x5cd0 / 0x5cd4 | `L2_DATA_OUT_A/B/C` | Entry read; a VLAN read also lands in OUT_A. On a lookup miss, A/B hold the key and C reads 0 | confirmed |

**VLAN entry** (one word): 9:0 member ports (bit 9 CPU), 19:10 untagged ports,
25 valid. VLAN 1 at boot: 0x0207ffff (RTL8373), 0x0207e3f8 (RTL8372).

**L2 unicast entry**:
- A: MAC bytes 2-5.
- B: 31:30 port bits 1:0, 29 valid, 27:16 VLAN, 15:0 MAC bytes 0-1.
- C: 1:0 port bits 3:2, 4:2 age (0 hides the entry, static entries use 7),
  5 authenticated, 16 static.

**L2 multicast entry**: as unicast, but the port field is a port mask
(B 31:30 bits 1:0, C 7:0 bits 9:2). C 15:8 IGMP index, C 16 IGMP-managed.

**IPv4 multicast entry** (used by IGMP snooping): A source IP, B 27:0 group
address (224/4 implied), B bit 28 apparently the L3 flag, B 31:30 and C as
L2 multicast.

Sequences, all ending with a poll of `TBL_CTRL` bit 0:
- VLAN write: `TBL_DATA_IN_A` = entry, `TBL_CTRL` = vid << 16 | 3 << 8 | write | exec.
- VLAN read: `TBL_CTRL` = vid << 16 | 3 << 8 | exec, then read `L2_DATA_OUT_A`.
- L2 walk: method 3 in `TBL_DATA_0` with bit 18 cleared, `TBL_CTRL` = idx << 16 |
  4 << 8 | exec, read OUT_A/B/C, next index from `TBL_DATA_0` 11:0 + 1, stop when
  it wraps. A bit 18 left by a delete makes the walk return nothing (upstream #908).
- L2 lookup/insert by key: key in IN_A/B/C, method 0, `TBL_CTRL` = 4 << 8 | exec
  (lookup: hit in `TBL_DATA_0` bit 12, miss in bit 13) or | write (insert, the
  chip picks the slot).
- L2 delete: goes by the index in `TBL_CTRL` 27:16, not by the key; a write with
  bit 18 at index 0 leaves the entry. Look up, then set `TBL_DATA_0` bit 18 and
  write at the index found. The delete leaves `TBL_DATA_0` and `L2_DATA_OUT` as
  they were, so clear bit 18 afterwards.

Host tests: `test/hw_mock.h` describes the layouts independently.

## L2 learning and flushing

| Address | Name | Fields | |
|---|---|---|---|
| 0x5350 | `L2_CTRL` | bit 3 IP multicast lookups by IP instead of MAC (`L2_CTRL_LUT_IPMC_HASH`, a bit number) | comment |
| 0x5384 + 4 × port | `L2_LRN_PORT_CONSTRAINT` | Max learned addresses per port; set to 0x1040 = unlimited | comment |
| 0x53ac + 4 × port | `L2_LRN_PORT_CONSTRT_CNT` | Learned addresses on the port, 12:0 | confirmed |
| 0x4f80 | `L2_LRN_PORT_CONSTRT_ACT` | Action when the limit is reached; only bit 0 is set ("flood"). Probably 2 bits per port | guess |
| 0x53d4 | `L2_TBL_FLUSH_CTRL` | 9:0 ports to flush, bit 16 execute (`L2_TBL_FLUSH_EXEC`, a mask), self-clearing | confirmed |
| 0x53dc | `L2_TBL_FLUSH_CNF` | 0 = by port, dynamic entries only; 1:0 mode, bit 2 dynamic/static | value confirmed |

## VLAN

| Address | Name | Fields | |
|---|---|---|---|
| 0x4e10 | `REG_INGRESS` | 2 bits per port, accepted frames: 00 all, 01 tagged only, 10 untagged only | confirmed |
| 0x4e14 | `VLAN_CTRL` | bit 2 VLAN filtering (`VLAN_CVLAN_FILTER`, a mask); written whole | confirmed |
| 0x4e18 | `VLAN_PORT_IGR_FLTR` | bit n: drop frames of VLANs that port n is not a member of | confirmed |
| 0x4e1c + 4 × (port / 2) | `PVID_BASE_REG` | PVID, two ports per register: even port 11:0, odd port 23:12 | confirmed |
| 0x4e30 / 0x4e34 | `VLAN_L2_LRN_DIS_0/1` | Written 0; probably per-VLAN learning disable | guess |
| 0x6738 | `VLAN_PORT_EGR_TAG` | 2 bits per port egress tag mode; 00 = follow the VLAN's untagged set | 00 confirmed |

## Mirroring

| Address | Name | Fields | |
|---|---|---|---|
| 0x6048 | `MIRROR_CTRL` | bit 0 enable, 4:1 monitor port | confirmed |
| 0x604c | `MIRROR_CONF` | 31:16 ports whose RX is mirrored, 15:0 TX | confirmed |

## Link aggregation

| Address | Name | Fields | |
|---|---|---|---|
| 0x4f38 + 4 × group | `TRK_MBR_CTRL_BASE` | Groups 0-3, 9:0 member ports | confirmed |
| 0x4f48 + 4 × group | `TRK_HASH_CTRL_BASE` | Hash inputs: 0 source port, 1 SMAC, 2 DMAC, 3 SIP, 4 DIP, 5 L4 sport, 6 L4 dport | confirmed |

The power-on hash is 0x3f (`LAG_HASH_RESET`, measured). The firmware replaces a
hash of 0x3f or 0 with 0x7e (`LAG_HASH_DEFAULT`) when it sets a group's
members, so a deliberately chosen 0x3f does not survive.

## Port isolation

| Address | Name | Fields | |
|---|---|---|---|
| 0x50c0 + 4 × port | `PORT_ISOLATION_BASE` | 9:0 ports frames from this port may leave through; bit 9 CPU | confirmed |

## IGMP / multicast

See [igmp.md](igmp.md).

| Address | Name | Fields | |
|---|---|---|---|
| 0x4f78 / 0x4f7c | `IPV4_` / `IPV6_PORT_MC_LM_ACT` | Unknown IP multicast, 2 bits per port: 00 flood, 01 drop, 10 trap, 11 router port | comment |
| 0x5368 / 0x536c | `IPV4_` / `IPV6_UNKN_MC_FLD_PMSK` | Flood mask for unknown IP multicast | comment |
| 0x529c | `IGMP_ROUTER_PORT` | 15:0 static router ports; 31:16 probably ports allowed to become router ports. Not used | guess |
| 0x52a0 + 4 × port | `IGMP_PORT_CFG` | 2 bits per protocol, 00 ASIC, 01 flood, 10 trap, 11 drop: 1:0 IGMPv1, 3:2 v2, 5:4 v3, 7:6 MLDv1, 9:8 MLDv2. 10 dynamic router port, 11 MRP, 12 fast leave, 13 allow report, 14 allow query. 23:16 max groups | comment |
| 0x50bc | `IGMP_TRAP_CFG` | 2:0 priority of trapped IGMP/MLD, bit 16 to the CPU port | comment |

A trapped IGMP frame reaches the CPU in its ingress tagging: an untagged report
arrives with no 802.1Q tag, a VID 1 or priority-tagged one keeps its tag. Other
frames to the CPU arrive tagged. Measured on RTL8372 and RTL8373.

## STP and reserved multicast (RMA) trapping

See [stp.md](stp.md).

| Address | Name | Fields | |
|---|---|---|---|
| 0x5310 | `MSTP_STATES` | 2 bits per port (port p at 2p+1:2p): 00 disabled, 01 blocking, 10 learning, 11 forwarding. CPU port 19:18 kept at 11. One instance only | confirmed |
| 0x6724 | `EXT_CPU_CTRL` | Port trapped frames go to: 9 (CPU); the reset value 0xf drops them | confirmed |
| 0x4ecc | `RMA0_CONF` | 01:80:c2:00:00:00 (BPDU). 5:4 action: 00 forward, 01 trap, 10 drop (`RMA_ACT_*`). BPDUs reach the CPU even on a blocked port | confirmed |
| 0x4ed4 | `RMA2_CONF` | 01:80:c2:00:00:02 (LACP), same action field. Not exempt from blocking | confirmed |
| 0x4ee8 | `RMA0E_CONF` | 01:80:c2:00:00:0e (LLDP), same action field | confirmed |
| 0x4f1c | `RMA_CONF` | Same table, entry 20; which address it covers is unknown | guess |
| 0x1074 | `RTL8373_RLDP_TIMER` | Loop detection timer, the same ms value in both halves. Only in unused code | guess |

The RMA entries are 4 bytes apart but not one per address: 0x0e is entry 7,
which fits an RTL8367-style list (00, 01, 02, 03, 04, 08, 0d, 0e, 10, …).

## Bandwidth, storm control and meters

See [bandwidth.md](bandwidth.md) and [storm_control.md](storm_control.md).

| Address | Name | Fields | |
|---|---|---|---|
| 0x4c10 | `IGBW_CTRL` | Global ingress limit settings. Exempt from the limit: 0 IGMP, 1 RTK frames, 2 BPDU, 3 RMA, 4 ARP request, 5 DHCP. 7 count the IFG, 8 count bypassed frames | comment |
| 0x4c18 + 4 × port | `IGBW_PORT_CTRL` | 20 enable, 19:0 rate in 16 kbit/s units. Off = 0x0fffff | confirmed (measured) |
| 0x4c8c | `IGBW_PORT_FC_CTRL` | bit n: send pause instead of dropping | confirmed |
| 0x1c34 + 0x400 × port | `EGBW_PORT_CTRL` | Egress limit, same encoding | confirmed in use |
| 0x447c | `EGBW_CTRL` | 0 CPU mode (frames from the CPU), 1 count the IFG | comment |
| 0x54e4 + 4 × type | `STORM_CTRL` | Types 0 broadcast, 1 multicast, 2 unknown unicast, 3 unknown multicast; bit n enables port n | confirmed |
| 0x54f4 + 8 × type + 4 × (port ≥ 5) | `STORM_MIDX` | Meter index of each port and type, five 6-bit fields per word | confirmed |
| 0x5cf0 + 4 × meter | `METER_RATE` | 64 meters. 23:0 rate, kbit/s or packets/s | confirmed |
| 0x5df0 + 4 × meter | `METER_BURST` | Burst; default 0x2000, set to the rate in pps mode | confirmed for pps |
| 0x5ef0 + 4 × (meter / 32) | `METER_MODE` | bit (meter mod 32): 1 pps, 0 kbit/s | confirmed |
| 0x5f08 + 4 × (meter / 32) | (no name) | Include preamble and IFG per meter; left 0. Documented in storm_control.md only | comment |

Storm control uses meter `24 + 4 × port + type` (`STORM_METER()`): 24-59 for
ports 0-8 (36-59 on the RTL8372). Meters 0-23 and the CPU port's 60-63 are
unused, and on REM-8x `peer.c` uses `METER_RATE` 0-23 as an I2C mailbox
between the two chips. Any future ACL policer support would have to keep clear
of them.

## EEE and MTU

| Address | Name | Fields | |
|---|---|---|---|
| 0x125c + (port << 8) | `EEE_CTRL_BASE` | 0 RX LPI, 1 TX LPI | comment |
| 0x642c | `RTL8373_PHY_EEE_ABLTY` | Shown as "EEE active" per port; only ports 0-7 are read | guess |
| 0x1250 + (port << 8) | `RTL8373_REG_MAC_L2_PORT_MAX_LEN` | 13:0 max frame length (MTU, 64-16383). The `mtu` command writes the same value to 27:14 too | 13:0 confirmed |

## Random numbers

| Address | Name | Fields | |
|---|---|---|---|
| 0x106c | `RLDP_RLPP` | Set bit 3 (`RLDP_RND_EN`) before each read for a new number | comment |
| 0x107c | `RAND_NUM0` | 32 random bits (DHCP xid, session ids) | comment |
| 0x1080 | `RAND_NUM1` | 15:0 16 more | comment |

## Registers known only by address

Copied from stock firmware traces, used without a name:

| Address | Use | |
|---|---|---|
| 0x02f4 / 0x02f8 | The stock firmware writes the new / old value of each RTL8224 MMD30 register here before writing it over MDIO; copied in `rtl837x_init.c` and `rtl837x_phy.c` | trace |
| 0x0b7c | bit 5 set at init | trace |
| 0x5fd4 | bits 19, 20 set at init (interrupt block) | trace |
| 0x632c | 20:12 apparently MAC access enable per port (0x001f8540 RTL8372, 0x001ff540 RTL8373) | trace |
| 0x7124 … 0x7148 | Ten registers, one per port, set to 0x1050 by the RTL8373 init; the RTL8372 stock firmware writes them too, this firmware does not | trace |

## Known problems

Found while writing this, not fixed:

- `rtl837x_leds.c` builds `LED_RLDP_2` with `sfr_data[3 - (i >> 2)]`, which should
  be `i >> 1` (two ports per byte).
- `rtl837x_leds_dump.c` does not mask the per-port set with `& 3`.
- `rtl8373_revision()` prints `sfr_data[2]` (the unlock key) twice instead of the
  revision; the comment on the second write says "Enable" where it disables.
- `sfp_read_block()` waits for the I2C GO bit without a timeout.
- `igmp_packet_handler()` (`struct igmp_pkt`) expects EtherType 0x0800 right
  after the RTL tag, which only holds for untagged reports. A VID 1 or
  priority-tagged report has 0x8100 there, is passed to the IP stack, and joins
  no group. Skipping an 802.1Q tag would also give the VID for the hard-coded
  `entry.vlan = 1`.
- `TBL_DATA_0`: the L2 walk in `page_impl.c` and `l2_delete()` clear only bits
  17:16, so bit 18 left by a delete is still set at the next walk and `/l2.json`
  comes back empty (upstream #908). `port_l2mc_set()` does not clear it either.
  `l2_delete()` also writes `TBL_L2_UNICAST` (a table type) into the method byte.
- `L2_LRN_PORT_CONSTRT_ACT`: only bit 0 is set, probably port 0's action only.
- `LAG_HASH_RESET`: a hash chosen as 0x3f is replaced by 0x7e on the next member
  change.
- `port_rldp_on()` (unused) would write `RMA0_CONF = 0` and break STP.
- `MAX_LEN`, `PHY_EEE_ABLTY` and `RLDP_TIMER` carry an RTL8373 prefix but apply
  to both chips. `TBL_L2_UNICAST` covers multicast too. `I2C_IN` holds the
  register address. Some defines are bit numbers (`L2_CTRL_LUT_IPMC_HASH`), others
  masks (`VLAN_CVLAN_FILTER`, `L2_TBL_FLUSH_EXEC`).
- Unused names: `REG_SEC_COUNTER2`, `MAC_EEE_ABLTY`, `RESET_SOC_BIT`,
  `I2C_DEV_ADDR`. Raw addresses where a name exists: 0x65d8 (`LED_GLB_ACTIVE`),
  0x3f8 (`SDS_INDACS_CMD`), literal I2C GO/FAIL bits in `sfp_read_block()`.
- Docs: [vlan.md](vlan.md) gives the VLAN table type as 2 (it is 3);
  [l2.md](l2.md) calls entry bit B29 "IVL" (it is the valid bit);
  [hardware.md](hardware.md) says the RX interrupt is not used (it is).
