# Minimal compatible Display Sub System

Independent MC6809 firmware, 20 KiB at B000–FFFF. Build with lwasm
through the normal CMake build. Select build/roms/compatible/display6809.bin
with the emulator's --rom option. No proprietary ROM or HLE is required.

Scope: fixed 80x25 ANK console, commands 12 (character output) 34 (JIS Kanji at cursor, 400-line only), and 2D
(raw nonblocking keyboard), mailbox BUSY handshake, basic native control
codes and ESC G/T/Y/*. Other commands return 46.

Private implementation: CRTC hardware scroll on a 4 KiB text ring,
direct interleaved character/attribute stores, a byte-stream parser,
and a FIRQ-driven 63-entry FIFO. CRTC addresses are in pairs of cells;
AFE0 bit5 selects the odd cursor cell. AFE4 bit0 is keyboard FIRQ.

Initial limitations: BEL is ignored, BS and TAB edge behavior is
provisional, no PF expansion, no graphics commands,
no internal ROM entrypoint compatibility. Real hardware initialization
and timing, including keyboard startup, need verification.

## K-1 Kanji

Command 34h uses mailbox +3/+4 as JIS78 code, draws two cells at the
current cursor without moving it. Both font banks use the existing
compatible generator layout. Color and reverse are supported; blink
and intensity for graphics glyphs are not. Invalid ranges and column
79 are ignored. The second bank must be installed for level-2 glyphs.

Graphics is deliberately OFF by default in both compatible main ROMs,
like stock boot firmware. Applications must enable it via FDA0 (0Fh
for both layers). The sub-ROM does not change main CPU layer gating.
Use 400-line presentation for K-1; 200-line conversion and character
stream/Shift-JIS are deferred. The firmware owns the graphics region
used by console Kanji; arbitrary overlapping graphics are not preserved.
Text and graphics rings scroll independently; a logical 2000-byte
cell map tracks glyph halves, avoiding metadata packing overhead.
