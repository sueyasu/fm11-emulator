# Stage 2-Disk — initial opt-in FDC Type-I timing

The 2D and 2HD FDCs now share a small per-drive mechanical head state.
`[floppy] timing = fast` remains the default, preserving pre-existing
immediate transfer and boot behavior. `timing = timed` opts in to a first
mechanical approximation, not strict WD179x conformance.

## Implemented in this stage

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

## Important limitations

This is the **first** timed-mode change, not a complete FDC rewrite.
FDC status differences between the 2D and 2HD controller adapters remain,
as do approximations in interrupt publication and RESET behavior.
READ/WRITE command timing, disk rotation, INDEX pulses, DMA byte pacing,
Lost Data, seek verification against on-media IDs, and motor spin-up
are **not yet modeled**. The current fast mode is the regression baseline.

## Configuration and validation

```ini
[floppy]
timing = timed
```

`timing = fast` is the default; `strict` is intentionally rejected.
Test the new mode separately with 2D and 2HD boot media, and keep
`timing = fast` for everyday work until those tests pass. The
`fdc_command_decode` regression tests the shared seek model and
rejects reads made solely by changing the track register.

```sh
git pull
cmake --build build
ctest --test-dir build --output-on-failure
```
