# Stage 1-A: CPU timing regression baseline

This stage adds a data-driven, CPU-crossing **single-instruction** cycle
comparison, without changing any CPU interpreter or device timing. It is a
regression baseline, **not** a claim of datasheet-accurate cycle counts.

## Run

```sh
cmake -S . -B build -DFM11_BUILD_TESTS=ON
cmake --build build --target cpu_timing_matrix
ctest --test-dir build -R '^cpu_timing_matrix$' --output-on-failure
./build/cpu_timing_matrix tests/data/cpu_timing_cases.csv
python3 tests/report_cpu_timing.py
python3 tests/report_cpu_timing.py --missing
```

Every CSV record contains:
`cpu,name,bytes,expected,status,reference`.

- `cpu`: 6809, 6309 (emulation mode), 6309-native, z80, 8088, v20.
- `bytes`: hexadecimal instruction bytes without spaces; cases start from
  a fresh reset state (6809 at $1000, Z80 at $0000, 8088/V20 at reset).
- `expected`: integer CPU clocks/T-states from a cited source.
- `baseline`: existing emulator regression expectation, not independently
  validated against a hardware reference.
- `verified`: independently checked against a documented CPU timing
  reference; maintainers must record that reference in the final column.
- Each case runs only **one** instruction. Multi-instruction state, taken
  branches, interrupts, bus wait states and string/block repetitions require
  dedicated fixtures and are not covered by this runner.

The reporter counts registered fixtures per CPU, including independently verified
versus baseline cases; its optional missing-byte list is **not** a list of
missing valid instructions (prefixes and invalid encodings are included).

The new test intentionally records only a few known, stable cases; it must
not be mistaken for comprehensive coverage. Existing focused CTests remain
in force.

## Initial implementation inventory (not verified coverage)

| CPU | Existing implementation | Remaining Stage 1 work |
| --- | --- | --- |
| 6809 | Base/page-10 cycle tables, indexed/branch/stack corrections | Validate legal opcodes; distinguish invalid from unclassified zeros |
| 6309 | Page-11 table and partial native adjustments | Complete native and prefixed timing, dynamic instructions |
| Z80 | Base/CB/ED/DD/FD T-state functions | Prefix combinations, interrupt boundaries, repeated instructions |
| 8088 | Coarse opcode table and memory/branch increments | Effective-address and bus/queue timing |
| V20 | Derived 8088 table, coarse 8080 emulation | Independent native and 8080 timing |

Sources: `src/core/sim09/45_timing.inc`,
`src/core/Z80Timing.h`, `src/boards/Main8088Board.cpp`,
and the existing `tests/*timing*.cpp` and `tests/*cycles*.cpp`.

## Exit criteria for Stage 1-A

- CSV test runs through CTest and reports exact CPU, case, expected, actual.
- A mismatch causes a non-zero exit code; invalid rows also fail.
- Baseline and reference-verified data are explicitly distinct.
- No CPU timing behavior is changed.
- Later stages add reference-backed cases and an opcode/addressing-mode
  inventory before claiming comprehensive coverage.

## Stage 1-B: MC6809 timing corrections

Reference: Motorola **MC6809-MC6809E Microprocessor Programming
Manual**, Appendix F, Tables F-1 (opcode cycles) and F-2 (indexed
postbyte cycles):
https://www.maddes.net/m6809pm/appendix_f.htm

Changes in this stage:

- Apply indexed postbyte cycles consistently to the memory RMW $60-$6F
  family, loads/stores and control transfers in $A0-$AF/$E0-$EF,
  LEA $30-$33, and indexed prefixed opcode families. The previous
  predicate omitted e.g. `NEG ,X`, `STA ,X`, and `STB ,X`.
- Correct the 16-bit PCR displacement from 4 to 5 additional cycles
  (8 with indirection) and extended indirect `[nn]` to 5 additional
  cycles in total.
- Correct the MC6809-standard CMPD/CMPY/LDY/STY/LDS/STS cycles
  on page 2 ($10 prefix), and CMPU/CMPS on page 3 ($11 prefix).
  HD6309-only opcodes are not recharacterized here.
- Add 38 reference-backed entries to `cpu_timing_cases.csv`, retaining
  the original Stage 1-A baselines. Adjust the existing LDS/PTM stack
  test's expected accumulator time from 6 to 4 cycles.

Scope: each new fixture exercises one opcode and addressing mode
from a clean reset. It does not prove exhaustive opcode coverage,
or measure hardware-level bus contention. Existing long-branch,
interrupt, and stack CTests remain part of the regression suite.

To check all timing-focused tests:

```sh
cmake --build build --target cpu_timing_matrix main6809_stack_timing
ctest --test-dir build -R 'cpu_timing_matrix|main6809_.*timing|cpu6309_interrupt_timing|cpu6809_full_cycle_db' --output-on-failure
python3 tests/report_cpu_timing.py
```

Remaining for MC6809 coverage audit: enumerate every legal opcode,
indexed postbyte and dynamic branch/interrupt case against the manual;
record additional reference-backed fixtures for all currently uncovered
variants. Completing this inventory is separate from the corrections
above; the runner's case count is not a completeness metric. The 6309
native-mode cycle audit belongs to Stage 1-C.

## Stage 1-B: complete MC6809 opcode and indexed-postbyte audit

The legal MC6809 instruction set has been transcribed independently from
Motorola **M6809PM/AD Appendix C, Table C-1**, into
`tests/data/mc6809_reference.csv`. It contains **268 legal opcode
encodings**: 221 primary, 38 page-2 (`10` prefix), and 9 page-3
(`11` prefix). Prefixes are not counted as standalone instructions.
This inventory excludes HD6309-only opcodes.

The new audit tests are:

- `mc6809_opcode_coverage` (Python): compare the complete reference
  inventory with the interpreter's `OPDEF` dispatch and all three timing
  tables. A missing handler or mismatched base cycle is a test failure.
- `cpu6809_opcode_cycles` (C++): execute each of those 268 legal
  opcodes in **MC6809 personality**, with known reset state, and compare
  the measured cycles to the reference. Indexed instructions use zero
  five-bit displacement (one extra clock). Long conditional branches use
  reset CC=0 to derive taken/not-taken costs.
- `cpu6809_indexed_postbytes` (C++): exercise **all 256** possible
  postbytes across eight instruction families (LEAX, NEG, JMP, LDA,
  STA, STB, LDY, CMPU). Appendix F, Table F-2 gives **217 legal** and
  **39 invalid** encodings in the MC6809 personality. Invalid operands
  must report unknown/invalid timing (zero), never plausible base cycles.

Two other MC6809 base-cycle mismatches found by the full audit have
been corrected: `DAA` (`19`) is 2 clocks (was 3), and extended
`JMP` (`7E`) is 4 clocks (was 3). The postbyte validity filter now
rejects indirect auto-increment/decrement-by-one and the duplicate or
6309-only extended-indirect encodings. Illegal indexed postbytes no
longer leak a nonzero instruction-cycle count after fault handling.

Reference documents:

- Opcode/cycle inventory:
  https://www.maddes.net/m6809pm/appendix_c.htm
- Indexed postbyte modes and cycle adjustments:
  https://www.maddes.net/m6809pm/appendix_f.htm

Run:

```sh
cmake -S . -B build -DFM11_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build -R 'mc6809_opcode_coverage|cpu6809_(opcode_cycles|indexed_postbytes)|main6809_.*timing|cpu_timing_matrix' --output-on-failure
```

These checks establish complete *opcode enumeration*, baseline-cycle
coverage and exhaustive *postbyte encoding* coverage. They do **not**
exhaustively exercise every combination of flags, indexed postbyte and
instruction, all 256 push/pull register masks, interrupt permutations,
or physical bus wait-state timing. Focused dynamic tests cover selected
branch, stack and interrupt cases. Those dimensions should not be
mistaken for a full cycle-by-cycle physical hardware model.

The MC6809 reference CSV is independent of the shared HD6309 timing
implementation; the native HD6309 audit remains Stage 1-C.

## Stage 1-C: HD6309 cycle accuracy

Reference: HD63B09EP Technical Reference Guide, opcode tables
(pp. 11-18), and Darren Atkinson, *Motorola 6809 and Hitachi 6309
Programming Reference*, indexed addressing table (p. 150) and TFM.

The MC6809 personality remains distinct from the HD6309 personality.

Implemented here:

- The native-mode timing pass runs after resolving the full 16-bit
  prefixed opcode, preserving the mode active at instruction entry
  (important for LDMD), instead of losing mode corrections when page
  10/11 tables overwrite the initial value.
- HD6309 extension tables for register, word, E/F accumulator, BITMD
  and LDMD instructions now use documented emulation-mode counts.
  Reference-backed native-mode adjustments cover these opcode
  families and selected non-prefixed instructions.
- All four TFM forms now charge **6 + 3 * bytes transferred** clocks,
  in both HD6309 modes. The interpreter still runs TFM atomically, so
  interrupt delivery *during* a transfer is not yet accurate.
- HD6309 long conditional branches take five clocks, whether taken or
  not, unlike MC6809's six clocks when taken.
- HD6309 indexed addressing supports E/F/W displacement forms and the
  special ,W, [,W], n,W, [n,W], ,W++, [ ,W++ ], ,--W and
  [ ,--W ] encodings. Native indexed-address timing reductions are
  applied separately; unrecognized postbytes are rejected.
- `cpu6309_cycle_reference` compares representative instructions in
  emulation/native modes, including mode-invariant instructions,
  prefixed arithmetic, TFM transfers and long branches.
  `cpu6309_indexed_cycles` checks indexed cycle differences and
  illegal postbytes in both modes.

```sh
cmake -S . -B build -DFM11_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build -R 'cpu6309_|cpu6809_|main6809_.*timing|mc6809_opcode_coverage|cpu_timing_matrix' --output-on-failure
```

**Still outside verified Stage 1-C coverage:** a complete reference
enumeration of every HD6309 opcode, every indexed postbyte across every
opcode, operand-dependent DIVD/DIVQ/MULD cycle counts, TFM
interruptibility, detailed illegal-instruction timing, and the entire
native-mode interrupt/stack permutation matrix. A completed focused
CTest suite does not imply cycle-exact implementation of these cases.
Do not classify Stage 1-C as fully complete until those gaps are closed.

References:
- https://colorcomputerarchive.com/repo/Documents/Microprocessors/HD6309/HD63B09EP%20Technical%20Reference%20Guide.pdf
- https://colorcomputerarchive.com/repo/Documents/Books/Motorola%206809%20and%20Hitachi%206309%20Programming%20Reference%20%28Darren%20Atkinson%29.pdf
