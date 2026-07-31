# PoE driver — RTL8238B PSE controllers

The RTL8238B PSE driver ([poe_rtl8238b.c](../poe_rtl8238b.c)). For the generic, chip-agnostic PoE
framework — the machine descriptor, the `POE_CHIP_*` / `POE_PRESENT` gating, the driver interface
and the console / web / `/poe.json` consumers — see **[poe.md](poe.md)**. This document covers
only what is specific to the **RTL8238B**.

Used on switches built around a Realtek **RTL8373** SoC driving one or more **RTL8238B** octal PSE
controllers. Verified on the keepLink KP-9000-9XHPML-X V3.1 board — see
[devices/KP-9000-9XHPML-X-V3.1.md](devices/KP-9000-9XHPML-X-V3.1.md).

> **Firmware-image notice.** The RTL8238B runs a *volatile* MCU firmware image that the driver
> uploads at every boot. That image is Realtek/OEM proprietary and is **not** included in this
> repository — you extract it from your own device's OEM firmware (see
> [Obtaining the PSE firmware image](#obtaining-the-pse-firmware-image)).

## Register-based, not host-command

Other open PoE projects for MCU-fronted Realtek/Broadcom PSEs talk to the controller with a
**12-byte host-command** protocol over I2C/UART (an opcode + parameters + checksum, answered by a
companion management MCU):

- poemgr — <https://github.com/blocktrron/poemgr>
- realtek-poe — <https://github.com/Hurricos/realtek-poe>
- OpenWrt PSE driver (`realtek-pse-*`) — drivers/net/pse-pd

This driver does **not** use that protocol. On this board the RTL8238B's host-command application
does not answer — the chip responds only as a raw I2C **register slave**. So RTLPlayground drives
PoE entirely by **reading and writing the controller's registers** directly, which is exactly
what the OEM's own web UI does for status/telemetry. The only thing the volatile firmware image is
needed for is the analog PoE state machine (detection / classification / power-up); once it is
uploaded, *control and telemetry are pure register I/O*. This makes the approach useful for boards
where the host-command MCU layer is unreachable.

## Hardware / firmware model

- The PSE sits on **I2C bus 0** (`GPIO46 = SCL0`, `GPIO47 = SDA0`). For the RTL8238B the 7-bit
  address selects the port bank with A0: **`0x20` = ports 1–4, `0x21` = ports 5–8**. These are
  the `addr0` / `addr1` in the board's `.poe` descriptor (see [poe.md](poe.md)).
- The RTL8238B has an integrated MCU with only a mask boot ROM; its PoE application firmware is
  **volatile**. On a board strapped for host-download, the host (the RTL8373 running this
  firmware) must upload the image over I2C at every power-on. Until then the I2C slave exposes
  only a raw register file and no port is powered.

## Register map

The RTL8238B register file is proprietary and undocumented; the layout below is what RTLPlayground
established empirically — by probing the chip and cross-checking against the reads the OEM web UI
makes — and is the entire register surface the driver relies on. Access is plain I2C: one address
byte, then the data bytes; multi-byte values are **little-endian** (the first byte on the wire is
the least-significant). Each controller drives four ports as channels `local = 0..3`. On this board
the front-panel-port ↔ channel wiring is **`0x20` → ports 1–4 = channels 3, 2, 1, 0** and **`0x21`
→ ports 5–8 = channels 0, 1, 2, 3** — i.e. on the first controller the channel order is reversed,
so front-panel port 1 is channel 3 of `0x20`.

**Control & download**

| Register | Access | Meaning |
|----------|--------|---------|
| `0x1a` | write `0x20` | whole-chip reset — drops the chip back into its boot ROM |
| `0x10` block (4 B) | RMW | per-port state/mode block; **byte 0** is live status (see below), **byte 2** (= `reg 0x12`) is the admin mode you write |
| `0x12` | 2 bits per port at `local*2` | per-port **admin mode**: `3` = on (auto-detect), `0` = off |
| `0x14` block (4 B) | RMW | per-port **enable**: 1 bit per port at `local` in byte 0; written together with the `0x12` mode |
| `0xec` | write 4 B, then read | **download arm + status**: arm with payload `03 00 00 00`; on read-back bit 2 = armed, **bit 5 = image accepted** |
| `0x18` | write 4 B | **alternate arm** (payload `00 00 20 39`), sent only if `0xec` alone doesn't set the armed bit |
| `0xf4` | stream | **firmware-download data port** — the whole image is clocked in here in one transaction |
| `0x0b` / `0x0c` | read | post-download **result mailbox** (16-bit verify result, low / high byte) |

**Status & telemetry** (meaningful once the uploaded app is running)

| Register | Access | Decode |
|----------|--------|--------|
| `0x10` byte 0 | read | **Power-On** status — bit `4 + local` is set once the chip has powered a detected PD on that port |
| `0x0c + local` | read | **PD class** — high nibble: `0xc` = Class 0, `0x1`–`0x8` = Class 1–8 |
| `0x30 + 4*local` (4 B) | read | per-port **voltage & current**: volts = byte 3 × 0.5 V; mA = (low 16 bits) × 125 / 4096 |
| `0x1b`, `0xe8` | read | chip identity / state markers (`0x39` / `0x02` before a download) — used as pre/post-upload sanity checks |

Two addresses are state-dependent: `0x0c` is a result-mailbox byte right after a download (the
verify phase) but the per-port class byte once the app is running; and the `0x10` block carries
**both** live Power-On status (byte 0) and the admin mode you write (byte 2 = `reg 0x12`).

## Bring-up sequence

`poe_bringup()` runs the full sequence (at boot after PHY init, and on demand via `poe load`):

| # | Step | What it does |
|---|------|--------------|
| 0 | **Reset** | Whole-chip reset (`reg 0x1a = 0x20`) on each controller. |
| 1 | **Disable** | Clear the port registers (`reg 0x10 = 0`) on `0x20`/`0x21`. |
| 2 | **Per-port pre-load config** | RMW that clears the per-port mode field (`reg 0x10` block) and a per-port bit (`reg 0x14`) on each controller. |
| 3 | **Arm download mode** | Write the arm payload to `reg 0xec`; if the arm bit doesn't set, send the alternate arm (`reg 0x18`) and re-arm. |
| 4 | **Stream the image** | One continuous bit-banged I2C transaction `START · (addr0<<1) · 0xf4 · <entire image> · STOP`, MSB-first, ACK-clocked. (Bit-banged on GPIO46/47 because the image far exceeds the SoC HW I2C engine's 16-byte/transfer limit.) The chip accepts the image (`reg 0xec` bit 5). |
| 5 | **Per-port enable** | A second per-port pass that **sets** the mode/enable fields (`reg 0x10`/`0x14`) — arms each port for auto-detect. |

Interrupts (`EA`) are cleared across the prep + image stream: the on-chip command parser needs
the bytes within a tight timing window, and an ISR stretching an inter-byte gap can make it miss
the arm even though every byte is ACKed. The image is read from flash at `PSE_IMG_ADDR` (injected
by the Makefile, see [poe.md](poe.md)) and the driver takes its length from the image's own header.

## Per-port enable and auto-detect

Enabling a port writes the OEM's own admin path — the 2-bit mode field in the `reg 0x10` block
(byte 2 = `reg 0x12`, value `3`=on / `0`=off) plus the enable bit in `reg 0x14`. Verified
on-device, this is **real hardware auto-detect** (no running MCU app): with mode `3` the chip runs
detect → classify → power-up itself and sets the `reg 0x10` Power-On status bit (bit `4+local`)
only once a genuine PD is present. An enabled-but-empty port reads Power-On `0` / class `0` / 0 V;
on insertion the chip flips Power-On to `1`, fills the class and ramps the current. On PD
**removal** the chip leaves Power-On / class / voltage *stale* while the live current drops to 0 —
which is why the driver reports a port "delivering" only when current is actually flowing.
Disabling a port (`poe port <n> off`) cleanly de-energizes it and clears the latches.

## Telemetry — register decode

`poe_get_port()` decodes the controller's registers into the normalized, chip-agnostic
`poe_st_*` fields that [poe.md](poe.md)'s interface defines — so no consumer needs to know this
layout. The decode (little-endian; `local` = 0–3 channel within a controller):

| Normalized field | Decoded from |
|------------------|--------------|
| `admin` (enabled) | `reg 0x10` block byte 2 (`reg 0x12`) per-port mode field ≠ 0 |
| `on` (delivering) | `reg 0x10` Power-On bit `(4 + local)` **and** real current draw |
| `class` (0–8) | high nibble of `reg 0x0c + local` (nibble `0x0c` = Class 0, `1`–`8` = Class 1–8) |
| `volt` | top byte of the 32-bit `reg 0x30 + 4·local`, 0.5 V/LSB → `(raw>>24)/2` |
| `ma` | low 16 bits of the same register → `(raw & 0xffff)·125/4096` |

Verified against hardware (a Class 4 PD read 51 V / 153 mA). The Power-On bit + class + voltage
latch stale after a PD is unplugged, so the driver reports `on` only when current is actually
flowing (and otherwise hides the measurements).

## Obtaining the PSE firmware image

The image is **not** in this repo (proprietary). You supply your own, extracted from your device's
OEM firmware; the Makefile embeds the single file `tools/poe/pse_image.bin`, and the firmware just
uploads whatever that file contains, taking the length from the image's own header. An OEM dump can
bundle PSE firmware for **more than one chip**: the KP-9000 dump carries a ~21 KB image for a
*different* PSE chip alongside the ~8 KB RTL8238B image — the RTL8238B one (the ~8 KB image) is what
this board uses.

1. **Get your device's OEM firmware** — from the vendor, or by dumping the SPI-NOR flash with a
   SOIC-8 clip + CH341A-style programmer **before** flashing RTLPlayground (afterwards the OEM
   image is gone unless you kept a backup), or from a compatible rebrand's published firmware.
2. **Extract** with [`tools/poe/extract_rtl8238b_image.py`](../tools/poe/extract_rtl8238b_image.py):
   ```sh
   python tools/poe/extract_rtl8238b_image.py oem_firmware.bin
   ```
   It scans for the 16-byte header whose **magic word is `0x8239`** (bytes `39 82`), validates
   `image_len == 16 + Σ section_len[0..3] + 4`, carves each container, and writes the one the
   board uploads straight to `tools/poe/pse_image.bin`. (The KP-9000 dump holds two PSE
   images — 21164 B and 8028 B; the 8028 B one is this board's RTL8238B firmware, the 21164 B one is
   for a *different* chip. The extractor lists each with its size and auto-writes the 8028 B one; the
   firmware takes the length from the image header. On a different board whose dump holds several,
   confirm which image to keep from the **OEM firmware's boot log** — it reports the PSE image it
   uploads at startup — and save that as `tools/poe/pse_image.bin`.)

The Makefile embeds it via two RTL8238B-specific knobs: **`RTL8238B_MACHINES`** (the machines that
get the image — your board must be listed) and **`RTL8238B_IMAGE_LOCATION`** (the flash offset,
which **must equal** `PSE_IMG_ADDR` in [poe_rtl8238b.c](../poe_rtl8238b.c) — both `0x60000`). A
board with a different PoE chip uses neither.

Then build for your PoE machine (see [poe.md](poe.md#building--flashing)) and **cold power-cycle**.
Connect a PD; it powers up a few seconds after boot.

## Troubleshooting

- **`poe load` reports REJECTED / no power:** confirm a valid PSE image is embedded — the bring-up
  prints its download-status verdict and the image-container check — and that you **cold
  power-cycled**: the chip's loaded state survives a warm reboot, so always retest from a physical
  power-cut.
- **`poe load`'s post-enable register dump shows reg 0x10/0x14 unchanged:** the prep writes aren't
  landing — check the I2C bus 0 wiring / pull-ups.
- **Accepted but nothing powers:** confirm a real PD is attached; detection takes a few seconds.
