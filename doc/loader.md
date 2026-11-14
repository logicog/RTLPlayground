# Bootloader

The loader is optional. Build with `make LOADER=1` and you get two extra files next to the normal image:

- `rtlplayground-boot-<ver>-<machine>.bin`: loader and application in one 512 KiB image, used once to move a switch to the loader.
- `rtlplayground-loader-<ver>-<machine>.bin`: the loader alone, 16 KiB, to update the loader of a switch that already has one.

The normal `rtlplayground-<ver>-<machine>.bin` does not change and is what you upload from then on.

## Why

Without the loader, an update copies the uploaded image over the running one at 0x0. A reset or a power loss during that copy leaves a switch that does not start and needs a programmer. With the loader, 0x0-0x3fff holds only the loader, and the application is installed somewhere else and checked before it runs. An install cut short simply starts again on the next boot.

## Flash layout

| Flash | Content |
|-------|---------|
| 0x00000-0x03fff | loader (home bank, the copy routine at 0x3e00, CRC in the last 2 bytes) |
| 0x04000-0x39fff | application banks below the loader data |
| 0x3a000 | boot counter |
| 0x3b000 | metadata: `RTLB`, application CRC, layout version |
| 0x3c000-0x3fffd | copy of the application home bank |
| 0x40000-0x6ffff | application banks, default configuration at 0x6f000 |
| 0x70000 | saved configuration, never touched by the loader |
| 0x71000-0x74fff | staging area for a new loader |
| 0x80000-0xfffff | upload area |

0x3a000-0x3ffff is the upper part of bank 5 (0x34000-0x3ffff). A normal build leaves it empty, so the image format stays the same and every image installs as it is. An image that uses this area is refused, unless it is a boot image that carries its own metadata.

All addresses are in `loader/layout.h`, `tools/bootpack.py` and the Makefile read them from there. `LDR_LAYOUT` is stored in the metadata, and a boot image built for another layout is refused with "Image needs another loader".

## Boot

1. The chip loads the first 16 KiB of flash into code RAM and starts the loader.
2. The loader checks the upload area. A complete image with a valid CRC is installed: the old metadata is erased first, then the home bank goes to the slot at 0x3c000 and the banks to their places. New metadata is written only after the CRC of the installed copy matches. Then the boot counter and the start of the upload area are erased. If the switch loses power anywhere before that, the next boot repeats the install.
3. The loader adds a mark to the boot counter.
4. It checks the application CRC against the metadata, reads the home bank copy into XDATA and checks it again.
5. `app_start` copies the home bank from XDATA into code RAM and jumps to 0x0000. From here on the application runs as if it was started by the chip.

## Code RAM and app_start

Code RAM can be written while the CPU runs:

- register 0x6040 bit 1 opens code RAM for writes,
- bits 2-3 of the same register select one of four 4 KiB pages,
- each byte of the page is written to register 0x80b0 + 4 * offset, in the low data byte.

`app_start` (`loader/loader_alias.c`) does the copy. The tricky part is that it overwrites the home bank, which is the RAM the rest of the loader runs from. So it can not run from there and can not call anything there:

- The loader is linked so that `app_start` sits at 0x3e00 in flash. The flash is also visible through the bank window (PSBANK) at some bank and address. `alias_find()` in `loader.c` searches the banks for the window whose last 512 bytes of the loader match the code RAM copy of `app_start`, and `start_app()` jumps there. On a 2 MiB flash this is bank 0x2b, address 0xbe00. If no window matches, the loader says "Loader window check failed" and does not start the application.
- `app_start` is plain asm with relative jumps only (`sjmp`, `djnz`, `cjne`, `jz`) and one `ljmp 0x0000` at the end, so it works at whatever address the window maps it to. The Makefile checks `loader_alias.lst` and stops the build if any other `ljmp`, `lcall`, `ajmp` or `acall` shows up.
- It uses r0-r7, a, b, dptr, two IRAM bytes (0x08, 0x09) as wait counters, the SFR register interface and XDATA. No stack and no calls.

The numeric labels are local labels of the sdcc inline assembler (`nnnnn$`). They are grouped by step:

| Labels | Step |
|--------|------|
| 20101-20102 | disable interrupts, read 0x6040 and keep it in r2-r5 |
| 20110-20116 | for each of the 4 pages: set bit 1 and the page in 0x6040, write 4096 bytes from `slotbuf` to 0x80b0, 0x80b4, ... 0xc0ac |
| 20120-20122 | write the saved value back to 0x6040 |
| 20123-20124 | compare code RAM (movc) with `slotbuf`, on a match `ljmp 0x0000` |
| 20125-20126 | on a mismatch try again through 20117, up to 3 times, then reset the chip through register 0x0024 |

Each register access waits for the busy flag with a bounded loop, so a stuck register interface can not hang the copy forever.

## Confirming a start and recovery

The boot counter at 0x3a000 is `BC` followed by one zero byte per start. The application erases it 15 s after a start, which confirms the start.

If the loader finds 3 or more unconfirmed starts, it prints

    Recovery boot, press x within 3 s for XMODEM, c for the console

and waits 3 s. Without a key it starts the application, which sees the same counter and runs with the default configuration and password, so a broken configuration can not lock you out. So to get into recovery, reset or power cycle the switch 4 times, each time within 15 s of the start.

Builds without the loader hooks never confirm, so with such a build every start from the fourth one on waits 3 s.

## XMODEM

`x` in the recovery window, or `xmodem` in the console, waits for an image over XMODEM: 115200 8N1, 128 byte blocks, checksum only. Any sender that does plain XMODEM works, 1K blocks are refused. The image lands in the upload area and is installed the same way as one uploaded from the web UI. Enter while XMODEM waits opens the console instead.

## Console

`c` in the recovery window opens a small console. Numbers are hex. A command is read only after the `> ` prompt, the loader has no receive buffer.

| Command | |
|---------|--|
| `info` | flash ID and unique ID, loader CRC, application CRC, unconfirmed starts, upload header |
| `dump ADDR [LEN]` | show flash bytes |
| `crc ADDR LEN` | CRC16 of a flash range |
| `xmodem` | receive an image |
| `net [IP]` | bring up the built-in ports 4-7 for the CPU only and answer ARP and ping |
| `ipaddr [IP]` | show or set the own address |
| `serverip [IP]` | show or set the TFTP server |
| `tftp FILE` | fetch an image from the TFTP server and install it |
| `boot` | leave the console and start the application |
| `reset` | restart the switch |

The console closes after 60 s without input. After `net` or `tftp` leaving it resets the switch, because the ports were set up for the loader.

TFTP works like in U-Boot:

    > ipaddr 192.168.2.2
    > serverip 192.168.2.1
    > tftp rtlplayground-<ver>-<machine>.bin

The server has to be on a built-in port and in the same subnet. The file name can be up to 120 characters. `#` is printed every 64 blocks, any key stops the transfer. The image goes through the same checks and install as with XMODEM. TFTP has no authentication, so use it only on a network you trust.

## Updating the loader

The loader never writes itself. Upload the 16 KiB loader file on the Firmware tab of the running application (`POST /loader`). The application stores it in the staging area, checks the length and the CRC, and then, before it resets, erases 0x0-0x3fff, writes the new loader and reads it back, up to 3 times. If all 3 fail it does not reset and asks for the upload again. A file identical to the installed loader is not written. A loader file on a switch without a loader is refused.

The write takes a second or two. A power loss in that second needs a programmer, the same as any update without the loader.

## Moving to the loader

1. Build with `LOADER=1`.
2. Upload `rtlplayground-boot-<ver>-<machine>.bin` on the Firmware tab of the running firmware. The old update code copies it to the start of flash and resets. The saved configuration stays, the copy stops below 0x70000.
3. The console shows `RTLPlayground loader` and then `Starting application`.

Step 2 is still the old copy, so a power loss during it needs a programmer. Going back to a build without the loader also needs a programmer.

## Small flash

With less than 1 MiB of flash there is no room for an upload area. The loader prints "Small flash, updates over XMODEM only" and writes XMODEM and TFTP blocks straight to their place: the home bank to XDATA, the banks to flash. The metadata is erased with the first block and written only after the whole image checks out, so an interrupted transfer leaves "No valid application" and the next transfer fixes it. The old firmware takes no upload on such a switch, so the boot image has to be written once with a programmer.

## Tests

`test/test_loader.c` builds `loader/loader.c` and `loader/net.c` unmodified for the host, against a model of the flash, UART, timer 0 and NIC. It covers install and resume, CRC checks, XMODEM, TFTP, small flash, lost flash writes and the recovery window.
