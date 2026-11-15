#!/usr/bin/env python3
"""Offline smoke test for the NEC V20 native conformance adapter."""
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

from run_v20_singlestep import MAGIC,pack_test

initial={
    "ax":0x1234,"bx":0x5678,"cx":0x8765,"dx":0xabcd,
    "sp":0x8000,"bp":0,"si":0,"di":0,
    "es":0,"cs":0x1000,"ss":0,"ds":0,
    "ip":0,"flags":2,
}
test={
    "name":"MOV AX,5678h on NEC V20",
    "bytes":[0xb8,0x78,0x56],
    "initial":{"regs":initial,"ram":[[0x10000,0xb8],[0x10001,0x78],[0x10002,0x56]]},
    "final":{"regs":{"ax":0x5678,"ip":3},"ram":[]},
}
with tempfile.TemporaryDirectory() as folder:
    file=Path(folder)/"cases.bin"
    file.write_bytes(MAGIC+struct.pack("<I",1)+pack_test(test,0xffff))
    completed=subprocess.run([sys.argv[1],"--cpu","v20",str(file)],
                             capture_output=True,text=True,check=False)
    print(completed.stdout,end="")
    print(completed.stderr,end="",file=sys.stderr)
    sys.exit(completed.returncode)
