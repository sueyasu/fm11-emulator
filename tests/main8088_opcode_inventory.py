#!/usr/bin/env python3
"""Conservative 8088 primary-opcode inventory (not a behavioral conformance test).

Reads the production decoder and prints a 16x16 opcode grid.
'+' = recognized by the switch, 'r' = recognized by one of the range
dispatches, '.' = no primary-opcode dispatch, 'p' = prefix.
Grouped opcodes still require ModR/M subopcode-level validation.
"""
import argparse
import pathlib
import re

def collect(source):
    body = source.split("void Main8088Board::Core::executeOne()", 1)[1]
    direct = {int(v, 16) for v in re.findall(r"\bcase\s+0x([0-9a-fA-F]{2})\s*:", body)}
    ranged = set()
    for i in range(256):
        if i <= 0x3b and i & 7 <= 3 and not i & 4:
            ranged.add(i)
        if 0x40 <= i <= 0x5f or 0x70 <= i <= 0x7f:
            ranged.add(i)
        if 0x91 <= i <= 0x97 or 0xb0 <= i <= 0xbf:
            ranged.add(i)
    prefixes = {0x26, 0x2e, 0x36, 0x3e, 0xf2, 0xf3}
    # Require prefix parsing to actually exist in executeOne.
    for opcode in prefixes:
        token = f"opcode==0x{opcode:02x}"
        if token not in body:
            raise AssertionError(f"prefix dispatch missing: {opcode:02x}")
    return direct, ranged, prefixes

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=pathlib.Path)
    args = parser.parse_args()
    direct, ranged, prefixes = collect(args.source.read_text(encoding="utf-8"))
    # Guards against accidentally dropping already supported instruction families.
    for op in (0x04, 0x27, 0x2f, 0x37, 0x3f, 0x80, 0x81,
               0xc4, 0xc5, 0xd0, 0xd3, 0xd4, 0xd5, 0xd7, 0xf6, 0xf7, 0xfe, 0xff):
        assert op in direct, f"missing explicit opcode {op:02X}"
    assert 0x60 not in direct | ranged | prefixes
    covered = direct | ranged | prefixes
    print("8088 primary opcode dispatch inventory")
    print("    " + " ".join(f"{i:X}" for i in range(16)))
    for row in range(16):
        cells = []
        for col in range(16):
            op = row * 16 + col
            cells.append("p" if op in prefixes else "+" if op in direct
                         else "r" if op in ranged else ".")
        print(f"{row:X}0: " + " ".join(cells))
    missing = [f"{op:02X}" for op in range(256) if op not in covered]
    print(f"Dispatch recognition: {len(covered)}/256 (not instruction completeness)")
    print("Unrecognized primary opcode bytes: " + " ".join(missing))
    print("NOTE: grouped ModR/M operations, prefix combinations, 8088 quirks,")
    print("FLAGS accuracy, timings and V20-specific functions are not validated.")

if __name__ == "__main__":
    main()
