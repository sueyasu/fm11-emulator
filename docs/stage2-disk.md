# Stage 2-Disk — opt-in FDC timing

The 2D and 2HD FDCs now share a small per-drive mechanical head state.
`[floppy] timing = fast` remains the default, preserving pre-existing
immediate transfer and boot behavior. `timing = timed` enables the
Stage 2-B2 mechanical and byte-DRQ model, not strict WD179x conformance.

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

## Type-I history and remaining limits

The initial Type-I implementation covered mechanical SEEK but not
rotation, INDEX, Type-II timing or byte-level DRQ. These were added
subsequently (see below). FDC status differences between the 2D and
2HD adapters, interrupt/RESET approximations, on-media seek verification
and motor spin-up remain limitations. `fast` remains the compatibility
baseline.

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

## Stage 2-A/B1: rotational timing and Type-II commands (historical baseline)

With `[floppy] timing=timed`, each physical drive now tracks its spindle
phase in CPU-derived virtual nanoseconds. Nominal rates are 300 rpm (2D)
and 360 rpm (2HD). A 2 ms INDEX pulse is derived from the phase rather
than the number of polling reads; the two FDC channels share a common
virtual timebase while each drive retains its own phase.

READ/WRITE SECTOR enters BUSY until the requested sector arrives.
In the B1 baseline, DMA/PIO transfer was atomic at sector arrival.
B2 replaces that with individual DRQ deadlines (described below).
Nominal transfer-rate approximations are 250 kbps (2D) and 500 kbps
(2HD). Sector IDs are provisionally uniformly distributed around the
track, with a small lead-in offset. Absent IDs or mismatched physical
cylinders report RNF after five revolutions rather than immediately.

The current B2 implementation retains approximations in physical
gap/CRC spacing, full READ TRACK and READ ADDRESS, Force Interrupt
modes, motor spin-up and media-error timing. It is not cycle-accurate
WD179x emulation.

The `fast` mode bypasses this new scheduling and keeps the existing
immediate transfer behavior. `--speed` is not forcibly changed: both
FDC and CPU time advance from the same modeled CPU cycle counts.

## WRITE TRACK rotational timing (Stage 2-B2)

With `[floppy] timing = timed`, WD179x WRITE TRACK (`F0h`) now
uses the same virtual FDC scheduler as byte-paced READ/WRITE SECTOR.
The guest's existing HD6844 (6809/6309) or 8237 (8088/V20)
channel supplies each stream byte at evenly spaced DRQ deadlines
over a full physical revolution: 200 ms for 2D at 300 rpm and
approximately 166.7 ms for 2HD at 360 rpm. The nominal format
streams remain 6,400 and 10,900 bytes respectively.

The FDC stays BUSY throughout the write interval. DMA address,
terminal count and completion advance with transferred bytes.
FDC completion/INTRQ occurs only at the end of the revolution.
The backing RAW/D88 track is updated only if every DRQ was serviced
and the stream parses successfully; missed bytes (e.g. a masked
DMA channel) report an error without committing a partial track.
A timed WRITE TRACK rejected by a mismatched physical head position or
ongoing SEEK reports an FDC error and never falls back to synchronous
DMA. `--trace-fdc` reports WRITE TRACK begin/completion and bytes served.

Untimed `fast` mode retains its prior immediate WRITE TRACK behavior.
The rotational model does not yet simulate individual index/ID gaps,
CRC timing or mechanical track-to-track stepping beyond existing
seek timing.

CTest `timed_floppy_write_track` checks both CPU families and both
FDC channels, partial progress, completion, masked-DMA failure, and
8088 adapter rejection for mismatched head position and pending SEEK.

## Stage 2-B2 diagnostic retry branch

The `stage2-b2-diag-retry` branch enables byte-paced DRQ for the
6809 HD6844 and 8088/V20 8237 floppy interfaces in `timed` mode.
The `fast` controller path retains sector-atomic transfers.

The 8237 now uses one DMA service per DRQ byte; masked or disabled
channels leave DRQ unserviced and cause Lost Data at the subsequent
byte slot. 2HD C0/H0 FM uses 32 microseconds per byte and normal
2HD MFM uses 16 microseconds. Legacy PIO regression tests must
consume each byte while DRQ is asserted: reading after a full
revolution correctly fails under B2.

Run `ctest --test-dir build --output-on-failure` first, then boot
`fm11-diag` with both 2D and 2HD diagnostic ROMs. In addition,
regress 2D/2HD NitrOS-9 L1/L2, CP/M-86 and CP/M-80 where available
before integration into main. Boot and FORMAT have been exercised
manually, but the final post-commit full CTest remains a separate check.

## Opt-in sector-arrival diagnostics

Use `fm11-sdl --trace-fdc` with `[floppy] timing = timed` when a
boot ROM reports BUSY but the HD6844 count remains unchanged. Standard
error receives a line at each Type-II command, ID/data-field arrival,
DMA dispatch, and FDC transfer completion. For example:

```text
FDC-TIME begin ch=0 d=0 C=0 H=0 R=1 wait_ns=3125000 bytes=256
FDC-TIME event ch=0 sector-arrival overrun_ns=...
FDC-TIME DMA ch=0 mode=132 remaining=0
FDC-TIME event ch=0 complete overrun_ns=...
```

Values above are illustrative, not measured. If the `begin` event is
present but `sector-arrival` never appears, investigate the virtual-time
advance path; if an arrival appears but DMA remains unchanged, examine the
board's controller/DMA dispatch. Diagnostics are off by default and do not
change FDC behavior.

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
