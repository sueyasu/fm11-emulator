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
