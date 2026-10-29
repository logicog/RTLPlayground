"""Negative regression fixtures for the linked-image bank audit."""
import tempfile
from pathlib import Path
import unittest
import sys

sys.dont_write_bytecode = True
from check_banked_calls import audit, image_offset


class BankAuditTest(unittest.TestCase):
    def fixture(self, caller, target, name='_cmd_word_is'):
        temp = tempfile.TemporaryDirectory()
        self.addCleanup(temp.cleanup)
        root = Path(temp.name)
        root.joinpath('rtlplayground.map').write_text(f'C: {target:08X} {name}\n')
        code = bytes([0x12, (target >> 8) & 255, target & 255])
        image = bytearray(524288)
        offset = image_offset(caller)
        image[offset:offset + 3] = code
        root.joinpath('fixture.bin').write_bytes(image)
        root.joinpath('snmp_control.rst').write_text(
            f'{caller:06X}  1 _snmp_command:\n'
            f'{caller:06X} {code.hex(" ").upper()} [24] 2 lcall {name}\n')
        return root

    def test_same_bank_helper(self):
        for n in (2, 3):
            root = self.fixture(n * 0x10000 + 0x4000, n * 0x10000 + 0x4010)
            self.assertEqual(audit(root)['errors'], [])

    def test_original_bank3_to_bank2_call(self):
        root = self.fixture(0x34000, 0x2480f, '_cmd_compare')
        errors = audit(root)['errors']
        self.assertTrue(any('BANK3 -> BANK2' in e for e in errors))
        self.assertTrue(any('own module' in e for e in errors))

    def test_same_bank_external_compare_is_not_relocation_safe(self):
        root = self.fixture(0x24000, 0x2480f, '_cmd_compare')
        self.assertTrue(any('own module' in e for e in audit(root)['errors']))

    def test_common_code_is_reachable(self):
        root = self.fixture(0x34000, 0x2000, '__sdcc_banked_call')
        self.assertEqual(audit(root, snmp_policy=False)['errors'], [])

    def test_code_literal_in_wrong_bank(self):
        root = self.fixture(0x34000, 0x34010)
        with root.joinpath('rtlplayground.map').open('a') as output:
            output.write('C: 00034000 _snmp_command\n')
        with root.joinpath('snmp_control.rst').open('a') as output:
            output.write('024100  3 ___str_0:\n')
        self.assertTrue(any('outside the CLI bank' in e for e in audit(root)['errors']))

    def test_packaged_instruction_tamper(self):
        root = self.fixture(0x24000, 0x24010)
        image = bytearray(root.joinpath('fixture.bin').read_bytes())
        image[image_offset(0x24000)] ^= 1
        root.joinpath('fixture.bin').write_bytes(image)
        self.assertTrue(any('byte mismatch' in e for e in audit(root)['errors']))

    def test_explicit_image_ignores_historical_images(self):
        root = self.fixture(0x24000, 0x24010)
        root.joinpath('old.bin').write_bytes(b'old image')
        with self.assertRaises(ValueError):
            audit(root)
        self.assertEqual(audit(root, image_path=root / 'fixture.bin')['errors'], [])

    def test_unresolved_target_fails_closed(self):
        root = self.fixture(0x24000, 0x24010)
        root.joinpath('rtlplayground.map').write_text('C: 00002000 _unrelated\n')
        self.assertTrue(any('unresolved target' in e for e in audit(root)['errors']))


if __name__ == '__main__':
    unittest.main()
