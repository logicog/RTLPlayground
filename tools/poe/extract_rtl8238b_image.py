#!/usr/bin/env python3
"""
RTL8238B PSE firmware-image extractor.

This tool extracts the **RTL8238B** PoE controller's firmware image. PSE images are wrapped in a
16-byte container with magic word 0x8239 (layout below); an OEM dump can hold more than one such
container - the OEM firmware can carry PSE firmware for several different chips - so the tool picks
the RTL8238B's image out by its size. See doc/poe-rtl8238b.md.

The RTL8238B's PoE application firmware is volatile and must be uploaded to the controller at
every boot. RTLPlayground embeds it (see doc/poe.md), but the image is Realtek/OEM proprietary
and is NOT shipped in this repo - you carve it out of your own device's OEM firmware with this
tool, which writes tools/poe/pse_image.bin for the Makefile to embed.

Container layout (little-endian) - a 16-byte header with magic 0x8239 (bytes 39 82) at offset +2:

    +0   uint16   0x0000          (prefix)
    +2   uint16   0x8239          magic
    +4   uint16   version/flags
    +6   uint16   section_len[0]
    +8   uint16   section_len[1]
    +10  uint16   section_len[2]
    +12  uint16   section_len[3]
    +14  uint16   0x0000
    total length = 16 (header) + sum(section_len[0..3]) + 4 (trailer)

An OEM dump can bundle PSE firmware for more than one chip (the KP-9000-9XHPML-X V3.1 dump carries
two 0x8239 containers: 0x52ac = 21164 B for a different PSE chip, and 0x1f5c = 8028 B for this
board's RTL8238B). This tool writes the RTL8238B image - the 8028-byte one - as pse_image.bin; the
firmware reads the length from the header, so it simply uploads that single file.

Usage:
    python extract_rtl8238b_image.py <oem_firmware.bin> [-o OUTDIR]
"""
import os
import struct
import sys

MAGIC = b"\x39\x82"          # 0x8239 little-endian, at header offset +2
HDR = 16
TRAILER = 4
MIN_LEN = 0x400             # sanity bounds for a carved container
MAX_LEN = 0x40000
BOARD_IMAGE_LEN = 0x1f5c    # RTL8238B PSE app the KP-9000-9XHPML-X V3.1 uploads (8028 B)


def u16(b, o):
    return struct.unpack_from("<H", b, o)[0]


def find_images(data):
    """Return [(start, total, blob), ...] for every valid RTL8238B container, largest first."""
    out, seen, pos = [], set(), 0
    while True:
        m = data.find(MAGIC, pos)
        if m < 0:
            break
        pos = m + 1
        start = m - 2                                  # container begins 2 bytes before the magic
        if start < 0 or start + HDR > len(data):
            continue
        if data[start] != 0 or data[start + 1] != 0:   # expect the 0x0000 prefix
            continue
        total = HDR + sum(u16(data, start + off) for off in (6, 8, 10, 12)) + TRAILER
        if not (MIN_LEN <= total <= MAX_LEN) or start + total > len(data):
            continue
        if start in seen:
            continue
        seen.add(start)
        out.append((start, total, data[start:start + total]))
    return sorted(out, key=lambda x: x[1], reverse=True)


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    src = sys.argv[1]
    outdir = os.path.dirname(os.path.abspath(__file__))
    if "-o" in sys.argv:
        outdir = sys.argv[sys.argv.index("-o") + 1]

    with open(src, "rb") as f:
        data = f.read()

    found = find_images(data)
    if not found:
        sys.exit("No RTL8238B PSE image found (magic 0x8239 + valid length equation).")

    print("Found %d PSE image(s) in %s:" % (len(found), src))
    for start, total, _ in found:
        mark = "   <- the KP-9000 uploads this" if total == BOARD_IMAGE_LEN else ""
        print("  @0x%06x  %6d B (0x%04x)%s" % (start, total, total, mark))

    # Choose the single image to embed. The firmware uploads whatever pse_image.bin contains
    # (length from its header), so we write the one the board actually uses: the known KP-9000
    # image if present, otherwise the sole image found.
    board = [b for (_, t, b) in found if t == BOARD_IMAGE_LEN]
    if board:
        blob = board[0]
    elif len(found) == 1:
        blob = found[0][2]
    else:
        sys.exit("\nThis dump holds several PSE images and none matches the known KP-9000 "
                 "size (%d B).\nThis looks like a different board: save the one it uses as "
                 "%s." % (BOARD_IMAGE_LEN, os.path.join(outdir, "pse_image.bin")))

    path = os.path.join(outdir, "pse_image.bin")
    with open(path, "wb") as f:
        f.write(blob)
    print("\nwrote %s (%d B) - ready to build" % (path, len(blob)))


if __name__ == "__main__":
    main()
