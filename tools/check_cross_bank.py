#!/usr/bin/env python3
"""
check_cross_bank.py - find direct calls that cross a code bank.

The firmware runs code from 48 KiB flash banks selected by the PSBANK
register. A call between banks must go through the banked-call trampoline
(declared with __banked); a plain `lcall`/`ljmp` to a symbol in another bank
silently lands at that offset in the *current* bank instead - it compiles
cleanly and crashes at runtime. This is the one correctness risk when code is
moved between banks, so it gates the bin-packer.

It reads a linked build's listings (post-link .rst, with real addresses) and
map. Each instruction's own address gives the caller's bank (addr >> 16, with
the resident home/CSEG region as bank 0); the map gives the target symbol's
bank. A direct call is flagged when the target is in a nonzero bank different
from the caller's. The fix for a flag is to declare the target __banked.

With --placement FILE (a file->bank JSON) it instead checks
that proposed packing, using each listing's own file as the caller's bank and
the map's module column for each target's - so a repack can be proven call-safe
before any relink.

On linked addresses it also checks data references: an immediate operand
(`mov dptr,#___str_7`) naming a string literal or a __code const object that
sits in another nonzero bank. Code reads __code data through the bank window
with the *current* PSBANK, so such a read returns whatever the running bank
holds at that offset - garbled strings, no crash. tools/place_banks.py keeps
such files together, so this would be a bug there.

Usage: check_cross_bank.py <build-output-dir> [--placement placement.json]
Exit status is nonzero if any violation is found.
"""
import sys, os, re, bisect, json, argparse

TRAMPOLINE = {"__sdcc_banked_call", "__sdcc_banked_ret"}
MAP_SYM = re.compile(r"^C:\s+([0-9A-Fa-f]{8})\s+(\S+)\s+(\S+)?")
# a listing instruction line: leading hex address ... <mnemonic> <operand>
INSN = re.compile(r"^\s+([0-9A-Fa-f]{4,6})\s.*\b(lcall|ljmp)\s+(\S+)\s*$")
# an instruction with an immediate symbol operand, e.g. mov dptr,#___str_7
IMM = re.compile(r"^\s+([0-9A-Fa-f]{4,6})\s.*\bmov\s+\S+,#\(?\s*(_\w+)")
# a linked label in a listing: address, line number, name
LABEL = re.compile(r"^\s+([0-9A-Fa-f]{6})\s+\d+\s+(\w+):")
FUNC = re.compile(r";\s+function\s+(\w+)\s*$")


def bank_of(addr):
    """Banks are linked on a 64 KiB stride (BANK1=0x14000, BANK2=0x24000, ...);
    the resident home/CSEG/CONST region is below 0x10000. So bank = addr>>16,
    and 0 means 'resident, always reachable'."""
    return addr >> 16


def load_map(path):
    syms = {}                       # name -> (addr, module)
    with open(path) as f:
        for line in f:
            m = MAP_SYM.match(line)
            if not m:
                continue
            addr = int(m.group(1), 16)
            name = m.group(2)
            if name.startswith("l_") or addr == 0:
                continue            # area-length labels / unplaced
            syms[name] = (addr, m.group(3) or "")
    # sorted function table for naming the caller of a flagged call
    funcs = sorted((a, n) for n, (a, _) in syms.items())
    return syms, funcs


def caller_name(funcs, addr):
    i = bisect.bisect_right(funcs, (addr, "\xff")) - 1
    return funcs[i][1] if i >= 0 else "?"


def main(argv):
    ap = argparse.ArgumentParser(usage=__doc__)
    ap.add_argument("outdir")
    ap.add_argument("--placement", help="file->bank JSON to check instead of the linked addresses")
    args = ap.parse_args(argv[1:])
    outdir = args.outdir
    mappath = os.path.join(outdir, "rtlplayground.map")
    if not os.path.exists(mappath):
        sys.exit(f"no map at {mappath}")
    syms, funcs = load_map(mappath)

    placement = None
    if args.placement:
        placement = json.load(open(args.placement))

    rsts = []
    for root, _, files in os.walk(outdir):
        rsts += [os.path.join(root, f) for f in files if f.endswith(".rst")]

    edges = 0
    violations = []
    unresolved = set()
    data_refs = 0
    data_violations = []
    funcs_named = set()             # function symbols, so code pointers aren't data
    for rst in rsts:
        with open(rst) as f:
            funcs_named.update("_" + m.group(1) for m in map(FUNC.search, f) if m)
    for rst in rsts:
        caller_mod = os.path.basename(rst)[:-4]     # the listing's own file
        with open(rst) as f:
            lines = f.readlines()
        labels = {m.group(2): int(m.group(1), 16) for m in map(LABEL.match, lines) if m}
        for line in lines:
            m = IMM.match(line)
            if m and placement is None:
                name = m.group(2)
                if name.startswith("___str_"):
                    taddr = labels.get(name)
                elif name in syms and name not in funcs_named:
                    taddr = syms[name][0]
                else:
                    taddr = None
                if taddr is not None:
                    data_refs += 1
                    addr = int(m.group(1), 16)
                    cb, tb = bank_of(addr), bank_of(taddr)
                    if tb != 0 and tb != cb:
                        data_violations.append((cb, tb, caller_mod, name, addr))
            m = INSN.match(line)
            if not m:
                continue
            addr = int(m.group(1), 16)
            target = m.group(3)
            if target in TRAMPOLINE or not (target[0] == "_"):
                continue
            if target not in syms:
                unresolved.add(target)
                continue
            edges += 1
            taddr, tmod = syms[target]
            if placement is None:
                cb, tb = bank_of(addr), bank_of(taddr)
            else:
                cb, tb = placement.get(caller_mod, 0), placement.get(tmod, 0)
            if tb != 0 and tb != cb:
                violations.append((cb, tb, caller_name(funcs, addr), target, addr))

    how = f"proposed placement {args.placement}" if placement else "linked addresses"
    print(f"checked {edges} direct call/jump edges across {len(rsts)} listings ({how})")
    if unresolved:
        print(f"note: {len(unresolved)} call target(s) not in the map (library/runtime), skipped")
    if not violations:
        print("OK: no direct call crosses a bank")
    else:
        print(f"\n{len(violations)} CROSS-BANK DIRECT CALL(S) - declare the target __banked:\n")
        for cb, tb, caller, target, addr in sorted(violations):
            print(f"  bank {cb} -> bank {tb}:  {caller} (@0x{addr:06x}) calls {target}")
    if placement is None:
        print(f"checked {data_refs} __code data references")
        if not data_violations:
            print("OK: no __code data read crosses a bank")
        else:
            mods = sorted({mod for _, _, mod, _, _ in data_violations})
            print(f"\n{len(data_violations)} CROSS-BANK __code DATA REFERENCE(S) in {len(mods)} file(s)"
                  " - reader and data must share a bank (tools/place_banks.py):\n")
            for cb, tb, mod, target, addr in sorted(data_violations)[:40]:
                print(f"  bank {cb} reads bank {tb}:  {mod} (@0x{addr:06x}) {target}")
            if len(data_violations) > 40:
                print(f"  ... {len(data_violations) - 40} more; files: {' '.join(mods)}")
    return 1 if violations or data_violations else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
