#!/usr/bin/env python3
"""Report local SDCC allocation and image bounds; this is not a flashing approval."""

import argparse
import hashlib
from pathlib import Path
import re
import sys


def crc16(data):
    crc = 0
    for byte in data:
        crc ^= byte
        for _ in range(8):
            crc = (crc >> 1) ^ (0xA001 if crc & 1 else 0)
    return crc


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("ram", "size"))
    parser.add_argument("--build-dir", type=Path, required=True)
    args = parser.parse_args()
    base = args.build_dir / "rtlplayground"
    mem = base.with_suffix(".mem").read_text()
    mapping = base.with_suffix(".map").read_text()
    print(f"Local build: {args.build_dir} (not a runtime measurement)")
    print(f"Report timestamp: {base.with_suffix('.mem').stat().st_mtime_ns} ns since epoch")
    errors = []

    def usage(label, used, limit):
        print(f"{label}: {used:,} / {limit:,} B; remaining {limit - used:,} B ({used / limit:.1%} used)")
        if used > limit:
            errors.append(f"{label} exceeds physical/layout limit")

    stack = re.search(r"stack starts at:\s*0x([0-9a-f]+).*?with (\d+) bytes available", mem, re.I)
    xram = re.search(r"EXTERNAL RAM\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)\s+(\d+)", mem, re.I)
    if not stack or not xram:
        raise ValueError("Unrecognized SDCC memory report; refusing to guess")
    stack_start, stack_free = int(stack[1], 16), int(stack[2])
    usage("IRAM before stack (registers/data/overlay)", stack_start, 256)
    print(f"Stack allocation: {stack_free} B from 0x{stack_start:02x}; NOT measured stack headroom.")
    usage("XRAM allocated", int(xram[3]), 65536)
    xram_end = int(xram[2], 16) + 1
    print(f"XRAM highest address + 1: 0x{xram_end:x}; address space above: {65536 - xram_end:,} B.")
    if xram_end > 65536 or stack_start + stack_free > 256:
        errors.append("RAM addresses exceed physical memory")
    print("SDCC's generic 16 MiB Max column is not the chip's physical capacity.")
    if args.action == "size":
        areas = {}
        for match in re.finditer(r"^(\w+)\s+([0-9A-Fa-f]{8})\s+([0-9A-Fa-f]{8})\s*=.*\(.*CODE.*\)", mapping, re.M):
            areas[match[1]] = (int(match[2], 16), int(match[3], 16))
        banks = [("BANK0", 0, 0x3ffe), ("BANK1", 0x14000, 0x20000),
                 ("BANK2", 0x24000, 0x30000)]
        if not areas or "BANK1" not in areas or "BANK2" not in areas:
            raise ValueError("Missing expected code areas in linker map")
        ends = {name: start for name, start, _ in banks}
        for name, (start, length) in areas.items():
            if not length:
                continue
            for bank, lower, upper in banks:
                if lower <= start < upper and start + length <= upper:
                    ends[bank] = max(ends[bank], start + length)
                    break
            else:
                errors.append(f"Code area {name} outside supported bank boundaries")
        for name, start, end in banks:
            usage(name + " address span", ends[name] - start, end - start)
        print("BANK0 budget reserves 2 B for the image header.")
        # The final image is selected only when exactly one exists, avoiding stale selection.
        images = list(args.build_dir.glob("rtlplayground-*.bin"))
        if len(images) != 1:
            raise ValueError("Expected exactly one versioned .bin in BUILD_DIR; use an isolated build directory")
        image = images[0]
        data = image.read_bytes()
        print(f"Image: {image}")
        print(f"SHA-256: {hashlib.sha256(data).hexdigest()}")
        if len(data) != 524288:
            errors.append(f"Image must be exactly 524288 B, got {len(data)}")
        if data[:2] != b"\x00\x40":
            errors.append("Image header is not RTLPlayground 00 40")
        checksum = crc16(data)
        print(f"Image size: {len(data):,} B; CRC16: 0x{checksum:04x} (expected 0xb001)")
        if checksum != 0xB001:
            errors.append("Image CRC mismatch")
        header = Path("html_data.h")
        definitions = dict(re.findall(r"#define\s+(FDATA_(?:START|SIZE)_\w+)\s+(0x[0-9a-f]+|\d+)", header.read_text()))
        if not definitions:
            raise ValueError("Missing generated HTML layout")
        html_end = 0x40000
        for key, value in definitions.items():
            if not key.startswith("FDATA_START_"):
                continue
            start = int(value, 0)
            size = int(definitions[key.replace("FDATA_START_", "FDATA_SIZE_")], 0) + 1
            html_end = max(html_end, start + size)
            if start < 0x40000 or start + size > 0x6f000:
                errors.append(f"{key} overlaps non-HTML flash area")
        usage("HTML span (generated header; ensure same build)", html_end - 0x40000, 0x6f000 - 0x40000)
        print("User config sector reserved: 0x70000..0x70fff. Image contains repository defaults.")
    print("Allocation/format checks only. No runtime, configuration preservation or recovery guarantee.")
    for error in errors:
        print(f"ERROR: {error}")
    return bool(errors)


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError, KeyError) as exc:
        sys.exit(f"ERROR: {exc}. Build locally with SDCC 4.5 and MACHINE=SWTGW218AS first.")
