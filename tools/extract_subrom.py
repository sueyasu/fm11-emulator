#!/usr/bin/env python3
"""Convert the user-supplied reference emulator's SubRom.h to a raw 20KiB ROM.

Usage: python3 tools/extract_subrom.py /path/to/Fm11/SubRom.h /tmp/subrom.bin
The extracted ROM is not committed to the repository.
"""
import argparse
from pathlib import Path
import re

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("header", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    source = args.header.read_text(encoding="latin-1")
    match = re.search(r"\bSubRom\s*\[\s*\]\s*=\s*\{(.*?)\}", source, re.S)
    if not match:
        parser.error("SubRom array not found")
    body = re.sub(r"/\*.*?\*/|//[^\n]*", "", match.group(1), flags=re.S)
    words = re.findall(r"0[xX][0-9a-fA-F]+|\b\d+\b", body)
    rom = bytes(int(word, 0) for word in words)
    if len(rom) != 0x5000:
        parser.error(f"SubRom size is {len(rom)} bytes, expected 20480")
    args.output.write_bytes(rom)
    print(f"Wrote {len(rom)} bytes to {args.output}")

if __name__ == "__main__":
    main()
