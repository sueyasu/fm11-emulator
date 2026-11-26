#!/usr/bin/env python3
"""Integration smoke test for optional third-party disassemblers.

No locally implemented decoder is used by this test or by the emulator.
Run Capstone checks if its Python bindings are installed, and DeZ80
checks when its independent Rust bridge has been built.
"""
import importlib.util
import pathlib
import subprocess
import sys


ROOT = pathlib.Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "tools" / "disassemble.py"
Z80_BRIDGE = ROOT / "tools" / "dez80-bridge" / "target" / "release" / "fm11-dez80-bridge"


def run(arch, hexbytes, expected):
    result = subprocess.run(
        [sys.executable, str(SCRIPT), arch, "0000", "1", hexbytes],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
        check=False,
    )
    if result.returncode:
        raise AssertionError(f"{arch} failed: {result.stderr}")
    if expected.lower() not in result.stdout.lower():
        raise AssertionError(
            f"{arch}: expected {expected!r}, got {result.stdout!r}"
        )


def main():
    if importlib.util.find_spec("capstone"):
        run("m6809", "12", "nop")
        run("hd6309", "12", "nop")
        run("i8088", "90", "nop")
        run("v20", "90", "nop")
        print("External Capstone: PASS")
    else:
        print("External Capstone: SKIP (python3-capstone not installed)")
    if Z80_BRIDGE.is_file():
        run("z80", "00", "nop")
        print("External DeZ80: PASS")
    else:
        print("External DeZ80: SKIP (Rust bridge not built)")


if __name__ == "__main__":
    main()
