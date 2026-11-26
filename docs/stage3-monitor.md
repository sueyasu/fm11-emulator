# Stage 3-A: terminal debugger

Use `./build/fm11-sdl --monitor` to open the debug prompt on the
launching terminal. SDL rendering remains active. A polling stdin
thread only collects full lines; SDL's UI thread executes commands
after the main CPU, sub CPU and realtime PTM workers acknowledge pause.

Commands (addresses are **hexadecimal**; step counts and dump lengths are **decimal** by default, or `0x`-prefixed hexadecimal):
- `help`: command summary
- `info cpus`: installed CPUs and current execution owner
- `info machine`: read-only board status (RAM, 6809 IRQ/FDC state, 8088 PIC/DMA masks, Z80 bank)
- `info io FDXX`: 6809 FD00–FDFF register access counters and last write (does not read the actual I/O port)
- `cpu main6809|main8088|z80|sub`: change debugger target ONLY
- `stop` and `cont`: stop/restart all workers
- `step [COUNT]`: single-step selected CPU while other CPUs remain stopped; prints registers and the next instruction when unidasm is installed
- `regs`: show target CPU register bank
- `dis [ADDRESS] [COUNT]` (alias `u`): decode selected CPU memory at ADDRESS, or current PC; default 8 and maximum 64 instructions. Addresses are hexadecimal and COUNT decimal by default
- `x ADDRESS [LENGTH]`: side-effect-free target CPU memory dump (main8088 also accepts `SEG:OFF`)
- `xp ADDRESS [LENGTH]`: 1 MiB main physical RAM dump
- `b HEXADDR`: set an execution breakpoint on the selected CPU; main8088 accepts `SEG:OFF` and stores its 20-bit physical address
- `bl`: list all CPU-specific execution breakpoints
- `bc [HEXADDR]`: clear one breakpoint, or all breakpoints for the selected CPU
- `w r|w|rw HEXADDR`: set a one-byte memory access watchpoint on the selected main CPU (read, write, or both)
- `wl`: list memory watchpoints
- `wc [HEXADDR]`: clear watchpoints on the selected CPU (all if address omitted)
- `log start FILE`, `log stop`, `log status`: append monitor input/output to a log. Logging is off by default; repeated `log start` does not reopen the file and `log stop` closes it. Starting again appends to the same file

`step` on an inactive main/optional card is refused; select that
card using guest hardware mechanisms first. The sub CPU can be stepped
independently. Device I/O locations return FF from `x`, without
consuming FIFOs, acknowledging interrupts or changing emulation state.
Bus-accurate I/O inspection can be added later as an explicit operation.

Stage 3-B (in progress): execution breakpoints are checked before instruction
execution for main6809, main8088, z80 and the independent display CPU (sub).
They do not patch guest memory. On a hit, all CPU/PTM workers pause and the
monitor prints the CPU and PC. `cont` skips the triggering breakpoint once
to avoid immediately stopping at the same instruction. With no breakpoints,
the main CPU hot path checks only an atomic enabled flag before reading
CPU type or PC. With breakpoints, each CPU uses a compact, per-CPU atomic
bitset (main8088: 20-bit physical addresses; other CPUs: 16-bit addresses).
The execution threads perform no string conversions, allocations, set lookups,
or mutex acquisitions to test breakpoints. UI commands retain an ordered set
for listing entries; atomic bitmaps allow the execution workers to read them
safely without blocking. Main8088 breakpoints
are matched against the 20-bit physical CS:IP address.

Stage 3-B-1: external MAME `unidasm` integration (complete)

Obtain the MAME `unidasm` executable for Stage 3-B-1. **Ubuntu 24.04 LTS
`mame-tools` does not include `unidasm`** (confirmed against the Ubuntu
package file list), whereas newer Ubuntu releases such as 26.04 do.

On a release that packages it:

```sh
sudo apt install mame-tools
command -v unidasm
```

For Ubuntu 24.04, build from the [MAME source](https://github.com/mamedev/mame)
using the [official build instructions](https://docs.mamedev.org/initialsetup/compilingmame.html).
The build system supports `TOOLS=1 EMULATOR=0` to build utility programs
without the MAME emulator executable. After building, provide the resulting
`unidasm` executable via your `PATH` or set `FM11_UNIDASM` to its
absolute path. MAME build dependencies are separate from fm11-emulator's.

```sh
# Run inside an appropriately prepared MAME source checkout:
make TOOLS=1 EMULATOR=0 -j"$(nproc)"
export FM11_UNIDASM="/absolute/path/to/built/unidasm"
```

Verify the path with `"$FM11_UNIDASM" -arch z80` (this command displays
usage because an input file is required).

The monitor keeps its `dis`/`u` interface and invokes
`tools/disassemble.py` only on demand. The Python adapter sends the
side-effect-free memory snapshot to MAME `unidasm` using binary stdin.
There are no emulator-local opcode tables, and the MAME source is not
vendored into this repository. `FM11_UNIDASM` can override the
executable path, including a custom MAME build.

CPU mapping (MAME's architecture identifiers):

- MC6809: `m6809`
- HD6309: `hd6309`
- Z80: `z80`
- 8088: `x86_16` (16-bit x86 decoding; instruction legality on an
  original 8088 is not verified by this general-purpose mode)
- V20 native mode: `nec`

Stage 3-B-2: V20 8080 mode integration

MAME's `nec_disassembler` already implements native mode and 8080
emulation mode (including V20-specific `CALLN` and `RETEM`). The
`tools/mame/unidasm-nec80.patch` patch changes only MAME's
`src/tools/unidasm.cpp`: it registers a new `nec80` architecture
that selects MAME's existing 8080 decoder. No MAME opcode tables or
emulator CPU code are changed.

For an existing MAME source checkout, with the FM-11 checkout in
`/path/to/fm11-emulator`:

```sh
cd /path/to/mame
git apply --check /path/to/fm11-emulator/tools/mame/unidasm-nec80.patch
git apply /path/to/fm11-emulator/tools/mame/unidasm-nec80.patch
make TOOLS=1 EMULATOR=0 -j"$(nproc)"
# The executable location depends on the MAME build setup:
export FM11_UNIDASM="/absolute/path/to/patched/unidasm"
```

The patch was prepared against the upstream MAME `src/tools/unidasm.cpp`
structure. If `git apply --check` fails on a different source version,
do not force it: inspect the mode configuration and architecture table
before adapting the two small hunks.

You can smoke-test the two NEC modes:

```sh
printf '\220' | "$FM11_UNIDASM" - -arch nec -basepc 100 -count 1
printf '\000' | "$FM11_UNIDASM" - -arch nec80 -basepc 100 -count 1
```

The monitor automatically selects `v20`/MAME `nec` or
`v20_8080`/MAME `nec80` using the V20 CPU's `emulation8080()`
state. A stock unpatched `unidasm` remains supported for all
Stage 3-B-1 CPU modes, but will explicitly report the need for a
patched binary if an 8080-mode V20 disassembly is requested.
`FM11_UNIDASM` must point at the rebuilt version when using
`nec80`; no separate CPU-mode selection command is necessary.

Stage 3-B-3: final integration checks

The monitor captures at most `16 * COUNT` bytes, wrapping the selected
CPU's visible address space at 64 KiB for 6809/6309/Z80 and 1 MiB for
8088/V20. The external MAME backend remains the sole instruction decoder.
A disassembly listing is linear from the requested address: a jump does
not cause `dis` to follow its target, so subsequent data may appear as
instructions. Stepping is distinct and executes a guest instruction.

The adapter converts the monitor's instruction count into a bounded
byte-span for `unidasm -count`, because MAME's `-count` argument counts
PC units rather than decoded instructions. It then limits the output
to the requested number of instructions. MAME's original mnemonics and
operand formatting are preserved. Guest memory is not modified.

Tests now cover: 6809 indexed addressing and page-10 opcodes; HD6309
LDQ/TFM/BITMD; Z80 CB/ED/DD-CB prefixes; 8088 ModR/M and segment
overrides; V20 native mode and V20/8080 jump sequences; V20 CALLN/RETEM;
20-bit address labels, the 64-instruction limit, and next-instruction
display after `step`. The unidasm integration checks do not implement
CPU opcode decoding locally.

To require *actual* patched MAME NEC 8080 verification, set
`FM11_REQUIRE_NEC80=1`; this turns an otherwise skipped nec80
integration check into a failure if the patch is missing.

```sh
export FM11_UNIDASM=/absolute/path/to/patched/unidasm
export FM11_REQUIRE_NEC80=1
cmake --build build
ctest --test-dir build -R 'unidasm_|debug_monitor' --output-on-failure
ctest --test-dir build --output-on-failure
```

For V20 CP/M-80 images that map 8080 address DC00 into physical
address 1DC00, use `cpu main8088`, then `b 1DC00` to stop on that
physical address. A displayed instruction's address is the physical
address; `JMP $DF5C` still names the 8080 logical jump destination.
This particular memory mapping must not be assumed for other guest
configurations.

For automated verification, `unidasm_adapter` uses a mocked external
process and has no MAME dependency. `unidasm_live` checks the installed
MAME decoders for all CPU modes and skips tests if the required binary
is absent (or the NEC extension is absent unless explicitly required).
`debug_monitor` checks the output after `step` and `dis` when MAME is
installed and validates the missing-tool error otherwise. Neither Python
nor MAME is needed for the core emulator's normal execution.

Stage 3-C-A: read-only machine/board diagnostics (in progress)

`info machine` pauses guest workers, then displays per-board information
using existing getters only. It does not read live device I/O ports or
acknowledge IRQs. It includes installed memory size, active CPU board,
6809 IRQ/FDC state and counters, 8088 PIC/DMA masks, and Z80 RAM bank.

`info io FDXX` displays the 6809 board's cumulative read/write
counters and the last written value for the named FDxx register. It
does not return or consume the device's *current* register value.
This is intentionally limited to the 6809 board's existing counters,
not a universal I/O trace. For example:

```text
fm11[main6809]> info machine
fm11[main6809]> info io FD06
```

Stage 3-C-B: main-CPU memory watchpoints (initial integration)

The `w`, `wl`, and `wc` commands use optional access observers on the
6809, Z80 and 8088/V20 memory bus entry points. Read and write bitmaps
are per target. The worker only sets an atomic event when an access
matches; the SDL thread pauses workers and formats the event afterward.
Execution occurs through the watched instruction, so a watchpoint
stops *after* the reported memory access rather than before it.

```text
fm11[main8088]> w rw 1DC00
fm11[main8088]> wl
fm11[main8088]> c
Watchpoint: main8088 write 1DC00 value=C3
fm11[main8088]> wc 1DC00
```

6809 and Z80 watchpoint addresses are 16-bit **CPU-visible logical**
addresses, including memory-mapped I/O addresses read/written through
their CPU memory access entry points. Z80 IN/OUT instructions use a
separate I/O port path and are not included. The 8088/V20 watchpoint
addresses are 20-bit bus addresses; for the current V20 CP/M-80 memory
layout, 8080 logical DC00 maps to physical 1DC00.

The regular debugger `x`/`dis` inspection uses peek methods and
does not trigger a watchpoint or consume I/O input. A `read` watchpoint
can also match instruction fetch bytes when they travel through the
CPU's ordinary memory read entry point. Watchpoints monitor bus
access attempts, including writes that hardware may discard.

**Current scope limits:** 6809 DMA accesses that go through
`readPhysical`/`writePhysical` rather than CPU memory access methods
are not observed; display-sub-CPU watchpoints are not yet supported.
8088/V20 DMA accesses do pass through the shared bus methods and can
trigger a watchpoint, but the current report does not distinguish the
initiating DMA channel from CPU instructions. Shared-memory accesses
by other boards are only reported under the board whose observed entry
point was called. Thus these commands must not yet be treated as a
complete physical-memory access tracer.

Stage 3-C remaining: DMA-aware physical-memory observation, display
sub-CPU coverage, and explicit read/write I/O access tracking with
source attribution. Memory access observers remain uninstalled in
normal execution without `--monitor` and do not poll memory.


Input editing uses the terminal's
canonical/echo mode initially; history and Tab completion can be
added independently with libedit. Existing --trace-* output does not
become part of the monitor session log.
