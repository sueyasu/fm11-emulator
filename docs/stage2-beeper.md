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
With SDL audio, the callback owns the one-shot's wall-clock duration
independent of guest CPU --speed multiplier. In headless or audio-disabled
operation, virtual CPU time expires the one-shot after 205 ms. Main CPU
RESET silences audio. Sound generation is independent of IRQ status.
The application continues operating if no host audio device exists.

**Hardware verification outstanding:** the original FM-11 emulator's
beeper port data semantics have not been independently confirmed against
the FM-11 hardware manual. Compare FD03 bit behavior and tone duration
with a real FM-11 before treating these provisional values as final.

CTest `beeper_device` validates 6809 and 8088 writes, the 1,200 Hz
waveform, 205 ms cutoff, silencing, and reset.

## Independent reference and listening check

The MAME Fujitsu FM-family beeper implementation confirms FD03 bit 0
speaker enable, bit 7 continuous tone and bit 6 approximately 205 ms
pulse. The 1,200 Hz nominal tone is also used by MAME for FM-11.
FM-11 original emulator source/real-hardware verification remains useful.

Run `fm11-sdl --beep-test` with the normal ROM/config options. Before
starting guest execution it plays a 205 ms pulse, brief pause and 350 ms
continuous tone through the ordinary FD03 I/O and SDL output; then it
silences the speaker. The flag is opt-in and errors if the host has no
SDL audio output. The portable regression test checks headless virtual
time expiration, pending-pulse reset, and audio-clock ownership.
