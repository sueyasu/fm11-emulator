#!/usr/bin/env python3
"""Check the native runner masks undefined DIV flags saved by INT 0.

The test deliberately supplies a different value for the undefined
status bits of the saved FLAGS word. The saved IP/CS and all other
registers and RAM are still checked exactly.
"""
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parent))
from run_8088_singlestep import MAGIC, pack_test

regs = dict(ax=0x1234, bx=0, cx=0, dx=0, sp=0x8000, bp=0,
            si=0, di=0, es=0, cs=0x1000, ss=0x3000, ds=0,
            ip=0, flags=0x0082)
# F6 F3 = DIV BL. BL=0, so CPU enters INT 0 at 0000:0400.
case = {
    "name": "F6.6: undefined status bits in stacked FLAGS",
    "bytes": [0xF6, 0xF3],
    "initial": {
        "regs": regs,
        "ram": [
            [0x10000, 0xF6], [0x10001, 0xF3],
            [0x00000, 0x00], [0x00001, 0x04],
            [0x00002, 0x00], [0x00003, 0x00],
        ],
    },
    "final": {
        "regs": {"sp": 0x7FFA, "ip": 0x0400, "cs": 0,
                 "flags": 0x0082},
        "ram": [
            [0x37FFA, 0x02], [0x37FFB, 0x00], # saved IP
            [0x37FFC, 0x00], [0x37FFD, 0x10], # saved CS
            # Deliberately different AF/PF/SF/CF: undefined for DIV.
            [0x37FFE, 0x16], [0x37FFF, 0x00], # saved FLAGS
        ],
    },
}
with tempfile.TemporaryDirectory() as directory:
    packed = Path(directory) / "cases.bin"
    packed.write_bytes(MAGIC + struct.pack("<I", 1)
                       + pack_test(case, 0xF72A))
    result = subprocess.run([sys.argv[1], str(packed)],
                            capture_output=True, text=True, check=False)
    print(result.stdout, end="")
    print(result.stderr, end="", file=sys.stderr)
    sys.exit(result.returncode)
