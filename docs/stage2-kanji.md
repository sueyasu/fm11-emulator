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
