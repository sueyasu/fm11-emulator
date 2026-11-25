#!/usr/bin/env python3
"""Generate a 128-KiB test JIS78 level-2 FM-11 Kanji ROM from an OFL font.

This uses the selector computed by the standard sub-ROM, NOT the earlier
(ku-48)*96+(ten-1) test-only packing. Requires Pillow only for generation.
Original Fujitsu character glyphs are not distributed.
"""
import argparse
import csv
import hashlib
from pathlib import Path
from fm11_kanji2_address import (
    FIRST_KU, LAST_KU, FIRST_TEN, LAST_TEN, jis_to_fm11_level2_slot
)

SIZE = 0x20000

def generate(font_path, font_index=0):
    from PIL import Image, ImageDraw, ImageFont
    font = ImageFont.truetype(str(font_path), 16, index=font_index)
    rom = bytearray(SIZE)
    mapping = []
    occupied = set()
    for ku in range(FIRST_KU, LAST_KU + 1):
        for ten in range(FIRST_TEN, LAST_TEN + 1):
            slot = jis_to_fm11_level2_slot(ku, ten)
            if slot in occupied:
                raise RuntimeError(f"ROM address collision: {ku}-{ten} -> {slot:04X}")
            occupied.add(slot)
            ch = bytes([ku + 0xa0, ten + 0xa0]).decode("euc_jp")
            if len(ch) != 1:
                raise ValueError(f"invalid JIS mapping: {ku}-{ten}")
            left, top, right, bottom = font.getbbox(ch, anchor="lt")
            glyph = Image.new("L", (max(1, right-left), max(1, bottom-top)), 0)
            ImageDraw.Draw(glyph).text((-left, -top), ch, font=font, fill=255,
                                       anchor="lt", stroke_width=0)
            if glyph.width > 16 or glyph.height > 16:
                glyph = glyph.resize((min(16, glyph.width), min(16, glyph.height)),
                                     Image.Resampling.LANCZOS)
            image = Image.new("L", (16, 16), 0)
            image.paste(glyph, ((16-glyph.width)//2, (16-glyph.height)//2))
            bitmap = image.point(lambda v: 255 if v >= 128 else 0, mode="1")
            packed = bytearray(32)
            for y in range(16):
                for x in range(16):
                    if bitmap.getpixel((x,y)):
                        packed[y*2+x//8] |= 0x80 >> (x%8)
            rom[slot*32:(slot+1)*32] = packed
            mapping.append((slot, f"{0x1000|slot:04X}", ku, ten,
                            f"U+{ord(ch):04X}", ch))
    assert len(occupied) == 3384
    return rom, mapping

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--font", type=Path, required=True)
    parser.add_argument("--index", type=int, default=0)
    parser.add_argument("--output", type=Path, default=Path("kanji2_fm11.bin"))
    parser.add_argument("--csv", type=Path)
    args = parser.parse_args()
    rom, mapping = generate(args.font, args.index)
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_bytes(rom)
    if args.csv:
        args.csv.parent.mkdir(parents=True,exist_ok=True)
        with args.csv.open("w", newline="", encoding="utf-8") as out:
            w = csv.writer(out)
            w.writerow(["slot_decimal", "AFF4_AFF5_hex", "JIS_ku",
                        "JIS_ten", "unicode", "character"])
            w.writerows(mapping)
    print(f"{args.output}: {len(rom)} bytes, {len(mapping)} JIS78 glyphs")
    print("sha256:",hashlib.sha256(rom).hexdigest())

if __name__ == "__main__":
    main()
