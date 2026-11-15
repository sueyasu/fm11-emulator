#!/usr/bin/env python3
"""Run SingleStepTests/v20 native V1 hardware-generated tests.

Download reference data separately:
  git clone --filter=blob:none --sparse https://github.com/SingleStepTests/v20 external/v20-tests
  git -C external/v20-tests sparse-checkout set v1_native

Examples:
  python3 tests/run_v20_singlestep.py --opcode 00 --limit 100
  python3 tests/run_v20_singlestep.py --opcode 0F10 --opcode 0F31
  python3 tests/run_v20_singlestep.py --all --limit 100
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

from run_8088_singlestep import MAGIC, REG_ORDER


def pack_test(case, flag_mask):
    initial=case["initial"]
    initial_regs=initial["regs"]
    final_regs=dict(initial_regs)
    final_regs.update(case["final"].get("regs", {}))
    output=bytearray()
    for regs in (initial_regs,final_regs):
        output.extend(struct.pack("<14H",*(int(regs[k])&0xffff for k in REG_ORDER)))
    data=case.get("bytes", [])
    mask=flag_mask&0xffff
    # OF is undefined for CL/immediate shifts with multiple steps.
    # This is independent of the metadata flags mask.
    index=0
    while index<len(data) and data[index] in (0x26,0x2e,0x36,0x3e,0xf0,0xf2,0xf3,0x64,0x65):
        index+=1
    opcode=data[index] if index<len(data) else None
    if opcode in (0xd2,0xd3) and (initial_regs["cx"]&0xff)>1:
        mask &= ~0x0800
    if opcode in (0xc0,0xc1) and len(data)>index+2 and data[-1]>1:
        mask &= ~0x0800
    output.extend(struct.pack("<H",mask))
    # The native core executes REP string instructions incrementally.
    prefixes=data[:index]
    repeated=bool(any(x in (0xf2,0xf3,0x64,0x65) for x in prefixes) and
                  opcode in (*range(0xa4,0xa8),*range(0xaa,0xb0),*range(0x6c,0x70)))
    output.extend(struct.pack("<B",int(repeated)))
    for ram in (initial.get("ram",[]),case["final"].get("ram",[])):
        output.extend(struct.pack("<I",len(ram)))
        for address,value in ram:
            output.extend(struct.pack("<IB",int(address),int(value)))
    return output


def run_one(path,runner,metadata,limit):
    opcode=path.name[:-8].upper()
    group=opcode.split(".")
    spec=metadata["opcodes"].get(group[0],{})
    if len(group)>1:
        spec=spec.get("reg",{}).get(group[1],{})
    if spec.get("status") in ("undocumented","undefined","fpu","prefix","extension"):
        return "SKIP",opcode,spec.get("status"),0
    mask=int(spec.get("flags-mask",0xffff))
    with gzip.open(path,"rt",encoding="utf-8") as source:
        cases=json.load(source)
    if limit:
        cases=cases[:limit]
    with tempfile.TemporaryDirectory(prefix="fm11-v20-") as tmp:
        path_bin=Path(tmp)/"tests.bin"
        with path_bin.open("wb") as stream:
            stream.write(MAGIC)
            stream.write(struct.pack("<I",len(cases)))
            for case in cases:
                stream.write(pack_test(case,mask))
        done=subprocess.run([str(runner.resolve()),"--cpu","v20",str(path_bin)],
                            capture_output=True,text=True,check=False)
    return ("PASS" if done.returncode==0 else "FAIL"),opcode,(done.stdout+done.stderr).strip(),len(cases)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--data-dir",type=Path,default=Path("external/v20-tests/v1_native"))
    parser.add_argument("--runner",type=Path,default=Path("build/fm11-8088-singlestep"))
    parser.add_argument("--opcode",action="append",help="hex opcode, e.g. 00, C0.4, 0F31")
    parser.add_argument("--all",action="store_true")
    parser.add_argument("--limit",type=int,default=100,help="cases per opcode; 0 = all")
    parser.add_argument("--fail-fast",action="store_true")
    args=parser.parse_args()
    if args.limit<0:
        parser.error("--limit must be nonnegative")
    if not args.runner.is_file():
        parser.error(f"runner not found: {args.runner}")
    if not args.data_dir.is_dir():
        parser.error(f"V20 data missing: {args.data_dir}")
    metadata=json.loads((args.data_dir/"metadata.json").read_text(encoding="utf-8"))
    if args.all:
        paths=sorted(args.data_dir.glob("*.json.gz"))
    else:
        paths=[]
        for name in args.opcode or ["00"]:
            if not re.fullmatch(r"(?:[0-9a-fA-F]{2}|0[fF][0-9a-fA-F]{2})(?:\.[0-7])?",name):
                parser.error(f"invalid opcode: {name}")
            upper=name.upper()
            matches=sorted(args.data_dir.glob(f"{upper}.json.gz"))
            if "." not in name:
                matches+=sorted(args.data_dir.glob(f"{upper}.[0-7].json.gz"))
            if not matches:
                parser.error(f"opcode data not found: {name}")
            paths+=matches
    counts={"PASS":0,"FAIL":0,"SKIP":0}
    for path in paths:
        status,opcode,detail,count=run_one(path,args.runner,metadata,args.limit)
        counts[status]+=1
        print(f"{opcode}: {status} ({count} cases) {detail}",flush=True)
        if status=="FAIL" and args.fail_fast:
            break
    print("OPCODES "+" ".join(f"{k}={v}" for k,v in counts.items()))
    return 1 if counts["FAIL"] else 0


if __name__=="__main__":
    sys.exit(main())
