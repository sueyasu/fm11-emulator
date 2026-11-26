# Stage 3-A: terminal debugger

Use `./build/fm11-sdl --monitor` to open the debug prompt on the
launching terminal. SDL rendering remains active. A polling stdin
thread only collects full lines; SDL's UI thread executes commands
after the main CPU, sub CPU and realtime PTM workers acknowledge pause.

Commands (addresses are **hexadecimal**; step counts and dump lengths are **decimal** by default, or `0x`-prefixed hexadecimal):
- `help`: command summary
- `info cpus`: installed CPUs and current execution owner
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

Stage 3-B-1: external MAME `unidasm` integration

Install the MAME command-line tools on Ubuntu:

```sh
sudo apt install mame-tools
command -v unidasm
```

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

**V20 8080 emulation mode is not yet supported in Stage 3-B-1.** The
monitor explicitly reports this condition instead of displaying native
NEC instructions. Stage 3-B-2 will enable it using a small patch to
MAME's `unidasm` mode selection (not to the FM-11 CPU core).

The adapter converts the monitor's instruction count into a bounded
byte-span for `unidasm -count`, because MAME's `-count` argument counts
PC units rather than decoded instructions. It then limits the output
to the requested number of instructions. MAME's original mnemonics and
operand formatting are preserved. Guest memory is not modified.

For automated verification, `unidasm_adapter` uses a mocked external
process and has no MAME dependency; `debug_monitor` checks the output
when MAME is installed and the missing-tool error otherwise. Neither
Python nor MAME is needed for the core emulator's normal execution.

Memory watchpoints and machine register inspection remain Stage 3-C work.
Input editing uses the terminal's
canonical/echo mode initially; history and Tab completion can be
added independently with libedit. Existing --trace-* output does not
become part of the monitor session log.
