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

    def test_nec80_extended_instructions_when_available(self):
        # A stock unidasm lacks -arch nec80. Keep Stage 3-B-1 viable,
        # but verify MAME's CALLN and RETEM decoder when patched.
        import subprocess
        binary = os.environ.get("FM11_UNIDASM") or shutil.which("unidasm")
        probe = subprocess.run(
            [binary, "-", "-arch", "nec80", "-basepc", "0", "-count", "3"],
            input=bytes.fromhex("ED ED 03"), capture_output=True, check=False,
        )
        if probe.returncode:
            if os.environ.get("FM11_REQUIRE_NEC80") == "1":
                self.fail("FM11_REQUIRE_NEC80=1 but MAME unidasm lacks nec80")
            self.skipTest("MAME unidasm does not have the nec80 patch")
        calln = adapter.disassemble(
            "v20_8080", 0, 1, bytes.fromhex("ED ED 03")
        ).lower()
        self.assertIn("calln", calln)
        retem = adapter.disassemble(
            "v20_8080", 0, 1, bytes.fromhex("ED FD 00")
        ).lower()
        self.assertIn("retem", retem)

    def test_6809_indexed_and_prefixed_instructions(self):
        # Sources: MAME 6x09dasm opcode maps. No local decoding.
        for code, mnemonic in (
            ("A6 84", "lda"),     # LDA ,X (indexed postbyte)
            ("10 3F", "swi2"),    # 6809 prefix 10
        ):
            with self.subTest(opcode=code):
                listing = adapter.disassemble(
                    "m6809", 0x2000, 1, bytes.fromhex(code)
                ).lower()
                self.assertIn(mnemonic, listing)
                self.assertTrue(listing.startswith("2000:"))

    def test_hd6309_exclusive_instructions(self):
        # MAME 6x09dasm: CD = LDQ immediate, 11 38 = TFM.
        for code, mnemonic in (
            ("CD 12 34 56 78", "ldq"),
            ("11 38 12", "tfm"),
            ("11 3C 01", "bitmd"),
        ):
            with self.subTest(opcode=code):
                listing = adapter.disassemble(
                    "hd6309", 0x4000, 1, bytes.fromhex(code)
                ).lower()
                self.assertIn(mnemonic, listing)
                self.assertTrue(listing.startswith("4000:"))

    def test_z80_prefixed_instructions(self):
        for code, mnemonic in (
            ("CB 7E", "bit"),
            ("ED B0", "ldir"),
            ("DD CB 05 46", "bit"),
        ):
            with self.subTest(opcode=code):
                listing = adapter.disassemble(
                    "z80", 0x8000, 1, bytes.fromhex(code)
                ).lower()
                self.assertIn(mnemonic, listing)
                self.assertTrue(listing.startswith("8000:"))

    def test_x86_modrm_and_segment_override(self):
        for code in (
            "8B 46 FE",     # MOV AX,[BP-2] (ModR/M + displacement)
            "26 8A 07",     # ES: MOV AL,[BX] (segment override)
        ):
            with self.subTest(opcode=code):
                listing = adapter.disassemble(
                    "i8088", 0x12345, 1, bytes.fromhex(code)
                ).lower()
                self.assertIn("mov", listing)
                self.assertTrue(listing.startswith("12345:"))

    def test_v20_8080_bootstrap_jumps_when_available(self):
        import subprocess
        executable = os.environ.get("FM11_UNIDASM") or shutil.which("unidasm")
        probe = subprocess.run(
            [executable, "-", "-arch", "nec80", "-basepc", "1dc00", "-count", "3"],
            input=bytes.fromhex("C3 5C DF"), capture_output=True, check=False,
        )
        if probe.returncode:
            if os.environ.get("FM11_REQUIRE_NEC80") == "1":
                self.fail("FM11_REQUIRE_NEC80=1 but MAME unidasm lacks nec80")
            self.skipTest("MAME unidasm lacks the nec80 patch")
        lines = adapter.disassemble(
            "v20_8080", 0x1dc00, 2,
            bytes.fromhex("C3 5C DF C3 58 DF"),
        ).lower().splitlines()
        self.assertEqual(len(lines), 2)
        self.assertTrue(lines[0].startswith("1dc00:"))
        self.assertTrue(lines[1].startswith("1dc03:"))
        self.assertIn("jmp", lines[0])
        self.assertIn("df5c", lines[0])
        self.assertIn("df58", lines[1])

    def test_instruction_count_is_not_byte_count(self):
        lines = adapter.disassemble("z80", 0x1234, 3, b"\x00" * 64).splitlines()
        self.assertEqual(len(lines), 3)
        self.assertTrue(lines[0].startswith("1234:"))
        self.assertTrue(lines[1].startswith("1235:"))
        self.assertTrue(lines[2].startswith("1236:"))


if __name__ == "__main__":
    unittest.main()
