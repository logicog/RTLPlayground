#!/usr/bin/env python3
"""
check_linked.py - checks on a linked build that the compiler and linker don't make.

  absolute  A variable placed with __at must not lie where the linker puts
            relocatable variables: the linker doesn't reserve an __at address,
            so it hands the same bits, bytes or xdata to someone else. (The main
            loop's event flags at bits 0-3 of byte 0x20 shared their bits with
            the command parser's bools and the serial TX flag.)

  banks     No code bank holds more than the 48 KiB its window (0x4000-0xffff)
            shows: the rest would land outside it.

  psbank    Code that writes PSBANK (SFR 0x96, the code bank mapped at
            0x4000-0xffff) must put the caller's bank back before it returns: a
            home-bank function called from bank 2 that returns with bank 1
            mapped lands in bank 1 (crc16_bank1() did this, 67d09dd). Banked
            code must not write PSBANK at all: it switches out the code it is
            running from. The bank-call trampolines are exempt.

The psbank check follows each function from its label in program order: a save
is a push of PSBANK or a copy of it into A, R0-R7 or a direct byte; a write
that copies a saved value back restores it; any other write leaves the bank
changed, and a ret or reti while it is changed is reported. A save is
forgotten when its register or byte is written, and a save in A by any
instruction other than the write itself. It is conservative: a function this
cannot follow must be rewritten or added to EXEMPT with a reason.

Usage: check_linked.py <build-output-dir>      every program linked in it
       check_linked.py <build-output-dir> MAP  the one program linked to MAP
(exit status 1 on a finding)
"""
import os, re, sys

PSBANK = 0x96
# __sdcc_banked_call/_ret switch banks for a living (crtbank.asm); the loader's
# start_app() maps the application's bank and jumps into it, never returning.
EXEMPT = {"__sdcc_banked_call", "__sdcc_banked_ret", "_start_app"}

SEG_SYM = re.compile(r"^C:\s+([0-9A-Fa-f]{8})\s+([sl])_(\w+)\s*$")
BANK_AREA = re.compile(r"^(BANK[0-9])\s+([0-9A-Fa-f]{8})\s+([0-9A-Fa-f]{8}) =")
BANK_WINDOW = 0xC000
MEM_ROW = re.compile(r"^0x([0-9a-f]{2}):\|(.*)\|\s*$")
ASM_AREA = re.compile(r"^\s*\.area\s+(\S+)")
ASM_ABS = re.compile(r"^(_\w+)\s*=\s*0x([0-9A-Fa-f]+)\s*$")
RST = re.compile(r"^\s+([0-9A-F]{6})((?: [0-9A-F]{2})+)\s+\[\s*\d+\]\s+\d+\s+(.*)$")
RST_LABEL = re.compile(r"^\s+(?:[0-9A-F]{6})?\s+\d+\s+(\w+)::?\s*$")

XDATA_AREAS = ("XSEG", "XISEG", "PSEG")
# The .mem file's internal RAM grid: what the linker put in each byte.
MEM_KIND = {"B": "relocatable bits", "T": "bit registers", "Q": "overlay",
            "I": "idata", "S": "the stack"}


def files(outdir, ext):
    for root, _, names in os.walk(outdir):
        for n in names:
            if n.endswith(ext):
                yield os.path.join(root, n)


def link_dirs(outdir):
    """Each linked program in the build (the app, the loader): dir with its .map."""
    return sorted({os.path.dirname(m) for m in files(outdir, ".map")})


def map_base(linkdir):
    return os.path.join(linkdir, next(f for f in os.listdir(linkdir) if f.endswith(".map")))[:-4]


def rsts(linkdir, outdir):
    """The listings of the program linked in linkdir (C and assembly modules)."""
    loader = os.path.join(outdir, "loader")
    for r in files(outdir, ".rst"):
        if (os.path.dirname(r) == loader) == (linkdir == loader):
            yield r


def modules(linkdir, outdir):
    """The modules of the program linked in linkdir: the loader's live in
    loader/, the app's everywhere else."""
    loader = os.path.join(outdir, "loader")
    for a in files(outdir, ".asm"):
        if (os.path.dirname(a) == loader) == (linkdir == loader):
            yield a


def segments(mapfile):
    """Relocatable bit and xdata segments from the map's s_/l_ symbols:
    {"bit": [(name, start, end)], "xdata": [...]}."""
    sym = {}
    for line in open(mapfile):
        m = SEG_SYM.match(line)
        if m:
            sym[(m.group(2), m.group(3))] = int(m.group(1), 16)
    out = {"bit": [], "xdata": []}
    for space, names in (("bit", ("BSEG",)), ("xdata", XDATA_AREAS)):
        for n in names:
            s, l = sym.get(("s", n)), sym.get(("l", n))
            if s is not None and l:
                out[space].append((n, s, s + l))
    return out


def ram_grid(memfile):
    """Internal RAM byte -> the .mem grid's letter for it (digits: register
    banks, a-z: data, A: absolute, B/T/Q/I/S as MEM_KIND)."""
    grid = {}
    for line in open(memfile):
        m = MEM_ROW.match(line)
        if m:
            for i, c in enumerate(m.group(2).split("|")):
                grid[int(m.group(1), 16) + i] = c
    return grid


def ram_user(c):
    if c in MEM_KIND:
        return MEM_KIND[c]
    if c.isdigit():
        return f"register bank {c}"
    if c.islower():
        return "relocatable data"
    return None     # free or absolute


def absolutes(asmfiles):
    """__at variables -> (symbol, space, address, module). SDCC writes them as
    `_sym = 0xaddr` inside the area of their space."""
    for asm in asmfiles:
        area = None
        for line in open(asm, errors="replace"):
            m = ASM_AREA.match(line)
            if m:
                area = m.group(1)
                continue
            m = ASM_ABS.match(line)
            if not m:
                continue
            space = {"BSEG": "bit", "DSEG": "data", "IABS": "data", "ISEG": "data",
                     "XSEG": "xdata", "XABS": "xdata", "XISEG": "xdata"}.get(area)
            if space:   # RSEG: SFRs and SFR bits, not memory
                yield m.group(1), space, int(m.group(2), 16), os.path.basename(asm)


def check_absolute(linkdir, outdir):
    found = []
    base = map_base(linkdir)
    seg = segments(base + ".map")
    grid = ram_grid(base + ".mem")
    for sym, space, addr, mod in absolutes(modules(linkdir, outdir)):
        hits = [f"{n} (0x{s:x}-0x{e - 1:x})" for n, s, e in seg.get(space, []) if s <= addr < e]
        if space == "data" and ram_user(grid.get(addr, " ")):
            hits.append(f"byte 0x{addr:x}, which holds {ram_user(grid[addr])}")
        for h in hits:
            found.append(f"{mod}: {sym} __at {space} 0x{addr:x} lies in {h}: "
                         f"the linker gives it to other variables too")
    return found


def check_banks(linkdir):
    size = {}
    for line in open(map_base(linkdir) + ".map"):
        m = BANK_AREA.match(line)
        if m:
            size[m.group(1)] = int(m.group(3), 16)
    return [f"{b} holds {n} bytes, more than the {BANK_WINDOW} its window shows: "
            f"the code no longer fits the banks (see tools/place_banks.py)" for b, n in sorted(size.items()) if n > BANK_WINDOW]


def insns(rst):
    """(label or None, address, bytes) in listing order."""
    for line in open(rst, errors="replace"):
        m = RST_LABEL.match(line)
        if m:
            yield m.group(1), None, None
            continue
        m = RST.match(line)
        if m:
            yield None, int(m.group(1), 16), [int(b, 16) for b in m.group(2).split()]


def writes_reg(b):
    """The R0-R7 index an instruction writes, else None."""
    op = b[0]
    if 0x78 <= op <= 0x7F or 0xF8 <= op <= 0xFF or 0xA8 <= op <= 0xAF or \
       0x08 <= op <= 0x0F or 0x18 <= op <= 0x1F or 0xD8 <= op <= 0xDF or 0xC8 <= op <= 0xCF:
        return op & 7
    return None


def writes_direct(b):
    """The direct byte an instruction writes, else None."""
    op = b[0]
    if op in (0x75, 0xF5, 0xD0, 0x42, 0x43, 0x52, 0x53, 0x62, 0x63, 0x05, 0x15, 0xC5, 0xD5) or \
       0x86 <= op <= 0x8F:
        return b[1] if len(b) > 1 else None
    if op == 0x85 and len(b) == 3:   # mov dir,dir: dest is the second operand byte
        return b[2]
    return None


def check_psbank(linkdir, outdir):
    found = []
    for rst in rsts(linkdir, outdir):
        func = None
        for label, addr, b in insns(rst):
            if label:
                func, saved, changed = label, set(), False
                continue
            if func is None or func in EXEMPT:
                continue
            op = b[0]
            banked = addr >= 0x10000
            if writes_direct(b) == PSBANK:
                src = {0xD0: "stack", 0xF5: "A"}.get(op)
                if 0x88 <= op <= 0x8F:
                    src = f"R{op & 7}"
                elif op == 0x85:
                    src = f"D{b[1]:02x}"
                if banked:
                    found.append(f"{os.path.basename(rst)}: {func} (banked, 0x{addr:x}) writes PSBANK")
                if src in saved:
                    changed = False
                    if src == "stack":
                        saved.discard("stack")
                else:
                    changed = True
                saved.discard("A")
                continue
            # saves: push PSBANK, mov A/Rn/dir,PSBANK
            if op == 0xC0 and b[1] == PSBANK and not changed:
                saved.add("stack")
                continue
            if op == 0xE5 and b[1] == PSBANK and not changed:
                saved.add("A")
                continue
            if 0xA8 <= op <= 0xAF and b[1] == PSBANK and not changed:
                saved.add(f"R{op & 7}")
                continue
            if op == 0x85 and b[1] == PSBANK and not changed:
                saved.add(f"D{b[2]:02x}")
                continue
            r = writes_reg(b)
            if r is not None:
                saved.discard(f"R{r}")
            d = writes_direct(b)
            if d is not None:
                saved.discard(f"D{d:02x}")
            saved.discard("A")
            if op in (0x22, 0x32) and changed:
                found.append(f"{os.path.basename(rst)}: {func} returns at 0x{addr:x} "
                             f"with PSBANK changed (the caller's bank not restored)")
    return found


def main():
    if len(sys.argv) not in (2, 3):
        sys.exit(__doc__)
    outdir = os.path.normpath(sys.argv[1])
    dirs = [os.path.normpath(os.path.dirname(sys.argv[2]))] if len(sys.argv) == 3 else link_dirs(outdir)
    found = []
    for d in dirs:
        found += check_absolute(d, outdir) + check_banks(d) + check_psbank(d, outdir)
    for f in found:
        print("check_linked: " + f)
    if found:
        sys.exit(1)
    print(f"check_linked: {outdir}: no __at overlaps, banks fit, PSBANK restored everywhere")


if __name__ == "__main__":
    main()
