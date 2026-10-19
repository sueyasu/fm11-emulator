#!/usr/bin/env python3
"""Generate tiny *synthetic* 6809 binary ROMs for shared-memory display test."""
from pathlib import Path
import sys

out=Path(sys.argv[1] if len(sys.argv)>1 else "build/test-roms")
out.mkdir(parents=True,exist_ok=True)
main=bytearray(0x1000)
# $5000: LDA #$80; STA $FC80; LDA #$01; STA $FC81; BRA *
main[0:12]=bytes.fromhex("86 80 b7 fc 80 86 01 b7 fc 81 20 fe")
main[-2:]=bytes.fromhex("50 00")
display=bytearray(0x5000)
# $B000: LDA $9F80; CMPA #$80; BNE $B000
#        LDA #$07; STA $AFE1; LDA #$80; STA $0000; BRA *
display[0:20]=bytes.fromhex(
    "b6 9f 80 81 80 26 f9 86 07 b7 af e1 86 80 b7 00 00 20 fe 12")
display[-2:]=bytes.fromhex("b0 00")
(out/"main_test.bin").write_bytes(main)
(out/"display_test.bin").write_bytes(display)
print(out/"main_test.bin")
print(out/"display_test.bin")
