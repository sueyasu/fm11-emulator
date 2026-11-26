#!/usr/bin/env python3
"""Tests the MAME unidasm adapter with a mocked external process.

These tests don't implement or compare CPU opcode decoding.
"""
import importlib.util
from pathlib import Path
import subprocess
import unittest
from unittest.mock import patch

SCRIPT = Path(__file__).resolve().parents[1] / "tools" / "disassemble.py"
spec = importlib.util.spec_from_file_location("fm11_unidasm", SCRIPT)
adapter = importlib.util.module_from_spec(spec)
spec.loader.exec_module(adapter)


class TestUnidasmAdapter(unittest.TestCase):
    def test_architecture_mapping_and_byte_count(self):
        cases = {
            "m6809": "m6809", "hd6309": "hd6309",
            "z80": "z80", "i8088": "x86_16", "v20": "nec",
        }
        for arch, mame in cases.items():
            with self.subTest(arch=arch):
                output = b"0000: 12  nop\n0001: 12  nop\n0002: 12  nop\n"
                result = subprocess.CompletedProcess([], 0, output, b"")
                with patch.object(adapter.subprocess, "run", return_value=result) as run:
                    text = adapter.disassemble(arch, 0, 2, bytes([0x12] * 48), "unidasm")
                cmd = run.call_args.args[0]
                self.assertEqual(cmd[:4], ["unidasm", "-", "-arch", mame])
                self.assertEqual(cmd[-2:], ["-count", "32"])
                self.assertEqual(run.call_args.kwargs["input"], bytes([0x12] * 48))
                pad = 5 if arch in ("i8088", "v20") else 4
                self.assertEqual(text.splitlines(), [
                    f"{0:0{pad}X}: 12  nop", f"{1:0{pad}X}: 12  nop"
                ])

    def test_wrap_and_width(self):
        result = subprocess.CompletedProcess([], 0, b"10000: 12  nop\n", b"")
        with patch.object(adapter.subprocess, "run", return_value=result):
            self.assertEqual(
                adapter.disassemble("m6809", 0xffff, 1, b"\x12" * 16, "unidasm"),
                "0000: 12  nop\n",
            )
        with patch.object(adapter.subprocess, "run", return_value=result):
            self.assertEqual(
                adapter.disassemble("v20", 0xfffff, 1, b"\x90" * 16, "unidasm"),
                "10000: 12  nop\n",
            )

    def test_invalid_parameters(self):
        for arch, start, count, data in (
            ("unknown", 0, 1, b"\x12"),
            ("z80", 0, 0, b"\x00"),
            ("z80", 0, 65, b"\x00"),
            ("z80", 0x10000, 1, b"\x00"),
            ("v20", 0x100000, 1, b"\x90"),
            ("m6809", 0, 1, b""),
        ):
            with self.subTest(arch=arch, start=start, count=count):
                with self.assertRaises(ValueError):
                    adapter.disassemble(arch, start, count, data, "unidasm")

    def test_missing_executable(self):
        with patch.object(adapter.shutil, "which", return_value=None), \
             patch.dict(adapter.os.environ, {}, clear=True):
            with self.assertRaisesRegex(RuntimeError, "install mame-tools"):
                adapter.disassemble("z80", 0, 1, b"\x00")

    def test_unidasm_failure_and_timeout(self):
        with patch.object(adapter.subprocess, "run", return_value=
                          subprocess.CompletedProcess([], 1, b"", b"unknown architecture")):
            with self.assertRaisesRegex(RuntimeError, "unknown architecture"):
                adapter.disassemble("z80", 0, 1, b"\x00", "unidasm")
        with patch.object(adapter.subprocess, "run",
                          side_effect=subprocess.TimeoutExpired("unidasm", 15)):
            with self.assertRaisesRegex(RuntimeError, "timed out"):
                adapter.disassemble("z80", 0, 1, b"\x00", "unidasm")

    def test_empty_output(self):
        with patch.object(adapter.subprocess, "run", return_value=
                          subprocess.CompletedProcess([], 0, b"banner\n", b"")):
            with self.assertRaisesRegex(RuntimeError, "no instruction"):
                adapter.disassemble("z80", 0, 1, b"\x00", "unidasm")


if __name__ == "__main__":
    unittest.main()
