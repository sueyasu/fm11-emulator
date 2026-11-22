#!/usr/bin/env python3
"""Report manifest case coverage without claiming opcode completeness."""
import argparse
import csv
from collections import defaultdict
from pathlib import Path

CPUS = ("6809", "6309", "6309-native", "z80", "8088", "v20")

def read_cases(path):
    with path.open(newline="", encoding="utf-8") as f:
        rows = list(csv.DictReader(line for line in f if not line.startswith("#")))
    expected = {"cpu", "name", "bytes", "expected", "status", "reference"}
    if not rows:
        raise ValueError("empty manifest")
    for i, row in enumerate(rows, 1):
        if set(row) != expected or row["cpu"] not in CPUS:
            raise ValueError(f"case {i}: invalid columns or CPU")
        if row["status"] not in {"baseline", "verified"}:
            raise ValueError(f"case {i}: invalid status")
        if not row["reference"]:
            raise ValueError(f"case {i}: missing reference")
        int(row["expected"])
        bytes.fromhex(row["bytes"])
    return rows

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("manifest", nargs="?", type=Path,
                        default=Path(__file__).resolve().parent / "data" / "cpu_timing_cases.csv")
    parser.add_argument("--missing", action="store_true",
                        help="list first opcode bytes with no cases (not a legal-opcode inventory)")
    args = parser.parse_args()
    by_cpu = defaultdict(list)
    for row in read_cases(args.manifest):
        by_cpu[row["cpu"]].append(row)
    print("Stage 1-A single-instruction fixture inventory")
    for cpu in CPUS:
        rows = by_cpu[cpu]
        verified = sum(row["status"] == "verified" for row in rows)
        heads = {bytes.fromhex(row["bytes"])[0] for row in rows}
        print(f"{cpu:12s} cases={len(rows):3d} verified={verified:3d} "
              f"baseline={len(rows)-verified:3d} first_bytes={len(heads):3d}/256")
        if args.missing:
            missing = sorted(set(range(256)) - heads)
            print("  No fixture starting with: " + " ".join(f"{n:02X}" for n in missing))
    print("NOTE: 256 includes invalid opcodes and prefix families; counts are "
          "fixture coverage only, not validated instruction coverage.")

if __name__ == "__main__":
    main()
