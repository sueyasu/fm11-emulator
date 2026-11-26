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

Stage 3-B instruction inspection is implemented as a separate, read-only decoder, so it does not execute instructions or alter the guest. It understands common MC6809/HD6309, Z80 and 8088/V20 instruction families, including indexed operands and branch targets for many common cases. It is **not yet an exhaustive decoder**: unknown byte patterns are shown as `FCB` (6809) or `DB` (Z80/8088), without guessing undocumented instruction lengths. In particular, 6309-specific, extended Z80 ED and the full 8088 ModR/M instruction space still need broader coverage. Do not rely on instruction boundaries following an unknown opcode. The disassembler is separate from the per-instruction breakpoint hot path and runs only upon a monitor command or explicit single-step.


memory watchpoints and machine register inspection are Stage 3-C.
 Input editing uses the terminal's
canonical/echo mode initially; history and Tab completion can be
added independently with libedit. Existing --trace-* output does not
become part of the monitor session log.
