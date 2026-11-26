#!/usr/bin/env python3
"""Standalone disassembler frontend (Capstone / DeZ80, no opcode tables).

Usage: disassemble.py ARCH START_HEX COUNT HEX_BYTES
ARCH: m6809, hd6309, i8088, v20, z80

This is an optional *external tool*, not part of the emulator CPU/core.
Output is a human-readable listing for the SDL monitor.
"""
import os
import pathlib
import subprocess
import sys


def format_line(address, raw, mnemonic, operands, width):
    encoded = " ".join(f"{b:02X}" for b in raw)
    name = (mnemonic + (" " + operands if operands else "")).strip()
    return f"{address:0{width}X}  {encoded:<47} {name}"


def capstone_listing(arch, start, count, data):
    try:
        import capstone
    except ImportError as exc:
        raise RuntimeError(
            "Capstone Python bindings unavailable; install python3-capstone "
            "(or pip install capstone)"
        ) from exc

    if arch == "m6809":
        kind, mode, mask = capstone.CS_ARCH_M680X, capstone.CS_MODE_M680X_6809, 0xFFFF
    elif arch == "hd6309":
        kind, mode, mask = capstone.CS_ARCH_M680X, capstone.CS_MODE_M680X_6309, 0xFFFF
    else:
        kind, mode, mask = capstone.CS_ARCH_X86, capstone.CS_MODE_16, 0xFFFFF

    decoder = capstone.Cs(kind, mode)
    decoder.skipdata = True
    width = 5 if mask == 0xFFFFF else 4
    for insn in decoder.disasm(data, start, count=count):
        address = insn.address & mask
        print(format_line(address, bytes(insn.bytes), insn.mnemonic, insn.op_str, width))


def dez80_listing(start, count, data):
    # The Rust bridge uses the unmodified DeZ80 crate API and supplies sizes;
    # dez80_cli itself prints only mnemonics, without instruction boundaries.
    root = pathlib.Path(__file__).resolve().parent.parent
    default = root / "tools" / "dez80-bridge" / "target" / "release" / "fm11-dez80-bridge"
    bridge = os.environ.get("FM11_DEZ80_BRIDGE", str(default))
    if not os.path.isfile(bridge) or not os.access(bridge, os.X_OK):
        raise RuntimeError(
            "DeZ80 bridge missing; run: cargo build --manifest-path "
            "tools/dez80-bridge/Cargo.toml --release"
        )
    completed = subprocess.run(
        [bridge, str(count), data.hex()],
        text=True, capture_output=True, check=True
    )
    address = start & 0xFFFF
    position = 0
    for line in completed.stdout.splitlines():
        parts = line.split("\t", 1)
        if len(parts) != 2:
            raise RuntimeError("Unexpected DeZ80 bridge response")
        size = int(parts[0])
        if size < 1 or position + size > len(data):
            raise RuntimeError("Invalid DeZ80 instruction size")
        print(format_line(address, data[position:position + size], parts[1], "", 4))
        position += size
        address = (address + size) & 0xFFFF


def main():
    if len(sys.argv) != 5:
        raise RuntimeError("Usage: disassemble.py ARCH START_HEX COUNT HEX_BYTES")
    arch = sys.argv[1]
    if arch not in {"m6809", "hd6309", "i8088", "v20", "z80"}:
        raise RuntimeError("Unknown architecture")
    start = int(sys.argv[2], 16)
    count = int(sys.argv[3], 10)
    if count < 1 or count > 64:
        raise RuntimeError("Instruction count must be 1..64")
    data = bytes.fromhex(sys.argv[4])
    if not data or len(data) > 4096:
        raise RuntimeError("Invalid input byte count")
    if arch == "z80":
        dez80_listing(start, count, data)
    else:
        capstone_listing(arch, start, count, data)


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, RuntimeError, subprocess.CalledProcessError) as error:
        print(f"External disassembler error: {error}", file=sys.stderr)
        sys.exit(1)
