#!/usr/bin/env python3
"""Run SingleStepTests/8088 V2 against Main8088Board::Core.

Download test data separately (not vendored):
  git clone --filter=blob:none --sparse https://github.com/SingleStepTests/8088 external/8088-tests
  git -C external/8088-tests sparse-checkout set v2

Use:
  python3 tests/run_8088_singlestep.py --data-dir external/8088-tests/v2 \
      --runner build/fm11-8088-singlestep --opcode 00 --limit 100
  python3 tests/run_8088_singlestep.py --data-dir external/8088-tests/v2 \
      --runner build/fm11-8088-singlestep --all --limit 100
"""
import argparse
import gzip
import json
from pathlib import Path
import re
import struct
import subprocess
import sys
import tempfile

REG_ORDER = ("ax", "bx", "cx", "dx", "sp", "bp", "si", "di",
             "es", "cs", "ss", "ds", "ip", "flags")
MAGIC = b"SST8088A"


def pack_test(test, flag_mask):
    initial = test["initial"]
    final = test["final"]
    initial_regs = initial["regs"]
    final_regs = dict(initial_regs)
    final_regs.update(final.get("regs", {}))
    result = bytearray()
    for regs in (initial_regs, final_regs):
        result.extend(struct.pack("<14H", *(int(regs[k]) & 0xFFFF for k in REG_ORDER)))
    bytes_ = test.get("bytes", [])
    # The reference README explicitly warns that OF is undefined for
    # D2/D3 with CL > 1. metadata.json's mask cannot express this
    # per-test condition. Preserve OF comparison for CL=0 or 1.
    first_opcode = next((b for b in bytes_
                         if b not in (0x26, 0x2E, 0x36, 0x3E, 0xF0)),
                        None)
    case_mask = flag_mask & 0xFFFF
    if first_opcode in (0xD2, 0xD3) and (initial_regs["cx"] & 0xFF) > 1:
        case_mask &= ~0x0800
    # Only architecturally defined bits are compared by the C++ runner.
    result.extend(struct.pack("<H", case_mask))
    # REP prefixed strings: the FM-11 core runs one iteration per instruction.
    # The actual instruction may carry a segment override prefix.
    repeat = False
    for b in bytes_:
        if b in (0x26, 0x2E, 0x36, 0x3E, 0xF0):
            continue
        if b in (0xF2, 0xF3):
            repeat = True
            continue
        break
    result.extend(struct.pack("<B", int(repeat)))
    initial_ram = initial.get("ram", [])
    result.extend(struct.pack("<I", len(initial_ram)))
    for addr, value in initial_ram:
        result.extend(struct.pack("<IB", int(addr), int(value)))
    final_ram = final.get("ram", [])
    result.extend(struct.pack("<I", len(final_ram)))
    for addr, value in final_ram:
        result.extend(struct.pack("<IB", int(addr), int(value)))
    return result


def run_one(path, runner, metadata, limit):
    opcode = path.name[:-8].upper()  # e.g. "00" or "80.0" (.json.gz)
    group = opcode.split(".")
    spec = metadata.get("opcodes", {}).get(group[0], {})
    if len(group) > 1:
        spec = spec.get("reg", {}).get(group[1], {})
    if spec.get("status") in ("undocumented", "undefined", "fpu", "prefix"):
        return ("SKIP", opcode, spec.get("status"), 0)
    mask = int(spec.get("flags-mask", 0xFFFF))
    with gzip.open(path, "rt", encoding="utf-8") as src:
        cases = json.load(src)
    if not isinstance(cases, list):
        raise ValueError(f"{path}: expected an array of cases")
    if limit:
        cases = cases[:limit]
    with tempfile.TemporaryDirectory(prefix="fm11-8088-") as tmp:
        packed = Path(tmp) / "cases.bin"
        with packed.open("wb") as output:
            output.write(MAGIC)
            output.write(struct.pack("<I", len(cases)))
            for case in cases:
                output.write(pack_test(case, mask))
        result = subprocess.run([str(runner.resolve()), str(packed)],
                                capture_output=True, text=True, check=False)
    output = (result.stdout + result.stderr).strip()
    status = "PASS" if result.returncode == 0 else "FAIL"
    return status, opcode, output, len(cases)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--data-dir", type=Path, default=Path("external/8088-tests/v2"))
    parser.add_argument("--runner", type=Path, default=Path("build/fm11-8088-singlestep"))
    parser.add_argument("--opcode", action="append", help="hex opcode (repeatable)")
    parser.add_argument("--all", action="store_true", help="run every available V2 opcode")
    parser.add_argument("--limit", type=int, default=100,
                        help="test cases per opcode; 0 = all (default: 100)")
    parser.add_argument("--fail-fast", action="store_true")
    args = parser.parse_args()
    if args.limit < 0:
        parser.error("--limit must be nonnegative")
    if not args.runner.is_file():
        parser.error(f"runner not found: {args.runner} (build it with cmake)")
    if not args.data_dir.is_dir():
        parser.error(f"V2 data missing: {args.data_dir}")
    meta_file = args.data_dir / "metadata.json"
    if not meta_file.is_file():
        parser.error(f"metadata.json missing: {meta_file}")
    metadata = json.loads(meta_file.read_text(encoding="utf-8"))
    if args.all:
        paths = sorted(args.data_dir.glob("*.json.gz"))
    else:
        names = args.opcode or ["00"]
        paths = []
        for name in names:
            if not re.fullmatch(r"[0-9a-fA-F]{2}(?:\.[0-7])?", name):
                parser.error(f"invalid opcode: {name}")
            matches = sorted(args.data_dir.glob(f"{name.upper()}.json.gz"))
            if "." not in name:
                matches += sorted(args.data_dir.glob(f"{name.upper()}.[0-7].json.gz"))
            if not matches:
                parser.error(f"opcode data not found: {name}")
            paths.extend(matches)
    if not paths:
        parser.error("no test files found")
    bad = 0
    counts = {"PASS": 0, "FAIL": 0, "SKIP": 0}
    for path in paths:
        status, opcode, output, count = run_one(path, args.runner, metadata, args.limit)
        counts[status] += 1
        print(f"{opcode}: {status} ({count} cases) {output}", flush=True)
        if status == "FAIL":
            bad += 1
            if args.fail_fast:
                break
    print("OPCODES " + " ".join(f"{k}={v}" for k, v in counts.items()))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
