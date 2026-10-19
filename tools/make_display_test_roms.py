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

# Bidirectional command/acknowledgement test. This tests the established
# shared-RAM transport, NOT an as-yet-unverified hardware IRQ trigger.
#
# Main $5000: issue command $81 through $FC80, wait for $55 at $FC81,
# mark successful acknowledgement with $66 at $FC82.
handshake_main=bytearray(0x1000)
handshake_main[:19]=bytes.fromhex(
    "86 81 b7 fc 80 "      # LDA #$81; STA $FC80
    "b6 fc 81 81 55 26 f9 " # poll ACK $55: BNE back to LDA
    "86 66 b7 fc 82 20 fe"  # mark success then loop
)
handshake_main[-2:]=bytes.fromhex("50 00")
# Display $B000: poll command $81, fill first pixel via GVU, return ACK.
handshake_display=bytearray(0x5000)
handshake_display[:24]=bytes.fromhex(
    "b6 9f 80 81 81 26 f9 " # read command, BNE $B000
    "86 07 b7 af e1 "       # enable writes to three planes
    "86 80 b7 00 00 "       # white pixel at VRAM start
    "86 55 b7 9f 81 "       # completion byte
    "20 fe"                # idle
)
handshake_display[-2:]=bytes.fromhex("b0 00")
(out/"main_handshake.bin").write_bytes(handshake_main)
(out/"display_handshake.bin").write_bytes(handshake_display)
print(out/"main_handshake.bin")
print(out/"display_handshake.bin")

# FD05 HALT/release test: unlike the shared-RAM polling tests, the main
# CPU writes the real modeled control register.
halt_main=bytearray(0x1000)
halt_main[0:22]=bytes.fromhex(
    "86 80 b7 fd 05 "     # HALT display CPU
    "86 11 b7 fc 80 "     # marker while HALTed
    "86 00 b7 fd 05 "     # release HALT
    "86 22 b7 fc 81 "     # marker after release
    "20 fe")
halt_main[-2:]=bytes.fromhex("50 00")
# Display CPU writes BUSY only when allowed to execute.
halt_display=bytearray(0x5000)
halt_display[0:12]=bytes.fromhex("86 42 b7 af f0 86 33 b7 9f 82 20 fe")
halt_display[-2:]=bytes.fromhex("b0 00")
(out/"main_halt.bin").write_bytes(halt_main)
(out/"display_halt.bin").write_bytes(halt_display)
print(out/"main_halt.bin")
print(out/"display_halt.bin")

# A diagnostic ATTENTION event is injected by the host; main CPU firmware
# reads FD04 twice. First read must see bit 0, second must see it clear.
attention_main=bytearray(0x1000)
attention_code=bytes.fromhex(
    "b6 fd 04 b7 fc 80 "  # LDA FD04; STA FC80
    "b6 fd 04 b7 fc 81 "  # LDA FD04; STA FC81
    "20 fe")               # idle
attention_main[:len(attention_code)]=attention_code
attention_main[-2:]=bytes.fromhex("50 00")
(out/"main_attention.bin").write_bytes(attention_main)
print(out/"main_attention.bin")
