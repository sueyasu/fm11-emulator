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
discover SDL2. This frontend loads and executes the **display sub-CPU**
ROM and renders its VRAM using the existing 8-color pixel decoder:

```sh
sudo apt install libsdl2-dev
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
./build/fm11-sdl --rom external/roms/display6809.bin
```

The default ROM path is `external/roms/display6809.bin`; use the extraction
script described above to populate it. Controls: Escape to quit, Space
to pause/resume, F1 to switch 640x200/640x400 display interpretation.
`--high` starts in 400-line mode; `--cycles N` controls the number of
display CPU instructions executed per frame (default 10000). This is a
bring-up throttle, **not** a calibrated real-time CPU clock.

The SDL frontend is optional: when SDL2 is missing the existing core,
headless programs and CTests still build. Set `-DFM11_BUILD_SDL=OFF`
to disable SDL detection explicitly.

**Scope:** This is a display-subsystem bring-up viewer, not yet a complete
FM-11 main CPU + keyboard + disk GUI. Hardware mode selection and
text-mode composition are not yet connected; selecting 200/400 lines is
manual. No ROM image is stored in Git.

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
