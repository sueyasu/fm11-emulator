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
    "ax": 0x1234, "bx": 0x2000, "cx": 0x3003, "dx": 0x4000,
    "sp": 0x8000, "bp": 0, "si": 0, "di": 0,
    "es": 0, "cs": 0x1000, "ss": 0, "ds": 0,
    "ip": 0, "flags": 2,
}
case = {
    "name": "add [bx],cl (local adapter mapping smoke case)",
    "bytes": [0x00, 0x0F],
    "initial": {
        "regs": regs,
        "ram": [[0x10000, 0x00], [0x10001, 0x0F],
                [0x02000, 0x10]],
    },
    "final": {"regs": {"ip": 2, "flags": 0x0002},
              "ram": [[0x02000, 0x13]]},
}
with tempfile.TemporaryDirectory() as d:
    path = Path(d) / "smoke.bin"
    path.write_bytes(MAGIC + struct.pack("<I", 1) + pack_test(case, 0xFFFF))
    result = subprocess.run([sys.argv[1], str(path)], capture_output=True,
                            text=True, check=False)
    print(result.stdout, end="")
    print(result.stderr, end="", file=sys.stderr)
    sys.exit(result.returncode)
