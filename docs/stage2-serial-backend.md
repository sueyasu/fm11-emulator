# FM-11 serial backend (Stage 2-A)

The system-board 8251A (FD06/FD07) uses a common Linux host endpoint
in both SDL and headless:

```ini
[serial]
serial0 = pty
# flow_control = none
# serial0 = /dev/ttyUSB0
# serial0 = none
```

- `pty` (default): existing pseudoterminal with immediate byte transfer,
  preserving NitrOS-9 and CP/M-86 interactive console behavior.
- `/dev/ttyUSB0`: Linux character-device TTY opened nonblocking, no
  controlling terminal. Guest MODE commands configure host word length,
  parity and stop bits. The DIP SW4..8 selector determines the host baud.
  Unsupported/undefined speed and framing fail rather than guess.
- `none`: no external serial endpoint; the guest 8251A still exists.

`flow_control = none` (default) permits three-wire TX/RX/GND USB adapters:
CTS is treated as asserted in the guest even when a host device reports
an inactive hardware CTS pin. Set `flow_control = rtscts` only when the
adapter and terminal have the CTS/RTS wiring. The guest DTR/RTS commands
continue to control the host outputs in either mode.

The backend is serviced only by the CPU owner, not by the SDL renderer or
the PTM thread. Linux UART hardware performs real bit timing, so the
optional `Usart8251::setBaudRate()` model is **not** also enabled for
physical TTY. PTY keeps the proven immediate-byte behavior.

The asynchronous guest's first mode word updates the live physical TTY.
Synchronous USART modes,
and 1.5-stop-bit hardware formats are not yet implemented. Headless
supports `--config FILE` for the same `[serial]` configuration; its
monitor stays on stdin.

```sh
cmake --build build
ctest --test-dir build --output-on-failure
```

## Physical RS-232C modem control

For physical TTYs only, `SerialBackend::synchronize()` now samples
`TIOCMGET` on each host I/O service. It passes hardware CTS/DSR/DCD
into the shared 8251A; CTS gates TxRDY and the resulting transmit IRQ,
and DSR appears in the USART status bit 7. The guest's command bit 1
(DTR) and bit 5 (RTS) are mapped to Linux `TIOCMBIS` / `TIOCMBIC`
without changing other output lines. An internal USART reset releases
both DTR and RTS at the next host service.

Devices that report `ENOTTY`, `EINVAL`, `ENOSYS`, or `EOPNOTSUPP`
for modem ioctls retain the preexisting virtual asserted inputs, allowing
three-wire USB UARTs to operate without hardware modem pins. Unexpected
I/O failures remain explicit errors. PTY and `none` do not issue modem
ioctls or change the virtual line levels.

Modem pins on real UARTs are sampled at the CPU worker's service cadence,
not at a simulated per-bit clock edge. The direction of signal mapping
is physical host-side DTR/RTS **outputs**, CTS/DSR/DCD **inputs**.

## Physical TTY regression and stop-bit accuracy

Linux POSIX represents 1.5 stop bits only with CS5+CSTOPB. The Intel
8251A encodes one, one-and-a-half, or two stop bits independently of
word length; unsupported host combinations are now rejected instead of
silently changing the framing. The serial_backend regression creates
an extra PTY master/slave pair and opens the slave through the physical
/dev/ path. It verifies 4800 baud, 8N1/8N2 reconfiguration and both
byte directions without requiring a USB adapter.

Physical modem input changes now re-publish 6809 serial IRQ state even
without FD06/FD07 guest access. Queued physical host bytes are withheld
while sampled CTS is low; the PTY backend remains byte-immediate.

## Opt-in virtual character clock

```ini
[serial]
serial0 = pty
timing = clocked
```

`timing = immediate` is the default and retains the fast NitrOS-9 and
CP/M-86 PTY console. `clocked` uses the asynchronous DIP SW4..8 baud
table, then advances the shared 8251A once per *main CPU instruction*
using the instruction cycle count and selected CPU clock. The same
clock applies to 6809/6309, Z80, 8088, and V20. Fractional nanoseconds
are retained across instructions; CPU clock changes reset the fractional
converter. The serial clock is independent of the MC6840 PTM worker,
avoiding double advancement with real-time PTM scheduling.

`clocked` supports `pty` and `none` for explicit timing/diagnostics;
a physical TTY uses host UART framing and rejects `clocked` to avoid
double pacing. Undefined DIP baud or synchronous DIP mode also fail
explicitly. The 8251A's asynchronous mode word still determines the
character length and stop/parity overhead; the external DIP determines
the line baud. Host PTY bulk input is not hardware-clock synchronous,
so clocked mode may intentionally report receive overrun.

A separate deterministic test `machine_serial_timing` verifies both
6809 and 8088 instruction-time progression and serial IRQ publication.

## Stage 2-A completion criteria and scope

Confirmed on external physical 4800-baud connections: NitrOS-9 /T1
interactive shell and CP/M-86 interactive console, including bidirectional
commands and output. Starting picocom before launching the guest shell
eliminated the reported one-time startup garbling; cause is not proven.

Automated regression verifies 75, 150, 300, 600, 1200, 2400, 4800 and
9600 baud mappings through Linux PTYs; it tests 8N1/8N2 configuration
and 7E1, 7O1, 8E1, and CS5/1.5-stop mode translation independent of the
TTY driver. PTY drivers can silently normalize data bits/parity, so this
is **not** a physical adapter parity test. CTS/DSR/DCD and DTR/RTS
are mapped through TIOCMGET/TIOCMBIS/TIOCMBIC when available, but
real modem-pin wiring and full RTS/CTS behavior remain unverified.

Stage 2-A is functionally complete for the tested 4800-baud three-wire
terminal use case; other baud rates, parity, modem wiring and electrical
error behavior remain optional physical-hardware validation.

## Real serial terminal smoke test

On Linux, connect a USB serial adapter to the host, with TX/RX crossed
and a shared GND to the terminal. Use a real RS-232 level converter when
connecting to an RS-232 terminal (never connect TTL UART pins directly).
Set `[serial] serial0 = /dev/ttyUSB0`, `flow_control = none`, and
`[DIPSW] serial_baud_sw4_8 = 4` for 4800 baud. Keep `timing = immediate`
(the physical UART already performs bit timing).

On the terminal select 4800 baud, 8N1, and no hardware/software flow
control. Start `fm11-sdl --config PATH` or `fm11-headless --config PATH`
using the normal boot options and an OS image with serial console enabled.
The startup log should show `Serial device: /dev/ttyUSB0` in SDL or
`Serial: /dev/ttyUSB0` in headless. Verify boot output, input commands,
and bidirectional multi-line text (e.g., NitrOS-9 `dir` or CP/M-86 `dir`).
For a wired RTS/CTS terminal select `flow_control = rtscts` and configure
appropriate handshaking at the terminal. The terminal-side baud/framing
must match the guest mode and DIP setting.

Only one process can safely operate the serial port at a time. Stop
ModemManager and other port clients if they claim it, and make sure the
user has permissions (usually the `dialout` group). Automated PTY tests
validate host I/O and configuration but cannot prove electrical levels,
USB cable wiring, or a particular terminal's handshake behavior.
