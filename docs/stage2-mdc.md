# Stage 2-B4: MDC/HDD control

Stage 2-B3 (FDC) is complete after full CTest and guest-OS boot/FORMAT
verification. B4 work is isolated on branch `stage2-b4`.

## B4-A: command/status compatibility (current commit)

Reference: the original FM-11 emulator `Fm11/MainCpu.cpp`,
`Fm11MDC`, `Fm11MDCSetData`, `Fm11MDCGetData`.

- Parameter requests have status PREQ|DRQ and no IRQ; the 10-byte
  result phase asserts the MDC interrupt until fully consumed.
- SEEK (20h) checks CHR or binary sector addresses against the HDD
  geometry and reports reference SEEK error 90/40/10 for absent
  drives or invalid CHR/LBA, rather than unconditional success.
- Return the AD code (parameter byte 0 low nibble) in result byte 0.
- Return zero in result status byte 3 for NOP, DIAG and SENSE.
- Existing synchronous READ/WRITE DMA and immediate image persistence
  are unchanged. CTest `mdc_command_state` covers the regressions.

## B4-B: timed MDC (implementation)

Provide independent `[hdd] timing = fast|timed`, default fast;
seek, rotation and transfer phases; advancing DMA and completion IRQ
on the virtual clock. Keep the CPU-specific 6844 and 8237 adapters.

## B4-C: completion and error regressions (planned)

Test DMA TC/partial transfer, invalid addresses, reset/interruption
while busy, persistence failure, IRQ gating, and HDD boot/FORMAT/
read-write regressions for NitrOS-9 L1/L2 and CP/M-80/86.

The current B4-B virtual clock defers command result and IRQ until SEEK or
record DMA completion. A read/write command begins with a seek-time estimate
(2 ms settle plus 0.2 ms/cylinder if moving), half-revolution wait
(8.333 ms at nominal 3600 RPM) and 1 ms for the first record. Subsequent
256-byte records advance at 1 ms intervals. Each record is serviced through
the installed CPU-specific HD6844/8237 DMA callback. These are provisional
latencies, not a cycle-accurate MDC model. Writes commit one complete
record after DMA plus successful host persistence; reset/cancellation leaves
future records untouched. The fast mode and preexisting whole-command DMA
remain unchanged. The `mdc_timed_dma` test exercises both CPU cards,
intermediate DMA progress, IRQ/result timing, and reset during WRITE.
