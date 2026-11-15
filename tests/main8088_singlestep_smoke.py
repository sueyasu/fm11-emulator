#!/usr/bin/env python3
"""Smoke test the native CPU-state protocol without downloading the full suite."""
from pathlib import Path
import json
import struct
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parent))
from run_8088_singlestep import MAGIC, pack_test

regs = {
    "ax": 0, "bx": 0, "cx": 0, "dx": 0,
    "sp": 0x8000, "bp": 0, "si": 0, "di": 0,
    "es": 0, "cs": 0x1000, "ss": 0, "ds": 0,
    "ip": 0, "flags": 2,
}
case = {
    "name": "mov ax,1234h (local adapter smoke case)",
    "bytes": [0xB8, 0x34, 0x12],
    "initial": {
        "regs": regs,
        "ram": [[0x10000, 0xB8], [0x10001, 0x34], [0x10002, 0x12]],
    },
    "final": {"regs": {"ax": 0x1234, "ip": 3}, "ram": []},
}
with tempfile.TemporaryDirectory() as d:
    path = Path(d) / "smoke.bin"
    path.write_bytes(MAGIC + struct.pack("<I", 1) + pack_test(case, 0xFFFF))
    result = subprocess.run([sys.argv[1], str(path)], capture_output=True,
                            text=True, check=False)
    print(result.stdout, end="")
    print(result.stderr, end="", file=sys.stderr)
    sys.exit(result.returncode)
