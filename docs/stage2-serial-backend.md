# FM-11 serial backend (Stage 2-A)

The system-board 8251A (FD06/FD07) uses a common Linux host endpoint
in both SDL and headless:

```ini
[serial]
serial0 = pty
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
