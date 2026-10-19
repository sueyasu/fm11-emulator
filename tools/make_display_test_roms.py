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

# Test-only font: distinguish the glyph path from blank/attribute fill.
# The SDL visual test should instead use the original extracted ANK ROM.
text_ank=bytearray(4096)
for ch in b"HELLO FM-11 40/80 COL TEST":
    text_ank[ch*16]=0x01
    text_ank[ch*16+1]=0x02
(out/"ank_text_stage1.bin").write_bytes(text_ank)
print(out/"ank_text_stage1.bin")

# Stage 2: same character/CRTC/ANK pipeline at 40 and 80 columns.
# Both variants use the same main CPU ROM; CRTC R1 controls width.
for columns in (40,80):
    rom=bytearray(0x5000)
    code=bytearray()
    def wr(addr,val):
        code.extend((0x86,val,0xb7,(addr>>8)&255,addr&255))
    wr(0xafee,1)
    wr(0xafef,columns//2)
    wr(0xafee,6)
    wr(0xafef,25)
    wr(0xafee,12)
    wr(0xafef,0)
    wr(0xafee,13)
    wr(0xafef,0)
    for i,ch in enumerate(b"40/80 COL TEST"):
        wr(0x8000+2*i,ch)
        wr(0x8001+2*i,7)
    wr(0x9f80,0x66)
    code.extend((0x20,0xfe))
    rom[:len(code)]=code
    rom[-2:]=bytes.fromhex("b0 00")
    (out/f"display_text_{columns}col.bin").write_bytes(rom)
    print(out/f"display_text_{columns}col.bin")

# Stage 3: color, bright and reverse-video text attributes.  The glyphs
# and attribute bytes are written by the display 6809, not the frontend.
attribute_rom=bytearray(0x5000)
attribute_code=bytearray()
def awrite(address,value):
    attribute_code.extend((0x86,value,0xb7,(address>>8)&255,address&255))
awrite(0xafee,1)
awrite(0xafef,40)   # 80 columns
awrite(0xafee,6)
awrite(0xafef,25)
awrite(0xafee,12)
awrite(0xafef,0)
awrite(0xafee,13)
awrite(0xafef,0)
# Plain red / green / blue / reverse red / bright white / normal white.
for index,(letter,attribute) in enumerate(zip(b"RGBRHW",
                                              (2,4,1,0x0a,0x27,7))):
    awrite(0x8000+index*2,letter)
    awrite(0x8001+index*2,attribute)
awrite(0x9f80,0x66)
attribute_code.extend((0x20,0xfe))
attribute_rom[:len(attribute_code)]=attribute_code
attribute_rom[-2:]=bytes.fromhex("b0 00")
(out/"display_text_attributes.bin").write_bytes(attribute_rom)
print(out/"display_text_attributes.bin")
# Test font has just bit 0 in scanline 0 and bit 1 in scanline 1.
attribute_ank=bytearray(4096)
for letter in b"RGBRHW":
    attribute_ank[letter*16]=1
    attribute_ank[letter*16+1]=2
(out/"ank_text_attributes.bin").write_bytes(attribute_ank)
print(out/"ank_text_attributes.bin")

# Stage 4: actual 6809 text/graphics overlap.
# Main turns on both layers (FDA0=$0F); sub paints a blue striped
# background through GVU, then a yellow glyph and reverse-video cell.
overlay_main=bytearray(0x1000)
# The physical blue plane is RGB index 4. The original 16-color
# table uses palette value 1 for blue, so program FD9C explicitly.
overlay_main_code=bytes.fromhex("86 01 b7 fd 9c 86 0f b7 fd a0 20 fe")
overlay_main[:len(overlay_main_code)]=overlay_main_code
overlay_main[-2:]=bytes.fromhex("50 00")
overlay_sub=bytearray(0x5000)
oc=bytearray()
def ow(addr,value):
    oc.extend((0x86,value,0xb7,(addr>>8)&255,addr&255))
ow(0xafee,1); ow(0xafef,40)
ow(0xafee,6); ow(0xafef,25)
ow(0xafee,12);ow(0xafef,0)
ow(0xafee,13);ow(0xafef,0)
# first cell: yellow foreground over graphics, second cell reverse red
ow(0x8000,ord("T"));ow(0x8001,6)
ow(0x8002,ord("R"));ow(0x8003,0x0a)
# graphics writes: all three plane bits enabled; $80 lights x=0
ow(0xafe1,7)
ow(0x0000,0x80)
# Make background BLUE only: reset the red and green plane bytes by
# temporarily enabling blue-only writes and clearing other planes.
ow(0xafe1,4)
ow(0x0000,0x80)
ow(0x0001,0x80)
ow(0x9f80,0x66)
oc.extend((0x20,0xfe))
overlay_sub[:len(oc)]=oc
overlay_sub[-2:]=bytes.fromhex("b0 00")
(out/"main_text_overlay.bin").write_bytes(overlay_main)
(out/"display_text_overlay.bin").write_bytes(overlay_sub)
print(out/"main_text_overlay.bin")
print(out/"display_text_overlay.bin")
overlay_ank=bytearray(4096)
overlay_ank[ord("T")*16]=1
overlay_ank[ord("R")*16]=1
(out/"ank_text_overlay.bin").write_bytes(overlay_ank)
print(out/"ank_text_overlay.bin")

# Stage 5: explicit FDA0 layer gating with a visible blue stripe field
# and yellow text. Three main ROM variants differ ONLY in FDA0.
for name,enable in (("text",0x03),("graphics",0x0c),("both",0x0f)):
    m=bytearray(0x1000)
    # palette index 4 => blue (reference color index 1)
    body=bytes((0x86,1,0xb7,0xfd,0x9c,0x86,enable,0xb7,0xfd,0xa0,0x20,0xfe))
    m[:len(body)]=body
    m[-2:]=bytes.fromhex("50 00")
    (out/f"main_layers_{name}.bin").write_bytes(m)
    print(out/f"main_layers_{name}.bin")
layers=bytearray(0x5000)
lc=bytearray()
def lw(address,value):
    lc.extend((0x86,value,0xb7,(address>>8)&255,address&255))
for reg,value in ((1,40),(6,25),(12,0),(13,0)):
    lw(0xafee,reg)
    lw(0xafef,value)
for i,ch in enumerate(b"LAYER TEST"):
    lw(0x8000+2*i,ch)
    lw(0x8001+2*i,6)  # yellow text
# Paint 200 scanlines of high-contrast blue stripes in a broad 80-pixel
# region: first 10 packed bytes of each scanline, with 0xAA pattern.
# 6809 uses an indexed fill loop to keep the test ROM small.
lw(0xafe1,4)  # write blue plane, clear red/green
lc.extend(bytes.fromhex(
    "86 aa "       # LDA #$AA
    "8e 00 00 "    # LDX #0
    "a7 80 "       # STA ,X+
    "8c 3e 80 "    # CMPX #16000
    "26 f9 "       # BNE preceding STA
))
lw(0x9f80,0x66)
lc.extend((0x20,0xfe))
layers[:len(lc)]=lc
layers[-2:]=bytes.fromhex("b0 00")
(out/"display_layers.bin").write_bytes(layers)
print(out/"display_layers.bin")
