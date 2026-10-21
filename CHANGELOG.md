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

- LLDP
  - Learn one neighbour per port from received LLDPDUs (chassis, port, names, description, capabilities,
    management IPv4, Port VLAN ID), aged by their TTL and removed on link down; shown by `lldp` and on the LLDP page.
  - Optional TLVs Port description, System capabilities, Management address, Port VLAN ID, MAC/PHY status and
    Maximum frame size, switched with `lldp tlv <name> on|off`. See `doc/lldp.md`.
  - The LLDP page shows what each port sends and the neighbour details, and works as cards on a phone.

## Changed

- `/lldp.json` has a new shape and `/lldp.json?port=<n>` the details of one port, see `doc/lldp.md`.

## Fixed

- LLDP
  - `lldp` without an argument showed nothing and switched LLDP off; it now shows the LLDP state.
  - Save to flash dropped `port <n> lldp block` when a speed command for the port followed, and the reverse.
- Config
  - A startup configuration line longer than the command buffer no longer stops the replay:
    the offending line is skipped and the following lines are still applied.
  - The web UI and the configuration upload now refuse a line the replay cannot take,
    instead of writing it to flash.

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