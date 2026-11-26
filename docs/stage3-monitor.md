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
- `step [COUNT]`: single-step selected CPU while other CPUs remain stopped; prints registers and the next instruction
- `regs`: show target CPU register bank
- `dis [ADDRESS] [COUNT]` (alias `u`): inspect instructions from ADDRESS, or the current PC; default 8, maximum 64 instructions; addresses hexadecimal, COUNT decimal by default
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

Stage 3-B instruction inspection uses **external disassemblers only**.
The locally implemented `MonitorDisassembler.h` and its private test
have been deleted. No instruction decoder, instruction tables or
disassembler classes are built into the emulator.

- MC6809/HD6309: Capstone M680X (`CS_MODE_M680X_6809` / `CS_MODE_M680X_6309`)
- 8088/V20: Capstone x86 16-bit mode. V20-specific native and 8080
  compatibility opcodes require further upstream validation; the tool does
  **not** claim to recognize those extensions.
- Z80: DeZ80 4.0.1, via a separate small Rust command-line adapter.
  The adapter calls DeZ80's `Instruction::decode_one`; no opcode tables
  or locally implemented decoder are used.

**Installation on Ubuntu** (for `dis`; not required for emulation, building
`fm11-sdl` or running the portable CTest suite):

```sh
sudo apt install python3-capstone cargo
cargo build --manifest-path tools/dez80-bridge/Cargo.toml --release
```

The Capstone Python module must be available to `python3`, for example
using `python3 -c "import capstone"`. If distro Python cannot import it,
install Capstone into a suitable Python environment instead.

On `dis [ADDRESS] [COUNT]` or after `step`, the monitor copies a bounded,
read-only instruction byte window and invokes `tools/disassemble.py` as a
separate process. The script uses Capstone or the DeZ80 Rust adapter and
returns formatted instructions. Neither dependency is loaded into the
emulator process, and no decoding work is added to the guest hot path.
You can override the Rust adapter path with `FM11_DEZ80_BRIDGE`.
External errors (missing Python module or unbuilt Rust adapter) are
reported in the monitor; no silent fallback to the removed decoder occurs.

memory watchpoints and machine register inspection are Stage 3-C.
 Input editing uses the terminal's
canonical/echo mode initially; history and Tab completion can be
added independently with libedit. Existing --trace-* output does not
become part of the monitor session log.
