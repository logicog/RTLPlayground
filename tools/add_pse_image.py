#!/usr/bin/env python3
"""Add an RTL8238B PSE image to a built RTLPlayground firmware image.

    tools/add_pse_image.py FIRMWARE.bin IMAGE0 IMAGE1 OUTPUT.bin

For the MokerLink POE-2G040210GSM (machine POE_2G040210GSM), whose PoE
controller needs an image uploaded at every boot (machine/POE_2G040210GSM_init.c).
IMAGE0 is loaded when the chip's register 0xe8 doesn't read 2, IMAGE1 when it
does; pass '-' for one you don't have. The images are the PoE controller
vendor's firmware and are not part of this repository: take them from your own
switch's stock flash, see doc/devices/2M-PCB49-V1.1.md.

The region at 0x34000 must be empty in FIRMWARE.bin (it is between the last
code bank and the web UI). The image CRC is recalculated, as crc_calculator -u
does, so OUTPUT.bin can be flashed or uploaded through the web UI.
"""
import struct
import sys

IMAGE_SIZE = 0x80000
PSE_IMAGE_START = 0x34000  # machine/POE_2G040210GSM_init.c
PSE_IMAGE_MAX = 0xC000


def crc16(data):
    """CRC-16, polynomial 0xa001, as tools/crc_calculator.c."""
    crc = 0
    for b in data:
        crc ^= b
        for _ in range(8):
            crc = (crc >> 1) ^ 0xA001 if crc & 1 else crc >> 1
    return crc


def load(path):
    if path == "-":
        return b""
    with open(path, "rb") as f:
        return f.read()


def main():
    if len(sys.argv) != 5:
        sys.exit(__doc__)
    fw = bytearray(load(sys.argv[1]))
    images = [load(p) for p in sys.argv[2:4]]

    if len(fw) != IMAGE_SIZE:
        sys.exit("add_pse_image: %s is %d bytes, expected %d" % (sys.argv[1], len(fw), IMAGE_SIZE))
    if crc16(fw) != 0xB001:
        sys.exit("add_pse_image: %s has a bad CRC" % sys.argv[1])
    if not any(images):
        sys.exit("add_pse_image: no image given")

    region = b"PSE1" + struct.pack("<II", len(images[0]), len(images[1])) + images[0] + images[1]
    if len(region) > PSE_IMAGE_MAX:
        sys.exit("add_pse_image: %d bytes, more than the region's %d" % (len(region), PSE_IMAGE_MAX))
    old = fw[PSE_IMAGE_START:PSE_IMAGE_START + PSE_IMAGE_MAX]
    if old.count(0) != len(old) and old.count(0xFF) != len(old):
        sys.exit("add_pse_image: 0x%x-0x%x is not empty in %s"
                 % (PSE_IMAGE_START, PSE_IMAGE_START + PSE_IMAGE_MAX - 1, sys.argv[1]))

    fw[PSE_IMAGE_START:PSE_IMAGE_START + len(region)] = region
    crc = crc16(fw[:-2]) ^ 0xFFFF
    fw[-2] = crc & 0xFF
    fw[-1] = crc >> 8
    assert crc16(fw) == 0xB001

    with open(sys.argv[4], "wb") as f:
        f.write(fw)
    print("%s: image 0 %d bytes, image 1 %d bytes at 0x%x, CRC updated"
          % (sys.argv[4], len(images[0]), len(images[1]), PSE_IMAGE_START))


if __name__ == "__main__":
    main()
