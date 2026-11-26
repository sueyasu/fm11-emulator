# Stage 3-A: terminal debugger

Use \`./build/fm11-sdl --monitor\` to open the debug prompt on the
launching terminal. SDL rendering remains active. A polling stdin
thread only collects full lines; SDL's UI thread executes commands
after the main CPU, sub CPU and realtime PTM workers acknowledge pause.

Commands (numbers are **hexadecimal**):
- \`help\`: command summary
- \`info cpus\`: installed CPUs and current execution owner
- \`cpu main6809|main8088|z80|sub\`: change debugger target ONLY
- \`stop\` and \`cont\`: stop/restart all workers
- \`step [COUNT]\`: single-step selected CPU while other CPUs remain stopped
- \`regs\`: show target CPU register bank
- \`x ADDRESS [LENGTH]\`: side-effect-free target CPU memory dump
- \`xp ADDRESS [LENGTH]\`: 1 MiB main physical RAM dump
- \`log start FILE\`, \`log stop\`, \`log status\`: append session I/O to a log

\`step\` on an inactive main/optional card is refused; select that
card using guest hardware mechanisms first. The sub CPU can be stepped
independently. Device I/O locations return FF from \`x\`, without
consuming FIFOs, acknowledging interrupts or changing emulation state.
Bus-accurate I/O inspection can be added later as an explicit operation.

Stage 3-A deliberately excludes breakpoints, disassembly and
watchpoints, reserved for 3-B/3-C. Input editing uses the terminal's
canonical/echo mode initially; history and Tab completion can be
added independently with libedit. Existing --trace-* output does not
become part of the monitor session log.
