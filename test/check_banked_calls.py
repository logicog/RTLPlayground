#!/usr/bin/env python3
"""Audit relocated 8051 direct calls/jumps against linked banks and image bytes.

Run after packaging: python3 test/check_banked_calls.py output/<build>.
Indirect jumps/function pointers and runtime PSBANK writes need separate review.
"""
import argparse
import json
from pathlib import Path
import re
import sys


def bank(address):
    return address >> 16 if address & 0xffff >= 0x4000 else 0


def image_offset(address):
    n = bank(address)
    return (n - 1) * 0xc000 + (address & 0xffff) if n else address + 2


def audit(directory, snmp_policy=True, image_path=None):
    directory = Path(directory)
    symbols = {}
    for line in (directory / 'rtlplayground.map').read_text().splitlines():
        m = re.match(r'C:\s+([0-9A-Fa-f]{8})\s+(\S+)', line)
        if m:
            symbols[m[2]] = int(m[1], 16)
    images = [Path(image_path)] if image_path else list(directory.glob('*.bin'))
    if len(images) != 1:
        raise ValueError('expected exactly one packaged image')
    image = images[0].read_bytes()
    listings = list(directory.rglob('*.rst'))
    if not symbols or not listings:
        raise ValueError('missing linked symbols or relocated listings')
    errors, calls, snmp_calls = [], 0, 0
    for path in listings:
        lines = path.read_text().splitlines()
        local = {}
        scope = None
        for line in lines:
            m = re.match(r'\s*([0-9A-Fa-f]{6})\s+.*?\s([\w.$]+):\s*$', line)
            if m:
                if not m[2][0].isdigit():
                    scope = m[2]
                local[(scope, m[2])] = int(m[1], 16)
        named = {name: address for (_, name), address in local.items() if not name[0].isdigit()}
        if path.stem == 'snmp_control' and snmp_policy:
            command_bank = bank(symbols.get('_snmp_command', 0))
            for name, address in named.items():
                if name.startswith('___str_') and bank(address) != command_bank:
                    errors.append(f'{path.name}: literal {name} is outside the CLI bank')
        scope = None
        for number, line in enumerate(lines, 1):
            label = re.match(r'\s*([0-9A-Fa-f]{6})\s+.*?\s([\w.$]+):\s*$', line)
            if label and not label[2][0].isdigit():
                scope = label[2]
            m = re.search(r'\b(lcall|ljmp|acall|ajmp)\s+([\w.$]+)\s*(?:;.*)?$', line)
            if not m:
                continue
            instruction, target = m.groups()
            encoded = re.match(r'\s*([0-9A-Fa-f]{6})\s+((?:[0-9A-Fa-f]{2}\s+){2,3})', line)
            if not encoded:
                errors.append(f'{path.name}:{number}: missing relocated instruction bytes')
                continue
            pc = int(encoded[1], 16)
            code = bytes.fromhex(encoded[2])
            expected_size = 3 if instruction in ('lcall', 'ljmp') else 2
            code = code[:expected_size]
            destination = local.get((scope, target), named.get(target, symbols.get(target)))
            if destination is None:
                errors.append(f'{path.name}:{number}: unresolved target {target}')
                continue
            calls += 1
            offset = image_offset(pc)
            if image[offset:offset + expected_size] != code:
                errors.append(f'{path.name}:{number}: listing/image byte mismatch')
            if instruction in ('lcall', 'ljmp'):
                address = int.from_bytes(code[1:], 'big')
                opcode = 0x12 if instruction == 'lcall' else 0x02
                if code[0] != opcode:
                    errors.append(f'{path.name}:{number}: incorrect opcode')
            else:
                address = ((pc + 2) & 0xf800) | ((code[0] & 0xe0) << 3) | code[1]
            if address != destination & 0xffff:
                errors.append(f'{path.name}:{number}: target relocation mismatch {target}')
            if bank(destination) and bank(pc) != bank(destination):
                errors.append(f'{path.name}:{number}: unsafe {instruction} {target}: BANK{bank(pc)} -> BANK{bank(destination)}')
            if path.stem == 'snmp_control':
                if target == '_cmd_compare' and snmp_policy:
                    errors.append(f'{path.name}:{number}: SNMP must compare literals in its own module')
                if target == '_cmd_word_is':
                    snmp_calls += 1
    if snmp_policy and (directory / 'snmp_control.rst').exists() and not snmp_calls:
        errors.append('SNMP local comparison helper has no audited callers')
    return {'direct_transfers': calls, 'snmp_local_comparisons': snmp_calls,
            'errors': errors, 'scope': 'symbolic direct calls/jumps with relocated listings and packaged bytes; excludes indirect transfers and runtime bank writes'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('build', type=Path)
    parser.add_argument('--image', type=Path, help='exact packaged image (allows older images in the build directory)')
    args = parser.parse_args()
    try:
        result = audit(args.build, image_path=args.image)
    except (OSError, ValueError) as error:
        print(error, file=sys.stderr)
        return 1
    print(json.dumps(result, indent=2))
    return bool(result['errors'])


if __name__ == '__main__':
    sys.exit(main())
