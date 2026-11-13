#!/usr/bin/env python3
import sys

DATA = 0x3a000
DATA_END = 0x40000
ALIAS_FLASH = 0x3800
ALIAS_CODE = 0xb800
ALIAS_RAM = ALIAS_FLASH - 2
LOADER_LEN = 0x4000
BOOTCNT = 0x3a000
META = 0x3b000
SLOT = 0x3c000
HOME_LEN = 0x3ffe
APP = ((0x04000, 0x3a000), (0x40000, 0x70000))
IMAGE_LEN = 0x80000
LAYOUT = 1


def crc16(data, crc=0):
    for b in data:
        crc ^= b
        for _ in range(8):
            crc = (crc >> 1) ^ 0xa001 if crc & 1 else crc >> 1
    return crc


def read_ihx(path):
    mem = {}
    upper = 0
    with open(path) as f:
        for line in f:
            line = line.strip()
            if not line.startswith(':'):
                continue
            rec = bytes.fromhex(line[1:])
            n, addr, typ = rec[0], (rec[1] << 8) | rec[2], rec[3]
            data = rec[4:4 + n]
            if typ == 0:
                for i, b in enumerate(data):
                    mem[upper + addr + i] = b
            elif typ == 4:
                upper = ((data[0] << 8) | data[1]) << 16
            elif typ == 1:
                break
    return mem


def main():
    if len(sys.argv) != 5:
        sys.exit("usage: bootpack.py LOADER.ihx APP.bin OUT.bin LOADER_OUT.bin")
    mem = read_ihx(sys.argv[1])
    img = bytearray(open(sys.argv[2], 'rb').read())
    if len(img) != IMAGE_LEN:
        sys.exit("application image must be %d bytes" % IMAGE_LEN)
    if img[0] != 0x00 or img[1] != 0x40 or img[2] != 0x02:
        sys.exit("not an application image")
    if any(img[DATA:DATA_END]):
        sys.exit("application uses the loader data area")
    if crc16(img) != 0xb001:
        sys.exit("application image checksum is wrong")

    img[SLOT:SLOT + HOME_LEN] = img[2:2 + HOME_LEN]
    img[0:LOADER_LEN] = bytes(LOADER_LEN)
    img[0] = 0x00
    img[1] = 0x40

    home = 0
    alias = 0
    for a, b in mem.items():
        if a < LOADER_LEN:
            if a >= ALIAS_RAM:
                sys.exit("loader HOME too large at 0x%x" % a)
            img[2 + a] = b
            home = max(home, a + 1)
        elif ALIAS_CODE <= a < ALIAS_CODE + LOADER_LEN - ALIAS_FLASH:
            img[a - ALIAS_CODE + ALIAS_FLASH] = b
            alias = max(alias, a + 1 - ALIAS_CODE)
        else:
            sys.exit("loader byte outside HOME and the alias area at 0x%x" % a)

    if alias > LOADER_LEN - ALIAS_FLASH - 2:
        sys.exit("loader alias too large")
    c = crc16(img[:LOADER_LEN - 2]) ^ 0xffff
    img[LOADER_LEN - 2] = c & 0xff
    img[LOADER_LEN - 1] = c >> 8
    if crc16(img[:LOADER_LEN]) != 0xb001:
        sys.exit("loader checksum failed")
    open(sys.argv[4], 'wb').write(img[:LOADER_LEN])

    img[BOOTCNT:BOOTCNT + 0x1000] = b'\xff' * 0x1000
    crc = crc16(img[SLOT:SLOT + HOME_LEN])
    for lo, hi in APP:
        crc = crc16(img[lo:hi], crc)
    img[META:META + 0x1000] = b'\xff' * 0x1000
    img[META:META + 7] = b'RTLB' + bytes((crc & 0xff, crc >> 8, LAYOUT))

    c = crc16(img[:-2]) ^ 0xffff
    img[-2] = c & 0xff
    img[-1] = c >> 8
    if crc16(img) != 0xb001:
        sys.exit("image checksum failed")

    open(sys.argv[3], 'wb').write(img)
    print("loader HOME %d B, alias %d B, application CRC 0x%04x" % (home, alias, crc))


main()
