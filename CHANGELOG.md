# Change Log

## [0.x] - 2026-xx-XX

## Added

- Devices
  - Support the hongyavision HYWS-SGT0204S (PCB marking `2G5F4_10G2_V1.01`), an
    RTL8372 based 4x2.5G + 2xSFP+ unmanaged switch with a 512 KB Bright Moon
    `T25S40` flash. It is a sibling of the KP-9000-6XH-X2 (2M-PCB43-V1.1) with
    the two SFP+ cages wired the other way round (physical 5 is on SDS1,
    physical 6 on SDS0), no reset button fitted, wrong silkscreen labels on the
    serial header, `LED_GLB_IO_EN` bit 6 has to be set for the port LEDs to
    work, and the fitted T25S40 cannot be read in DIO mode on this board, so the
    target selects single-IO flash reads (`flash_sio`). New machine target
    `MACHINE_2G5F4_10G2_V1_01`, see `doc/devices/2G5F4_10G2_V1.01.md`.

- Web UI
  - Compress the embedded assets (minify + gzip) and switch to a single-page layout. #315
  - Replace the multi-page UI with a themed single-page app: light, dark and Selenized themes following the browser by default,
    English, Japanese and Chinese, save to flash merges the command log into the startup config and verifies the write,
    firmware images are checked in the browser before upload. #429

## Changed

- Web UI
  - The web session expires after one hour without a request (was 200 seconds). The UI stops
    polling while its tab is hidden, so with 200 seconds switching tabs for a few minutes logged
    you out. `session <seconds>` still sets it.

## Fixed

- Time
  - `rtl8372_init()` sets SDS 0 to SGMII as the stock firmware does. The masks were 0, so on RTL8372
    boards without 10G ports both SerDes stayed off, and with SDS 0 off `RTL837X_REG_SEC_COUNTER`
    does not hold a written value: NTP never retried after a failed first attempt at boot and
    froze the clock after a sync.

- Web UI
  - Firefox no longer fills a saved login into the new admin password fields. It also put the
    user name into the NTP poll interval, which marked the Time card as edited, so it kept its
    defaults (NTP off, no server, UTC-12:00) instead of the switch's settings.
  - Accept firmware uploads regardless of the file part's MIME type, including
    `application/macbinary` sent by Chrome on macOS for `.bin` files.

- Health
  - `HEALTH=1` builds no longer write past the phase timing arrays at the LLDP checkpoint, which
    corrupted the `link` slow counter and the DHCP client state. #658
- Config
  - A startup configuration line longer than the command buffer no longer stops the replay:
    the offending line is skipped and the following lines are still applied.
  - The web UI and the configuration upload now refuse a line the replay cannot take,
    instead of writing it to flash.

- Startup
  - Initialised and zero-initialised globals above xdata 0x4000 now get their values at boot.
    Hardware reset leaves SFR 0x97 at 1, which maps xdata 0x4000-0xbfff to the flash image,
    and it was cleared only in `main()`, after the C startup code had copied and cleared them.
    This showed as a random web session timeout and LLDP blocked on some ports without config.

## Breaking changes

- Config
  - VLAN don't accept port `u`-suffix anymore.
    So `vlan 1 4u` is not valid anymore.
    Replace it with `vlan 1 4`.
- Commands
  - port zero/`0` is treated as the `CPU_PORT`. #326
  - Many commands don't accept the CPU_PORT anymore. See #334.
    When the CPU_PORT is needed, the command/service will add the CPU_PORT automaticly.
    Only `isolate` accept the CPU_PORT as destination port.

## [v0.1_aplha] - 2025-11-03

First release.