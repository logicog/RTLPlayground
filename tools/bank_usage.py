#!/usr/bin/env python3
"""
Print RTLPlayground code-bank usage from an SDCC hexadecimal linker map.
"""

import argparse
from pathlib import Path
import re
import sys


RESIDENT_SIZE = 0x4000 - 2  # imagebuilder puts a two-byte header before code.
BANK_SIZE = 0xC000
BANK_STRIDE = 0x10000
AREA_ROW = re.compile(
    r"^\s*(\S+)\s+([0-9A-Fa-f]+)\s+([0-9A-Fa-f]+)"
    r"\s+=\s+\d+\.\s+bytes\s+\(([^)]+)\)\s*$"
)


def code_areas(map_text):
    """Read CODE areas once each; sdld repeats area rows on subsequent pages."""
    if "Hexadecimal" not in map_text:
        raise ValueError("expected an SDCC hexadecimal linker .map file")
    areas = {}
    for line in map_text.splitlines():
        match = AREA_ROW.match(line)
        if not match:
            continue
        name, address, size, attributes = match.groups()
        if "CODE" not in (attribute.strip() for attribute in attributes.split(",")):
            continue
        area = (int(address, 16), int(size, 16))
        if name in areas and areas[name] != area:
            raise ValueError(f"conflicting area rows for {name}")
        areas[name] = area
    if not areas or not any(size for _, size in areas.values()):
        raise ValueError("no nonempty CODE areas found in linker map")
    return areas


def bank_usage(areas):
    """Return high-water usage, including gaps, and any invalid placements."""
    used = {0: 0}
    errors = []
    for name, (address, size) in areas.items():
        if not size:
            continue
        # Other CODE areas (HOME, CSEG, CONST, XINIT, etc.) are resident.
        match = re.fullmatch(r"BANK(\d+)", name)
        bank = int(match.group(1)) if match else 0
        base = bank * BANK_STRIDE + 0x4000 if bank else 0
        capacity = BANK_SIZE if bank else RESIDENT_SIZE
        used[bank] = max(used.get(bank, 0), address + size - base)
        if address < base or address + size > base + capacity:
            errors.append(
                f"{name}: [0x{address:x}, 0x{address + size:x}) is outside "
                f"[0x{base:x}, 0x{base + capacity:x})"
            )
    rows = []
    for bank, size in sorted(used.items()):
        name = f"BANK{bank}" if bank else "Resident"
        capacity = BANK_SIZE if bank else RESIDENT_SIZE
        rows.append((name, size, capacity))
    return rows, errors


def main():
    parser = argparse.ArgumentParser(
        description=__doc__,
        epilog="Usage includes layout gaps. Resident capacity reserves the "
               "two-byte image header; switched banks each hold 48 KiB.",
    )
    parser.add_argument("map_file", type=Path, help="path to rtlplayground.map")
    args = parser.parse_args()
    try:
        areas = code_areas(args.map_file.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, ValueError) as error:
        parser.exit(2, f"{parser.prog}: {error}\n")
    rows, errors = bank_usage(areas)
    print(f"{'Region':<10} {'Used':>10} {'Capacity':>10} {'Free':>10} {'Usage':>8}")
    for name, used, capacity in rows:
        print(f"{name:<10} {used:>10,} {capacity:>10,} "
              f"{capacity - used:>10,} {used / capacity:>7.1%}")
    print("Bytes; usage includes gaps. Resident reserves 2 bytes for the image header.")
    for error in errors:
        print(f"ERROR: {error}", file=sys.stderr)
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
