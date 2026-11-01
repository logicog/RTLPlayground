#!/usr/bin/env python3
"""Interactive helper to add a new device to RTLPlayground.

Implements the workflow of doc/adding-a-device.md: it derives the device
names from the PCB silkscreen identifier, checks whether the PCB is already
documented, then either reuses an existing machine/pcb definition (symlink +
header) or creates a new board definition, device documentation and photos
directory for you.

Usage: add_device.py [--pcb <PCB_IDENTIFIER>] [--vendor <NAME>] [--model <MODEL>]
                     [--chipset RTL8372|RTL8373] [--dry-run]

pcb/vendor/model and chipset are prompted for when not given as arguments.

Run it from the repository root; run tools/add_device.md for the full
description of the naming rules and the step-by-step behaviour.
"""
import argparse
import os
import re
import shutil
import subprocess
import sys

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
    return to_machine_pcb(('%s %s' % (vendor, model)).upper())


def ask(prompt):
    try:
        return input(prompt).strip()
    except EOFError:
        print()
        sys.exit('add_device.py: input required, aborting.')


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


def collect_inputs(args):
    pcb_id = args.pcb or ask('PCB identifier (the silkscreen marking, e.g. '
                             '"S1100W-88T-1SX_SE V1.05"): ')
    vendor = args.vendor or ask('Vendor name: ')
    model = args.model or ask('Model number: ')
    while not pcb_id or not vendor or not model:
        if not pcb_id:
            pcb_id = ask('PCB identifier: ')
        if not vendor:
            vendor = ask('Vendor name: ')
        if not model:
            model = ask('Model number: ')
    chipset = normalize_chipset(args.chipset)
    while chipset is None:
        chipset = normalize_chipset(ask('Chipset (RTL8372 or RTL8373): '))
    return pcb_id.strip(), vendor.strip(), model.strip(), chipset


def normalize_chipset(value):
    """Accept RTL8372/RTL8373 (optional N suffix) and return the plain symbol."""
    if not value:
        return None
    m = re.fullmatch(r'(?:RTL)?837([23])N?', value.upper().replace(' ', ''))
    return 'RTL837' + m.group(1) if m else None


def prepare(path):
    """Return True if it is okay to create/overwrite path."""
    if not os.path.lexists(path):
        return True
    print('  ! %s already exists.' % path)
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
        print('Found machine definition: machine/pcb/%s' % candidates[0])
        return candidates[0]
    print('Several machine definitions match this PCB; pick the one that '
          'matches your hardware:')
    for i, name in enumerate(candidates, 1):
        print('  %d) machine/pcb/%s' % (i, name))
    while True:
        choice = ask('Selection [1-%d]: ' % len(candidates))
        if choice.isdigit() and 1 <= int(choice) <= len(candidates):
            return candidates[int(choice) - 1]
        if choice in candidates:
            return choice


def create_symlink_and_header(pcb_file, vendor, model, chipset):
    """Step 7: machine/<MACHINE>.c symlink and machine/<MACHINE>.h."""
    mdl = machine_name(vendor, model)
    created = []
    link = os.path.join(MACHINE_DIR, mdl + '.c')
    target = os.path.join('pcb', pcb_file)  # relative, as in the doc
    if os.path.islink(link) and os.readlink(link).replace(os.sep, '/') == target:
        print('Symlink machine/%s -> %s already in place.' % (mdl + '.c', target))
    elif prepare(link):
        if DRY_RUN:
            print('Would add symlink: machine/%s -> %s' % (mdl + '.c', target))
        else:
            if os.path.lexists(link):
                os.remove(link)
            os.symlink(target, link)
            print('Added symlink: machine/%s -> %s' % (mdl + '.c', target))
        created.append(link)
    header = os.path.join(MACHINE_DIR, mdl + '.h')
    if prepare(header):
        if DRY_RUN:
            print('Would add header:   machine/%s' % (mdl + '.h'))
        else:
            with open(header, 'w') as f:
                f.write(header_template(mdl, vendor, model, chipset))
            print('Added header:   machine/%s' % (mdl + '.h'))
        created.append(header)
    return created, mdl


def header_template(mdl, vendor, model, chipset):
    return (
        '#ifndef _MACHINE_%s_H_\n'
        '#define _MACHINE_%s_H_\n'
        '\n'
        '#define MACHINE_MODEL "%s %s"\n'
        '#define MACHINE_CHIPSET %s\n'
        '\n'
        '#endif\n'
    ) % (mdl, mdl, vendor, model, chipset)


def collect_images():
    """Steps 9+10: optional front/back PCB photos."""
    def ask_image(kind):
        while True:
            path = ask('%s PCB image file (enter to skip): ' % kind)
            if not path:
                return None
            if os.path.isfile(path):
                return path
            print('  ! %s is not a file; try again.' % path)
    front = ask_image('Front')
    back = ask_image('Back')
    return front, back


def create_new_device(pcb_id, vendor, model, chipset, front, back):
    """Steps 11+12 for an undocumented PCB: device doc, photos directory,
    board definition, then the same symlink and header as step 7."""
    pcb_doc_name = to_pcb_doc_name(pcb_id)
    machine_pcb = to_machine_pcb(pcb_doc_name)
    created = []

    photo_dir = os.path.join(DOC_PHOTOS, pcb_doc_name)
    if prepare(photo_dir):
        if DRY_RUN:
            print('Would create directory: %s' % photo_dir)
        else:
            os.makedirs(photo_dir, exist_ok=True)
        created.append(photo_dir)
    copied = {}
    for kind, src in (('front', front), ('back', back)):
        if not src:
            continue
        dst = os.path.join(photo_dir, os.path.basename(src))
        if not prepare(dst):
            continue
        if DRY_RUN:
            print('Would copy %s: %s -> %s' % (kind, src, dst))
        else:
            shutil.copyfile(src, dst)
            print('Copied %s: %s -> %s' % (kind, src, dst))
        copied[kind] = os.path.basename(src)

    doc = os.path.join(DOC_DEVICES, pcb_doc_name + '.md')
    if prepare(doc):
        if DRY_RUN:
            print('Would add device doc: %s' % doc)
        else:
            with open(doc, 'w') as f:
                f.write(device_doc_template(pcb_id, vendor, model, chipset,
                                            pcb_doc_name, copied))
            print('Added device doc: %s' % doc)
        created.append(doc)

    pcb_file = machine_pcb + '.c'
    pcb_path = os.path.join(PCB_DIR, pcb_file)
    if prepare(pcb_path):
        if DRY_RUN:
            print('Would add board definition: %s' % pcb_path)
        else:
            with open(pcb_path, 'w') as f:
                f.write(pcb_template(machine_pcb))
            print('Added board definition: %s' % pcb_path)
        created.append(pcb_path)

    link_files, mdl = create_symlink_and_header(pcb_file, vendor, model, chipset)
    created.extend(link_files)
    return created, mdl, pcb_doc_name, photo_dir


def device_doc_template(pcb_id, vendor, model, chipset, pcb_doc_name, photos):
    lines = [
        '# %s %s' % (vendor, model),
        '',
        '%s-based switch (TODO: one-line description of the board).' % chipset,
        '',
        '### PCB identifier',
        '',
        '`%s`' % pcb_id,
        '',
    ]
    if photos:
        lines += ['### Photos', '']
        for kind, label in (('front', 'PCB front'), ('back', 'PCB back')):
            if kind not in photos:
                continue
            lines += [
                '%s:' % label,
                '',
                '<img src="photos/%s/%s" width="600" />' % (pcb_doc_name, photos[kind]),
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
        '\tmake MACHINE=%s' % machine_name(vendor, model),
        '',
    ]
    return '\n'.join(lines)


def pcb_template(machine_pcb):
    return '''#include "machine_defs.h"

/* TODO: template for machine/pcb/%s.c - fill in the values for this board.
 * The easiest start is the closest existing machine/pcb/*.c with the same
 * number and type of Ethernet ports and SFP+ cages; adjust it from there.
 * Field meanings are documented in doc/adding-a-device.md. */
__code const struct machine machine = {
	MACHINE_COMMON_SETTING,
	.n_sfp = 0,															/* TODO: number of SFP+ cages */
	.log_to_phys_port = {0, 0, 0, 0, 0, 0, 0, 0, 0},		/* TODO: logical port -> physical front-panel port */
	.phys_to_log_port = {0, 0, 0, 0, 0, 0, 0, 0, 0},		/* TODO: the inverse, physical port 1 is index 0 */
	.is_sfp = {0, 0, 0, 0, 0, 0, 0, 0, 0},							/* TODO: 0 copper, 1 left SFP slot, 2 right SFP slot */
#if 0 /* enable and adjust if the board has SFP+ cages */
	.sfp_port[0].pin_detect = GPIO_NA,								/* TODO */
	.sfp_port[0].pin_los = GPIO_NA,
	.sfp_port[0].pin_tx_disable = GPIO_NA,
	.sfp_port[0].sds = 0,
	.sfp_port[0].i2c = I2CBUS( GPIO_NA, GPIO_NA ),
#endif
	.reset_pin = GPIO_NA,												/* TODO: reset button GPIO, GPIO_NA if unpopulated */
	.high_leds = { .mux = LED_27 | LED_28_SYS | LED_29, .enable = LED_28_SYS | LED_29 },	/* TODO */
	.port_led_set = {0, 0, 0, 0, 0, 0, 0, 0, 0},				/* TODO: LED set 0-3 per logical port */
	/* TODO: led_sets[4] condition groups, see doc/adding-a-device.md. */
};
''' % machine_pcb


def git_add_files(paths):
    """-g/--git-add: stage newly added files, never commits them."""
    if not GIT_ADD or not paths:
        return
    if DRY_RUN:
        print('Dry run: nothing was written, so nothing to git add.')
        return
    in_repo = subprocess.run(
        ['git', 'rev-parse', '--is-inside-work-tree'],
        stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
    if in_repo.returncode != 0:
        print('  ! Not a git repository; skipping git add.')
        return
    r = subprocess.run(['git', 'add', '--'] + list(paths))
    if r.returncode == 0:
        print('Staged with git add (not committed):')
        for path in paths:
            print('  %s' % path)
    else:
        print('  ! git add failed (exit %d); files remain unstaged.'
              % r.returncode)


def report(created, mdl, pcb_doc_name, doc_target, new_device=False, photo_dir=None):
    """Steps 8/12 printout and step 20: what was added and what to do next."""
    print()
    print('=== Summary ===%s' % (' (dry run)' if DRY_RUN else ''))
    if new_device:
        print('New PCB (%s): device doc, photos directory, board definition,'
              ' symlink and header %s.'
              % (pcb_doc_name, 'would be added' if DRY_RUN else 'added'))
        if photo_dir:
            print('  Photos directory: %s' % photo_dir)
    if created:
        print('Files that would be added:' if DRY_RUN else 'Added files:')
        for path in created:
            print('  %s' % path)
    if DRY_RUN:
        print('Dry run: no files were written.')
    print()
    print('Next steps:')
    print('  1. Fill in the TODOs in the new files (port map, LEDs, SFP pins).')
    if doc_target:
        print('  2. Update doc/devices/%s.md and doc/supported_devices.md.'
              % doc_target)
    print('  3. Build and test:  make MACHINE=%s CI=1' % mdl)
    print('  4. Run the CI checks:  make machine_check && make -C test')
    print('  5. Verify behaviour on hardware (port, stat, sfp, leds console'
          ' commands).')


def main():
    parser = argparse.ArgumentParser(
        description='Interactive helper to add a new device to RTLPlayground.')
    parser.add_argument('-p', '--pcb', metavar='PCB_IDENTIFIER',
                        help='PCB silkscreen identifier (prompted if missing)')
    parser.add_argument('-v', '--vendor', metavar='NAME',
                        help='vendor name (prompted if missing)')
    parser.add_argument('-m', '--model', metavar='MODEL',
                        help='model number (prompted if missing)')
    parser.add_argument('-c', '--chipset', metavar='CHIPSET',
                        help='RTL8372 or RTL8373 (prompted if missing)')
    parser.add_argument('-n', '--dry-run', action='store_true',
                        help='run the whole flow but do not write any files')
    parser.add_argument('-g', '--git-add', action='store_true',
                        help='stage the newly added files with git add'
                             ' (they are not committed)')
    args = parser.parse_args()

    global DRY_RUN, GIT_ADD
    DRY_RUN = args.dry_run
    GIT_ADD = args.git_add
    if DRY_RUN:
        print('DRY RUN: the flow is walked and every step reports what it'
              ' would do, but nothing is written to disk.')

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
        link_desc = 'machine/%s.c -> %s (symlink)' % (mdl,
                    os.readlink(link).replace(os.sep, '/'))
    else:
        link_desc = 'machine/%s.c (regular file)' % mdl
    m = re.search(r'^#define MACHINE_MODEL "(.*)"\s*$', open(header).read(),
                  re.M)
    return [
        '  %s' % link_desc,
        '  machine/%s.h%s' % (mdl,
                              ' (MACHINE_MODEL "%s")' % m.group(1) if m
                              else ''),
    ]


def run(pcb_id, vendor, model, chipset):
    pcb_doc_name = to_pcb_doc_name(pcb_id)
    machine_pcb = to_machine_pcb(pcb_doc_name)
    print()
    print('<PCB_IDENTIFIER> : %s' % pcb_id)
    print('<PCB_DOC_NAME>   : %s' % pcb_doc_name)
    print('<MACHINE_PCB>    : %s' % machine_pcb)

    mdl = machine_name(vendor, model)
    info = machine_already_supported(mdl)
    if info:
        print()
        print('=== Device already supported ===')
        print('machine/%s.c and machine/%s.h are both in place:' % (mdl, mdl))
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
            print('  doc/devices/%s' % name)
        print('Your input (%s) does not match the documented name'
              ' verbatim.' % pcb_doc_name)
        if confirm('Is your PCB identifier correct?'):
            print('Please file a bug report so the document name is'
                  ' corrected (it should be doc/devices/%s.md).'
                  % pcb_doc_name)
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
            sys.exit('No machine/pcb/%s*.c definition found even though the'
                     ' device is documented under doc/devices/%s - please'
                     ' file a bug report.' % (machine_pcb, doc_name))
        created, mdl = create_symlink_and_header(pcb_file, vendor, model, chipset)
        doc_target = key_matches[0] if key_matches else pcb_doc_name
        doc_target = doc_target[:-len('.md')]
        git_add_files(created)
        report(created, mdl, doc_target, doc_target)
        return

    # Steps 9-12: undocumented PCB -> create everything.
    front, back = collect_images()
    created, mdl, doc_target, photo_dir = create_new_device(
        pcb_id, vendor, model, chipset, front, back)
    git_add_files(created)
    report(created, mdl, doc_target, doc_target, new_device=True,
           photo_dir=photo_dir)


if __name__ == '__main__':
    main()
