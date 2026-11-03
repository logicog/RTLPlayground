#!/usr/bin/env python3
"""Unit tests for tools/add_device.py (the interactive add-device helper).

Pure-stdlib unittest, no third-party dependencies. Covers the name
derivation rules of doc/adding-a-device.md, the doc-devices lookup, the
machine/pcb selection, and the scaffolding of a new device (doc, photos,
board definition, symlink, header) - everything runs against a temporary
repository tree, so the real doc/ and machine/ directories are never touched.

Runs via test/Makefile (make) or directly:  python3 test/test_add_device.py
"""
import argparse
import contextlib
import io
import os
import subprocess
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', 'tools'))
sys.path.insert(0, HERE)

import add_device  # noqa: E402


class AddDeviceNames(unittest.TestCase):
    """The <PCB_DOC_NAME> / <MACHINE_PCB> / <MACHINE> derivation rules."""

    def test_pcb_doc_name_replaces_spaces_only(self):
        self.assertEqual(add_device.to_pcb_doc_name('S1100W-88T-1SX_SE V1.05'),
                         'S1100W-88T-1SX_SE_V1.05')
        self.assertEqual(add_device.to_pcb_doc_name('EXAMPLE-BOARD V1.0'),
                         'EXAMPLE-BOARD_V1.0')

    def test_machine_pcb_converts_every_punctuation(self):
        self.assertEqual(add_device.to_machine_pcb('S1100W-88T-1SX_SE_V1.05'),
                         'S1100W_88T_1SX_SE_V1_05')
        self.assertEqual(add_device.to_machine_pcb('fg 8 gt 1sx'),
                         'fg_8_gt_1sx')

    def test_machine_name_is_upper_with_punctuation_converted(self):
        self.assertEqual(add_device.machine_name('CSCI', 'SW-425A'),
                         'CSCI_SW_425A')
        self.assertEqual(add_device.machine_name('Hasivo', 'S1100WP-8GT-1SX-SE'),
                         'HASIVO_S1100WP_8GT_1SX_SE')

    def test_full_identifier_chain(self):
        # The flow derives <PCB_DOC_NAME> and <MACHINE_PCB> from one
        # identifier and <MACHINE> from vendor and model; all three
        # conversions must compose the way run()/collect_inputs() use them.
        for pcb_id in ('S1100W-88T-1SX_SE V1.05',
                       'PCB-SWTG024AS-A-V2.0.1_19650',
                       'FG-4GT-2SX V2.0'):
            doc_name = add_device.to_pcb_doc_name(pcb_id)
            machine_pcb = add_device.to_machine_pcb(doc_name)
            # Machine PCB must be safe as a C identifier-ish name: only
            # letters, digits and underscores, no spaces or dots.
            self.assertRegex(machine_pcb, r'^[A-Za-z0-9_]+$')
            if ' ' in pcb_id:
                self.assertIn('_V', machine_pcb)
                self.assertNotIn('.', machine_pcb)

        # Real repo pairs, checked against both steps at once.
        self.assertEqual(
            add_device.to_machine_pcb(
                add_device.to_pcb_doc_name('S1100W-88T-1SX_SE V1.05')),
            'S1100W_88T_1SX_SE_V1_05')
        self.assertEqual(
            add_device.to_machine_pcb(
                add_device.to_pcb_doc_name('FG-4GT-2SX V2.0')),
            'FG_4GT_2SX_V2_0')

    def test_machine_name_edges(self):
        # Multiple spaces collapse only per word boundary gaps; each
        # separator run becomes one or more underscores - but never
        # uppercase/lowercase surprises: everything is uppercased first.
        self.assertEqual(add_device.machine_name('hasivo', 's1100w'),
                         'HASIVO_S1100W')
        self.assertEqual(add_device.machine_name(' Test ', 'A-B '),
                         '_TEST__A_B_')
        self.assertEqual(add_device.machine_name('', ''), '_')
        # Result always matches the same safe-name shape.
        self.assertRegex(add_device.machine_name('Ven', 'Mod X1.5'),
                         r'^[A-Z0-9_]+$')

    def test_normalize_chipset(self):
        for raw, expected in [('RTL8373', 'RTL8373'), ('rtl8372', 'RTL8372'),
                              ('8372', 'RTL8372'), ('RTL8373N', 'RTL8373'),
                              (' rtl8373 ', 'RTL8373'), ('', None),
                              ('2', 'RTL8372'), ('3', 'RTL8373'),
                              (' 2 ', 'RTL8372'), ('rtl8372n', 'RTL8372'),
                              ('RTL8375', None), ('garbage', None)]:
            self.assertEqual(add_device.normalize_chipset(raw), expected,
                             'input %r' % raw)

    def test_normalize_ws_collapses_whitespace(self):
        self.assertEqual(add_device.normalize_ws('A  B \t C\nD'), 'A B C D')
        self.assertEqual(add_device.normalize_ws('  pad  '), 'pad')
        self.assertEqual(add_device.normalize_ws('S1100W-88T-1SX_SE   V1.05'),
                         'S1100W-88T-1SX_SE V1.05')

    def test_header_template(self):
        text = add_device.header_template('HASIVO_S1100WP_8GT_1SX_SE',
                                          'Hasivo', 'S1100WP-8GT-1SX-SE',
                                          'RTL8373')
        self.assertIn('#ifndef _MACHINE_HASIVO_S1100WP_8GT_1SX_SE_H_', text)
        self.assertIn('#define MACHINE_MODEL "Hasivo S1100WP-8GT-1SX-SE"', text)
        self.assertIn('#define MACHINE_CHIPSET RTL8373', text)
        self.assertIn('#endif', text)


class AddDeviceTestBase(unittest.TestCase):
    """Runs each test inside a temporary repository-shaped tree."""

    def setUp(self):
        self.old_cwd = os.getcwd()
        self.repo = tempfile.mkdtemp(prefix='adddev-test-')
        os.makedirs(os.path.join(self.repo, 'doc', 'devices', 'photos'))
        os.makedirs(os.path.join(self.repo, 'machine', 'pcb'))
        os.chdir(self.repo)
        self.addCleanup(self._teardown)

    def _teardown(self):
        os.chdir(self.old_cwd)
        import shutil
        shutil.rmtree(self.repo, ignore_errors=True)

    @staticmethod
    def _make(filename, content=b''):
        with open(filename, 'wb') as f:
            f.write(content)
        return filename


@contextlib.contextmanager
def stdin_feed(lines):
    """Feed answers to all input() calls of the block, then EOF."""
    old = sys.stdin
    sys.stdin = io.StringIO('\n'.join(lines) + '\n')
    try:
        yield
    finally:
        sys.stdin = old


@contextlib.contextmanager
def captured_stdout():
    with contextlib.redirect_stdout(io.StringIO()) as out:
        yield out


def read(path):
    with open(path, 'r') as f:
        return f.read()


class DocLookupTest(AddDeviceTestBase):
    """Step 3/4: doc/devices/*.md names as <MACHINE_PCB> keys."""

    def _add_docs(self, *names):
        for name in names:
            self._make(os.path.join('doc', 'devices', name + '.md'), b'-\n')

    def test_exact_match(self):
        self._add_docs('FG-8GT-1SX', 'S1100W-8GT-1SX_SE-V1.05')
        doc, matches = add_device.find_device_doc('FG-8GT-1SX')
        self.assertEqual(doc, 'FG-8GT-1SX.md')
        self.assertEqual(matches, ['FG-8GT-1SX.md'])

    def test_key_matches_but_verbatim_name_differs(self):
        # Same characters, different punctuation: key matches, doc name not.
        self._add_docs('FG-8GT-1SX')
        doc, matches = add_device.find_device_doc('FG_8GT_1SX')
        self.assertIsNone(doc)
        self.assertEqual(matches, ['FG-8GT-1SX.md'])

    def test_different_content_is_no_match(self):
        self._add_docs('FG-8GT-1SX')
        doc, matches = add_device.find_device_doc('S1100W-88T-1SX_SE V1.05')
        self.assertIsNone(doc)
        self.assertEqual(matches, [])

    def test_documented_name_mismatch_by_punctuation(self):
        self._add_docs('S1100W-8GT-1SX_SE-V1.05')
        doc, matches = add_device.find_device_doc('S1100W_8GT_1SX_SE V1.05')
        self.assertIsNone(doc)
        self.assertEqual(matches, ['S1100W-8GT-1SX_SE-V1.05.md'])


class MachineSelectionTest(AddDeviceTestBase):
    """Step 6: picking among machine/pcb/<MACHINE_PCB>*.c files."""

    def _add_pcb(self, *names):
        for name in names:
            self._make(os.path.join('machine', 'pcb', name + '.c'), b'x\n')

    def test_single_match_selected_without_prompt(self):
        self._add_pcb('FG_8GT_1SX', 'FG_8GT_1SX_init')
        with captured_stdout():
            self.assertEqual(add_device.select_pcb_file('FG_8GT_1SX'),
                             'FG_8GT_1SX.c')

    def test_init_files_never_listed(self):
        self._add_pcb('FG_8GT_1SX_init')
        with captured_stdout():
            self.assertIsNone(add_device.select_pcb_file('FG_8GT_1SX'))

    def test_no_match_at_all(self):
        with captured_stdout():
            self.assertIsNone(add_device.select_pcb_file('FG_8GT_1SX'))

    def test_multiple_matches_prompted_and_ext_variants_included(self):
        self._add_pcb('PCB_SWTG024AS_A_V2_0_1_19650',
                      'PCB_SWTG024AS_A_V2_0_1_19650__1S1C',
                      'PCB_SWTG024AS_A_V2_0_1_19650__UNMANAGED',
                      'PCB_SWTG024AS_A_V2_0_1_19650_init')
        base = 'PCB_SWTG024AS_A_V2_0_1_19650'
        with stdin_feed(['2']), captured_stdout() as out:
            self.assertEqual(add_device.select_pcb_file(base),
                             base + '__1S1C.c')
        self.assertIn('__UNMANAGED', out.getvalue())
        self.assertNotIn('_init.c', out.getvalue())

    def test_selection_rejects_out_of_range_then_accepts_number(self):
        self._add_pcb('X', 'X__POE')
        with stdin_feed(['9', 'x', '', '2']), captured_stdout():
            self.assertEqual(add_device.select_pcb_file('X'), 'X__POE.c')

    def test_selection_accepts_filename_as_answer(self):
        self._add_pcb('X', 'X__POE')
        with stdin_feed(['X__POE.c']), captured_stdout():
            self.assertEqual(add_device.select_pcb_file('X'), 'X__POE.c')


class OverwriteGuardTest(AddDeviceTestBase):
    """Existing files are never replaced without confirmation."""

    def test_decline_leaves_existing_file_alone(self):
        self._make('somefile', b'original\n')
        with stdin_feed(['n']), captured_stdout():
            self.assertFalse(add_device.prepare('somefile'))
        self.assertEqual(read('somefile'), 'original\n')

    def test_confirm_allows_replacement(self):
        with stdin_feed(['y']), captured_stdout():
            self.assertTrue(add_device.prepare('somefile'))

    def test_nonexistent_path_is_ready(self):
        self.assertTrue(add_device.prepare('nonexistent'))


class SymlinkAndHeaderTest(AddDeviceTestBase):
    """Step 7: machine/<MACHINE>.c symlink and machine/<MACHINE>.h."""

    def test_creates_relative_symlink_and_header(self):
        self._make(os.path.join('machine', 'pcb', 'FG_8GT_1SX.c'), b'x\n')
        with captured_stdout() as out:
            created, mdl = add_device.create_symlink_and_header(
                'FG_8GT_1SX', 'FG_8GT_1SX.c', 'Ruiying', 'RY-8GT-1SX',
                'RTL8373')
        self.assertEqual(mdl, 'RUIYING_RY_8GT_1SX')
        self.assertEqual(created, ['machine/RUIYING_RY_8GT_1SX.c',
                                   'machine/RUIYING_RY_8GT_1SX.h'])
        self.assertTrue(os.path.islink('machine/RUIYING_RY_8GT_1SX.c'))
        self.assertEqual(os.readlink('machine/RUIYING_RY_8GT_1SX.c'),
                         'pcb/FG_8GT_1SX.c')
        header = read('machine/RUIYING_RY_8GT_1SX.h')
        self.assertIn('#define MACHINE_MODEL "Ruiying RY-8GT-1SX"', header)
        self.assertIn('#define MACHINE_CHIPSET RTL8373', header)
        self.assertFalse(os.path.lexists('machine/RUIYING_RY_8GT_1SX_init.c'))

    def test_init_symlink_prefers_ext_init_over_base_init(self):
        self._make(os.path.join('machine', 'pcb', 'X__EXT.c'), b'x\n')
        self._make(os.path.join('machine', 'pcb', 'X__EXT_init.c'), b'i\n')
        self._make(os.path.join('machine', 'pcb', 'X_init.c'), b'i\n')
        with captured_stdout():
            created, mdl = add_device.create_symlink_and_header(
                'X', 'X__EXT.c', 'Ven', 'Mod', 'RTL8372')
        self.assertEqual(mdl, 'VEN_MOD')
        self.assertIn('machine/VEN_MOD_init.c', created)
        self.assertEqual(os.readlink('machine/VEN_MOD_init.c'),
                         'pcb/X__EXT_init.c')

    def test_init_symlink_falls_back_to_base_init(self):
        self._make(os.path.join('machine', 'pcb', 'X__EXT.c'), b'x\n')
        self._make(os.path.join('machine', 'pcb', 'X_init.c'), b'i\n')
        with captured_stdout():
            created, _ = add_device.create_symlink_and_header(
                'X', 'X__EXT.c', 'Ven', 'Mod', 'RTL8372')
        self.assertIn('machine/VEN_MOD_init.c', created)
        self.assertEqual(os.readlink('machine/VEN_MOD_init.c'),
                         'pcb/X_init.c')

    def test_init_symlink_skipped_when_neither_exists(self):
        self._make(os.path.join('machine', 'pcb', 'X__EXT.c'), b'x\n')
        with captured_stdout() as out:
            created, _ = add_device.create_symlink_and_header(
                'X', 'X__EXT.c', 'Ven', 'Mod', 'RTL8372')
        self.assertNotIn('machine/VEN_MOD_init.c', created)
        self.assertFalse(os.path.lexists('machine/VEN_MOD_init.c'))
        self.assertIn('not adding machine/VEN_MOD_init.c', out.getvalue())

    def test_init_symlink_plain_definition_reuses_own_init(self):
        # Without an _EXT variant the selected file IS <MACHINE_PCB>.c,
        # so its _init.c is found as the plain <MACHINE_PCB>_init.c.
        self._make(os.path.join('machine', 'pcb', 'Y.c'), b'x\n')
        self._make(os.path.join('machine', 'pcb', 'Y_init.c'), b'i\n')
        with captured_stdout():
            created, _ = add_device.create_symlink_and_header(
                'Y', 'Y.c', 'Ven', 'Mod', 'RTL8372')
        self.assertIn('machine/VEN_MOD_init.c', created)
        self.assertEqual(os.readlink('machine/VEN_MOD_init.c'),
                         'pcb/Y_init.c')

    def test_existing_correct_symlink_recognized_previous_run(self):
        self._make(os.path.join('machine', 'pcb', 'FG_8GT_1SX.c'), b'x\n')
        with captured_stdout():
            add_device.create_symlink_and_header(
                'FG_8GT_1SX', 'FG_8GT_1SX.c', 'Ruiying', 'RY-8GT-1SX',
                'RTL8373')
            before = read('machine/RUIYING_RY_8GT_1SX.h')
            # Second run: symlink recognized, header must prompt before replace.
            with stdin_feed(['n']), captured_stdout():
                created, _ = add_device.create_symlink_and_header(
                    'FG_8GT_1SX', 'FG_8GT_1SX.c', 'Ruiying', 'RY-8GT-1SX',
                    'RTL8373')
        self.assertEqual(created, [])
        self.assertEqual(read('machine/RUIYING_RY_8GT_1SX.h'), before)


class NewDeviceScaffoldTest(AddDeviceTestBase):
    """Steps 9-12: creating doc, photos, board definition, symlink, header."""

    def test_full_scaffold_with_photos(self):
        self._make(os.path.join(self.repo, 'src_front.jpg'),
                   content=b'\xff\xd8FAKEJPG')
        self._make(os.path.join(self.repo, 'src_back.jpg'), b'BACKJPG')
        photos = {'front': 'src_front.jpg', 'bottom': 'src_back.jpg'}
        with captured_stdout() as out:
            created, mdl, pb_doc_name, photo_dir = add_device.create_new_device(
                'EXAMPLE-BOARD V1.0', 'TestCo', 'TC-100', 'RTL8373', photos)
        self.assertEqual(mdl, 'TESTCO_TC_100')

        doc = 'doc/devices/EXAMPLE-BOARD_V1.0.md'
        pcb = 'machine/pcb/EXAMPLE_BOARD_V1_0.c'
        link = 'machine/TESTCO_TC_100.c'
        header = 'machine/TESTCO_TC_100.h'
        photos = os.path.join('doc', 'devices', 'photos', 'EXAMPLE-BOARD_V1.0')
        for path in (doc, pcb, header, photos + '/src_front.jpg',
                     photos + '/src_back.jpg'):
            self.assertTrue(os.path.exists(path), path)

        self.assertEqual(os.readlink(link), 'pcb/EXAMPLE_BOARD_V1_0.c')
        self.assertIn('__code const struct machine machine = {', read(pcb))
        self.assertIn('TODO', read(pcb))
        header_text = read(header)
        self.assertIn('#define MACHINE_MODEL "TestCo TC-100"', header_text)
        doc_text = read(doc)
        self.assertIn('# TestCo TC-100', doc_text)
        self.assertIn('`EXAMPLE-BOARD V1.0`', doc_text)
        self.assertIn('photos/EXAMPLE-BOARD_V1.0/src_front.jpg', doc_text)
        self.assertIn('make MACHINE=TESTCO_TC_100', doc_text)
        with open(photos + '/src_front.jpg', 'rb') as f:
            self.assertEqual(f.read(), b'\xff\xd8FAKEJPG')
        self.assertIn('Added device doc', out.getvalue())

    def test_scaffold_with_existing_doc_declined_overwrites(self):
        self._make('doc/devices/EXAMPLE-BOARD_V1.0.md', b'keep\n')
        # Only the doc exists, so exactly one overwrite prompt. Nothing else
        # is prompted; the missing files are all scaffoldded outright.
        with stdin_feed(['', '', 'n']), captured_stdout():
            created, mdl, _, photo_dir = add_device.create_new_device(
                'EXAMPLE-BOARD V1.0', 'TestCo', 'TC-100', 'RTL8373', {})
        self.assertEqual(read('doc/devices/EXAMPLE-BOARD_V1.0.md'), 'keep\n')
        self.assertEqual(os.readlink('machine/TESTCO_TC_100.c'),
                         'pcb/EXAMPLE_BOARD_V1_0.c')
        self.assertTrue(os.path.exists('machine/TESTCO_TC_100.h'))
        self.assertTrue(os.path.exists('machine/pcb/EXAMPLE_BOARD_V1_0.c'))
        self.assertEqual(mdl, 'TESTCO_TC_100')
        self.assertIn('doc/devices/photos/EXAMPLE-BOARD_V1.0', photo_dir)


class DryRunTest(AddDeviceTestBase):
    """-n/--dry-run: the flow is walked, no files are generated."""

    def setUp(self):
        super().setUp()
        self.addCleanup(setattr, add_device, 'DRY_RUN', False)

    def test_dry_run_scaffold_writes_nothing(self):
        add_device.DRY_RUN = True
        self._make(os.path.join(self.repo, 'src_front.jpg'), b'FRONTBYTES')
        with captured_stdout() as out:
            created, mdl, pcb_doc_name, photo_dir = add_device.create_new_device(
                'EXAMPLE-BOARD V1.0', 'TestCo', 'TC-100', 'RTL8373',
                {'front': 'src_front.jpg'})
        # The plan is reported in full...
        self.assertEqual(mdl, 'TESTCO_TC_100')
        for path in (created + [photo_dir]):
            self.assertIn('Would', out.getvalue())
        self.assertIn('Would add device doc: doc/devices/EXAMPLE-BOARD_V1.0.md',
                      out.getvalue())
        self.assertIn('Would add symlink: machine/TESTCO_TC_100.c',
                      out.getvalue())
        self.assertIn('Would copy front: src_front.jpg', out.getvalue())
        # ...but nothing exists on disk.
        self.assertFalse(os.path.exists('doc/devices/EXAMPLE-BOARD_V1.0.md'))
        self.assertFalse(os.path.exists('machine/pcb/EXAMPLE_BOARD_V1_0.c'))
        self.assertFalse(os.path.lexists('machine/TESTCO_TC_100.c'))
        self.assertFalse(os.path.lexists('machine/TESTCO_TC_100.h'))
        self.assertFalse(os.path.exists(
            'doc/devices/photos/EXAMPLE-BOARD_V1.0'))
        with open(os.path.join(self.repo, 'src_front.jpg'), 'rb') as f:
            self.assertEqual(f.read(), b'FRONTBYTES')

    def test_dry_run_reuse_flow_writes_nothing(self):
        add_device.DRY_RUN = True
        self._make(os.path.join('machine', 'pcb', 'FG_8GT_1SX.c'), b'x\n')
        self._make(os.path.join('machine', 'pcb', 'FG_8GT_1SX_init.c'), b'i\n')
        with captured_stdout() as out:
            created, _ = add_device.create_symlink_and_header(
                'FG_8GT_1SX', 'FG_8GT_1SX.c', 'Ruiying', 'RY-8GT-1SX',
                'RTL8373')
        self.assertIn('Would add symlink: machine/RUIYING_RY_8GT_1SX.c ->'
                      ' pcb/FG_8GT_1SX.c', out.getvalue())
        self.assertIn('Would add symlink: machine/RUIYING_RY_8GT_1SX_init.c ->'
                      ' pcb/FG_8GT_1SX_init.c', out.getvalue())
        self.assertIn('Would add header:   machine/RUIYING_RY_8GT_1SX.h',
                      out.getvalue())
        self.assertFalse(os.path.lexists('machine/RUIYING_RY_8GT_1SX_init.c'))
        self.assertFalse(os.path.lexists('machine/RUIYING_RY_8GT_1SX.c'))
        self.assertFalse(os.path.lexists('machine/RUIYING_RY_8GT_1SX.h'))
        self.assertEqual(created, ['machine/RUIYING_RY_8GT_1SX.c',
                                   'machine/RUIYING_RY_8GT_1SX_init.c',
                                   'machine/RUIYING_RY_8GT_1SX.h'])

    def test_dry_run_flag_then_real_run_still_writes(self):
        add_device.DRY_RUN = True
        with captured_stdout():
            add_device.create_new_device('EXAMPLE-BOARD V1.0', 'TestCo',
                                         'TC-100', 'RTL8373', {})
        self.assertFalse(os.path.exists('machine/pcb/EXAMPLE_BOARD_V1_0.c'))
        add_device.DRY_RUN = False
        with captured_stdout():
            add_device.create_new_device('EXAMPLE-BOARD V1.0', 'TestCo',
                                         'TC-100', 'RTL8373', {})
        self.assertTrue(os.path.exists('machine/pcb/EXAMPLE_BOARD_V1_0.c'))
        self.assertTrue(os.path.lexists('machine/TESTCO_TC_100.c'))


class AlreadySupportedTest(AddDeviceTestBase):
    """machine/<MACHINE>.c and .h both exist -> report and exit immediately."""

    def _add_machine_files(self, mdl='RUIYING_RY_8GT_1SX',
                           link_target='pcb/FG_8GT_1SX.c',
                           model='Ruiying RY-8GT-1SX'):
        os.symlink(link_target, os.path.join('machine', mdl + '.c'))
        self._make(os.path.join('machine', mdl + '.h'),
                   content=b'#ifndef _MACHINE_%s_H_\n\n'
                           b'#define MACHINE_MODEL "%s"\n\n'
                           b'#endif\n' % (mdl.encode(), model.encode()))

    def test_helper_reports_link_target_and_model(self):
        self._add_machine_files()
        info = add_device.machine_already_supported('RUIYING_RY_8GT_1SX')
        self.assertIn('machine/RUIYING_RY_8GT_1SX.c -> pcb/FG_8GT_1SX.c'
                      ' (symlink)', ' '.join(info))
        self.assertIn('MACHINE_MODEL "Ruiying RY-8GT-1SX"', ' '.join(info))

    def test_helper_reports_regular_file(self):
        self._make(os.path.join('machine', 'X.c'), b'x\n')
        self._make(os.path.join('machine', 'X.h'), b'x\n')
        info = add_device.machine_already_supported('X')
        self.assertIn('machine/X.c (regular file)', ' '.join(info))

    def test_helper_none_when_files_missing_or_partial(self):
        self.assertIsNone(add_device.machine_already_supported('X'))
        self._make(os.path.join('machine', 'X.h'), b'x\n')
        self.assertIsNone(add_device.machine_already_supported('X'))
        self._make(os.path.join('machine', 'X.c'), b'x\n')
        self.assertIsNotNone(add_device.machine_already_supported('X'))

    def test_run_exits_zero_before_any_selection(self):
        self._add_machine_files()
        with self.assertRaises(SystemExit) as cm, captured_stdout() as out:
            add_device.run('FG-8GT-1SX', 'Ruiying', 'RY-8GT-1SX', 'RTL8373')
        self.assertEqual(cm.exception.code, 0)
        self.assertIn('=== Device already supported ===', out.getvalue())
        self.assertIn('machine/RUIYING_RY_8GT_1SX.c -> pcb/FG_8GT_1SX.c',
                      out.getvalue())
        self.assertNotIn('Found machine definition', out.getvalue())

    def test_run_reports_header_model(self):
        self._add_machine_files(mdl='TESTCO_TC_100',
                                link_target='pcb/EXAMPLE_BOARD_V1_0.c',
                                model='TestCo TC-100')
        with self.assertRaises(SystemExit), captured_stdout() as out:
            add_device.run('EXAMPLE-BOARD V1.0', 'TestCo', 'TC-100',
                           'RTL8373')
        self.assertIn('machine/TESTCO_TC_100.h (MACHINE_MODEL'
                      ' "TestCo TC-100")', out.getvalue())


class WriteConfirmationTest(AddDeviceTestBase):
    """-y/--yes and the confirmation gate before disk writes (write and,
    when neither -y nor -g is given, git-add staging)."""

    def setUp(self):
        super().setUp()
        self.addCleanup(setattr, add_device, 'YES', False)
        self.addCleanup(setattr, add_device, 'DRY_RUN', False)
        self.addCleanup(setattr, add_device, 'GIT_ADD', False)

    def _documented_repo(self):
        self._make(os.path.join('machine', 'pcb', 'FG_8GT_1SX.c'), b'x\n')
        self._make('doc/devices/FG-8GT-1SX.md', b'-\n')

    def test_yes_flag_skips_the_prompt(self):
        add_device.YES = True
        with captured_stdout():
            add_device.confirm_write()
        # No stdin provided (and stdin_feed would EOF-exit on a prompt), so
        # getting here means nothing was asked.

    def test_dry_run_skips_the_prompt(self):
        add_device.DRY_RUN = True
        # '' would abort the prompt loop; succeeding proves no prompt ran.
        with stdin_feed(['']), captured_stdout():
            add_device.confirm_write()

    def test_declined_aborts(self):
        with stdin_feed(['n']), self.assertRaises(SystemExit), \
                captured_stdout():
            add_device.confirm_write()

    def test_accepted_declines_git_add(self):
        # 'y' answers the write confirmation, 'n' declines the git add
        # question; GIT_ADD stays False.
        with stdin_feed(['y', 'n']), captured_stdout() as out:
            add_device.confirm_write()
        self.assertFalse(add_device.GIT_ADD)

    def test_accepted_answers_git_add_yes(self):
        with stdin_feed(['y', 'y']), captured_stdout() as out:
            add_device.confirm_write()
        self.assertTrue(add_device.GIT_ADD)

    def test_declined_run_writes_nothing(self):
        self._documented_repo()
        with stdin_feed(['n']), self.assertRaises(SystemExit), \
                captured_stdout():
            add_device.run('FG-8GT-1SX', 'Ruiying', 'RY-8GT-1SX', 'RTL8373')
        self.assertFalse(os.path.lexists('machine/RUIYING_RY_8GT_1SX.c'))
        self.assertFalse(os.path.lexists('machine/RUIYING_RY_8GT_1SX.h'))

    def test_accepted_run_writes(self):
        self._documented_repo()
        with stdin_feed(['y', 'n']), captured_stdout():
            add_device.run('FG-8GT-1SX', 'Ruiying', 'RY-8GT-1SX', 'RTL8373')
        self.assertTrue(os.path.islink('machine/RUIYING_RY_8GT_1SX.c'))
        self.assertTrue(os.path.exists('machine/RUIYING_RY_8GT_1SX.h'))


class GitAddTest(AddDeviceTestBase):
    """-g/--git-add: stage newly added files, never commit them."""

    def setUp(self):
        super().setUp()
        self.addCleanup(setattr, add_device, 'GIT_ADD', False)

    def _init_git(self):
        subprocess.run(['git', 'init', '-q', '.'], check=True,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    def _staged(self):
        out = subprocess.run(
            ['git', 'status', '--porcelain', '--untracked-files=all'],
            stdout=subprocess.PIPE, check=True)
        return [l[3:] for l in out.stdout.decode().splitlines()
                if l[:2] != '??']

    def test_run_stages_via_flag(self):
        add_device.GIT_ADD = True
        self._init_git()
        self._make(os.path.join('machine', 'pcb', 'FG_8GT_1SX.c'), b'x\n')
        self._make('doc/devices/FG-8GT-1SX.md', b'-\n')
        # The single 'y' answers the write-to-disk confirmation; no other
        # prompt in this flow (all files are created outright).
        with stdin_feed(['y']), captured_stdout():
            add_device.run('FG-8GT-1SX', 'Ruiying', 'RY-8GT-1SX', 'RTL8373')
        staged = self._staged()
        # Only the files the run added are staged - the already existing
        # pcb definition and doc stay unstaged.
        self.assertEqual(staged, ['machine/RUIYING_RY_8GT_1SX.c',
                                  'machine/RUIYING_RY_8GT_1SX.h'])
        # Staged, but never committed: no HEAD exists yet.
        self.assertNotEqual(subprocess.run(
            ['git', 'rev-parse', '--verify', '--quiet', 'HEAD']).returncode, 0)

    def test_yes_flag_stages_without_prompt(self):
        add_device.GIT_ADD = True
        add_device.YES = True
        self.addCleanup(setattr, add_device, 'YES', False)
        self._init_git()
        self._make(os.path.join('machine', 'pcb', 'FG_8GT_1SX.c'), b'x\n')
        self._make('doc/devices/FG-8GT-1SX.md', b'-\n')
        with captured_stdout():
            add_device.run('FG-8GT-1SX', 'Ruiying', 'RY-8GT-1SX', 'RTL8373')
        self.assertEqual(self._staged(), ['machine/RUIYING_RY_8GT_1SX.c',
                                          'machine/RUIYING_RY_8GT_1SX.h'])

    def test_skips_when_not_a_git_repo(self):
        add_device.GIT_ADD = True
        with captured_stdout() as out:
            add_device.git_add_files(['machine/X.h'])
        self.assertIn('Not a git repository', out.getvalue())
        self.assertFalse(os.path.exists('.git/index'))

    def test_skips_in_dry_run(self):
        add_device.GIT_ADD = True
        add_device.DRY_RUN = True
        self.addCleanup(setattr, add_device, 'DRY_RUN', False)
        self._init_git()
        with captured_stdout() as out:
            add_device.git_add_files(['machine/X.h'])
        self.assertIn('nothing to git add', out.getvalue())

    def test_flag_without_g_leaves_index_empty(self):
        self._init_git()
        self._make('doc/devices/FG-8GT-1SX.md', b'-\n')
        self._make(os.path.join('machine', 'pcb', 'FG_8GT_1SX.c'), b'x\n')
        add_device.GIT_ADD = False
        # 'y' answers the write confirmation, 'n' declines the git add
        # staging question.
        with stdin_feed(['y', 'n']), captured_stdout() as out:
            add_device.run('FG-8GT-1SX', 'Ruiying', 'RY-8GT-1SX', 'RTL8373')
        self.assertIn('Stage the files with git add as well?',
                      out.getvalue())
        self.assertEqual(self._staged(), [])

    def test_prompted_git_add_yes_stages(self):
        self._init_git()
        self._make('doc/devices/FG-8GT-1SX.md', b'-\n')
        self._make(os.path.join('machine', 'pcb', 'FG_8GT_1SX.c'), b'x\n')
        add_device.GIT_ADD = False
        # 'y' answers the write confirmation, the second 'y' accepts the
        # git add staging question.
        with stdin_feed(['y', 'y']), captured_stdout():
            add_device.run('FG-8GT-1SX', 'Ruiying', 'RY-8GT-1SX', 'RTL8373')
        self.assertEqual(self._staged(), ['machine/RUIYING_RY_8GT_1SX.c',
                                          'machine/RUIYING_RY_8GT_1SX.h'])

    def test_yes_flag_never_asks_or_stages(self):
        # With -y/--yes (no -g) the git add question is skipped entirely:
        # no stdin is provided, so any prompt would EOF-abort the run.
        self._init_git()
        self._make('doc/devices/FG-8GT-1SX.md', b'-\n')
        self._make(os.path.join('machine', 'pcb', 'FG_8GT_1SX.c'), b'x\n')
        add_device.GIT_ADD = False
        add_device.YES = True
        self.addCleanup(setattr, add_device, 'YES', False)
        with captured_stdout():
            add_device.run('FG-8GT-1SX', 'Ruiying', 'RY-8GT-1SX', 'RTL8373')
        self.assertEqual(self._staged(), [])
        self.assertFalse(add_device.GIT_ADD)


class PromptToolsTest(AddDeviceTestBase):
    """ask/confirm/collect_images behave on EOF and bad input."""

    def test_confirm_loops_until_valid_answer(self):
        with stdin_feed(['maybe', 'yes']), captured_stdout():
            self.assertTrue(add_device.confirm('Continue?'))
        with stdin_feed(['x', 'n']), captured_stdout():
            self.assertFalse(add_device.confirm('Continue?'))

    def test_collect_images_suggested_names_when_skipped(self):
        with stdin_feed(['', '', '']), captured_stdout() as out:
            photos = add_device.collect_images()
        self.assertEqual(photos, {})
        self.assertIn('Suggested file name for the front panel when added'
                      ' later: front.jpg', out.getvalue())
        self.assertIn('Suggested file name for the pcb top when added later:'
                      ' pcb_top.jpg', out.getvalue())
        self.assertIn('Suggested file name for the pcb bottom when added'
                      ' later: pcb_bottom.jpg', out.getvalue())

    def test_collect_images_returns_provided_paths(self):
        self._make(os.path.join(self.repo, 'shot.png'), b'f')
        with stdin_feed(['', 'shot.png', '']), captured_stdout() as out:
            photos = add_device.collect_images()
        self.assertEqual(photos, {'top': 'shot.png'})
        # The two skipped prompts get suggestions; the provided one does not.
        self.assertEqual(out.getvalue().count('Suggested'), 2)
        self.assertNotIn('later: pcb_top.jpg', out.getvalue())

    def test_collect_images_retries_and_suggests_within_retry_loop(self):
        with stdin_feed(['/nonexistent/img.jpg', '', '', '']), \
                captured_stdout() as out:
            photos = add_device.collect_images()
        self.assertEqual(photos, {})
        self.assertEqual(out.getvalue().count('is not a file'), 1)
        self.assertEqual(out.getvalue().count('Suggested'), 3)

    def test_collect_inputs_collapses_whitespace(self):
        args = argparse.Namespace(
            pcb='FG-8GT   1SX', vendor='Ven\t dor', model='Mo  del',
            chipset='2')
        with captured_stdout():
            pcb_id, vendor, model, chipset = add_device.collect_inputs(args)
        self.assertEqual((pcb_id, vendor, model, chipset),
                         ('FG-8GT 1SX', 'Ven dor', 'Mo del', 'RTL8372'))

    def test_collect_inputs_prompted_chipset_short_form(self):
        args = argparse.Namespace(
            pcb='FG-8GT-1SX', vendor='Ven dor', model='Mo del', chipset=None)
        with stdin_feed(['x', '3']), captured_stdout() as out:
            _, _, _, chipset = add_device.collect_inputs(args)
        self.assertEqual(chipset, 'RTL8373')
        self.assertIn('2/3 for short', out.getvalue())


if __name__ == '__main__':
    unittest.main()
