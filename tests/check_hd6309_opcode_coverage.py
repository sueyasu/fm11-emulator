#!/usr/bin/env python3
"""Audit the HD6309 source-derived opcode inventory (not a datasheet audit)."""
import csv
import re
import sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/"src/core/sim09"
BASE=ROOT/"tests/data/hd6309_opcode_baseline.csv"
FILES=["50_execute_01.inc","50_execute_02.inc","50_execute_03.inc"]
TABLE=["k6809BaseCycles","k6809Page10Cycles","k6309Page11Cycles"]
EXPECTED=(235,118,83)

def main():
    dispatch={}
    for filename in FILES:
        text=(SOURCE/filename).read_text(encoding="utf-8")
        for match in re.finditer(r"\bOPDEF\(0x([0-9a-fA-F]+),\s*([^\)]+)\)",text):
            op=int(match.group(1),16)
            if match.group(2).strip()=="O_ERR":continue
            if op in dispatch:raise ValueError(f"duplicate opcode {op:04X}")
            dispatch[op]=filename
    counts=(
        sum(x<256 for x in dispatch),
        sum(0x1000<=x<0x1100 for x in dispatch),
        sum(0x1100<=x<0x1200 for x in dispatch),
    )
    if counts!=EXPECTED:raise ValueError(f"opcode family counts {counts} != {EXPECTED}")
    src=(SOURCE/"45_timing.inc").read_text(encoding="utf-8")
    values={}
    for index,name in enumerate(TABLE):
        match=re.search(r"\b"+name+r"\[256\]\s*=\s*\{(.*?)\};",src,re.S)
        if not match:raise ValueError("missing table "+name)
        clean=re.sub(r"//[^\n]*","",match.group(1))
        table=[int(x.strip()) for x in clean.split(",") if x.strip()]
        if len(table)!=256:raise ValueError("not 256 entries: "+name)
        values[index]=table
    with BASE.open(newline="",encoding="utf-8") as f:
        rows=list(csv.DictReader(l for l in f if not l.startswith("#")))
    baseline={}
    for row in rows:
        if set(row)!={"opcode","base_cycles","status"} or row["status"]!="baseline":
            raise ValueError(f"bad manifest record {row}")
        op=int(row["opcode"],16)
        if op in baseline:raise ValueError(f"duplicate baseline {op:04X}")
        baseline[op]=int(row["base_cycles"])
    issues=[]
    if set(baseline)!=set(dispatch):
        issues.append(f"baseline/dispatch mismatch missing={sorted(set(dispatch)-set(baseline))} "
                      f"extra={sorted(set(baseline)-set(dispatch))}")
    for op in sorted(dispatch):
        page=0 if op<256 else 1 if op<0x1100 else 2
        clock=values[page][op&255]
        if clock<=0:issues.append(f"{op:04X}: unclassified zero cycle")
        if baseline.get(op)!=clock:
            issues.append(f"{op:04X}: baseline={baseline.get(op)} current={clock}")
    print(f"HD6309: {len(dispatch)} opcode forms, pages={counts}, "
          f"330 undefined encodings excluding two prefix bytes")
    print("NOTE: values are SOURCE-DERIVED baselines, not 436 independently verified timings")
    for item in issues:print("FAIL",item,file=sys.stderr)
    return bool(issues)
if __name__=="__main__":
    sys.exit(main())
