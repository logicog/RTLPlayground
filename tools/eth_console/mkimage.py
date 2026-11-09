#!/usr/bin/env python3
"""Package an SDCC build for the TL-SG108PE V5.

    sdcc -mmcs51 --code-loc 0x1000 -o build/blink.ihx blink.c
    ./mkimage.py build/blink.ihx build/blink_up.bin [flash_dump.bin build/flash.bin]

A build at CODE 0x1000 becomes a TP-Link update image that runs under the boot
code (layout and checksums as in doc/tl-sg108pe-v5-firmware.md). A build at
CODE 0x0000 becomes a full-flash image that replaces the boot code: the 0x4000
prefetch prefix `00 40`, then CODE 0x0000-0x3ffd. With a flash dump, also writes
a copy of the dump with that area replaced, for a SPI programmer.
"""
import sys

COMMON = 0x2FFE      # CODE 0x1000-0x3ffd, flash 0x1002
BANK3 = 0x1000       # bank 3 CODE 0x4000-0x4fff, flash 0x1c000
HEADER = 20
EMBEDDED = HEADER + COMMON + BANK3   # header copy, bank 3 CODE 0x5000, flash 0x1d000
MAGIC = bytes.fromhex("50451085")
TRAILER = bytes.fromhex("332256ff")  # unknown; copied from 1.0.1 Build 20260330


def read_ihx(path, base, size):
    code = bytearray(b"\xff" * size)
    for line in open(path):
        rec = bytes.fromhex(line.strip()[1:])
        count, addr, kind = rec[0], int.from_bytes(rec[1:3], "big"), rec[3]
        if kind == 0 and count:
            assert base <= addr and addr + count <= base + size, f"{addr:#x} outside common area"
            code[addr - base:addr - base + count] = rec[4:4 + count]
    return code


def ihx_base(path):
    return min(int(line[3:7], 16) for line in open(path) if line[7:9] == "00" and line[1:3] != "00")


def header(length, payload_sum):
    h = bytearray(MAGIC + length.to_bytes(4, "big") + bytes(4) + payload_sum.to_bytes(4, "big") + TRAILER)
    h[8:12] = (sum(h) & 0xFFFFFFFF).to_bytes(4, "big")
    return bytes(h)


def build(code):
    body = bytes(code) + b"\xff" * BANK3
    payload_sum = (sum(body) + HEADER * 0xFF) & 0xFFFFFFFF  # embedded header counts as 0xff
    h = header(len(body) + HEADER, payload_sum)
    return h + body + h


def main():
    full = ihx_base(sys.argv[1]) == 0
    if full:
        image = b"\x00\x40" + read_ihx(sys.argv[1], 0, 0x3FFE)  # hardware copies 0x4000 bytes to code RAM
    else:
        image = build(read_ihx(sys.argv[1], 0x1000, COMMON))
        # Self-check: the header formula reproduces the 2026 release header.
        assert header(0x10F340, 0x044A1B51)[8:12] == (0x4D1).to_bytes(4, "big")
        assert image[:HEADER] == image[EMBEDDED:EMBEDDED + HEADER]
    open(sys.argv[2], "wb").write(image)
    if len(sys.argv) == 5:
        flash = bytearray(open(sys.argv[3], "rb").read())
        assert len(flash) == 0x200000 and flash[:2] == b"\x00\x40", "not a TL-SG108PE V5 dump"
        if full:
            flash[:0x4000] = image
        else:
            flash[0x1002:0x4000] = image[HEADER:HEADER + COMMON]
            flash[0x1C000:0x1D014] = image[HEADER + COMMON:]
        open(sys.argv[4], "wb").write(flash)


if __name__ == "__main__":
    main()
