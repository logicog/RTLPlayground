#!/usr/bin/env python3
"""Package an SDCC build for the TL-SG108PE V5.

    sdcc -mmcs51 --code-loc 0x1000 -o build/blink.ihx blink.c
    ./mkimage.py build/blink.ihx build/blink_up.bin [flash_dump.bin build/flash.bin]

A build at CODE 0x1000 becomes a TP-Link update image that runs under the boot
code. A build at
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


def read_ihx(path):
    records = []
    eof = False
    with open(path) as source:
        for number, line in enumerate(source, 1):
            line = line.strip()
            if eof or not line.startswith(":"):
                raise ValueError(f"line {number}: unexpected Intel HEX record")
            rec = bytes.fromhex(line[1:])
            if len(rec) < 5 or len(rec) != rec[0] + 5 or sum(rec) & 0xFF:
                raise ValueError(f"line {number}: invalid Intel HEX length or checksum")
            count, addr, kind = rec[0], int.from_bytes(rec[1:3], "big"), rec[3]
            if kind == 0:
                if count:
                    records.append((addr, rec[4:-1]))
            elif kind == 1 and count == 0 and addr == 0:
                eof = True
            else:
                raise ValueError(f"line {number}: unsupported Intel HEX record")
    if not eof or not records:
        raise ValueError("Intel HEX requires data and a final EOF record")
    return records


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
    if len(sys.argv) not in (3, 5):
        sys.exit("usage: mkimage.py input.ihx output.bin [flash_dump.bin flash.bin]")
    records = read_ihx(sys.argv[1])
    full = min(addr for addr, data in records) == 0
    base, size = (0, 0x3FFE) if full else (0x1000, COMMON)
    code = bytearray(b"\xff" * size)
    for addr, data in records:
        if not base <= addr < addr + len(data) <= base + size:
            raise ValueError(f"{addr:#x} outside common area")
        code[addr - base:addr - base + len(data)] = data
    if full:
        image = b"\x00\x40" + code  # hardware copies 0x4000 bytes to code RAM
    else:
        image = build(code)
        # Self-check: the header formula reproduces the 2026 release header.
        assert header(0x10F340, 0x044A1B51)[8:12] == (0x4D1).to_bytes(4, "big")
        assert image[:HEADER] == image[EMBEDDED:EMBEDDED + HEADER]
    if len(sys.argv) == 5:
        with open(sys.argv[3], "rb") as source:
            flash = bytearray(source.read())
        if len(flash) != 0x200000 or flash[:2] != b"\x00\x40":
            raise ValueError("not a TL-SG108PE V5 dump")
        if full:
            flash[:0x4000] = image
        else:
            flash[0x1002:0x4000] = image[HEADER:HEADER + COMMON]
            flash[0x1C000:0x1D014] = image[HEADER + COMMON:]
        if len(flash) != 0x200000:
            raise ValueError("flash image size changed")
        with open(sys.argv[4], "wb") as output:
            output.write(flash)
    with open(sys.argv[2], "wb") as output:
        output.write(image)


if __name__ == "__main__":
    try:
        main()
    except ValueError as error:
        sys.exit(str(error))
