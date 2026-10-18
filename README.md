# FM-11 Emulator

Linux port and ongoing development of the FM-11 emulator.

This first revision establishes a portable CMake build for the existing 6809/6309 and Z80 CPU cores. The Windows MFC/Direct3D frontend is intentionally not part of the Linux build.

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

### CPU utilization

The headless runner now paces emulated CPU instruction cycles against the
requested `--clock` frequency (default 2 MHz) using a monotonic clock and
a short host-side poll wait when ahead. This avoids the previous unlimited
busy loop at an idle OS prompt. The guest CPU still executes its own wait
loops; this is real-time throttling rather than guest idle-loop detection.
