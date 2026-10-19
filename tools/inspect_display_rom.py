#!/usr/bin/env python3
"""Read-only inspection of the *original* 20 KiB FM-11 display ROM.

This is a source-anchored diagnostic, not a synthetic command protocol.
The actual ROM must be provided by the operator; no firmware is bundled.
"""
import argparse
from pathlib import Path

def examine(data: bytes) -> None:
    if len(data) != 0x5000:
        raise ValueError(f"expected 0x5000 bytes for B000-FFFF, got 0x{len(data):X}")
    def at(addr: int, length: int) -> bytes:
        if not 0xB000 <= addr <= 0x10000-length:
            raise ValueError(f"address out of range: {addr:04X}")
        return data[addr-0xB000:addr-0xB000+length]
    if at(0xB000, 3) != bytes.fromhex("7e b0 18"):
        raise ValueError(f"unexpected reset entry: {at(0xB000,3).hex(' ')}")
    print("reset: B000 JMP B018 (reference firmware signature matches)")
    # Actual firmware has an eight-entry table of 16-bit handler addresses.
    # Inspect entries rather than invent a matching command value.
    print("B07C: eight dispatch addresses (handler addresses, not command bytes)")
    for i in range(8):
        addr = 0xB07C+2*i
        target = int.from_bytes(at(addr,2),"big")
        print(f"  table[{i}] @ {addr:04X} -> {target:04X}")
        if not 0xB000 <= target <= 0xFFFF:
            raise ValueError(f"invalid handler target {target:04X}")
    print("CAA0 receive routine:",at(0xCAA0,0x2b).hex(" "))
    # This routine alone does not establish the text output command syntax.
    print("No character-display command or main/sub protocol is inferred.")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("rom",type=Path,nargs="?",default=Path("external/roms/display6809.bin"))
    args = parser.parse_args()
    examine(args.rom.read_bytes())
