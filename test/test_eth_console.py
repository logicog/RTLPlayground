"""Host-side safety checks for console image generation and flash updates."""
import contextlib
import importlib.util
import io
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

TOOLS = Path(__file__).resolve().parents[1] / "tools" / "eth_console"
spec = importlib.util.spec_from_file_location("eth_console", TOOLS / "console.py")
console = importlib.util.module_from_spec(spec)
spec.loader.exec_module(console)


def record(address, data=b"", kind=0):
    body = bytes([len(data)]) + address.to_bytes(2, "big") + bytes([kind]) + data
    return ":" + (body + bytes([-sum(body) & 255])).hex() + "\n"


class Images(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.ihx = self.root / "input.ihx"
        self.output = self.root / "update.bin"
        self.dump = self.root / "dump.bin"
        self.flash = self.root / "flash.bin"
        self.original = b"\x00\x40" + bytes(range(256)) * 8191 + bytes(range(254))
        self.dump.write_bytes(self.original)

    def package(self, optimized=False):
        return subprocess.run([sys.executable, *(["-O"] if optimized else []),
                               str(TOOLS / "mkimage.py"), str(self.ihx), str(self.output),
                               str(self.dump), str(self.flash)], capture_output=True)

    def test_rejects_bad_hex_without_overwriting_outputs(self):
        valid = record(0x1000, b"\x02\x10\x03")
        cases = {
            "short record": ":10100000021003DB\n",
            "checksum": valid[:-3] + "00\n",
            "missing colon": valid[1:],
            "missing EOF": valid,
            "data after EOF": record(0, kind=1) + valid,
            "invalid EOF": valid + record(0, b"x", kind=1),
            "outside code area": valid + record(0x3ffd, b"ab") + record(0, kind=1),
            "unsupported address record": valid + record(0, b"\x00\x01", kind=4) + record(0, kind=1),
        }
        for name, text in cases.items():
            for optimized in (False, True):
                with self.subTest(name=name, optimized=optimized):
                    self.ihx.write_text(text if name in ("missing EOF", "data after EOF", "invalid EOF", "outside code area", "unsupported address record") else text + record(0, kind=1))
                    self.output.write_bytes(b"previous update")
                    self.flash.write_bytes(b"previous flash")
                    result = self.package(optimized)
                    self.assertNotEqual(result.returncode, 0)
                    self.assertEqual(self.output.read_bytes(), b"previous update")
                    self.assertEqual(self.flash.read_bytes(), b"previous flash")

    def test_valid_images_preserve_all_unreplaced_bytes(self):
        for address in (0, 0x1000):
            with self.subTest(address=address):
                self.ihx.write_text(record(address, b"\x02\x10\x03") + record(0, kind=1))
                result = self.package()
                self.assertEqual(result.returncode, 0, result.stderr)
                image = self.output.read_bytes()
                flash = self.flash.read_bytes()
                self.assertEqual(len(flash), 0x200000)
                if address == 0:
                    self.assertEqual(len(image), 0x4000)
                    self.assertEqual(image[:5], b"\x00\x40\x02\x10\x03")
                    self.assertEqual(flash[:0x4000], image)
                    self.assertEqual(flash[0x4000:], self.original[0x4000:])
                else:
                    self.assertEqual(len(image), 0x4026)
                    self.assertEqual(flash[:0x1002], self.original[:0x1002])
                    self.assertEqual(flash[0x1002:0x1005], b"\x02\x10\x03")
                    self.assertEqual(flash[0x4000:0x1c000], self.original[0x4000:0x1c000])
                    self.assertEqual(flash[0x1d014:], self.original[0x1d014:])

    def test_invalid_dump_does_not_replace_either_output(self):
        self.ihx.write_text(record(0, b"\x02") + record(0, kind=1))
        self.dump.write_bytes(b"not a flash dump")
        self.output.write_bytes(b"previous update")
        self.flash.write_bytes(b"previous flash")
        result = self.package(optimized=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(self.output.read_bytes(), b"previous update")
        self.assertEqual(self.flash.read_bytes(), b"previous flash")

    def test_optimized_extractor_still_rejects_invalid_tables(self):
        for data in (b"bad dump", self.original):
            with self.subTest(size=len(data)):
                self.dump.write_bytes(data)
                result = subprocess.run([sys.executable, "-O", str(TOOLS / "gen_phy.py"),
                                         str(self.dump)], capture_output=True)
                self.assertNotEqual(result.returncode, 0)
                self.assertEqual(result.stdout, b"")


class FlashDevice:
    def __init__(self):
        self.memory = bytearray(bytes(range(256)) * 8192)
        self.armed = False
        self.corrupt = False
        self.read_fails = False

    def call(self, op, args=b""):
        address = int.from_bytes(args[:3], "big")
        if op == "flash_read":
            if self.read_fails:
                raise SystemExit("timeout")
            return bytes(self.memory[address:address + args[3]])
        if op == "flash_arm":
            self.armed = args == b"\x5a\xfe"
            return b"\0\0"
        if not self.armed:
            raise AssertionError("unarmed flash write")
        if op == "flash_erase":
            self.memory[address:address + 4096] = b"\xff" * 4096
        elif op == "flash_prog":
            self.memory[address:address + args[3]] = args[4:]
            if self.corrupt and address + args[3] == 0x2000:
                self.memory[address + args[3] - 1] ^= 1
        else:
            raise AssertionError(op)
        return b""


class FlashUpdates(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.file = Path(self.temp.name) / "payload.bin"
        self.device = FlashDevice()

    def update(self, address, data):
        self.file.write_bytes(data)
        with patch.object(console, "Console", return_value=self.device), \
             patch.object(sys, "argv", ["console.py", "en0", "flash-write", hex(address), str(self.file)]), \
             contextlib.redirect_stdout(io.StringIO()):
            console.main()

    def test_partial_sector_preserves_tail_and_neighbor_sectors(self):
        original = bytes(self.device.memory)
        self.update(0x1000, b"new firmware")
        self.assertEqual(self.device.memory, original[:0x1000] + b"new firmware" + original[0x100c:])
        self.assertFalse(self.device.armed)

    def test_failed_tail_read_leaves_flash_unchanged(self):
        original = bytes(self.device.memory)
        self.device.read_fails = True
        with self.assertRaises(SystemExit):
            self.update(0x1000, b"new firmware")
        self.assertEqual(self.device.memory, original)
        self.assertFalse(self.device.armed)

    def test_verification_checks_restored_tail(self):
        self.device.corrupt = True
        with self.assertRaises(SystemExit):
            self.update(0x1000, b"new firmware")
        self.assertFalse(self.device.armed)

    def test_protected_range_refused_before_erasing(self):
        original = bytes(self.device.memory)
        with self.assertRaises(SystemExit):
            self.update(0x1fc000, b"firmware")
        self.assertEqual(self.device.memory, original)


if __name__ == "__main__":
    unittest.main()
