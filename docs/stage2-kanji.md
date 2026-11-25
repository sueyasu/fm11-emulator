# Stage 2-E: Kanji ROM

## Access protocol verified against original FM-11 emulator

The Windows reference `Fm11/SubCpu.cpp` uses an embedded 128-KiB
`KanRom` array, not a direct sub-CPU memory window. The guest accesses
it through the sub-CPU I/O ports:

| Sub-CPU port | Function |
| --- | --- |
| AFF4 write | Glyph-number high byte; bit 7 enables RTC register window |
| AFF5 write | Glyph-number low byte; latches index times 32 and resets cursors |
| AFF6 read | 16 successive **left** bytes (offsets 0,2,...30) |
| AFF7 read | 16 successive **right** bytes (offsets 1,3,...31) |
| AFF7 when AFF4 bit7 set | RTC register selected by AFF4 low nibble |

A completed glyph returns 0 on AFF6 and 1 on AFF7, as in the original
source. An out-of-range glyph selection does not restart the prior
cursors. RESET clears glyph state but preserves ROM contents.
RTC registers are currently storage-only, not a real-time clock.

The frontend supports `[display_cpu] kanji_rom = /path/to/kanji.bin`
and `--kanji-rom FILE` (exactly 131072 bytes). No copyrighted ROM
image is included in the repository. `Machine::loadKanjiRomFile`
provides strict-size validation.

## Verification and later work

CTest `display_kanji_rom` tests selection, interleaved byte layout,
independent cursors, end-of-glyph behavior, upper boundary, RTC
multiplexing and reset. These are sub-CPU memory-mapped operations,
not an application-specific font API.

Guest-side Kanji display using the stock sub-ROM and higher-level text
protocol still needs to be verified. The existing bitmap/OpenGL text
composer remains ANK-only; the original emulator also only composes
ANK text directly, with Kanji glyphs accessed by the sub-CPU via these
ports and then drawn through the guest's display/VRAM commands.
Do not invent an additional direct UTF-8 -> renderer path.

## Stage 2-E integrated sub-CPU and graphics test

`display_kanji_subcpu` runs a real MC6809 sub-CPU program from a
synthetic 20-KiB display ROM. That program selects a glyph through AFF4/5,
reads 16 left/right bytes from AFF6/7, writes them to 16 separate
VRAM scanlines, and signals completion through the shared RAM mailbox.
The test then checks physical VRAM and the existing graphics scanout.
It does **not** implement a second host-side Kanji renderer or require
copyrighted ROM images.

This proves the CPU -> Kanji port -> VRAM -> software-graphics
path. The stock Fujitsu display ROM's higher-level Kanji command
protocol and OpenGL presentation still need an OS-level visual
regression with a user-supplied Kanji ROM, as the test uses a synthetic
sub-CPU program rather than the stock firmware.

## Stock-ROM / SDL final verification

The portable `display_kanji_subcpu` test confirms the guest-executed
AFF4/5/6/7 -> VRAM -> software graphics path. The OpenGL renderer
uploads the **same physical VRAM bytes** to its graphics texture; it
does not fetch the Kanji ROM or implement a separate Kanji glyph
overlay. Thus neither renderer needs a Kanji-specific text shader.

For a full OS-level visual check, supply the original display, ANK
and 128-KiB Kanji ROMs via the example config and run the normal
FM-11 boot with:

```sh
./build/fm11-sdl --config config/fm11emu.conf --trace-kanji --renderer software
./build/fm11-sdl --config config/fm11emu.conf --trace-kanji --renderer gl
```

Use an OS program that requests a Kanji glyph through the original
display ROM. `--trace-kanji` reports the number of accepted AFF5
selections and the successful left/right ROM-byte reads every 2 seconds
for either renderer. For a complete 16x16 glyph, 16 left and 16 right
bytes should be read, though higher-level firmware may cache or
interleave accesses. The existing video-revision value indicates that
display state was updated; it is **not** proof that the particular
glyph was drawn. Comparing the resulting image on both renderers
provides visual confirmation. No host-side glyph injection is used.

This manual OS/ROM test cannot be declared PASS on CI alone: the original
ROMs are user-supplied and the guest-side command sequence depends on
the OS program. `--trace-kanji` is diagnostic only; the default runtime
behavior and ANK output are unchanged.

## Interactive ROM gallery (`--test-kanji`)

`fm11-sdl --test-kanji` displays 1000 consecutive 16x16 glyphs at
40 columns x 25 rows on a 640x400 graphics screen without booting an
OS or loading the standard sub-ROM. It uses a user-provided 128-KiB
Kanji ROM: pass `--kanji-rom FILE`, set `[display_cpu] kanji_rom`,
or place a ROM at `external/roms/kanji.bin`. Missing or invalid ROMs
are errors, not silently replaced by a synthetic font.

The gallery selects each glyph using AFF4/AFF5 and reads exactly
16 left and 16 right bytes through AFF6/AFF7, then writes the
corresponding scanlines through the standard graphics VRAM ports.
The two existing SDL renderers display that same graphics RAM: no
host-side Unicode/font conversion, alternate text renderer or GLSL
Kanji path is added. The gallery forces the 400-line display mode and
does not start CPU workers or require the boot ROM. It never modifies
the ROM file. PageUp/PageDown cycle through 5 ROM pages; the window
title shows the current glyph-number range. Ctrl+F12 closes the viewer.

```sh
./build/fm11-sdl --test-kanji --kanji-rom external/roms/kanji.bin --renderer software
./build/fm11-sdl --test-kanji --kanji-rom external/roms/kanji.bin --renderer gl
```

The portable `display_kanji_gallery` CTest uses a synthetic ROM to
check port accesses, glyph positions including the bottom 400-line row,
page transitions, clearing, 400-line graphics scanout, and the end of
the 4096-glyph ROM. Visual fidelity
with a real ROM is a separate manual check, not claimed by CTest.

The 400-line gallery displays 16x16 ROM glyphs without the vertical
2x scaling used by the 200-line display. The last (fifth) page holds
96 glyphs. All 32,000 bytes per graphics plane are cleared on each page
change; no glyph pixels can persist from a preceding page.

## Optional JIS level-2 ROM (FM-11AD2/AD2+ Kanji board)

The revised FM-11 Kanji ROM board exposes one extra address bit:
AFF4 bit 4 is address A16 and selects the second-level 128-KiB
ROM. Bits 3..0 of AFF4 and AFF5 select a 32-byte glyph within the
bank; AFF6/AFF7 remain the left and right raster-byte read ports.
The original first-level-only board has no second bank.

The emulator retains the existing 128-KiB `kanji_rom` image and
adds an optional, separate 128-KiB `kanji2_rom` image. Both
loaders validate their file sizes. If the second ROM is absent,
the previous first-level-only behavior is preserved. Reset clears
the selected bank, without discarding loaded ROM data.

```ini
[display_cpu]
kanji_rom = external/roms/kanji.bin
kanji2_rom = external/roms/kanji2.bin
```

```sh
./build/fm11-sdl --test-kanji --kanji2-rom external/roms/kanji2.bin --test-kanji-level 2
```

In the gallery, **F3** changes between levels 1 and 2 and resets
the page to 1; **PageUp/PageDown** navigate the current level.
Both levels have five 640x400 pages. The normal OS mode also
loads both ROMs and lets the real sub-CPU firmware select A16.

The firmware for the later FM-11 model must translate JIS level-2
codes into a different ROM address mapping from level 1. That
translation belongs to the guest sub-ROM; the emulator models
only the ROM bank/address selection and data readback.

Hardware reference (reverse-engineered FM-11AD2/AD2+ ROM card):
https://hardyboy.livedoor.blog/archives/10763657.html

## Standard sub-ROM JIS78 level-2 address translation (verified)

The reference Windows emulator source archive contains a 20,480-byte
`Fm11/SubRom.h`. Its sub-CPU MC6809 code at **E8D4-E964** implements
JIS-to-ROM selection. The second-level case is **E928-E963**:
- E928 `CMPA #$74` rejects JIS lead byte >= $74.
- E92C `SUBD #$0020` yields 1-based JIS cell numbers.
- E933-E950 computes the high AFF4 selector (including A16=1).
- E952-E963 combines the row-in-block and column-in-block to form AFF5.

This firmware addresses **JIS78 rows 48-83, 94 cells per row = 3384
characters**. JIS83 row 84 (six added characters) is not accessible via
this routine. The correct selector, for `ku=48..83, ten=1..94`, is:

```python
a = ku + 0x20
group = 3 * ((a - 0x50) >> 4) + (ten >> 5)
g2 = group << 1
high = 0x1D if (g2 & 0x10) else ((g2 & 0x0E) | 0x10)
offset = ((a & 0x0F) << 5) + (ten & 0x1F)
selector = (high << 8) | offset  # write selector to AFF4/AFF5
slot = selector & 0x0FFF         # address within kanji2_rom
```

The prior diagnostic image used a provisional JIS row-major 96-slot
layout; it **was not** compatible with this mapping. Use
`tools/make_kanji2_rom.py` to generate a corrected 128-KiB test bank
from a locally installed OFL font (requires Pillow):

```sh
python3 tools/make_kanji2_rom.py --font /usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc --index 0 --output external/roms/kanji2.bin --csv build/kanji2_map.csv
```

`fm11_kanji2_address` CTest validates the 3384 distinct hardware
selectors, boundary handling and representative pairs. Font glyph shapes
and Unicode's modern JIS conversion are **not** byte-for-byte copies of
the original Fujitsu JIS78 mask ROM; this is a replacement **test image**
with the sub-ROM-compatible **address placement**.

Changing the image requires no hardware-emulation change: AFF4 bit4
already selects the optional second-level bank; AFF6/AFF7 read its
32-byte glyph at the designated selector. The standalone `--test-kanji`
gallery displays sequential ROM slots, not JIS code ordering.
