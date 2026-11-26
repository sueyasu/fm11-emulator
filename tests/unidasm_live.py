#!/usr/bin/env python3
"""Optional integration smoke test against installed MAME unidasm.

Skipped when mame-tools is absent. This exercises upstream opcode decoders,
not a copied or locally implemented instruction decoder.
"""
import importlib.util
import os
from pathlib import Path
import shutil
import unittest

SCRIPT = Path(__file__).resolve().parents[1] / "tools" / "disassemble.py"
spec = importlib.util.spec_from_file_location("fm11_unidasm", SCRIPT)
adapter = importlib.util.module_from_spec(spec)
spec.loader.exec_module(adapter)


@unittest.skipUnless(
    os.environ.get("FM11_UNIDASM") or shutil.which("unidasm"),
    "MAME unidasm is not installed",
)
class TestInstalledUnidasm(unittest.TestCase):
    def test_cpu_modes(self):
        # Each test uses a single, known opcode solely as an integration probe.
        for arch, opcode in (
            ("m6809", 0x12),
            ("hd6309", 0x12),
            ("z80", 0x00),
            ("i8088", 0x90),
            ("v20", 0x90),
        ):
            with self.subTest(arch=arch):
                lines = adapter.disassemble(
                    arch, 0, 1, bytes([opcode] * 32)
                ).lower().splitlines()
                self.assertEqual(len(lines), 1)
                self.assertIn("nop", lines[0])

    def test_instruction_count_is_not_byte_count(self):
        lines = adapter.disassemble("z80", 0x1234, 3, b"\x00" * 64).splitlines()
        self.assertEqual(len(lines), 3)
        self.assertTrue(lines[0].startswith("1234:"))
        self.assertTrue(lines[1].startswith("1235:"))
        self.assertTrue(lines[2].startswith("1236:"))


if __name__ == "__main__":
    unittest.main()
