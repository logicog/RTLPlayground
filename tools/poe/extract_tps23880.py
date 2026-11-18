#!/usr/bin/env python3
"""
Extract the TPS23880 SRAM firmware from TI's SLVC709 package.

The TPS23880 PSE runs its port state machines from an SRAM image that the
host downloads over I2C after every power-on. TI ships the image as C arrays
(SRAMCode[], ParityCode[]) in TPS23880Ref/Project/ccs/sram_code.c of
slvc709c.zip ("TPS23880 PoE MSP430 Reference Code for Basic Applications",
https://www.ti.com/tool/download/SLVC709). It is TI's code, so it is not part
of this repository: download the zip yourself, put it into stock_fw/ and the
build runs this tool (see stock_fw/README.md and doc/poe-tps23880.md).

Input: slvc709c.zip, or sram_code.c taken out of it.
Output: a binary the build places in flash at POE_IMAGE_LOCATION, which
poe/tps23880.c checks and streams to the chip:

    0..3    magic "TPSI"
    4..5    SRAM image length, little endian (0x4000)
    6..7    parity image length, little endian (0x800)
    8       SRAM version (SRAM_VERSION in sram_code.h): register 0x41
            reads it back once the image runs
    9..11   zero
    12..15  sum of all image bytes that follow, little endian
    16..    parity image, then SRAM image

Usage: extract_tps23880.py INPUT [-o OUTPUT]
"""
import argparse
import hashlib
import re
import struct
import sys
import zipfile

SRAM_C = "TPS23880Ref/Project/ccs/sram_code.c"
SRAM_H = "TPS23880Ref/Project/ccs/sram_code.h"
SRAM_LEN = 0x4000
PARITY_LEN = 0x800

# The image of slvc709c.zip 01.00.00.0C. Another one is accepted with a
# warning, as long as its sizes are right: the version check on the chip
# tells whether it runs.
KNOWN = {
    ("4762f744351b96a8", "bd67c2c3bfdedf43"): "SLVC709 01.00.00.0C, SRAM version 1",
}


def array(src, name):
    m = re.search(r"\b%s\s*\[[^\]]*\]\s*=\s*\{(.*?)\}\s*;" % name, src, re.S)
    if not m:
        sys.exit("%s not found in the input" % name)
    body = re.sub(r"/\*.*?\*/", "", m.group(1), flags=re.S)
    body = re.sub(r"//[^\n]*", "", body)
    vals = re.findall(r"0[xX][0-9a-fA-F]+|\b\d+\b", body)
    out = bytes(int(v, 0) for v in vals)
    if any(int(v, 0) > 0xff for v in vals):
        sys.exit("%s holds values above 0xff" % name)
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("input", help="slvc709c.zip or sram_code.c")
    ap.add_argument("-o", "--output", default="poe/firmware/tps23880.bin")
    a = ap.parse_args()

    version = 1
    if zipfile.is_zipfile(a.input):
        with zipfile.ZipFile(a.input) as z:
            names = z.namelist()
            if SRAM_C not in names:
                sys.exit("%s: no %s; is this slvc709c.zip?" % (a.input, SRAM_C))
            src = z.read(SRAM_C).decode("latin-1")
            if SRAM_H in names:
                hdr = z.read(SRAM_H).decode("latin-1")
                m = re.search(r"#define\s+SRAM_VERSION\s+(0x[0-9a-fA-F]+|\d+)", hdr)
                if m:
                    version = int(m.group(1), 0)
    else:
        src = open(a.input, encoding="latin-1").read()

    sram = array(src, "SRAMCode")
    parity = array(src, "ParityCode")
    if len(sram) != SRAM_LEN or len(parity) != PARITY_LEN:
        sys.exit("unexpected sizes: SRAM %d (want %d), parity %d (want %d)"
                 % (len(sram), SRAM_LEN, len(parity), PARITY_LEN))
    if not 0 < version < 0xff:
        sys.exit("SRAM version %d out of range" % version)

    key = (hashlib.sha256(sram).hexdigest()[:16], hashlib.sha256(parity).hexdigest()[:16])
    if key in KNOWN:
        print("TPS23880 SRAM image: %s" % KNOWN[key])
    else:
        print("warning: unknown TPS23880 SRAM image %s/%s, version %d" % (key + (version,)))

    body = parity + sram
    header = b"TPSI" + struct.pack("<HHB3xI", len(sram), len(parity), version, sum(body) & 0xffffffff)
    with open(a.output, "wb") as f:
        f.write(header + body)
    print("wrote %s: %d bytes" % (a.output, len(header) + len(body)))


if __name__ == "__main__":
    main()
