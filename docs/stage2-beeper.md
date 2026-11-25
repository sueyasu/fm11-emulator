# Stage 2-D: single-channel beeper

The common main-board speaker is driven by FD03 writes from either the
6809 or 8088 main CPU. FD03 reads retain existing IRQ-status semantics.
Stage 2-D introduces `BeeperDevice` for deterministic mono audio synthesis
and `SdlBeeper` for optional SDL2 playback. Headless builds do not
require SDL or an audio device.

The provisional FD03 write semantics follow Fujitsu FM-family beeper
behavior: bit 0 enables the speaker, bit 7 selects continuous tone,
and bit 6 triggers a 205 ms one-shot. The oscillator is 1,200 Hz,
rendered as a square wave at 48 kHz (or SDL's obtained device rate).
The audio callback owns the one-shot's real-time duration; it is
intentionally independent of guest CPU --speed multiplier. Main CPU
RESET silences audio. Sound generation is independent of IRQ status.
The application continues operating if no host audio device exists.

**Hardware verification outstanding:** the original FM-11 emulator's
beeper port data semantics have not been independently confirmed against
the FM-11 hardware manual. Compare FD03 bit behavior and tone duration
with a real FM-11 before treating these provisional values as final.

CTest `beeper_device` validates 6809 and 8088 writes, the 1,200 Hz
waveform, 205 ms cutoff, silencing, and reset.
