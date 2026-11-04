#!/usr/bin/env python3
"""Exercise fileadder's stored bytes and per-file HTTP compression flags."""
import gzip
from pathlib import Path
import random
import re
import subprocess
import sys
import tempfile
import unittest
import zlib


FILEADDER = Path(sys.argv.pop(1)).resolve()


class FileadderTests(unittest.TestCase):
    def test_mixed_assets(self):
        rng = random.Random(0)
        assets = {
            "app.js": b"console.log('hello');\n" * 256,
            "empty.txt": b"",
            "index.html": b"<p>hello</p>\n" * 64,
            "tiny.html": b"0123456789abcdefghij",
            "packed.bin": gzip.compress(rng.randbytes(1024), mtime=0),
            "tie.txt": b"A" * 23,
        }
        for compress in (False, True):
            with self.subTest(compress=compress), tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp)
                folder = root / "assets"
                folder.mkdir()
                for name, data in assets.items():
                    (folder / name).write_bytes(data)
                image = root / "image.bin"
                image.write_bytes(b"\xff" * 65536)
                output = root / "packed-image.bin"
                command = [str(FILEADDER), "-a", "128", "-d", str(folder),
                           "-o", str(output), "-p", str(root / "files")]
                if compress:
                    command.append("-z")
                subprocess.run(command + [str(image)], check=True, capture_output=True)
                data = output.read_bytes()
                header = (root / "files.h").read_text()
                table = (root / "files.c").read_text()
                cursor = 128
                for name, original in sorted(assets.items()):
                    with self.subTest(name=name):
                        symbol = name.replace(".", "_")
                        start = int(re.search(rf"FDATA_START_{symbol} (0x\w+)", header)[1], 16)
                        size = int(re.search(rf"FDATA_SIZE_{symbol} (\d+)", header)[1])
                        flag = int(re.search(rf'\{{"/{re.escape(name)}",[^\n]+, ([01])\}}', table)[1])
                        # Same encoder settings as fileadder, so a host zlib that
                        # breaks the tie differently still yields the right answer.
                        z = zlib.compressobj(9, zlib.DEFLATED, 31, 8)
                        smaller = len(z.compress(original) + z.flush()) < len(original)
                        expected_gzip = compress and smaller
                        self.assertEqual(flag, expected_gzip, name)
                        self.assertEqual(start, cursor, name)
                        stored = data[start:start + size]
                        if flag:
                            self.assertLess(size, len(original), name)
                            self.assertEqual(gzip.decompress(stored), original, name)
                        else:
                            self.assertEqual(stored, original, name)
                        cursor += size
                root_flag = int(re.search(r'\{"/",[^\n]+, ([01])\}', table)[1])
                index_flag = int(re.search(r'\{"/index\.html",[^\n]+, ([01])\}', table)[1])
                self.assertEqual(root_flag, index_flag, "root alias matches index.html")
                self.assertEqual(data[:128], b"\xff" * 128)


if __name__ == "__main__":
    unittest.main()
