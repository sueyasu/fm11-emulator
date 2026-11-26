#!/usr/bin/env python3
"""MAME unidasm adapter for the FM-11 debug monitor.

Usage: disassemble.py ARCH START_HEX COUNT HEX_BYTES
No local instruction decoding: raw guest bytes are sent to MAME's
unidasm through stdin, and its listing is limited to COUNT instructions.
ARCH: m6809, hd6309, z80, i8088, v20.
V20/8080 mode is intentionally deferred to Stage 3-B-2.
"""
import os
import re
import shutil
import subprocess
import sys

ARCHITECTURES = {
    "m6809": "m6809",
    "hd6309": "hd6309",
    "z80": "z80",
    "i8088": "x86_16",
    "v20": "nec",
}

# Standard unidasm emits "ADDRESS: RAW-BYTES  MNEMONIC OPERANDS".
# Preserve MAME's disassembly text instead of re-decoding the instruction.
LISTING = re.compile(r"^\s*([0-9a-fA-F]+):\s+(.+?)\s*$")


def disassemble(arch, start, count, data, executable=None):
    if arch not in ARCHITECTURES:
        raise ValueError("Unsupported CPU architecture: " + arch)
    if not 1 <= count <= 64:
        raise ValueError("Instruction count must be 1..64")
    if not 0 <= start <= (0xFFFFF if arch in ("i8088", "v20") else 0xFFFF):
        raise ValueError("Disassembly address is out of range")
    if not 1 <= len(data) <= 4096:
        raise ValueError("Instruction byte window must be 1..4096 bytes")

    executable = executable or os.environ.get("FM11_UNIDASM") or shutil.which("unidasm")
    if not executable:
        raise RuntimeError("MAME unidasm not found; install mame-tools (sudo apt install mame-tools)")

    # -count is a PC/byte span, NOT an instruction count. It must
    # include enough input bytes for COUNT maximum-length instructions.
    # Truncate the produced listing to COUNT lines below.
    byte_span = min(len(data), count * 16)
    command = [
        executable, "-", "-arch", ARCHITECTURES[arch],
        "-basepc", f"{start:X}", "-count", str(byte_span),
    ]
    try:
        result = subprocess.run(
            command, input=data, capture_output=True, timeout=15, check=False
        )
    except FileNotFoundError as exc:
        raise RuntimeError("MAME unidasm executable not found") from exc
    except subprocess.TimeoutExpired as exc:
        raise RuntimeError("MAME unidasm timed out") from exc
    if result.returncode:
        detail = result.stderr.decode("utf-8", "replace").strip()
        if not detail:
            detail = result.stdout.decode("utf-8", "replace").strip()
        raise RuntimeError(f"MAME unidasm failed ({result.returncode}): {detail[:600]}")

    lines = []
    address_mask = 0xFFFFF if arch in ("i8088", "v20") else 0xFFFF
    width = 5 if address_mask == 0xFFFFF else 4
    for raw_line in result.stdout.decode("utf-8", "replace").splitlines():
        match = LISTING.match(raw_line)
        if not match:
            continue
        address = int(match.group(1), 16) & address_mask
        lines.append(f"{address:0{width}X}: {match.group(2)}")
        if len(lines) == count:
            break
    if not lines:
        raise RuntimeError("MAME unidasm returned no instruction listing")
    return "\n".join(lines) + "\n"


def main(argv):
    if len(argv) != 5:
        raise ValueError("Usage: disassemble.py ARCH START_HEX COUNT HEX_BYTES")
    arch = argv[1]
    start = int(argv[2], 16)
    count = int(argv[3], 10)
    data = bytes.fromhex(argv[4])
    sys.stdout.write(disassemble(arch, start, count, data))


if __name__ == "__main__":
    try:
        main(sys.argv)
    except (OSError, ValueError, RuntimeError) as exc:
        print(f"External disassembler error: {exc}", file=sys.stderr)
        sys.exit(1)
