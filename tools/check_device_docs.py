#!/usr/bin/env python3
"""Check that doc/devices/ and machine/ agree with doc/adding-a-device.md.

The rules implemented here are the ones stated in doc/adding-a-device.md; run
the tool with --list-rules to print them all with their default severity.

Violations of these rules are errors. The remaining rules in the document are
not yet followed everywhere in this repository (init hooks borrowed from
another PCB, PCB-named machine targets, ...). Those are reported as warnings.

Some findings are only informational: they point at something worth knowing
(a doc that shows no build target at all, a doc that does not record the CPU
variant) rather than at a rule from doc/adding-a-device.md that a device must
meet to build. Those are reported as info, are listed only with --verbose, and
never fail the check.

A waiver file lists the findings that are known and accepted: it holds the
error and warning lines exactly as the report prints them, one per line. A
listed finding is not reported and does not fail the check; info findings are
never waived. See check_device_docs.waiver in the repository root for an
example, and pass it with --waiver. A line that matches no finding is simply
never used.

Every finding belongs to the machine target, the PCB definition or the device
doc it is about. --machine restricts the report to one device: its machine/<M>
files, its PCB and its doc. Findings about the repository as a whole (e.g. chip
names in doc/supported_devices.md) are only printed without --machine.

Usage:
  python3 tools/check_device_docs.py [--verbose] [--root DIR]
  python3 tools/check_device_docs.py --waiver check_device_docs.waiver
  python3 tools/check_device_docs.py --list-rules
  python3 tools/check_device_docs.py --machine <MACHINE> [--machine <MACHINE>]

Exit status is 1 when an error or a warning was found, not counting the ones
the waiver file lists.
Run it from the repository root.
"""
import argparse
import os
import re
import sys

ALLOWED_EXT_SUFFIXES = {'POE', 'VLANSW', 'UNMANAGED', '1S1C', '2C', '1C'}
# MACHINE_CHIPSET is compared with the numeric RTL837x macros of machine.h, so
# the header takes the bare symbol.
CHIPSETS = {'RTL8372', 'RTL8373'}
# Documentation may name the switch chipset with its N suffix. These are the only
# switch SoCs the docs may mention (RTL8370 is the legacy 836x family chip).
DOC_CHIPSETS = {'8370', '8372', '8373', '8372N', '8373N'}
# Anything else that looks like a member of the 837x family is a typo; the
# companion parts (RTL8224, RTL8238, RTL8261, ...) do not match this.
SWITCH_SOC_RE = re.compile(r'\b(?:RTL)?(8[0-9]7[0-9])(N?)\b')
# The CPU variants the docs must record; the N suffix may be written or implied.
DOC_CPU_RE = re.compile(r'837[23]')
MODEL_MAX_LEN = 29
PHOTO_EXTENSIONS = {'.jpg', '.jpeg'}
PHOTO_MAX_BYTES = 4 * 1024 * 1024
# A doc that describes a mains powered device has to carry the safety warning
# from doc/adding-a-device.md.
MAINS_RE = re.compile(r'(?i)\bmains\b|internal power supply|mains[- ]powered|AC input')
CAUTION_RE = re.compile(r'\s*>\s*\[!CAUTION\]')
MAINS_WARNING_RE = re.compile(r'(?i)mains|high[- ]?voltage')
ERROR = 'error'
WARNING = 'warning'
INFO = 'info'

# check id -> (default severity, the rule as stated in doc/adding-a-device.md).
# --list-rules prints this table, and Checker.add() takes the severity from it,
# so a rule and its description cannot drift apart.
RULES = {
    # names and files
    'doc-pcb-missing': (
        ERROR, 'every doc/devices/<PCB_DOC_NAME>.md has machine/pcb/<MACHINE_PCB>.c'),
    'doc-pcb-variant-only': (
        WARNING, 'a doc without <MACHINE_PCB>.c only has extension variants'),
    'doc-name-collision': (
        ERROR, 'no two doc names differ only in punctuation'),
    'pcb-suffix': (
        WARNING, 'extension suffixes come from the list in doc/adding-a-device.md'),
    'pcb-duplicate': (
        WARNING, 'PCBs that differ only in comments or whitespace share one definition'),
    'symlink-dangling': (
        ERROR, 'every symlink under machine/ and machine/pcb/ resolves'),
    # machine targets
    'machine-not-symlink': (
        ERROR, 'machine/<MACHINE>.c is a symlink'),
    'machine-link-form': (
        ERROR, 'machine/<MACHINE>.c uses the relative form pcb/<MACHINE_PCB>.c'),
    'machine-link-dangling': (
        ERROR, 'machine/<MACHINE>.c points at a file that exists'),
    'machine-pcb-named': (
        WARNING, 'a machine target is named after vendor + model, not the PCB'),
    'machine-header-missing': (
        ERROR, 'machine/<MACHINE>.h exists'),
    'machine-header-macro': (
        ERROR, 'the include guard of machine/<MACHINE>.h is '
                 '_MACHINE_<MACHINE>_H_'),
    'machine-header-model': (
        ERROR, 'machine/<MACHINE>.h defines MACHINE_MODEL'),
    'machine-header-chipset': (
        ERROR, 'MACHINE_CHIPSET is RTL8372 or RTL8373'),
    'machine-model-length': (
        WARNING, 'MACHINE_MODEL is at most 29 characters'),
    'machine-model-name': (
        WARNING, 'MACHINE_MODEL in capitals, optionally followed by the PCB '
                 'name, gives the machine target name'),
    'machine-init-form': (
        ERROR, 'machine/<MACHINE>_init.c is a symlink when present'),
    'machine-init-dangling': (
        ERROR, 'machine/<MACHINE>_init.c points at a file that exists'),
    'machine-init-name': (
        WARNING, "machine/<MACHINE>_init.c points at this PCB's own _init.c"),
    'machine-doc-missing': (
        WARNING, 'every machine PCB is documented in doc/devices/'),
    'pcb-orphan': (
        WARNING, 'every machine/pcb/*.c is used by a machine target'),
    # device documentation
    'supported-link-missing': (
        ERROR, 'every link in doc/supported_devices.md resolves'),
    'supported-doc-unlisted': (
        ERROR, 'every doc/devices/<PCB_DOC_NAME>.md is listed in supported_devices.md'),
    'supported-duplicate': (
        WARNING, 'no two rows in doc/supported_devices.md repeat the same '
                 'brand, type and PCB (case insensitive)'),
    'supported-repeat': (
        INFO, 'a brand and type used by several rows is listed, so that the '
              'PCB of each row can be checked'),
    'supported-order': (
        WARNING, 'the rows are sorted by brand, then type, ignoring case'),
    'doc-target-missing': (
        ERROR, 'a doc only names MACHINE= targets that exist'),
    'doc-target-pcb': (
        WARNING, "a doc's MACHINE= target links to this doc's PCB name"),
    'doc-target-prefix': (
        WARNING, 'a doc writes MACHINE=<NAME>, not MACHINE_<NAME>'),
    'doc-path-missing': (
        ERROR, 'backticked machine/ and pcb/ paths in a doc exist'),
    'doc-silkscreen': (
        WARNING, "a doc's silkscreen marking matches its file name"),
    'doc-photos-missing': (
        ERROR, 'doc/devices/photos/<PCB_DOC_NAME>/ exists'),
    'doc-image-missing': (
        ERROR, 'every image a doc references exists'),
    'doc-photo-format': (
        INFO, 'device photos are jpeg'),
    'doc-photo-size': (
        WARNING, 'device photos are at most 4MB'),
    'doc-mains-warning': (
        WARNING, 'a mains powered device carries the [!CAUTION] warning'),
    'doc-chip-typo': (
        WARNING, 'docs only name the 837x switch SoCs that exist'),
    # informational only
    'doc-no-target': (
        INFO, 'a doc shows its build target (make MACHINE=...)'),
    'doc-cpu-missing': (
        INFO, 'a doc records the CPU variant'),
}
# Findings belong to the machine target, the PCB or the doc they are about, so
# that --machine can select them. A finding without a scope is about the
# repository as a whole.
GLOBAL = ()


def pcb_name(doc_name):
    """<PCB_DOC_NAME> -> <MACHINE_PCB>: punctuation becomes '_'."""
    return re.sub(r'[^A-Za-z0-9_]', '_', doc_name)


def machine_name(model):
    """<MACHINE_MODEL> -> <MACHINE>: capitals, punctuation becomes '_'."""
    return re.sub(r'[^A-Z0-9]', '_', model.upper()).strip('_')


def squash(text):
    """Lower-case and drop everything but letters and digits, for matching
    a model string against the loosely formatted doc tables."""
    return re.sub(r'[^a-z0-9]', '', text.lower())


def tag_scope(scope):
    """Normalize a scope to a tuple of tags ('machine:M', 'pcb:PCB', 'doc:D')."""
    if not scope:
        return GLOBAL
    if isinstance(scope, str):
        return (scope,)
    return tuple(scope)


class UnknownMachine(Exception):
    pass


def load_waiver(path):
    """Read a waiver file into the set of report lines it waives.

    One finding per line, exactly as report() prints the message (the leading
    indent is stripped, so pasting the report works). Blank lines are skipped.
    """
    waived = set()
    with open(path, encoding='utf-8') as fh:
        for line in fh:
            line = line.strip()
            if line:
                waived.add(line)
    return waived


def waiver_matches(waived, finding):
    """Is this finding's message one of the lines the waiver file lists?"""
    severity, _check, message, _scope = finding
    return severity != INFO and message.strip() in waived


class Checker:
    def __init__(self, root, only=None):
        self.root = root
        self.findings = []
        self.docs = {}
        self.pcbs = {}
        self.machines = {}
        self.only = list(only or [])
        self.scope = None
        self.waived = []
        self.waiver_file = None

    # -- helpers ---------------------------------------------------------
    def path(self, *parts):
        return os.path.join(self.root, *parts)

    def add(self, check, message, scope=None):
        self.findings.append((RULES[check][0], check, message, tag_scope(scope)))

    def read(self, rel):
        with open(self.path(rel), encoding='utf-8', errors='replace') as fh:
            return fh.read()

    def load(self):
        for name in sorted(os.listdir(self.path('doc/devices'))):
            if name.endswith('.md'):
                self.docs[name[:-3]] = 'doc/devices/' + name
        for name in sorted(os.listdir(self.path('machine/pcb'))):
            if name.endswith('.c'):
                self.pcbs[name[:-2]] = 'machine/pcb/' + name
        for name in sorted(os.listdir(self.path('machine'))):
            if not name.endswith('.c'):
                continue
            stem = name[:-2]
            if stem.endswith('_init') or stem == 'default_machine_init':
                continue
            self.machines[stem] = 'machine/' + name

    # -- doc <-> pcb names ----------------------------------------------
    def check_doc_pcb_files(self):
        for doc, rel in self.docs.items():
            want = pcb_name(doc)
            if want in self.pcbs:
                continue
            variants = [s for s in self.pcbs if s.startswith(want + '__')]
            if variants:
                self.add('doc-pcb-variant-only',
                         '%s: no machine/pcb/%s.c, only variant(s) %s'
                         % (rel, want, ', '.join(sorted(variants))),
                         'doc:' + doc)
            else:
                self.add('doc-pcb-missing',
                         '%s: no machine/pcb/%s.c' % (rel, want),
                         'doc:' + doc)

    def check_doc_name_collisions(self):
        groups = {}
        for doc in self.docs:
            groups.setdefault(squash(doc), []).append(doc)
        for docs in groups.values():
            if len(docs) < 2:
                continue
            docs.sort()
            self.add('doc-name-collision',
                     '%s: these doc names differ only in punctuation, so one '
                     'PCB would be documented twice'
                     % ', '.join('doc/devices/%s.md' % d for d in docs),
                     ['doc:' + d for d in docs])

    def check_pcb_extension_suffixes(self):
        for stem, rel in self.pcbs.items():
            base = stem[:-5] if stem.endswith('_init') else stem
            if '__' not in base:
                continue
            # <MACHINE_PCB>__SUFFIX, with additional plugins after single '_'.
            for suffix in base.split('__', 1)[1].split('_'):
                if suffix in ALLOWED_EXT_SUFFIXES:
                    continue
                self.add('pcb-suffix',
                         '%s: extension suffix %r is not in the list in '
                         'doc/adding-a-device.md (%s)'
                         % (rel, suffix, ', '.join(sorted(ALLOWED_EXT_SUFFIXES))),
                         'pcb:' + base)

    def code_signature(self, rel):
        """The code with comments and whitespace removed, so that two files
        differing only in formatting compare equal."""
        text = re.sub(r'/\*.*?\*/', '', self.read(rel), flags=re.S)
        text = re.sub(r'//[^\n]*', '', text)
        return re.sub(r'\s+', '', text)

    def check_pcb_duplicates(self):
        groups = {}
        for stem, rel in self.pcbs.items():
            if os.path.islink(self.path(rel)):
                continue
            groups.setdefault(self.code_signature(rel), []).append(stem)
        for stems in groups.values():
            if len(stems) < 2:
                continue
            stems.sort()
            self.add('pcb-duplicate',
                     '%s are identical apart from comments and whitespace; keep '
                     'one definition and make the others symlinks'
                     % ', '.join('machine/pcb/%s.c' % s for s in stems),
                     ['pcb:' + s for s in stems])

    # -- machine targets --------------------------------------------------
    def machine_header(self, machine):
        return self.path('machine', machine + '.h')

    def check_machines(self):
        for machine, rel in self.machines.items():
            if not os.path.islink(self.path(rel)):
                self.add('machine-not-symlink',
                         '%s: is not a symlink to machine/pcb/' % rel,
                         'machine:' + machine)
                continue
            target = os.readlink(self.path(rel))
            match = re.fullmatch(r'pcb/([^/]+)\.c', target)
            if not match:
                self.add('machine-link-form',
                         '%s: points at %r, expected the relative form '
                         'pcb/<MACHINE_PCB>.c' % (rel, target),
                         'machine:' + machine)
                continue
            pcb = match.group(1)
            scope = 'machine:' + machine
            if not os.path.exists(self.path('machine', target)):
                self.add('machine-link-dangling',
                         '%s: points at missing machine/%s' % (rel, target), scope)
            if machine == pcb:
                self.add('machine-pcb-named',
                         '%s: machine target carries the PCB name %s, expected '
                         'vendor + model' % (rel, pcb), scope)
            self.check_machine_header(rel, machine, pcb, scope)
            self.check_machine_doc(rel, pcb, scope)
            self.check_machine_init(rel, machine, pcb, scope)

    def check_machine_header(self, rel, machine, pcb, scope):
        header = self.machine_header(machine)
        header_rel = 'machine/%s.h' % machine
        if not os.path.exists(header):
            self.add('machine-header-missing',
                     '%s: %s is missing' % (rel, header_rel), scope)
            return
        text = self.read(header_rel)
        self.check_machine_header_macro(header_rel, machine, text, scope)
        model = re.search(r'#define\s+MACHINE_MODEL\s+"([^"]*)"', text)
        chip = re.search(r'#define\s+MACHINE_CHIPSET\s+(\w+)', text)
        if not model:
            self.add('machine-header-model',
                     '%s: does not define MACHINE_MODEL' % header_rel, scope)
        else:
            if len(model.group(1)) > MODEL_MAX_LEN:
                self.add('machine-model-length',
                         '%s: MACHINE_MODEL is %d characters, the limit is %d'
                         % (header_rel, len(model.group(1)), MODEL_MAX_LEN), scope)
            # <MACHINE> is <MACHINE_MODEL> in capitals, or that name with this
            # PCB's name appended when the model string alone identifies more
            # than one board (e.g. HISOURCE_HI_K0402WS_PCB_K0402W_V2_0).
            names = [machine_name(model.group(1)),
                     machine_name('%s %s' % (model.group(1), pcb))]
            if machine not in names:
                self.add('machine-model-name',
                         '%s: MACHINE_MODEL %r gives the target name %s (or %s),'
                         ' not %s'
                         % (header_rel, model.group(1), names[0], names[1],
                            machine), scope)
        if not chip:
            self.add('machine-header-chipset',
                     '%s: does not define MACHINE_CHIPSET' % header_rel, scope)
        elif chip.group(1) not in CHIPSETS:
            self.add('machine-header-chipset',
                     '%s: MACHINE_CHIPSET is %s, expected one of %s'
                     % (header_rel, chip.group(1), ', '.join(sorted(CHIPSETS))),
                     scope)

    def check_machine_header_macro(self, rel, machine, text, scope):
        """The include guard of machine/<MACHINE>.h names that same file.

        A board header is copied from the closest existing board, so a guard
        left behind by the copy names another file and hides which board the
        header really belongs to.
        """
        expected = '_MACHINE_%s_H_' % machine
        guard = re.search(r'^\s*#\s*ifndef\s+(\w+)', text, re.M)
        define = re.search(r'^\s*#\s*define\s+(\w+)', text, re.M)
        if not guard:
            self.add('machine-header-macro',
                     '%s: has no #ifndef include guard, expected %s'
                     % (rel, expected), scope)
            return
        named = [guard.group(1)]
        if define:
            named.append(define.group(1))
        for found in named:
            if found == expected:
                continue
            self.add('machine-header-macro',
                     '%s: include guard %s does not match the file name, '
                     'expected %s' % (rel, found, expected), scope)
            return

    def docs_for_pcb(self, pcb):
        base = pcb.split('__', 1)[0]
        return [doc for doc in self.docs if pcb_name(doc) == base]

    def doc_for_pcb(self, pcb):
        docs = self.docs_for_pcb(pcb)
        return docs[0] if docs else None

    def check_machine_doc(self, rel, pcb, scope):
        if self.doc_for_pcb(pcb) is None:
            self.add('machine-doc-missing',
                     '%s: no doc/devices/*.md documents PCB %s' % (rel, pcb), scope)

    def check_machine_init(self, rel, machine, pcb, scope):
        init_rel = 'machine/%s_init.c' % machine
        init_path = self.path(init_rel)
        if not os.path.exists(init_path) and not os.path.islink(init_path):
            return
        if not os.path.islink(init_path):
            self.add('machine-init-form', '%s: is not a symlink' % init_rel, scope)
            return
        target = os.readlink(init_path)
        if not os.path.exists(self.path('machine', target)):
            self.add('machine-init-dangling',
                     '%s: points at missing machine/%s' % (init_rel, target), scope)
        expected = 'pcb/%s_init.c' % pcb
        if target != expected:
            self.add('machine-init-name',
                     '%s: points at %s, expected %s (share the hook through '
                     "this PCB's own init file)" % (init_rel, target, expected),
                     scope)

    # -- orphan definitions ----------------------------------------------
    def check_orphan_pcbs(self):
        used = set()
        for rel in self.machines.values():
            if os.path.islink(self.path(rel)):
                used.add(os.path.basename(os.readlink(self.path(rel)))[:-2])
        for stem, rel in self.pcbs.items():
            if stem.endswith('_init') or stem in used:
                continue
            target = ''
            if os.path.islink(self.path(rel)):
                target = ' -> ' + os.readlink(self.path(rel))
            self.add('pcb-orphan',
                     '%s%s: no machine target uses this PCB' % (rel, target),
                     'pcb:' + stem)

    # -- dangling symlinks ------------------------------------------------
    def check_symlinks(self):
        for directory in ('machine', 'machine/pcb'):
            for name in sorted(os.listdir(self.path(directory))):
                rel = directory + '/' + name
                full = self.path(rel)
                if not (os.path.islink(full) and not os.path.exists(full)):
                    continue
                stem = os.path.splitext(name)[0]
                if stem.endswith('_init'):
                    stem = stem[:-5]
                kind = 'pcb' if directory == 'machine/pcb' else 'machine'
                self.add('symlink-dangling',
                         '%s: points at missing %s' % (rel, os.readlink(full)),
                         '%s:%s' % (kind, stem))

    # -- doc contents -----------------------------------------------------
    def doc_machine_targets(self, text):
        return re.findall(r'MACHINE=(?:MACHINE_)?([A-Za-z0-9_]+)', text)

    def check_docs(self):
        for doc, rel in self.docs.items():
            self.check_doc(rel, doc)

    def check_doc(self, rel, doc):
        text = self.read(rel)
        scope = 'doc:' + doc
        self.check_doc_targets(rel, doc, text)
        self.check_doc_paths(rel, text, scope)
        self.check_doc_prefixed_targets(rel, text, scope)
        self.check_doc_silkscreen(rel, doc, text)
        self.check_doc_images(rel, text, scope)
        self.check_doc_photos(rel, doc)
        self.check_doc_mains_warning(rel, doc, text)
        self.check_doc_cpu(rel, doc, text)
        self.check_chip_names(rel, text, scope)

    def check_doc_targets(self, rel, doc, text):
        scope = 'doc:' + doc
        targets = self.doc_machine_targets(text)
        if not targets:
            self.add('doc-no-target',
                     '%s: shows no build target (make MACHINE=...)' % rel, scope)
            return
        for target in sorted(set(targets)):
            if target not in self.machines:
                self.add('doc-target-missing',
                         '%s: names MACHINE=%s, which has no machine/%s.c'
                         % (rel, target, target), scope)
                continue
            link = self.path('machine', target + '.c')
            if not os.path.islink(link):
                continue
            pcb = os.path.basename(os.readlink(link))[:-2]
            if pcb.split('__', 1)[0] != pcb_name(doc):
                self.add('doc-target-pcb',
                         '%s: MACHINE=%s links to pcb/%s.c, not to this doc\'s '
                         'PCB name' % (rel, target, pcb), scope)

    def check_doc_paths(self, rel, text, scope):
        for ref in sorted(set(re.findall(r'`((?:machine|pcb)/[A-Za-z0-9_./-]+)`',
                                         text))):
            candidate = ref if ref.startswith('machine/') else 'machine/' + ref
            if not os.path.exists(self.path(candidate)):
                self.add('doc-path-missing',
                         '%s: references %s, which does not exist' % (rel, ref),
                         scope)

    def check_doc_prefixed_targets(self, rel, text, scope):
        for target in sorted(set(re.findall(r'MACHINE_([A-Za-z0-9_]+)', text))):
            if target in self.machines:
                self.add('doc-target-prefix',
                         '%s: writes MACHINE_%s, the build takes '
                         'MACHINE=%s' % (rel, target, target), scope)

    def check_doc_silkscreen(self, rel, doc, text):
        for line in text.splitlines():
            match = re.match(
                r'(?i)^\s*(?:[-*]\s+)?(?:pcb\s+)?(?:top\s+)?silkscreen\s*:\s*(.+)$',
                line)
            if not match:
                continue
            value = re.sub(r'`', '', match.group(1)).strip().rstrip('.').strip()
            if not value or value.startswith('|') or re.search(r'(?i)\b(like|see)\b',
                                                              value):
                continue
            markings = [v.strip() for v in value.split('/')]
            if pcb_name(doc) in [pcb_name(v) for v in markings]:
                continue
            self.add('doc-silkscreen',
                     '%s: silkscreen %r does not match the doc file name '
                     '%s.md' % (rel, value, doc),
                     'doc:' + doc)

    def check_doc_images(self, rel, text, scope):
        for ref in re.findall(r'src="([^"]+)"', text):
            if ref.startswith(('http://', 'https://')):
                continue
            if not os.path.exists(self.path('doc/devices', ref)):
                self.add('doc-image-missing',
                         '%s: references %s, which does not exist' % (rel, ref),
                         scope)

    def check_doc_photos(self, rel, doc):
        scope = 'doc:' + doc
        folder_rel = 'doc/devices/photos/' + doc
        folder = self.path(folder_rel)
        if not os.path.isdir(folder):
            self.add('doc-photos-missing', '%s: %s/ is missing' % (rel, folder_rel),
                     scope)
            return
        for name in sorted(os.listdir(folder)):
            full = os.path.join(folder, name)
            if not os.path.isfile(full):
                continue
            photo_rel = folder_rel + '/' + name
            ext = os.path.splitext(name)[1].lower()
            if ext not in PHOTO_EXTENSIONS:
                self.add('doc-photo-format',
                         '%s: is %s, the photos must be jpeg'
                         % (photo_rel, ext or 'without an extension'), scope)
            size = os.path.getsize(full)
            if size > PHOTO_MAX_BYTES:
                self.add('doc-photo-size',
                         '%s: is %d bytes, more than the %dMB limit'
                         % (photo_rel, size, PHOTO_MAX_BYTES // (1024 * 1024)),
                         scope)

    def check_doc_mains_warning(self, rel, doc, text):
        if not MAINS_RE.search(text):
            return
        lines = text.splitlines()
        cautions = []
        for index, line in enumerate(lines):
            if not CAUTION_RE.match(line):
                continue
            block = [line]
            for follow in lines[index + 1:]:
                if not follow.lstrip().startswith('>'):
                    break
                block.append(follow)
            cautions.append('\n'.join(block))
        if any(MAINS_WARNING_RE.search(block) for block in cautions):
            return
        self.add('doc-mains-warning',
                 '%s: describes a mains powered device but carries no '
                 '[!CAUTION] warning about it' % rel, 'doc:' + doc)

    def check_doc_cpu(self, rel, doc, text):
        if DOC_CPU_RE.search(text):
            return
        self.add('doc-cpu-missing',
                 '%s: does not record the CPU variant (RTL8372, RTL8372N, '
                 'RTL8373 or RTL8373N)' % rel, 'doc:' + doc)

    def check_chip_names(self, rel, text, scope=GLOBAL):
        for line_no, line in enumerate(text.splitlines(), 1):
            for match in SWITCH_SOC_RE.finditer(line):
                if match.group(1) + match.group(2) in DOC_CHIPSETS:
                    continue
                self.add('doc-chip-typo',
                         '%s:%d: %r is not a switch chipset (RTL8372, RTL8373 '
                         'or one of their N variants)'
                         % (rel, line_no, match.group(0)), scope)

    # -- supported_devices.md ---------------------------------------------
    def supported_rows(self, text):
        """The data rows of the table in doc/supported_devices.md, as
        (line number, brand, type, pcb) tuples.

        The PCB is the target of the devices/ link in the fourth cell, so that
        two rows linking the same doc compare equal; a row without a link uses
        its fourth cell as it is written.
        """
        rows = []
        body = False
        for line_no, line in enumerate(text.splitlines(), 1):
            line = line.strip()
            if not line.startswith('|') or not line.endswith('|'):
                body = False
                continue
            cells = [cell.strip() for cell in line[1:-1].split('|')]
            if len(cells) < 4:
                continue
            if all(not cell or re.fullmatch(r'[-: ]+', cell) for cell in cells):
                # The header separator: the rows below it are the devices.
                body = True
                continue
            if not body:
                continue
            link = re.search(r'\]\((devices/[^)]+)\)', cells[3])
            rows.append((line_no, cells[0], cells[1],
                         link.group(1) if link else cells[3]))
        return rows

    def check_supported_duplicates(self, rel, text):
        """Compare the table rows by brand and type, ignoring case.

        The same brand and type may document several PCB revisions, so only the
        rows that repeat the brand, type and PCB are an error. The rows that
        repeat just the brand and type are reported as info: they are expected,
        but the PCB of each row is worth a second look.
        """
        groups = {}
        for row in self.supported_rows(text):
            groups.setdefault((row[1].lower(), row[2].lower()), []).append(row)
        for rows in groups.values():
            if len(rows) < 2:
                continue
            by_pcb = {}
            for line_no, _brand, _type, pcb in rows:
                by_pcb.setdefault(pcb, []).append(line_no)
            _line, brand, model, _pcb = rows[0]
            scope = ['doc:' + os.path.basename(pcb)[:-3]
                     for pcb in by_pcb if pcb.startswith('devices/')]
            duplicate = False
            for pcb, lines in sorted(by_pcb.items()):
                if len(lines) < 2:
                    continue
                duplicate = True
                self.add('supported-duplicate',
                         '%s: lines %s list %s %s with %s; keep one row'
                         % (rel, ', '.join(str(n) for n in lines), brand,
                            model,
                            os.path.basename(pcb) if pcb
                            else 'no PCB'), scope)
            if duplicate:
                continue
            self.add('supported-repeat',
                     '%s: lines %s repeat %s %s for different PCBs (%s)'
                     % (rel, ', '.join(str(n) for n, _b, _t, _p in rows),
                        brand, model,
                        ', '.join(sorted(os.path.basename(pcb) or 'no PCB'
                                         for pcb in by_pcb))),
                     scope)

    def supported_key(self, row):
        """The sort key of a table row: brand, then type, ignoring case."""
        return (row[1].lower(), row[2].lower())

    def supported_scope(self, rows):
        """The device docs a set of table rows points at, for --machine."""
        scope = []
        for _line, _brand, _type, pcb in rows:
            if pcb.startswith('devices/'):
                doc = 'doc:' + os.path.basename(pcb)[:-3]
                if doc not in scope:
                    scope.append(doc)
        return scope

    def check_supported_order(self, rel, text):
        """The table rows are sorted by brand and then type, ignoring case.

        A row is reported when it sorts before the one above it, so that the
        report names the two rows that are the wrong way round.
        """
        rows = self.supported_rows(text)
        for previous, row in zip(rows, rows[1:]):
            if self.supported_key(row) >= self.supported_key(previous):
                continue
            self.add('supported-order',
                     '%s: line %d (%s %s) comes after line %d (%s %s) but '
                     'sorts first; sort by brand, then type, ignoring case'
                     % (rel, row[0], row[1], row[2], previous[0],
                        previous[1], previous[2]),
                     self.supported_scope([previous, row]))

    def check_supported_devices(self):
        rel = 'doc/supported_devices.md'
        text = self.read(rel)
        linked = set()
        for link in re.findall(r'\]\((devices/[^)]+)\)', text):
            linked.add(os.path.basename(link))
            if not os.path.exists(self.path('doc', link)):
                self.add('supported-link-missing',
                         '%s: link to %s does not resolve' % (rel, link),
                         'doc:' + os.path.basename(link)[:-3])
        for doc, doc_rel in self.docs.items():
            if doc + '.md' not in linked:
                self.add('supported-doc-unlisted',
                         '%s: %s is not listed' % (rel, doc_rel), 'doc:' + doc)
        self.check_supported_duplicates(rel, text)
        self.check_supported_order(rel, text)
        self.check_chip_names(rel, text)

    # -- --machine --------------------------------------------------------
    def build_scope(self):
        """The tags of the machine targets asked for, their PCB and its doc."""
        tags = set()
        for machine in self.only:
            tags.add('machine:' + machine)
            rel = self.machines[machine]
            pcb = None
            if os.path.islink(self.path(rel)):
                match = re.fullmatch(r'pcb/([^/]+)\.c', os.readlink(self.path(rel)))
                if match:
                    pcb = match.group(1)
            if pcb:
                tags.add('pcb:' + pcb)
                for doc in self.docs_for_pcb(pcb):
                    tags.add('doc:' + doc)
            for doc, doc_rel in self.docs.items():
                if machine in self.doc_machine_targets(self.read(doc_rel)):
                    tags.add('doc:' + doc)
        return tags

    # -- run ---------------------------------------------------------------
    def run(self):
        self.load()
        if self.only:
            unknown = [m for m in self.only if m not in self.machines]
            if unknown:
                raise UnknownMachine(
                    'no %s; --machine takes the name passed to make MACHINE=...'
                    % ', '.join('machine/%s.c' % m for m in unknown))
            self.scope = self.build_scope()
        self.check_symlinks()
        self.check_supported_devices()
        self.check_doc_name_collisions()
        self.check_doc_pcb_files()
        self.check_pcb_extension_suffixes()
        self.check_pcb_duplicates()
        self.check_docs()
        self.check_machines()
        self.check_orphan_pcbs()

    def apply_waivers(self, waived, path):
        """Drop the errors and warnings whose message line is waived."""
        self.waiver_file = path
        kept = []
        for finding in self.findings:
            if waiver_matches(waived, finding):
                self.waived.append(finding)
            else:
                kept.append(finding)
        self.findings = kept

    def visible(self):
        if self.scope is None:
            return self.findings
        return [f for f in self.findings if set(f[3]) & self.scope]

    def report(self, verbose):
        findings = self.visible()
        for severity in (ERROR, WARNING, INFO):
            if severity == INFO and not verbose:
                continue
            grouped = {}
            for sev, check, message, _scope in findings:
                if sev == severity:
                    grouped.setdefault(check, []).append(message)
            for check in sorted(grouped):
                messages = grouped[check]
                print('%s [%s] %d' % (severity, check, len(messages)))
                for message in messages:
                    print('  ' + message)
        errors = sum(1 for f in findings if f[0] == ERROR)
        warnings = sum(1 for f in findings if f[0] == WARNING)
        infos = sum(1 for f in findings if f[0] == INFO)
        if self.scope is None:
            docs = len(self.docs)
            pcbs = len(self.pcbs)
            machines = len(self.machines)
        else:
            docs = sum(1 for d in self.docs if 'doc:' + d in self.scope)
            pcbs = sum(1 for p in self.pcbs if 'pcb:' + p in self.scope)
            machines = sum(1 for m in self.machines if 'machine:' + m in self.scope)
        stat = ('%d docs, %d pcb definitions, %d machine targets: %d errors, '
                '%d warnings, %d info'
                % (docs, pcbs, machines, errors, warnings, infos))
        waived = [f for f in self.waived
                  if self.scope is None or set(f[3]) & self.scope]
        if self.waiver_file is not None:
            stat += ', %d waived by "%s"' % (len(waived), self.waiver_file)
        print(stat)
        if verbose:
            for severity, check, message, _scope in waived:
                print('  waived %s [%s] %s' % (severity, check, message))
        return errors, warnings


def list_rules():
    print('The rules from doc/adding-a-device.md, as checked by this tool:')
    for severity in (ERROR, WARNING, INFO):
        print('\n%ss:' % severity)
        for check, (sev, description) in RULES.items():
            if sev == severity:
                print('  %-24s %s' % (check, description))
    print('\nerrors and warnings fail the check; info never fails.')
    print('--machine <MACHINE> reports only the rules about one device.')
    print('--waiver FILE ignores the error and warning lines that FILE lists.')


def main(argv=None):
    parser = argparse.ArgumentParser(
        description='Check doc/devices and machine/ against '
                    'doc/adding-a-device.md.')
    parser.add_argument('--root', default='.',
                        help='repository root (default: the current directory)')
    parser.add_argument('--verbose', action='store_true',
                        help='also list the info findings')
    parser.add_argument('--machine', action='append', metavar='MACHINE',
                        help='only report findings about this machine target '
                             '(repeatable)')
    parser.add_argument('--waiver', metavar='FILE',
                        help='ignore the errors and warnings listed in FILE, '
                             'one report line each (see check_device_docs.waiver)')
    parser.add_argument('--list-rules', action='store_true',
                        help='list every rule and its severity, then exit')
    args = parser.parse_args(argv)

    if args.list_rules:
        list_rules()
        return 0

    checker = Checker(args.root, args.machine)
    try:
        checker.run()
    except FileNotFoundError as exc:
        print('error: %s (run this from the repository root)' % exc,
              file=sys.stderr)
        return 2
    except UnknownMachine as exc:
        print('error: %s' % exc, file=sys.stderr)
        return 2
    if args.waiver:
        try:
            waived = load_waiver(args.waiver)
        except OSError as exc:
            print('error: cannot read waiver file %s: %s'
                  % (args.waiver, exc.strerror or exc), file=sys.stderr)
            return 2
        checker.apply_waivers(waived, args.waiver)
    errors, warnings = checker.report(args.verbose)
    if errors or warnings:
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
