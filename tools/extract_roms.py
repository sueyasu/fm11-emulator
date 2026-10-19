#!/usr/bin/env python3
"""Extract ROM bytes from the original FM-11 emulator source archive.

Usage:
    python3 tools/extract_roms.py /path/to/fm11_x64.zip
    python3 tools/extract_roms.py /path/to/Fm11/
Output: external/roms/*.bin (ignored by Git).
No ROM bytes are part of the repository.
"""
import argparse
from pathlib import Path
import re
import zipfile

ROMS = (
    ("BootRom.h", "BootRom", "boot6809.bin", 0x1000),
    ("BootRom86.h", "BootRom86", "boot8088.bin", 0x1000),
    ("SubRom.h", "SubRom", "display6809.bin", 0x5000),
    ("CharRom.h", "AnkRom", "ank.bin", 0x1000),
    ("KanRom.h", "KanRom", "kanji.bin", 0x20000),
)

def extract(data, symbol, expected):
    source = data.decode("latin-1")
    found = re.search(r"\b" + re.escape(symbol) + r"\s*\[\s*\]\s*=\s*\{(.*?)\}", source, re.S)
    if found is None:
        raise ValueError(f"missing ROM array {symbol}")
    body = re.sub(r"/\*.*?\*/|//[^\n]*", "", found.group(1), flags=re.S)
    values = re.findall(r"0[xX][0-9A-Fa-f]+|\b\d+\b", body)
    image = bytes(int(v, 0) for v in values)
    if len(image) != expected:
        raise ValueError(f"{symbol}: got {len(image)}, expected {expected}")
    return image

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("source", type=Path, help="source ZIP file or Fm11 directory")
    ap.add_argument("--output", type=Path, default=Path("external/roms"))
    args = ap.parse_args()
    with zipfile.ZipFile(args.source) if args.source.is_file() else _NullContext() as archive:
        images = []
        for header, symbol, name, size in ROMS:
            if archive is None:
                data = (args.source / header).read_bytes()
            else:
                paths = [p for p in archive.namelist() if p.endswith("/" + header) or p == header]
                if len(paths) != 1:
                    raise ValueError(f"expected one {header}, got {len(paths)}")
                data = archive.read(paths[0])
            images.append((name, extract(data, symbol, size)))
    args.output.mkdir(parents=True, exist_ok=True)
    for name, image in images:
        destination = args.output / name
        destination.write_bytes(image)
        print(f"{destination}: {len(image)} bytes")

class _NullContext:
    def __enter__(self):
        return None
    def __exit__(self, *args):
        return False

if __name__ == "__main__":
    main()
