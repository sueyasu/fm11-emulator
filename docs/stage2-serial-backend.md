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
Physical CTS/DSR/DCD modem-signal feedback, synchronous USART modes,
and 1.5-stop-bit hardware formats are not yet implemented. Headless
supports `--config FILE` for the same `[serial]` configuration; its
monitor stays on stdin.

```sh
cmake --build build
ctest --test-dir build --output-on-failure
```
