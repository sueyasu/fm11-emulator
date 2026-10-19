# FM-11 Emulator

Linux port and ongoing development of the FM-11 emulator.

This first revision establishes a portable CMake build for the existing 6809/6309 and Z80 CPU cores. The Windows MFC/Direct3D frontend is intentionally not part of the Linux build.

## External ROM images (not committed)

Original emulator firmware is supplied as C byte arrays. Extract them once
from the source ZIP or an unpacked `Fm11/` directory:

```sh
python3 tools/extract_roms.py /path/to/fm11_x64.zip
# Or: python3 tools/extract_roms.py /path/to/Fm11/
```

This writes the following files under `external/roms/`, which is excluded
by `.gitignore` and **must not be committed**:

| File | Size | Reference array |
|---|---:|---|
| `boot6809.bin` | 4096 | `BootRom.h:BootRom` |
| `boot8088.bin` | 4096 | `BootRom86.h:BootRom86` |
| `display6809.bin` | 20480 | `SubRom.h:SubRom` |
| `ank.bin` | 4096 | `CharRom.h:AnkRom` |
| `kanji.bin` | 131072 | `KanRom.h:KanRom` |

The final emulator will use ROM dumps supplied by the user from their own
machine, in the same binary file format and directory layout. ROM filenames
and sizes are validated, but ROM contents are never embedded in the binary.
The C++ core offers `loadDisplayRomFile` on `Machine` and
`loadBootRomFile` on the 6809 and 8088 adapters. The diagnostic runner
defaults to `external/roms/display6809.bin`:

```sh
./build/fm11-display-romtrace --instructions 100000
# Or override: --rom /somewhere/display6809.bin
```

The headless OS boottrack diagnostic currently uses a synthetic boot ROM,
not the extracted original boot firmware. The font ROMs are staged for the
later display/keyboard frontend, not consumed by current rendering code.

## SDL2 display frontend (experimental)

Install `libsdl2-dev` on Ubuntu, then reconfigure CMake so it can
discover SDL2. This frontend runs the display subsystem through the shared `Machine`
scheduler and renders VRAM using the existing 8-color pixel decoder:

```sh
sudo apt install libsdl2-dev
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
./build/fm11-sdl --rom external/roms/display6809.bin
```

To run the main 6809 CPU alongside the display CPU, supply its boot ROM:\n\n```sh\n./build/fm11-sdl --main-rom external/roms/boot6809.bin\n```\n\nThe default display ROM path is `external/roms/display6809.bin`; use the extraction
script described above to populate it. Controls: Escape to quit, Space
to pause/resume, F1 to switch 640x200/640x400 display interpretation.
SDL always scales the image by an **integer** factor and centers it on
dark-gray margins with a thin light-gray outline just outside the emulated
screen. Neither the margins nor the outline alter any emulated pixels. At the default 960-pixel window width, the 640-pixel image
is shown at native 1:1 size rather than distorted to 1.5x. Enlarge the
window to at least 1280x400 for a sharp 2x 640x200 display, or 1280x800
for a sharp 2x 640x400 display. Smaller windows clip instead of using
fractional resampling.
`--high` starts in 400-line mode; `--cycles N` controls main CPU instructions
per frame, or the approximate display CPU cycle budget without a main CPU
(default 10000). This is a
bring-up throttle, **not** a calibrated real-time CPU clock.

For a black screen while running **original** main and display ROMs, first
inspect the main CPU boot path and shared-bus handshake without changing
emulated memory or acknowledging IRQ/event registers:

```sh
./build/fm11-sdl --main-rom external/roms/boot6809.bin \\
    --rom external/roms/display6809.bin --trace-boot
```

`--trace-boot` prints the first 64 main 6809 instruction addresses
before execution. The periodic diagnostic line also reports the current
main PC (**decimal**), FD05 sub-CPU control latch, FD04 event latch,
display BUSY byte, number of main-to-sub shared RAM changes, first
shared byte, and display HALT state. All of these readings are
non-destructive; unlike a guest read from FD04, the diagnostic does
not acknowledge the event. Compare changes over multiple reports.
A constant display PC alone does not establish why firmware is waiting.

If the window stays black, use `./build/fm11-sdl --test-pattern` to
verify SDL rendering independently of firmware. This displays eight-color
stripes and does not require a ROM. Normal execution now prints the display
CPU PC, executed instructions, I/O writes, and count of nonzero physical
VRAM bytes every two seconds to stderr. An unchanged PC or zero VRAM count
is useful evidence that the firmware is waiting for main-CPU commands
rather than drawing a frame; the counters do not themselves establish why
it is waiting.

The SDL frontend is optional: when SDL2 is missing the existing core,
headless programs and CTests still build. Set `-DFM11_BUILD_SDL=OFF`
to disable SDL detection explicitly.

**Scope:** This is an early integrated CPU/display viewer, not yet a complete
FM-11 keyboard + disk GUI. Hardware mode selection and
text-mode composition are not yet connected; selecting 200/400 lines is
manual. No ROM image is stored in Git.

## Main/sub-CPU binary integration test

Before diagnosing original firmware, use the synthetic 6809 programs to
exercise only the main-to-display path. They are generated from opcode
bytes and **contain no original firmware**:

```sh
python3 tools/make_display_test_roms.py build/test-roms
./build/display_binary_integration build/test-roms/main_test.bin build/test-roms/display_test.bin
ctest --test-dir build -R display_binary_integration --output-on-failure
```

A second, separate regression tests a complete command/acknowledgement
exchange through the same transport. The synthetic main-CPU ROM writes
command `$81` to `$FC80`, the display ROM reads it from `$9F80`,
draws a white pixel, then writes acknowledgement `$55` to `$9F81`.
The main CPU polls `$FC81` and records success as `$66` at
`$FC82`. Run it independently:

```sh
ctest --test-dir build -R display_binary_handshake --output-on-failure
./build/display_binary_integration build/test-roms/main_handshake.bin build/test-roms/display_handshake.bin --handshake
```

This validates bidirectional shared memory and CPU scheduling, **not**
the original FM-11 interrupt/attention protocol; a hardware notification
register is not inferred from this test.

### AFE6 video status (reference SubCpu.cpp)

The display ROM polls bit 7 at `$AFE6`. The original emulator's
`CSubCpu::GetIoByte` **changes status on every read**: after a
`VSyncCount`-driven clear, bits 7 and 2 toggle on subsequent reads
when `VSyncMode` is zero, and bit 5 is set. `VSyncCount` is
decremented as the sub CPU executes instructions and is set to 100
when CRTC register 13 is written. A previously guessed periodic
60Hz/1ms source has been removed.

```sh
ctest --test-dir build -R 'display_binary_raster|display_cpu6809' --output-on-failure
```

The synthetic display ROM exercises the original `TST $AFE6`
branch loops. This fixes the status semantics tested here but does
not yet port the original character-display composition or every
sub-CPU peripheral behavior.

### Synthetic 6809 display-layer selection: stage 5

This test makes the graphics region **large enough to see**.
The common display ROM writes yellow text `LAYER TEST` plus a 640x200
blue/black alternating-pixel stripe field, and the main ROM selects
which layers are enabled via `FDA0`:

| Main ROM | FDA0 | Expected appearance |
| --- | --- | --- |
| `main_layers_text.bin` | `03` | Yellow text on black |
| `main_layers_graphics.bin` | `0C` | Blue/black stripes across the screen, no text |
| `main_layers_both.bin` | `0F` | Yellow text over blue/black stripes |

```sh
python3 tools/make_display_test_roms.py build/test-roms
ctest --test-dir build -R display_binary_layers --output-on-failure

# Change 'both' to 'text' or 'graphics' for the other two modes:
./build/fm11-sdl --main-rom build/test-roms/main_layers_both.bin \\
    --rom build/test-roms/display_layers.bin \\
    --ank-rom external/roms/ank.bin
```

The three new tests check actual main/sub-6809 writes and pixels for
both 200- and 400-line scanout. The 400-line view includes the
graphics plane's upper area, which is still blank because the test
draws only the first 200 scanlines.

### Synthetic 6809 graphics/text overlay: stage 4

This stage uses a **new pair of synthetic 6809 ROMs**. The main CPU
enables both text and graphics (`FDA0=$0F`). The display CPU sets
80-column/25-row CRTC text mode, writes a yellow `T` and a reverse
red `R` into text RAM, and paints two blue graphics pixels underneath
their cells using the graphics plane registers.

```sh
python3 tools/make_display_test_roms.py build/test-roms
ctest --test-dir build -R display_binary_text_overlay --output-on-failure

./build/fm11-sdl --main-rom build/test-roms/main_text_overlay.bin \\
    --rom build/test-roms/display_text_overlay.bin \\
    --ank-rom external/roms/ank.bin --high
```

The CTest runs the same ROMs with a generated minimal ANK font, and
checks both 200- and 400-line frames. The `T` glyph overlays the first
blue pixel in yellow, while the reverse-video `R` leaves the second
blue pixel visible through the glyph hole and paints the other pixels
of that character cell red. This tests actual output blending rather
than simply checking whether the two memory regions were written.

### Synthetic 6809 text attributes: stage 3

After validating basic text and 40/80-column geometry, the next
self-contained 6809 ROM writes six text cells with distinct attributes:

- `R`, `G`, `B`: red, green and blue character colors.
- `R` (fourth cell): reversed red, with the cell background filled.
- `H`: bright-white attribute.
- `W`: normal-white attribute.

```sh
python3 tools/make_display_test_roms.py build/test-roms
ctest --test-dir build -R display_binary_text_attributes --output-on-failure
./build/fm11-sdl --main-rom build/test-roms/main_text_stage1.bin \\
    --rom build/test-roms/display_text_attributes.bin \\
    --ank-rom external/roms/ank.bin --high
```

The CTest uses a generated minimal ANK glyph ROM, checking the text
attribute bytes written by the sub 6809 and the composited pixel colors
in both 200- and 400-line views. The SDL command uses the actual
extracted ANK ROM to show the characters.

F1 only changes the *200/400-line view*, not the CRTC 40/80-column
setting. In the current rendering model the 200-line view downsamples
400 logical character scanlines, so text appears vertically compressed.
Fidelity to original hardware scanline behavior remains to be verified.

### Synthetic 6809 text geometry: 40/80 columns and 200/400 lines

Stage 2 reuses the working stage-1 main ROM and two new display
6809 ROMs. The display CPU sets CRTC R1 to 20 (40 columns) or
40 (80 columns), R6 to 25 rows and writes `40/80 COL TEST`
into text RAM. The tests check both 640x200 and 640x400 framebuffer
geometry, using a deterministic test-only ANK font.

```sh
python3 tools/make_display_test_roms.py build/test-roms
ctest --test-dir build -R 'display_binary_text_(40|80)col' --output-on-failure

# 40 columns (16 logical pixels per character):
./build/fm11-sdl --main-rom build/test-roms/main_text_stage1.bin \\
    --rom build/test-roms/display_text_40col.bin \\
    --ank-rom external/roms/ank.bin --high

# 80 columns (8 logical pixels per character):
./build/fm11-sdl --main-rom build/test-roms/main_text_stage1.bin \\
    --rom build/test-roms/display_text_80col.bin \\
    --ank-rom external/roms/ank.bin --high
```

Press F1 in SDL to compare the 200- and 400-line representations.
The expected 40-column letters are twice as wide as the 80-column
letters. This stage confirms the current geometry model; fidelity
to all original CRTC/scanline behavior must still be compared with
the reference emulator.

### Synthetic 6809 text display: stage 1

Start with a small **end-to-end** text test before original firmware. Both
processors run real 6809 binary instructions: the main CPU writes `$FDA0=3`
(text enabled); the display CPU initializes the CRTC to 80x25, writes
`HELLO FM-11` as alternating character/attribute bytes starting at
`$8000`, and places `$66` in shared RAM on completion.

```sh
python3 tools/make_display_test_roms.py build/test-roms
ctest --test-dir build -R display_binary_text_stage1 --output-on-failure
./build/fm11-sdl --main-rom build/test-roms/main_text_stage1.bin \\
    --rom build/test-roms/display_text_stage1.bin \\
    --ank-rom external/roms/ank.bin --high
```

The headless regression uses a separate generated font
`ank_text_stage1.bin` with simple known glyph pixels, and checks
CPU-written text RAM, CRTC values, and output pixels. The SDL run above
uses the **actual** extracted ANK ROM, so `HELLO FM-11` should appear
at the top left. If the original ANK ROM is not available, the same SDL
command can use `build/test-roms/ank_text_stage1.bin` to show simple
glyph marks rather than letters.

This is **stage 1 only**: plain text on black. After confirming its
SDL result, later stages will cover colors, reverse attributes and
graphics/text overlays separately.

### Reference ANK text composition (first implementation)

The original `Fm11/fm11.hlsl` composites two-byte character/attribute
cells from the sub-CPU text RAM, with the CRTC start address and 8x16
ANK glyphs. That operation is now available in the SDL frontend, including
text reverse video, color attributes, and the 640x200/400 output sizes.
The software scanout matches the shader's LSB-first glyph bit ordering.

The reference archive includes `Fm11/CharRom.h`; convert its embedded
4096-byte ANK array without committing those font bytes:

```sh
python3 tools/extract_ank_rom.py /path/to/Fm11/CharRom.h external/roms/ank.bin
./build/fm11-sdl --main-rom external/roms/boot6809.bin \\
    --rom external/roms/display6809.bin --ank-rom external/roms/ank.bin
ctest --test-dir build -R display_text_composition --output-on-failure
```

Providing `--ank-rom` loads font data; it does **not** force the
text layer on when a main CPU is present. The original display control
register `$FDA0` enables text with bits 0/1 and graphics with bits
2/3. The renderer also applies the eight programmable palette entries
from `$FD98-$FD9F` and the original 16-entry color table.
The current software scanout supports attribute bit 5 (bright/bold)
and reverse video; blink, cursor and clip state remain future work.
Without a main CPU, specifying `--ank-rom` still explicitly enables
text for standalone diagnostics.
Other original display behavior, including blink, cursor and clip, remains to be ported.
Without `--ank-rom`, the established graphics-only regression frames
remain unchanged.

### Full-frame binary display test and SDL preview

To check the complete raster path without original firmware, generate
the synthetic ROM pair and run the test:

```sh
python3 tools/make_display_test_roms.py build/test-roms
ctest --test-dir build -R display_binary_pattern --output-on-failure
./build/display_binary_pattern build/test-roms/main_pattern.bin build/test-roms/display_pattern.bin
```

The main 6809 sends a command through shared RAM; the display 6809
writes all 16,000 bytes of the 640x200 plane with an alternating pixel
pattern in all three colors, then acknowledges completion. The test
checks **every rendered pixel**, not just the first byte.

To inspect the same pattern visually in the SDL frontend:

```sh
./build/fm11-sdl --main-rom build/test-roms/main_pattern.bin \\
    --rom build/test-roms/display_pattern.bin
```

This should display narrow alternating white and black vertical lines
across the entire 640x200 framebuffer. It is a deliberately artificial
bring-up image, not the FM-11 firmware's normal boot screen.

### Display-generated ATTENTION (original I/O semantics)

Original `Fm11/SubCpu.cpp`, `CSubCpu::GetIoByte`, case
`$AFE7`, raises main-CPU ATTENTION when the display CPU **reads**
that address. This is distinct from the earlier host-injected event
tests. The implementation now connects that read to the existing main
`$FD04` event latch.

A pair of tiny 6809 binaries verifies that the display CPU's `LDA
$AFE7` triggers an event that the main CPU receives with `LDA
$FD04` and then acknowledges by reading `$FD04` again:

```sh
ctest --test-dir build -R display_binary_afe7 --output-on-failure
python3 tools/make_display_test_roms.py build/test-roms
./build/display_binary_afe7 build/test-roms/main_afe7.bin build/test-roms/display_afe7.bin
```

### HALT and BUSY binary regression

```sh
ctest --test-dir build -R display_binary_halt --output-on-failure
python3 tools/make_display_test_roms.py build/test-roms
./build/display_binary_halt build/test-roms/main_halt.bin build/test-roms/display_halt.bin
```

This runs real, standalone MC6809 binary instructions on both CPU
cores: main writes `$FD05` to halt the display CPU, writes a marker to
shared `$FC80`, and releases HALT. The test checks that the display
PC remains frozen during HALT; after release, the display ROM sets
`$AFF0` (BUSY) and writes a shared-RAM marker at `$9F82`.
There is no dependency on an original ROM dump.

ATTENTION's sub-CPU hardware trigger is still not mapped. The existing
`MainBoard::signalAttention()` path does **not** establish how an
actual display ROM should raise the event; this test deliberately does
not fabricate one.

The main CPU program at `$5000` writes `$80` to `$FC80`.
The display CPU program at `$B000` waits for the same value at
`$9F80`, then writes `$80` to VRAM through the three color planes.
The test asserts shared RAM, physical VRAM and the first rendered white
pixel. CTest creates temporary binaries automatically; the manual commands
above keep them under `build/` for inspection.

## Build

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## Current scope

- Portable fixed-width compatibility types for legacy `BYTE`, `WORD`, `DWORD`, and `LONG` usage.
- Existing `Sim09` 6809/6309 instruction core compiled as portable C++.
- Existing `SimZ80` instruction core compiled as portable C++.
- Legacy MFC register-window callbacks removed from the portable core.
- Z80's FM-11-specific memory/MMR wiring is deliberately not embedded in the CPU core. The initial portable hook uses the supplied flat `Memory` buffer; board-specific address translation will be added in the CPU-board layer.

No SDL frontend or complete FM-11 machine is implemented yet.

## Headless serial CPU runner (experimental)

On Linux, `fm11-headless` runs an MC6809/HD6309 raw binary with
standard input/output connected to the main RS-232C byte queues.
It is intended for early serial driver bring-up, **not** NitrOS-9 booting:
there is no IPL, disk controller, OS image loader or execution pacing yet.

```sh
./build/fm11-headless --binary program.bin --load 0x1000 --pc 0x1000 --cpu 6809 --clock 2000000
```

Use `--instructions 100000` for a bounded smoke run. Ctrl-C stops an
unbounded run. Input is processed without canonical line buffering when
attached to a TTY; the original terminal settings are restored on exit.

## FM-11 NitrOS-9 boottrack bring-up

The floppy IPL sources in `sueyasu/fm11-nitros9` load the first
stage from track 0, side 0, sector 5 onwards. A raw, **contiguous**
boottrack payload (not a whole floppy image) may be supplied directly:

```sh
./build/fm11-headless --boottrack boottrack.bin --level 1 --cpu 6809
./build/fm11-headless --boottrack boottrack-l2.bin --level 2 --cpu 6809
```

Level 1 expects exactly 17 × 256 bytes at `$EB00`, starting at
`$EB02`. Level 2 expects exactly 21 × 256 bytes at `$E100`,
starting at `$E102`. This bypasses ROM IPL/FDC sector loading only;
it does **not** emulate any subsequent boot-time disk reads. Normal
NitrOS-9 startup therefore requires further disk-controller support.
Use `--instructions N` for bounded diagnostic runs.

## D88 floppy images

The headless runner accepts either a 327680-byte raw 2D image or a D88
2D image via `--floppy`. D88 sectors are selected by their C/H/R IDs,
not by assuming they appear in sector-number order. The initial
read-only MiniFDC implementation requires a complete 40-track, two-sided,
16-sector/side disk with 256-byte sectors and rejects malformed,
missing, duplicated or non-2D sector records.

## Host monitor and serial PTY

The headless emulator starts **halted**. Its original terminal is a
line-oriented monitor, not the guest console. Startup prints a serial
PTY path, e.g. `/dev/pts/5`; connect from another terminal with
`screen /dev/pts/5` (or `picocom`). The PTY slave is configured for
raw 8-bit communication. Guest Ctrl-C passes through the serial PTY
without terminating the emulator.

At the `fm11>` prompt, use `c` to run, `stop` to pause,
`s` to step one instruction, `r` for registers, `m HEXADDR [COUNT]`
for CPU-logical memory, `mp HEXADDR [COUNT]` for physical memory,
`reset` to reset and reload the supplied bootstrap, and `q` to quit.
The monitor is available while the CPU runs. `--instructions` pauses the
CPU at the limit instead of terminating the monitor.


### CPU idle waiting (6809/6309)

The headless runner executes instructions **without a real-time CPU throttle**
while running. When a guest executes `SYNC` or `CWAI`, the CPU stops
executing instructions and the host waits using `poll()` for PTY or monitor
activity and the next PTM timer edge. During that wait, host elapsed time
advances the 9700 Hz PTM timebase so a timer IRQ can wake NitrOS-9.

`CWAI` keeps its pre-stacked interrupt frame; `SYNC` does not stack.
A masked IRQ wakes `SYNC` without entering its vector, while `CWAI`
requires an accepted interrupt. The guest's normal (non-waiting) execution
still drives PTM from emulated CPU cycles; the host clock is used only while
the CPU is asleep. This is **not** a cycle-exact real-time emulation mode.

### Testing without USART Tx interrupts

To diagnose persistent CPU load while NitrOS-9 is waiting for input, launch
`fm11-headless` with `--no-tx-irq`. This temporarily suppresses only the
8251-compatible **transmit** interrupt request; serial transmission, reception
and receive interrupts remain operational. The default behavior is unchanged.
Compare host CPU usage at the Shell prompt with and without the switch.
This is a diagnostic option, not a verified FM-11 hardware correction.

### CPU idle diagnostics

Add `--idle-log idle.csv` to write a CSV sample approximately once per host
second. The samples contain executed instructions, SYNC/CWAI entries, measured
host wait duration for each state (milliseconds), IRQ entries accepted by the
CPU, PTM IRQ assertions, and current PC/CC/wait state. Counts are interval
deltas, not cumulative. Compare samples at the NitrOS-9 shell prompt.

## Boot from a 2D floppy image

With an FM-11 NitrOS-9 bootable raw 2D or D88 floppy image, no separate
boottrack file or OS level is needed:

```sh
./build/fm11-headless --floppy disk.d88 --cpu 6809
```

The FM-11 ROM's initial floppy load is simulated: track 0, side 0,
sectors 1 and 2 are copied to `$0400` and execution starts at `$0400`.
The IPL contained in those sectors then loads the boottrack using the
emulated MiniFDC. This is a disk IPL handoff, not execution of the
original FM-11 system ROM. `--boottrack FILE --level 1|2` remains available.
The monitor's `reset` command reloads the disk IPL from the mounted image.

## 8088/V20 boot-ROM trace (experimental)

The separate diagnostic runner executes an external raw **4096-byte** FM-11
8088 boot-ROM image, beginning at the x86 reset vector FFFF:0000. No ROM
binary is included in this repository. This is a CPU-card diagnostic, not
full FM-11 boot: the remaining hardware and CPU instructions are incomplete.

```sh
./build/fm11-8088-romtrace --rom bootrom86.bin --cpu 8088 --instructions 10000 --trace
./build/fm11-8088-romtrace --rom bootrom86.bin --cpu v20 --instructions 10000
```

The runner prints the first unimplemented opcode with its physical PC and
instruction number; `--trace` logs per-instruction register values. For now
8088 and V20 share the same implemented instruction subset. This tool does
not modify the 6809/6309 headless boot path.
