#!/usr/bin/env python3
"""Interactive helper to add a new device to RTLPlayground.

Implements the workflow of doc/adding-a-device.md: it derives the device
names from the PCB silkscreen identifier, checks whether the PCB is already
documented, then either reuses an existing machine/pcb definition (symlink +
header) or creates a new board definition, device documentation and photos
directory for you.

Usage: add_device.py [--pcb <PCB_IDENTIFIER>] [--vendor <VENDOR>] [--model <MODEL>]
                     [--chipset RTL8372|RTL8373] [--dry-run] [--yes]

add_device.py [--selftest|-T] runs the embedded unittest suite for the
pcb identifier / vendor / model naming conversions (also: -v for verbose).

pcb/vendor/model and chipset are prompted for when not given as arguments.
Unless -y/--yes is given, the user is asked to confirm before anything is
written to disk; unless -y or -g/--git-add is given, that confirmation
also asks whether the files should be staged with git add (answered
before anything is written).

Run it from the repository root; run tools/add_device.md for the full
description of the naming rules and the step-by-step behaviour.
"""
import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile
import unittest

DOC_DEVICES = 'doc/devices'
DOC_PHOTOS = os.path.join(DOC_DEVICES, 'photos')
MACHINE_DIR = 'machine'
PCB_DIR = os.path.join(MACHINE_DIR, 'pcb')

# Set by the -n/--dry-run command line flag: the whole flow is walked
# (prompts, lookups, selection) and every step prints what it would do,
# but nothing is written to disk.
DRY_RUN = False

# Set by the -g/--git-add command line flag: stage the newly added files
# with git add at the end of the run (never commits).
GIT_ADD = False

# Set by the -y/--yes command line flag: skip the confirmation before
# writing anything to disk.
YES = False

_NAME_CHARS = re.compile(r'[^A-Za-z0-9]')


def to_pcb_doc_name(pcb_id):
    """<PCB_IDENTIFIER> -> <PCB_DOC_NAME>: spaces become '_'.

    Example: 'S1100W-88T-1SX_SE V1.05' -> 'S1100W-88T-1SX_SE_V1.05'
    """
    return pcb_id.replace(' ', '_')


def to_machine_pcb(name):
    """<PCB_DOC_NAME> -> <MACHINE_PCB>: every remaining punctuation
    character (hyphens, dots, ...) becomes '_'.

    Example: 'S1100W-88T-1SX_SE_V1.05' -> 'S1100W_88T_1SX_SE_V1_05'
    """
    return _NAME_CHARS.sub('_', name)


def machine_name(vendor, model):
    """<MACHINE> = <MACHINE_MODEL> in capitals with punctuation -> '_'."""
    return to_machine_pcb(f'{vendor} {model}'.upper())


class TestConversions(unittest.TestCase):
    """Self-test for the pcb identifier, vendor and model naming rules.

    Run with:  python3 add_device.py --selftest   (also: -T)
    or via the unittest discovery mechanism:  python3 -m unittest add_device
    """

    def test_to_pcb_doc_name_spaces_become_underscores(self):
        self.assertEqual(to_pcb_doc_name('S1100W-88T-1SX_SE V1.05'),
                         'S1100W-88T-1SX_SE_V1.05')

    def test_to_pcb_doc_name_two_spaces(self):
        self.assertEqual(to_pcb_doc_name('a b c'), 'a_b_c')

    def test_to_pcb_doc_name_no_spaces(self):
        self.assertEqual(to_pcb_doc_name('GT-ST018M'), 'GT-ST018M')

    def test_to_machine_pcb(self):
        self.assertEqual(to_machine_pcb('S1100W-88T-1SX_SE_V1.05'),
                         'S1100W_88T_1SX_SE_V1_05')

    def test_to_machine_pcb_only_letters_digits_and_underscores_kept(self):
        self.assertEqual(to_machine_pcb('aAzZ09_._-'), 'aAzZ09____')

    def test_to_machine_pcb_chain_of_both(self):
        pcb_id = 'S1100W-88T-1SX_SE V1.05'
        self.assertEqual(to_machine_pcb(to_pcb_doc_name(pcb_id)),
                         'S1100W_88T_1SX_SE_V1_05')

    def test_machine_name(self):
        self.assertEqual(machine_name('Goodtop', 'GT-ST018M'),
                         'GOODTOP_GT_ST018M')

    def test_machine_name_spaces_in_vendor_or_model(self):
        self.assertEqual(machine_name('Steamemo', 'IG204 V1'),
                         'STEAMEMO_IG204_V1')

    def test_machine_name_lowercase(self):
        self.assertEqual(machine_name('vendor', 'model-1'), 'VENDOR_MODEL_1')

    def test_normalize_ws(self):
        self.assertEqual(normalize_ws('S1100W  88T-1SX_SE\tV1.05'),
                         'S1100W 88T-1SX_SE V1.05')

    def test_normalize_ws_strips(self):
        self.assertEqual(normalize_ws('  foo  '), 'foo')

    def test_normalize_chipset(self):
        for value, expected in (
                ('2', 'RTL8372'), ('3', 'RTL8373'),
                ('2N', 'RTL8372'), ('3N', 'RTL8373'),
                ('8373', 'RTL8373'), ('8 372', 'RTL8372'),
                ('RTL8372', 'RTL8372'), ('rtl8373', 'RTL8373'),
                ('RTL 8372', 'RTL8372')):
            self.assertEqual(normalize_chipset(value), expected, value)

    def test_normalize_chipset_invalid_returns_none(self):
        for value in ('', None, '1', '4', '8374', '8372N1', 'RTL8372X',
                      'abc', 'rtl 3n'):
            self.assertIsNone(normalize_chipset(value), repr(value))


def ask(prompt):
    try:
        return input(prompt).strip()
    except EOFError:
        print()
        sys.exit('add_device.py: input required, aborting.')


def confirm_write():
    """Ask once before anything is written to disk, unless -y/--yes was
    given (or the whole flow is a dry run).

    Unless -y or -g/--git-add was given, the same pre-write gate also asks
    whether the files should be staged with git add at the end of the run.
    """
    if DRY_RUN or YES:
        return
    if not confirm('Write the files to disk?'):
        sys.exit('add_device.py: confirmation declined, aborting without'
                 ' writing anything.')
    global GIT_ADD
    if not GIT_ADD:
        GIT_ADD = confirm('Stage the files with git add as well?')


def confirm(question, default=None):
    suffix = ' [y/n]: '
    while True:
        answer = ask(question + suffix).lower()
        if answer in ('y', 'yes'):
            return True
        if answer in ('n', 'no'):
            return False
        if default is not None:
            return default


def normalize_ws(value):
    """Collapse every run of white space into a single space and trim it.

    Example: 'S1100W  88T-1SX_SE\tV1.05' -> 'S1100W 88T-1SX_SE V1.05'
    """
    return re.sub(r'\s+', ' ', value).strip()


def collect_inputs(args):
    pcb_id = args.pcb or ask('PCB identifier (the silkscreen marking, e.g. '
                             '"S1100W-88T-1SX_SE V1.05"): ')
    vendor = args.vendor or ask('Vendor name: ')
    model = args.model or ask('Model number: ')
    pcb_id, vendor, model = normalize_ws(pcb_id), normalize_ws(vendor),\
        normalize_ws(model)
    while not pcb_id or not vendor or not model:
        if not pcb_id:
            pcb_id = normalize_ws(ask('PCB identifier: '))
        if not vendor:
            vendor = normalize_ws(ask('Vendor name: '))
        if not model:
            model = normalize_ws(ask('Model number: '))
    chipset = normalize_chipset(args.chipset)
    while chipset is None:
        chipset = normalize_chipset(ask('Chipset (RTL8372 or RTL8373, or'
                                        ' 2/3 for short): '))
    return pcb_id, vendor, model, chipset


def normalize_chipset(value):
    """Accept RTL8372/RTL8373 and the short forms 2/3 (optional N suffix)
    and return the plain symbol."""
    if not value:
        return None
    m = re.fullmatch(r'(?:(?:RTL)?837)?([23])N?', value.upper().replace(' ', ''))
    return 'RTL837' + m.group(1) if m else None


def prepare(path):
    """Return True if it is okay to create/overwrite path."""
    if not os.path.lexists(path):
        return True
    print(f'  ! {path} already exists.')
    return confirm('  Replace it?')


def find_device_doc(pcb_id):
    """Step 3+4: map every doc/devices/*.md name to <MACHINE_PCB> format and
    look up the key for the user's PCB identifier.

    Returns (doc_name or None, all docs whose key matched).
    """
    pcb_doc_name = to_pcb_doc_name(pcb_id)
    key = to_machine_pcb(pcb_doc_name)
    key_matches = []
    for name in sorted(os.listdir(DOC_DEVICES)):
        if not name.endswith('.md'):
            continue
        stem = name[:-len('.md')]
        if to_machine_pcb(stem) == key:
            key_matches.append(name)
    for name in key_matches:
        if name == pcb_doc_name + '.md':
            return name, key_matches
    return None, key_matches


def select_pcb_file(machine_pcb):
    """Step 6: pick the machine/pcb/<MACHINE_PCB>*.c (or <MACHINE_PCB_EXT>.c)
    file that matches the hardware."""
    candidates = []
    for name in sorted(os.listdir(PCB_DIR)):
        if not name.endswith('.c'):
            continue
        if name == machine_pcb + '_init.c':
            continue
        if name.startswith(machine_pcb):
            candidates.append(name)
    if not candidates:
        return None
    if len(candidates) == 1:
        print(f'Found machine definition: machine/pcb/{candidates[0]}')
        return candidates[0]
    print('Several machine definitions match this PCB; pick the one that '
          'matches your hardware:')
    for i, name in enumerate(candidates, 1):
        print(f'  {i}) machine/pcb/{name}')
    while True:
        choice = ask(f'Selection [1-{len(candidates)}]: ')
        if choice.isdigit() and 1 <= int(choice) <= len(candidates):
            return candidates[int(choice) - 1]
        if choice in candidates:
            return choice


def create_symlink_and_header(machine_pcb, pcb_file, vendor, model, chipset):
    """Step 7: machine/<MACHINE>.c symlink (plus the matching
    machine/<MACHINE>_init.c symlink when a machine/pcb *_init.c exists)
    and machine/<MACHINE>.h."""
    mdl = machine_name(vendor, model)
    created = []
    link = os.path.join(MACHINE_DIR, mdl + '.c')
    target = os.path.join('pcb', pcb_file)  # relative, as in the doc
    if os.path.islink(link) and os.readlink(link).replace(os.sep, '/') == target:
        print(f'Symlink machine/{mdl}.c -> {target} already in place.')
    elif prepare(link):
        if DRY_RUN:
            print(f'Would add symlink: machine/{mdl}.c -> {target}')
        else:
            if os.path.lexists(link):
                os.remove(link)
            os.symlink(target, link)
            print(f'Added symlink: machine/{mdl}.c -> {target}')
        created.append(link)
    # machine/<MACHINE>_init.c: prefer the _init.c belonging to the selected
    # definition (<MACHINE_PCB_EXT>_init.c), fall back to the plain
    # <MACHINE_PCB>_init.c; if neither exists, no link is added.
    init_target = None
    for name in (pcb_file[:-len('.c')] + '_init.c', machine_pcb + '_init.c'):
        if os.path.lexists(os.path.join(PCB_DIR, name)):
            init_target = os.path.join('pcb', name)
            break
    init_link = os.path.join(MACHINE_DIR, mdl + '_init.c')
    if init_target is None:
        print(f'No machine/pcb/{pcb_file[:-len(".c")]}_init.c;'
              f' not adding machine/{mdl}_init.c.')
    elif os.path.islink(init_link) and \
            os.readlink(init_link).replace(os.sep, '/') == init_target:
        print(f'Symlink machine/{mdl}_init.c -> {init_target} already in place.')
    elif prepare(init_link):
        if DRY_RUN:
            print(f'Would add symlink: machine/{mdl}_init.c -> {init_target}')
        else:
            if os.path.lexists(init_link):
                os.remove(init_link)
            os.symlink(init_target, init_link)
            print(f'Added symlink: machine/{mdl}_init.c -> {init_target}')
        created.append(init_link)
    header = os.path.join(MACHINE_DIR, mdl + '.h')
    if prepare(header):
        if DRY_RUN:
            print(f'Would add header:   machine/{mdl}.h')
        else:
            with open(header, 'w') as f:
                f.write(header_template(mdl, vendor, model, chipset))
            print(f'Added header:   machine/{mdl}.h')
        created.append(header)
    return created, mdl


def header_template(mdl, vendor, model, chipset):
    return f'''#ifndef _MACHINE_{mdl}_H_
#define _MACHINE_{mdl}_H_

#define MACHINE_MODEL "{vendor} {model}"
#define MACHINE_CHIPSET {chipset}

#endif
'''


# Canonical photo file names inside doc/devices/photos/<PCB_DOC_NAME>/,
# suggested whenever the user skips one of the prompts.
SUGGESTED_PHOTO_NAMES = {
    'front': 'front.jpg',
    'top': 'pcb_top.jpg',
    'bottom': 'pcb_bottom.jpg',
}


def collect_images():
    """Steps 9+10: optional front/top/bottom PCB photos.

    Returns {kind: source path} for the photos the user provided; every
    skipped prompt reports the name the photo should use once it is added
    later to doc/devices/photos/<PCB_DOC_NAME>/.
    """
    photos = {}
    for kind, label in (('front', 'Front panel'), ('top', 'PCB top'),
                        ('bottom', 'PCB bottom')):
        while True:
            path = ask(f'{label} image file (enter to skip): ')
            if not path:
                print(f'  Suggested file name for the {label.lower()} when'
                      f' added later: {SUGGESTED_PHOTO_NAMES[kind]}')
                break
            if os.path.isfile(path):
                photos[kind] = path
                break
            print(f'  ! {path} is not a file; try again.')
    return photos


def create_new_device(pcb_id, vendor, model, chipset, photos):
    """Steps 11+12 for an undocumented PCB: device doc, photos directory,
    board definition, then the same symlink and header as step 7.

    photos is the {kind: source path} mapping returned by collect_images().
    """
    pcb_doc_name = to_pcb_doc_name(pcb_id)
    machine_pcb = to_machine_pcb(pcb_doc_name)
    created = []

    photo_dir = os.path.join(DOC_PHOTOS, pcb_doc_name)
    if prepare(photo_dir):
        if DRY_RUN:
            print(f'Would create directory: {photo_dir}')
        else:
            os.makedirs(photo_dir, exist_ok=True)
        created.append(photo_dir)
    copied = {}
    for kind in ('front', 'top', 'bottom'):
        src = photos.get(kind)
        if not src:
            continue
        dst = os.path.join(photo_dir, os.path.basename(src))
        if not prepare(dst):
            continue
        if DRY_RUN:
            print(f'Would copy {kind}: {src} -> {dst}')
        else:
            shutil.copyfile(src, dst)
            print(f'Copied {kind}: {src} -> {dst}')
        copied[kind] = os.path.basename(src)

    doc = os.path.join(DOC_DEVICES, pcb_doc_name + '.md')
    if prepare(doc):
        if DRY_RUN:
            print(f'Would add device doc: {doc}')
        else:
            with open(doc, 'w') as f:
                f.write(device_doc_template(pcb_id, vendor, model, chipset,
                                            pcb_doc_name, copied))
            print(f'Added device doc: {doc}')
        created.append(doc)

    pcb_file = machine_pcb + '.c'
    pcb_path = os.path.join(PCB_DIR, pcb_file)
    if prepare(pcb_path):
        if DRY_RUN:
            print(f'Would add board definition: {pcb_path}')
        else:
            with open(pcb_path, 'w') as f:
                f.write(pcb_template(machine_pcb))
            print(f'Added board definition: {pcb_path}')
        created.append(pcb_path)

    link_files, mdl = create_symlink_and_header(
        machine_pcb, pcb_file, vendor, model, chipset)
    created.extend(link_files)
    return created, mdl, pcb_doc_name, photo_dir


def device_doc_template(pcb_id, vendor, model, chipset, pcb_doc_name, photos):
    lines = [
        f'# {vendor} {model}',
        '',
        f'{chipset}-based switch (TODO: one-line description of the board).',
        '',
        '### PCB identifier',
        '',
        f'`{pcb_id}`',
        '',
    ]
    if photos:
        lines += ['### Photos', '']
        for kind, label in (('front', 'Front panel'), ('top', 'PCB top'),
                            ('bottom', 'PCB bottom')):
            if kind not in photos:
                continue
            lines += [
                f'{label}:',
                '',
                f'<img src="photos/{pcb_doc_name}/{photos[kind]}" width="600" />',
                '',
            ]
    lines += [
        '<!-- TODO: complete this document following the other files in',
        'doc/devices/ (label specifications, what works, port layout, LED',
        'configuration, SFP GPIO assignments, ...). Register the device in',
        'doc/supported_devices.md. -->',
        '',
        '### Machine target',
        '',
        f'\tmake MACHINE={machine_name(vendor, model)}',
        '',
    ]
    return '\n'.join(lines)


def pcb_template(machine_pcb):
    return f'''#include "machine_defs.h"

/* TODO: template for machine/pcb/{machine_pcb}.c - fill in the values for this board.
 * The easiest start is the closest existing machine/pcb/*.c with the same
 * number and type of Ethernet ports and SFP+ cages; adjust it from there.
 * Field meanings are documented in doc/adding-a-device.md. */
__code const struct machine machine = {{
\tMACHINE_COMMON_SETTING,
\t.n_sfp = 0,\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t/* TODO: number of SFP+ cages */
\t.log_to_phys_port = {{0, 0, 0, 0, 0, 0, 0, 0, 0}},\t\t/* TODO: logical port -> physical front-panel port */
\t.phys_to_log_port = {{0, 0, 0, 0, 0, 0, 0, 0, 0}},\t\t/* TODO: the inverse, physical port 1 is index 0 */
\t.is_sfp = {{0, 0, 0, 0, 0, 0, 0, 0, 0}},\t\t\t\t\t\t\t/* TODO: 0 copper, 1 left SFP slot, 2 right SFP slot */
#if 0 /* enable and adjust if the board has SFP+ cages */
\t.sfp_port[0].pin_detect = GPIO_NA,\t\t\t\t\t\t\t\t/* TODO */
\t.sfp_port[0].pin_los = GPIO_NA,
\t.sfp_port[0].pin_tx_disable = GPIO_NA,
\t.sfp_port[0].sds = 0,
\t.sfp_port[0].i2c = I2CBUS( GPIO_NA, GPIO_NA ),
#endif
\t.reset_pin = GPIO_NA,\t\t\t\t\t\t\t\t\t\t\t\t/* TODO: reset button GPIO, GPIO_NA if unpopulated */
\t.high_leds = {{ .mux = LED_27 | LED_28_SYS | LED_29, .enable = LED_28_SYS | LED_29 }},\t/* TODO */
\t.port_led_set = {{0, 0, 0, 0, 0, 0, 0, 0, 0}},\t\t\t\t/* TODO: LED set 0-3 per logical port */
\t/* TODO: led_sets[4] condition groups, see doc/adding-a-device.md. */
}};
'''


def git_add_files(paths):
    """-g/--git-add: stage newly added files, never commits them."""
    if not GIT_ADD or not paths:
        return
    if DRY_RUN:
        print('Dry run: nothing was written, so nothing to git add.')
        return
    in_repo = subprocess.run(
        ['git', 'rev-parse', '--is-inside-work-tree'],
        stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, check=False)
    if in_repo.returncode != 0:
        print('  ! Not a git repository; skipping git add.')
        return
    r = subprocess.run(['git', 'add', '--'] + list(paths), check=False)
    if r.returncode == 0:
        print('Staged with git add (not committed):')
        for path in paths:
            print(f'  {path}')
    else:
        print(f'  ! git add failed (exit {r.returncode});'
              ' files remain unstaged.')


def report(created, mdl, pcb_doc_name, doc_target, new_device=False, photo_dir=None):
    """Steps 8/12 printout and step 20: what was added and what to do next."""
    print()
    print(f'=== Summary ==={" (dry run)" if DRY_RUN else ""}')
    if new_device:
        print(f'New PCB ({pcb_doc_name}): device doc, photos directory, board'
              f' definition, symlink and header'
              f' {"would be added" if DRY_RUN else "added"}.')
        if photo_dir:
            print(f'  Photos directory: {photo_dir}')
    if created:
        print('Files that would be added:' if DRY_RUN else 'Added files:')
        for path in created:
            print(f'  {path}')
    if DRY_RUN:
        print('Dry run: no files were written.')
    print()
    print('Next steps:')
    print('  1. Fill in the TODOs in the new files (port map, LEDs, SFP pins).')
    if doc_target:
        print(f'  2. Update doc/devices/{doc_target}.md and'
              ' doc/supported_devices.md.')
    print(f'  3. Build and test:  make MACHINE={mdl} CI=1')
    print('  4. Run the CI checks:  make machine_check && make -C test')
    print('  5. Verify behaviour on hardware (port, stat, sfp, leds console'
          ' commands).')


def check_symlink_support():
    """Abort with a helpful message if creating a symlink is not possible.

    The tool builds machine/<MACHINE>.c as a symlink into machine/pcb/, which
    fails on Windows without Developer Mode or administrator rights (winerror
    1314). A harmless probe in a temporary directory detects this up front so
    the tool exits before touching anything instead of failing midway.
    """
    try:
        with tempfile.TemporaryDirectory(prefix='add_device_probe_') as tmp:
            real = os.path.join(tmp, 'probe_target')
            with open(real, 'w') as f:
                f.write('probe')
            link = os.path.join(tmp, 'probe_link')
            os.symlink(real, link)
    except (OSError, NotImplementedError) as exc:
        print()
        print('WARNING: this tool needs to create symlinks'
              ' (machine/<MACHINE>.c -> machine/pcb/...), but creating a'
              ' symlink failed on this host:')
        print(f'  {exc}')
        print('On Windows, symlink creation requires Developer Mode or an'
              ' administrator shell:')
        print('  - enable Developer Mode (Settings > Update & Security >'
              ' For developers),')
        print('  - or start the terminal as administrator and run the tool'
              ' again.')
        sys.exit('Exiting without making any changes.')


def main():
    parser = argparse.ArgumentParser(
        description='Interactive helper to add a new device to RTLPlayground.')
    parser.add_argument('-p', '--pcb', metavar='PCB_IDENTIFIER',
                        help='PCB silkscreen identifier (prompted if missing)')
    parser.add_argument('-v', '--vendor', metavar='VENDOR',
                        help='vendor name (prompted if missing)')
    parser.add_argument('-m', '--model', metavar='MODEL',
                        help='model number (prompted if missing)')
    parser.add_argument('-c', '--chipset', metavar='CHIPSET',
                        help='RTL8372 or RTL8373 (2/3 also accepted, '
                             'prompted if missing)')
    parser.add_argument('-y', '--yes', action='store_true',
                        help='do not ask for confirmation before writing'
                             ' files to disk')
    parser.add_argument('-n', '--dry-run', action='store_true',
                        help='run the whole flow but do not write any files')
    parser.add_argument('-g', '--git-add', action='store_true',
                        help='stage the newly added files with git add'
                             ' (they are not committed); without -g and'
                             ' without -y this is asked before the files'
                             ' are written to disk')
    args = parser.parse_args()

    global DRY_RUN, GIT_ADD, YES
    DRY_RUN = args.dry_run
    GIT_ADD = args.git_add
    YES = args.yes
    if DRY_RUN:
        print('DRY RUN: the flow is walked and every step reports what it'
              ' would do, but nothing is written to disk.')
    else:
        check_symlink_support()

    os.chdir(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

    pcb_id, vendor, model, chipset = collect_inputs(args)
    run(pcb_id, vendor, model, chipset)


def machine_already_supported(mdl):
    """Return the info lines when machine/<MACHINE>.c AND .h already exist,
    otherwise None."""
    link = os.path.join(MACHINE_DIR, mdl + '.c')
    header = os.path.join(MACHINE_DIR, mdl + '.h')
    if not (os.path.lexists(link) and os.path.lexists(header)):
        return None
    if os.path.islink(link):
        link_desc = (f'machine/{mdl}.c -> '
                     f'{os.readlink(link).replace(os.sep, "/")} (symlink)')
    else:
        link_desc = f'machine/{mdl}.c (regular file)'
    with open(header, 'r') as f:
        header_text = f.read()
    m = re.search(r'^#define MACHINE_MODEL "(.*)"\s*$', header_text, re.MULTILINE)
    model_suffix = f' (MACHINE_MODEL "{m.group(1)}")' if m else ''
    return [
        f'  {link_desc}',
        f'  machine/{mdl}.h{model_suffix}',
    ]


def run(pcb_id, vendor, model, chipset):
    pcb_doc_name = to_pcb_doc_name(pcb_id)
    machine_pcb = to_machine_pcb(pcb_doc_name)
    print()
    print(f'<PCB_IDENTIFIER> : {pcb_id}')
    print(f'<PCB_DOC_NAME>   : {pcb_doc_name}')
    print(f'<MACHINE_PCB>    : {machine_pcb}')

    mdl = machine_name(vendor, model)
    info = machine_already_supported(mdl)
    if info:
        print()
        print('=== Device already supported ===')
        print(f'machine/{mdl}.c and machine/{mdl}.h are both in place:')
        for line in info:
            print(line)
        print('Nothing to add. Remove these files (or adjust them by hand) to'
              ' run the helper again, or use -n to preview the plan without'
              ' writing.')
        sys.exit(0)

    doc_name, key_matches = find_device_doc(pcb_id)
    if doc_name is None and key_matches:
        # Key matched, but the <PCB_DOC_NAME> does not exist verbatim:
        # a similarly named document exists.
        print()
        print('A device doc with a matching name exists:')
        for name in key_matches:
            print(f'  doc/devices/{name}')
        print(f'Your input ({pcb_doc_name}) does not match the documented'
              ' name verbatim.')
        if confirm('Is your PCB identifier correct?'):
            print(f'Please file a bug report so the document name is'
                  f' corrected (it should be doc/devices/{pcb_doc_name}.md).')
            print('Continuing with the existing definition.')
            doc_name = key_matches[0]
        else:
            # Restart the match with the corrected identifier.
            print('Please retype the PCB identifier.')
            run(ask('PCB identifier: '), vendor, model, chipset)
            return
    if doc_name is not None:
        # Steps 4-8: documented PCB -> reuse the existing definition.
        pcb_file = select_pcb_file(machine_pcb)
        print()
        if pcb_file is None:
            sys.exit(f'No machine/pcb/{machine_pcb}*.c definition found even'
                     f' though the device is documented under'
                     f' doc/devices/{doc_name} - please file a bug report.')
        confirm_write()
        created, mdl = create_symlink_and_header(
            machine_pcb, pcb_file, vendor, model, chipset)
        doc_target = key_matches[0] if key_matches else pcb_doc_name
        doc_target = doc_target[:-len('.md')]
        git_add_files(created)
        report(created, mdl, doc_target, doc_target)
        return

    # Steps 9-12: undocumented PCB -> create everything.
    photos = collect_images()
    confirm_write()
    created, mdl, doc_target, photo_dir = create_new_device(
        pcb_id, vendor, model, chipset, photos)
    git_add_files(created)
    report(created, mdl, doc_target, doc_target, new_device=True,
           photo_dir=photo_dir)


SELFTEST_FLAGS = ('-T', '--selftest')


if __name__ == '__main__':
    if any(arg in SELFTEST_FLAGS for arg in sys.argv[1:]):
        # Embedded unittest suite: run it and exit instead of main().
        # Remaining arguments are passed to unittest (e.g. -v).
        unittest.main(argv=[arg for arg in sys.argv
                            if arg not in SELFTEST_FLAGS])
    else:
        main()
