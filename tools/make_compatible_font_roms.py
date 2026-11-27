#!/usr/bin/env python3
"""Generate independent FM-11 compatible font ROMs from freely licensed fonts."""
import argparse
import hashlib
from pathlib import Path
from fm11_kanji2_address import jis_to_fm11_level2_slot

SWAPS = [(0x3033,0x724d),(0x3229,0x7274),(0x3342,0x695a),
         (0x3349,0x5978),(0x3376,0x635e),(0x3443,0x5e75),
         (0x3452,0x6b5d),(0x375b,0x7074),(0x395c,0x6268),
         (0x3c49,0x6922),(0x3f59,0x7057),(0x4128,0x6c4d),
         (0x445b,0x5464),(0x4557,0x626a),(0x456e,0x5b6d),
         (0x4573,0x5e39),(0x4676,0x6d6e),(0x4768,0x6a24),
         (0x4930,0x5b58),(0x4b79,0x5056),(0x4c79,0x692e),
         (0x4f36,0x6446)]
JIS78 = {a:b for pair in SWAPS for a,b in (pair,pair[::-1])}
JIS78.update({0x3646:0x7421,0x4b6a:0x7422,0x4d5a:0x7423,0x6076:0x7424})

def bdf(path, width, height):
    """Normalize BBX offsets to the font cell, preserving the baseline."""
    result = {}
    bbox = None
    encoding = None
    box = None
    rows = None
    for line in path.read_text(encoding="latin-1").splitlines():
        if line.startswith("FONTBOUNDINGBOX "):
            bbox = tuple(map(int,line.split()[1:]))
            if bbox[:2] != (width,height):
                raise ValueError(f"{path}: expected {width}x{height}, got {bbox}")
        elif line.startswith("STARTCHAR "):
            encoding = box = rows = None
        elif line.startswith("ENCODING "):
            encoding = int(line.split()[1])
        elif line.startswith("BBX "):
            box = tuple(map(int,line.split()[1:]))
        elif line == "BITMAP":
            rows = []
        elif line == "ENDCHAR":
            if encoding is None or encoding < 0 or rows is None:
                continue
            w,h,x,y = box
            if len(rows) != h:
                raise ValueError(f"{path}: bad bitmap at {encoding}")
            cell = [0]*height
            for j,row in enumerate(rows):
                dest = bbox[3]+height-y-h+j
                for i in range(w):
                    col = x-bbox[2]+i
                    if 0<=dest<height and 0<=col<width and row & (1<<(((w+7)//8)*8-1-i)):
                        cell[dest] |= 1<<(width-1-col)
            result[encoding] = cell
            rows = None
        elif rows is not None:
            rows.append(int(line,16))
    if not result:
        raise ValueError(f"{path}: no glyphs")
    return result

def hex_font(path):
    result = {}
    for line in path.read_text().splitlines():
        if ":" in line:
            c, data = line.split(":",1)
            if len(data)==32:
                result[int(c,16)] = list(bytes.fromhex(data))
    return result

def stretch(rows, height, top):
    """Extend strokes by inserting rows, favoring identical thin rows."""
    used = [i for i,r in enumerate(rows) if r]
    if not used:
        return [0]*16
    work = list(rows[used[0]:used[-1]+1])
    while len(work)<height:
        # Insert between existing rows; never duplicate an isolated horizontal bar.
        choices = [(abs(i-(len(work)-1)/2),i) for i in range(len(work)-1)
                   if work[i]==work[i+1] and work[i] and work[i].bit_count()<=4]
        if choices:
            i=min(choices)[1]
            work.insert(i,work[i])
        else:
            # For sloping strokes, interpolate the overlapping pixels only.
            i=min(range(len(work)-1),key=lambda i:(
                (work[i]^work[i+1]).bit_count(),
                abs(i-(len(work)-1)/2))) if len(work)>1 else 0
            work.insert(i+1,(work[i]&work[min(i+1,len(work)-1)]) or work[i])
    if len(work)>16:
        raise ValueError("glyph too tall")
    top=max(0,min(top,16-len(work)))
    return [0]*top+work+[0]*(16-top-len(work))

def adjust_an(c, rows):
    if chr(c).isupper() or chr(c).isdigit():
        return stretch(rows,13,1)
    if chr(c).islower():
        if chr(c) in "gjpqy":
            return stretch(rows,12,4)
        if chr(c) in "bdfhkl":
            return stretch(rows,13,1)
        if chr(c) in "it":
            return stretch(rows,12,2)
        return stretch(rows,10,4)
    if chr(c) in "()[]{}":
        return stretch(rows,13,1)
    return rows

def reverse_byte(x):
    return int(f"{x:08b}"[::-1],2)

def make_ank(an,k,graphics,half):
    g={c:[0]*16 for c in (0,0x20,0xa0,0xff)}
    for c in range(0x21,0x7f):
        source={0x5c:0xa5,0x7e:0x203e}.get(c,c)
        g[c]=adjust_an(c,an[source])
    for c in range(0xa1,0xe0):
        g[c]=k[c]
    # Two-letter control mnemonics, independently composed from small 3x5 letters.
    tiny = {
        'A':[2,5,7,5,5],'B':[6,5,6,5,6],'C':[3,4,4,4,3],
        'D':[6,5,5,5,6],'E':[7,4,6,4,7],'F':[7,4,6,4,4],
        'H':[5,5,7,5,5],'I':[7,2,2,2,7],'K':[5,5,6,5,5],
        'L':[4,4,4,4,7],'M':[5,7,7,5,5],'N':[5,7,7,7,5],
        'O':[2,5,5,5,2],'Q':[2,5,5,7,3],'R':[6,5,6,5,5],
        'S':[3,4,2,1,6],'T':[7,2,2,2,2],'U':[5,5,5,5,7],
        'X':[5,5,2,5,5],'Z':[7,1,2,4,7],
        '1':[2,6,2,2,7],'2':[6,1,2,4,7],'3':[6,1,2,1,6],
        '4':[5,5,7,1,1]}
    names="SH SX EX ET EQ AK BL BS HT LF HM CL CR SO SI DE D1 D2 D3 D4 NK SN EB CN EM SB EC".split()
    for c,name in enumerate(names,1):
        g[c]=[0]+[r<<3 for r in tiny[name[0]]]+[0]*4+[r<<2 for r in tiny[name[1]]]+[0]
    g[0x7f]=[0]+[r<<3 for r in tiny['D']]+[0]*4+[r<<2 for r in tiny['L']]+[0]
    for c,ch in enumerate("→←↑↓",0x1c):
        g[c]=graphics[ord(ch)]
    for i in range(8):
        g[0x80+i]=[0]*(14-2*i)+[255]*(2+2*i)
    for i in range(7):
        g[0x88+i]=[((1<<(i+1))-1)<<(7-i)]*16
    # Connected strokes meet the cell boundaries.
    def line(u=False,d=False,l=False,r=False,x=3,y=7):
        return [( (1<<(7-x)) if ((u and j<=y) or (d and j>=y)) else 0)
                | ((255<<(7-x))&255 if l and j==y else 0)
                | ((1<<(8-x))-1 if r and j==y else 0) for j in range(16)]
    for c,flags in zip(range(0x8f,0x94),[(1,1,1,1),(1,0,1,1),(0,1,1,1),(1,1,1,0),(1,1,0,1)]):
        g[c]=line(*flags)
    g[0x94]=[255]+[0]*15
    g[0x95]=line(l=True,r=True)
    g[0x96]=[128]*16
    g[0x97]=[1]*16
    for c,flags in zip(range(0x98,0x9c),[(0,1,0,1),(0,1,1,0),(1,0,0,1),(1,0,1,0)]):
        g[c]=line(*flags)
    for c,ch in enumerate("╭╮╰╯",0x9c):
        g[c]=graphics[ord(ch)]
    for c,ch in enumerate("═╞╪╡◢◣◥◤♠♥♦♣●○╱╲╳",0xe0):
        g[c]=graphics[ord(ch)]
    for c,ch in enumerate("円年月日時分秒千万区町村人",0xf1):
        g[c]=stretch(half[ord(ch)],13,1)
    g[0xfe]=graphics[ord("░")]
    if set(g)!=set(range(256)):
        raise ValueError("incomplete ANK")
    return bytes(reverse_byte(r) for c in range(256) for r in g[c])

def level1_slot(ku,ten):
    if 1<=ku<=7:
        return (0,0x200,0x100)[ten//32] + ku*32 + ten%32
    if 16<=ku<=47:
        return 0x400 + ((ku-16)//16)*0x600 + (ten//32)*0x200 + (ku%16)*32 + ten%32
    raise ValueError((ku,ten))

def make_kanji(font):
    # JIS78 non-kanji and first/second-level kanji.
    # 1983 additions in row 2 and row 8 are intentionally excluded.
    banks=[bytearray(bytes.fromhex("0001")*16*4096) for _ in range(2)]
    seen=[set(),set()]
    for ku in list(range(1,8))+list(range(16,84)):
        for ten in range(1,95):
            if ku==2 and ten>14 or ku==47 and ten>51:
                continue
            jis=((ku+32)<<8)|(ten+32)
            try:
                bytes((ku+160,ten+160)).decode("euc_jp")
            except UnicodeDecodeError:
                continue
            bank=int(ku>=48)
            slot=jis_to_fm11_level2_slot(ku,ten) if bank else level1_slot(ku,ten)
            if slot in seen[bank] or not 0<=slot<4096:
                raise ValueError(f"collision: {jis:04x}")
            seen[bank].add(slot)
            rows=font[JIS78.get(jis,jis)]
            banks[bank][slot*32:slot*32+32]=b"".join(r.to_bytes(2,"big") for r in rows)
    if list(map(len,seen)) != [3418,3384]:
        raise ValueError(f"unexpected JIS78 coverage: {list(map(len,seen))}")
    return banks

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for n in ("terminus","shinonome-ank","shinonome-kanji","unscii","k8x12","output"):
        p.add_argument("--"+n,type=Path,required=True)
    a=p.parse_args()
    ank=make_ank(bdf(a.terminus,8,16),bdf(a.shinonome_ank,8,16),
                 hex_font(a.unscii),bdf(a.k8x12,8,12))
    kanji,kanji2=make_kanji(bdf(a.shinonome_kanji,16,16))
    a.output.mkdir(parents=True,exist_ok=True)
    for name,data in zip(("ank.bin","kanji.bin","kanji2.bin"),(ank,kanji,kanji2)):
        (a.output/name).write_bytes(data)
        print(name,len(data),hashlib.sha256(data).hexdigest())

if __name__=="__main__":
    main()
