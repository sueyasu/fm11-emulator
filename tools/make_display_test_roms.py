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

# Two separate external events delivered before first FD04 read. The program
# captures two FD04 samples, then the event state is read again.
events_main=bytearray(0x1000)
events_code=bytes.fromhex(
    "b6 fd 04 b7 fc 80 "
    "b6 fd 04 b7 fc 81 "
    "20 fe")
events_main[:len(events_code)]=events_code
events_main[-2:]=bytes.fromhex("50 00")
(out/"main_events.bin").write_bytes(events_main)
print(out/"main_events.bin")

# Original hardware ATTENTION path: display CPU reads $AFE7.
# Main ROM polls $FD04 (read-to-clear) and stores the observed event.
afe7_main=bytearray(0x1000)
afe7_main_code=bytes.fromhex(
    "b6 fd 04 "          # $5000 LDA $FD04
    "85 01 "             #       BITA #$01
    "27 f9 "             #       BEQ $5000
    "b7 fc 80 "          #       STA $FC80
    "b6 fd 04 "          #       LDA $FD04 again
    "b7 fc 81 "          #       STA $FC81
    "20 fe")            #       BRA *
afe7_main[:len(afe7_main_code)]=afe7_main_code
afe7_main[-2:]=bytes.fromhex("50 00")
afe7_display=bytearray(0x5000)
afe7_display_code=bytes.fromhex(
    "b6 af e7 "          # $B000 LDA $AFE7 => ATTENTION
    "20 fe")            #       BRA *
afe7_display[:len(afe7_display_code)]=afe7_display_code
afe7_display[-2:]=bytes.fromhex("b0 00")
(out/"main_afe7.bin").write_bytes(afe7_main)
(out/"display_afe7.bin").write_bytes(afe7_display)
print(out/"main_afe7.bin")
print(out/"display_afe7.bin")

# Full 640x200 monochrome (white/black) display exercise.
# Main writes command $81 and waits for display's $55 response.
pattern_main=bytearray(0x1000)
pattern_main_code=bytes.fromhex(
    "86 81 b7 fc 80 "         # send command
    "b6 fc 81 81 55 26 f9 "  # wait for ACK
    "86 66 b7 fc 82 "        # record success
    "20 fe")
pattern_main[:len(pattern_main_code)]=pattern_main_code
pattern_main[-2:]=bytes.fromhex("50 00")
# Sub scans all 16000 bytes (80 bytes * 200 lines) with STA ,X+.
# 07 writes all RGB planes; AA yields alternating white/black pixels.
pattern_display=bytearray(0x5000)
pattern_display_code=bytes.fromhex(
    "b6 9f 80 81 81 26 f9 "  # wait for main command
    "86 07 b7 af e1 "        # enable all RGB planes
    "86 aa "                 # alternating pixel mask
    "8e 00 00 "              # LDX #$0000
    "a7 80 "                 # loop: STA ,X+
    "8c 3e 80 "              # CMPX #$3E80
    "26 f9 "                 # BNE loop
    "86 55 b7 9f 81 "        # completion ACK
    "20 fe")
pattern_display[:len(pattern_display_code)]=pattern_display_code
pattern_display[-2:]=bytes.fromhex("b0 00")
(out/"main_pattern.bin").write_bytes(pattern_main)
(out/"display_pattern.bin").write_bytes(pattern_display)
print(out/"main_pattern.bin")
print(out/"display_pattern.bin")

# Display CPU: wait for $AFE6 bit7 rising and falling edges, then
# write marker $66 to shared RAM. No original firmware is involved.
raster=bytearray(0x5000)
raster_code=bytes.fromhex(
    "7d af e6 2a fb "  # $B000 TST AFE6; BPL $B000
    "7d af e6 2b fb "  # $B005 TST AFE6; BMI $B005
    "86 66 b7 9f 80 "  # write success marker
    "20 fe")
raster[:len(raster_code)]=raster_code
raster[-2:]=bytes.fromhex("b0 00")
(out/"display_raster.bin").write_bytes(raster)
print(out/"display_raster.bin")

# Stage 1 text end-to-end: the main 6809 enables text; the display 6809
# configures the CRTC and writes HELLO FM-11 as character/attribute pairs.
# No display RAM is modified by the C++ regression test or SDL frontend.
text_main=bytearray(0x1000)
text_main_code=bytes.fromhex("86 03 b7 fd a0 20 fe")
text_main[:len(text_main_code)]=text_main_code
text_main[-2:]=bytes.fromhex("50 00")

text_display=bytearray(0x5000)
text_program=bytearray()
def tlda(v):
    text_program.extend((0x86,v))
def tsta(addr):
    text_program.extend((0xb7,(addr>>8)&255,addr&255))
def twrite(addr,val):
    tlda(val)
    tsta(addr)

twrite(0xafee,1)     # CRTC horizontal displayed: 40 units -> 80 chars
twrite(0xafef,40)
twrite(0xafee,6)     # 25 text rows
twrite(0xafef,25)
twrite(0xafee,12)    # start address = 0
twrite(0xafef,0)
twrite(0xafee,13)
twrite(0xafef,0)
for i,ch in enumerate(b"HELLO FM-11"):
    twrite(0x8000+i*2,ch)
    twrite(0x8001+i*2,7)
twrite(0x9f80,0x66) # signal done to the test runner
text_program.extend((0x20,0xfe)) # BRA *
text_display[:len(text_program)]=text_program
text_display[-2:]=bytes.fromhex("b0 00")
(out/"main_text_stage1.bin").write_bytes(text_main)
(out/"display_text_stage1.bin").write_bytes(text_display)
print(out/"main_text_stage1.bin")
print(out/"display_text_stage1.bin")
