# Minimal compatible Display Sub System

Independent MC6809 firmware, 20 KiB at B000–FFFF. Build with lwasm
through the normal CMake build. Select build/roms/compatible/display6809.bin
with the emulator's --rom option. No proprietary ROM or HLE is required.

Scope: fixed 80x25 ANK console, commands 12 (character output) and 2D
(raw nonblocking keyboard), mailbox BUSY handshake, basic native control
codes and ESC G/T/Y/*. Other commands return 46.

Private implementation: CRTC hardware scroll on a 4 KiB text ring,
direct interleaved character/attribute stores, a byte-stream parser,
and a FIRQ-driven 63-entry FIFO. CRTC addresses are in pairs of cells;
AFE0 bit5 selects the odd cursor cell. AFE4 bit0 is keyboard FIRQ.

Initial limitations: BEL is ignored, BS and TAB edge behavior is
provisional, no Kanji console, no PF expansion, no graphics commands,
no internal ROM entrypoint compatibility. Real hardware initialization
and timing, including keyboard startup, need verification.
