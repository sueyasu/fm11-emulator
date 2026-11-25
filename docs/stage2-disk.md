# Stage 2-Disk — opt-in FDC timing

The 2D and 2HD FDCs now share a small per-drive mechanical head state.
`[floppy] timing = fast` remains the default, preserving pre-existing
immediate transfer and boot behavior. `timing = timed` opts in to a first
mechanical approximation, not strict WD179x conformance.

## Initial Type-I stage (retained)

- Type-I RESTORE, SEEK, STEP, STEP IN and STEP OUT each start a pending
  head movement; the logical track register is distinct from the physical
  head position.
- STEP direction is remembered, and the U (update-track) bit controls
  whether the track register follows physical STEP motion.
- CPU instruction cycles advance a deterministic timebase for both
  6809 and 8088 configurations; no host sleep or PTM worker is involved.
- SEEK duration uses an approximate 6/12/20/30 ms step-rate table,
  plus 15 ms settling time. These are provisional, not calibrated
  measurements of any specific FM-11 drive.
- While moving the controller reports BUSY; after completion its physical
  head position and command-completion latches are updated.
- In timed mode, sector I/O requires a completed move to the requested
  physical track. A logical track-register write alone cannot reposition
  the head.

## Limitations of the initial Type-I stage

The original implementation was only a **first** timed-mode change, not a complete FDC rewrite.
FDC status differences between the 2D and 2HD controller adapters remain,
as do approximations in interrupt publication and RESET behavior.
READ/WRITE command timing, disk rotation, INDEX pulses, DMA byte pacing,
Lost Data, seek verification against on-media IDs, and motor spin-up
were **not modeled by the initial Type-I stage**. See Stage 2-A/B below
for the subsequently added rotation, INDEX and sector command delay. The current fast mode is the regression baseline.

## Configuration and validation

```ini
[floppy]
timing = timed
```

`timing = fast` is the default; `strict` is intentionally rejected.
Test the new mode separately with 2D and 2HD boot media, and keep
`timing = fast` as the compatibility baseline until OS regression tests pass. The
`fdc_command_decode` regression tests the shared seek model and
rejects reads made solely by changing the track register.

```sh
git pull
cmake --build build
ctest --test-dir build --output-on-failure
```

## Stage 2-A/B: rotational timing and Type-II commands

With `[floppy] timing=timed`, each physical drive now tracks its spindle
phase in CPU-derived virtual nanoseconds. Nominal rates are 300 rpm (2D)
and 360 rpm (2HD). A 2 ms INDEX pulse is derived from the phase rather
than the number of polling reads; the two FDC channels share a common
virtual timebase while each drive retains its own phase.

READ/WRITE SECTOR enters BUSY until the requested sector arrives.
At ID arrival, the adapter starts the existing atomic DMA or PIO transfer;
the modeled data-transfer interval continues to elapse before the FDC
completes the command and publishes its final interrupt. In PIO mode,
DRQ becomes visible at ID arrival, while DMA terminal count can precede
FDC completion. The first-stage transfer-rate approximations are
250 kbps (2D) and 500 kbps (2HD). Sector IDs are provisionally uniformly distributed around the
track, with a small lead-in offset. Absent IDs or mismatched physical
cylinders report RNF after five revolutions rather than immediately.
Both timing events are serviced on the owning CPU thread, and new commands
cancel a pending Type-II request.

**Known limitation / Stage 2-C boundary:** the actual DMA/PIO byte
transfer remains atomic after the timed interval; it is *not* paced by
individual DRQ assertions. Therefore this implementation does not yet
reproduce Lost Data, the separate DMA terminal-count and FDC completion
edges, physical gap/CRC spacing, or the exact temporal placement of
individual bytes. Full WRITE TRACK/READ TRACK, READ ADDRESS, Force
Interrupt modes, motor spin-up and media-error timing remain separate
future work. These approximations are intended for compatibility testing,
not cycle-accurate WD179x emulation.

The `fast` mode bypasses this new scheduling and keeps the existing
immediate transfer behavior. `--speed` is not forcibly changed: both
FDC and CPU time advance from the same modeled CPU cycle counts.

## Stock 8088/V20 boot-ROM completion handshake

The original 4096-byte FM-11 8088 ROM (version 3.3, supplied for
development analysis, not redistributed) issues a 2HD READ SECTOR
at ROM offset 074Ah and polls FD37 bit 6 in its routine at 0684h.
It then reads FD30 at 07C6h and expects zero. A premature nonzero
status without a recognized WD179x error bit is reported as
`Disk error 0F`.

In timed mode, the 8088 bus must expose FD37 and clear its interrupt
latch at the start of every 2HD Type-II command; it asserts bit 6
only after the modeled transfer end. The former Type-I completion
state must also be cleared when Type-II begins, not later when
its DMA starts. The `timed_floppy_rom_status` regression reproduces
the ROM's RESTORE/READ and SEEK/READ status-poll sequence without
embedding proprietary firmware in the repository.
