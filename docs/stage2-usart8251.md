# Stage 2-A: 8251A register-model refinement

The FM-11 main-board USART at FD06 (data) / FD07 (control/status)
is shared by the 6809 and 8088/V20 CPU cards. It now models:

- Hardware reset and internal-reset command (bit 6), returning to mode setup.
- Mode instruction first, then one/two sync characters for synchronous modes,
  followed by command instructions.
- TxEN, RxEN, send-break, CTS and DSR inputs; separate diagnostic Tx IRQ mask.
- Status TxRDY, RxRDY, TxEMPTY (byte-complete transport approximation),
  parity/overrun/framing errors, SYNDET and DSR.
- Error-reset and hunt-mode command handling; receiver IRQ when data is available.
- Separate host ingress FIFO and 8251 status. PTY bulk reads must not lose
  text simply because the host delivered several bytes in one batch.

Deliberate limits: no Tx/Rx bit-shift timing, baud-rate generator, serial
parity generation/validation, synchronous hunt detection, or real physical
CTS/DSR wiring yet. The external Communication DIP switch values are
stored but remain unapplied. The host output queue is not an on-chip
8251 holding register: TxEMPTY reflects its drain state until timing
modeling is introduced. Hardware receive overrun is injectable for
tests; host burst delivery alone does not cause overrun.

The reference Windows emulator's Fm11RS232C also bypasses the mode
instruction; unlike that approximation, the new model requires a mode
instruction after reset, as an actual 8251A does. Existing regression
tests use the mode word 4Eh before the Tx/Rx command.

```sh
cmake --build build
ctest --test-dir build -R 'usart8251_registers|main6809_serial_console|machine_config' --output-on-failure
```
