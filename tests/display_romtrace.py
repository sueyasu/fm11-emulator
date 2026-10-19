#!/usr/bin/env python3
import pathlib
import subprocess
import sys
import tempfile

exe = sys.argv[1]
with tempfile.TemporaryDirectory() as temporary:
    rom = bytearray(0x5000)
    # C000: LDA #12; C002: STA $AFE4; C005: BRA $C005
    rom[0x1000:0x1007] = bytes([0x86, 0x12, 0xb7, 0xaf, 0xe4, 0x20, 0xfe])
    rom[0x4ffe:0x5000] = bytes([0xc0, 0x00])
    path = pathlib.Path(temporary) / "sub.rom"
    path.write_bytes(rom)
    run = subprocess.run(
        [exe, "--rom", str(path), "--instructions", "3"],
        capture_output=True, text=True, check=False,
    )
    if run.returncode or "Display reset PC=c000" not in run.stdout or \
            "I/O WRITE PC=c002 ADDR=afe4 DATA=12 CPU" not in run.stdout:
        print(run.stdout, run.stderr, file=sys.stderr)
        raise SystemExit(1)

    path.write_bytes(rom[:-1])
    invalid = subprocess.run(
        [exe, "--rom", str(path)], capture_output=True, text=True,
        check=False,
    )
    if invalid.returncode != 2 or "expected 20480 bytes" not in invalid.stderr:
        print(invalid.stdout, invalid.stderr, file=sys.stderr)
        raise SystemExit(2)
