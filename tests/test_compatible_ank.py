#!/usr/bin/env python3
"""Native AN/K glyphs remain unchanged except for the slashed zero."""
import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"tools"))
from make_compatible_font_roms import make_ank, reverse_byte, slashed_zero

k={c:[0x42]*16 for c in list(range(0x21,0x7f))+list(range(0xa1,0xe0))}
k[0x30]=list(bytes.fromhex("00182424424242424242422424180000"))
graphics={ord(c):[0x18]*16 for c in "→←↑↓╭╮╰╯═╞╪╡◢◣◥◤♠♥♦♣●○╱╲╳░"}
half={ord(c):[0x42]*12 for c in "円年月日時分秒千万区町村人"}
rom=make_ank(k,graphics,half)
assert len(rom)==4096
for c,rows in k.items():
    actual=list(map(reverse_byte,rom[c*16:(c+1)*16]))
    assert actual==(slashed_zero(rows) if c==0x30 else rows),hex(c)
assert slashed_zero(k[0x30])!=k[0x30]
assert all((a&b)==a for a,b in zip(k[0x30],slashed_zero(k[0x30])))
print("PASS")
