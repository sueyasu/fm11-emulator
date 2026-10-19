#!/usr/bin/env python3
"""Extract the 4096-byte ANK glyph array from the user's original CharRom.h."""
import argparse
import pathlib
import re

p=argparse.ArgumentParser()
p.add_argument("header",type=pathlib.Path,help="original Fm11/CharRom.h")
p.add_argument("output",type=pathlib.Path,help="destination ANK binary")
args=p.parse_args()
text=args.header.read_text(encoding="latin-1")
match=re.search(r"\\bAnkRom\\s*\\[\\s*\\]\\s*=\\s*\\{(.*?)\\}",text,re.S)
if not match:
    p.error("AnkRom initializer not found")
values=re.findall(r"0x([0-9A-Fa-f]{1,2})\\b",match.group(1))
if len(values)!=4096:
    p.error(f"expected 4096 glyph bytes, got {len(values)}")
args.output.parent.mkdir(parents=True,exist_ok=True)
args.output.write_bytes(bytes(int(v,16) for v in values))
print(args.output)
