#!/usr/bin/env python3
"""Audit every legal MC6809 opcode against Appendix C and the interpreter.

This is a static source/fixture audit. Runtime indexed-postbyte checking
is performed separately by cpu6809_indexed_postbytes.cpp.
"""
import csv
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "tests/data/mc6809_reference.csv"
TABLE = ROOT / "src/core/sim09/45_timing.inc"
DISPATCH = [
    ROOT / "src/core/sim09/50_execute_01.inc",
    ROOT / "src/core/sim09/50_execute_02.inc",
    ROOT / "src/core/sim09/50_execute_03.inc",
]
COUNTS = (221, 38, 9)
REFERENCE = "Motorola M6809PM/AD Appendix C, Table C-1"

def read_reference():
    with MANIFEST.open(newline="", encoding="utf-8") as fh:
        records = list(csv.DictReader(
            line for line in fh if not line.startswith("#")))
    ref = {}
    for row in records:
        if set(row) != {"opcode", "base_cycles"}:
            raise ValueError(f"invalid CSV fields: {row}")
        op = int(row["opcode"], 16)
        cycles = int(row["base_cycles"])
        if op in ref or not 0 < cycles <= 32:
            raise ValueError(f"duplicate opcode or invalid cycles: {op:04X}")
        if op > 0xff and not (0x1000 <= op <= 0x11ff):
            raise ValueError(f"unsupported opcode page: {op:04X}")
        if op in (0x10, 0x11):
            raise ValueError("prefix bytes are not standalone instructions")
        ref[op] = cycles
    actual_counts = (
        sum(op < 0x100 for op in ref),
        sum(0x1000 <= op < 0x1100 for op in ref),
        sum(0x1100 <= op < 0x1200 for op in ref),
    )
    if actual_counts != COUNTS:
        raise ValueError(f"reference inventory incomplete: {actual_counts}")
    return ref

def read_cycle_tables():
    source = TABLE.read_text(encoding="utf-8")
    out = {}
    for name, prefix in (
        ("k6809BaseCycles", 0),
        ("k6809Page10Cycles", 0x1000),
        ("k6309Page11Cycles", 0x1100),
    ):
        m = re.search(r"\b" + name + r"\[256\]\s*=\s*\{(.*?)\};",
                      source, re.S)
        if not m:
            raise ValueError(f"missing cycle table {name}")
        text = re.sub(r"//[^\n]*", "", m.group(1))
        values = [int(v.strip()) for v in text.split(",") if v.strip()]
        if len(values) != 256:
            raise ValueError(f"{name}: expected 256 entries, got {len(values)}")
        out.update({prefix + i: value for i, value in enumerate(values)})
    return out

def read_dispatch():
    dispatch = set()
    for path in DISPATCH:
        source = path.read_text(encoding="utf-8")
        dispatch.update(int(op, 16) for op in
                        re.findall(r"\bOPDEF\(0x([0-9a-fA-F]+)", source))
    return dispatch

def main():
    ref = read_reference()
    timing = read_cycle_tables()
    dispatch = read_dispatch()
    issues = []
    for op, expected in sorted(ref.items()):
        if op not in dispatch:
            issues.append(f"{op:04X}: legal MC6809 opcode absent from dispatch")
        actual = timing[op]
        if actual != expected:
            issues.append(f"{op:04X}: base cycle expected {expected}, got {actual}")
    for op in (0x10, 0x11):
        if timing[op] != 0:
            issues.append(f"{op:04X}: prefix must not have standalone cycles")
    print(f"MC6809 reference: {len(ref)} legal opcodes "
          f"(base={COUNTS[0]}, page10={COUNTS[1]}, page11={COUNTS[2]})")
    print(f"Dispatch coverage: {sum(op in dispatch for op in ref)}/{len(ref)}")
    print(f"Cycle matches: {sum(timing[op] == c for op, c in ref.items())}/{len(ref)}")
    print(f"Reference: {REFERENCE}")
    for issue in issues:
        print("FAIL", issue, file=sys.stderr)
    return bool(issues)

if __name__ == "__main__":
    sys.exit(main())
