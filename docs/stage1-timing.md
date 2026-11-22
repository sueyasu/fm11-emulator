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
- All four TFM forms charge **6 + 3 * bytes transferred** clocks in
  both HD6309 modes. The follow-up now splits each byte into source-read
  and destination-write micro-steps to expose interrupt boundaries.
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

Ordinary-case base cycles for DIVD/DIVQ/MULD have also been added
for immediate, direct, indexed and extended forms. The reference tests
cover those 12 encodings in both modes. The later
`cpu6309_math_variable_cycles` and `cpu6309_math_results` tests
cover signed/overflow cases and the resulting registers/flags.

**Still outside verified Stage 1-C coverage:** a complete reference
enumeration of every HD6309 opcode, every indexed postbyte across every
opcode, operand-dependent DIVD/DIVQ/MULD cycle variations, TFM
interruptibility, detailed illegal-instruction timing, and the entire
native-mode interrupt/stack permutation matrix. A completed focused
CTest suite does not imply cycle-exact implementation of these cases.
Do not classify Stage 1-C as fully complete until those gaps are closed.

References:
- https://colorcomputerarchive.com/repo/Documents/Microprocessors/HD6309/HD63B09EP%20Technical%20Reference%20Guide.pdf
- https://colorcomputerarchive.com/repo/Documents/Books/Motorola%206809%20and%20Hitachi%206309%20Programming%20Reference%20%28Darren%20Atkinson%29.pdf

## Stage 1-C follow-up: HD6309 coverage and variable timing

The HD6309 opcode dispatch inventory is captured as
`tests/data/hd6309_opcode_baseline.csv` and checked by
`hd6309_opcode_coverage`. There are **436 executable opcode forms**
(235 primary / 118 page-10 / 83 page-11), plus two prefix bytes.
Of the three 256-byte opcode pages, 330 encodings are undefined.
This manifest is **source-derived**, not a 436-entry independent
datasheet transcription. The audit prevents silent opcode loss and
new zero-cycle entries. It found and corrected the missing five-cycle
`LDQ #imm` base timing ($CD).

`cpu6309_indexed_cycles` now also checks the legality of all 256
postbytes in both modes. The seven illegal HD6309 postbytes are $92,
$B2, $BF, $D2, $DF, $F2, and $FF; representative legal postbytes
continue to have explicit reference-backed cycle tests.

`cpu6309_math_variable_cycles` covers both modes of DIVD/DIVQ/MULD
with positive and negative operands, hard/soft quotient overflow and
zero divisors. The cycle correction rules follow the hardware-based
findings of hoglet67's 6809Decoder project. Note that these additions
are *timing accounting*, not a comprehensive validation of the
existing DIV instruction result registers and condition codes.

`cpu6309_stack_masks` covers every one of the 256 push/pull masks
for PSHS/PSHU/PULS/PULU in both execution modes. The existing
`cpu6309_interrupt_timing` continues to cover short/full FIRQ and
IRQ/NMI frames with MD[FM,N] combinations.

```sh
cmake -S . -B build -DFM11_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build -R 'hd6309_opcode_coverage|cpu6309_|cpu6809_' --output-on-failure
```

**Follow-up:** TFM now uses resumable read/write micro-steps.
Accepted IRQ/FIRQ/NMI discards a pending source read, restarts the
same TFM opcode after RTI, and preserves the remaining W count.
The implementation handles a shared source/destination register
(e.g. TFM X+,X+) and clears Z on interruption, setting Z after
normal completion. The `cpu6309_tfm_interrupt` CTest exercises
these conditions in both modes, including an ignored/masked IRQ.

Signed DIVD/DIVQ now compute signed quotient/remainder, update the
N/Z/V/C bits, preserve the right state on zero-divide, and distinguish
two's-complement overflow from range overflow. MULD computes a signed
32-bit product and reproduces the hardware-observed high-word Z flag.
`cpu6309_math_results` tests registers, flags, and traps in both
execution modes.

The opcode manifest is still source-derived rather than a fully
independent reference for all 436 instructions, and further
hardware-level differential testing of unusual operand/interrupt
conditions remains useful.

References:
- https://github.com/hoglet67/6809Decoder/wiki/Undocumented-6309-Behaviours
- https://colorcomputerarchive.com/repo/Documents/Books/Motorola%206809%20and%20Hitachi%206309%20Programming%20Reference%20%28Darren%20Atkinson%29.pdf

## Stage 1-C TFM and signed arithmetic follow-up

The TFM core now runs at two interruptible execution boundaries per
byte: **source read** (7 clocks on instruction start, then 1 clock
for later reads), followed by **destination write** (2 clocks).
The sum remains 6+3n clocks if no interrupt is taken. When interrupted
between read and write, the latched byte is discarded and read again
following RTI; timing setup is repeated upon restart. An accepted
interrupt clears Z; normal completion sets Z. A masked interrupt
does not reset the latch. All four directions, identical source/dest
register selection, and wraparound of W are supported.

DIVD/DIVQ/MULD helpers use signed arithmetic and implement the
hard- versus soft-overflow, divide-by-zero and unusual MULD Z-flag
semantics described by Atkinson and hoglet67.

Tests:
- `cpu6309_tfm_interrupt`: IRQ/FIRQ/NMI, each HD6309 mode, interruption
  before or after a destination write, shared registers, masked IRQ.
- `cpu6309_math_results`: signed normal and overflow outcomes, zero
  divisors, divide remainder and CC flags in both modes.
- `cpu6309_cycle_reference`: accumulates split TFM microcycles to
  preserve its preexisting whole-instruction timing assertions.

```sh
ctest --test-dir build -R '^cpu6309_' --output-on-failure
```

Important architectural scope: the CPU exposes a TFM read/write
boundary at each `SimONE()` call, which allows the existing board
interrupt scheduler to accept interrupts at the correct point.
The exact bus subcycle timing inside those micro-steps is not modelled.

## Stage 1-D: NMOS Z80 instruction and interrupt timing

Reference: [Zilog Z80 CPU User Manual](https://www.zilog.com/docs/z80/z80cpu_um.pdf)
(Instruction Set and Interrupt Response tables). The project models
ordinary NMOS Z80 timings, excluding external WAIT/BUSREQ contention.

Changes:

- The ED block-repeat family (LDIR/LDDR/CPIR/CPDR/INIR/INDR/OTIR/OTDR)
  executes **one iteration per `z80emu()`**, not the entire block
  atomically. When repeating, PC is rewound by two bytes and the
  iteration consumes 21 T-states; the final iteration consumes 16.
  IRQ/NMI can now be accepted between iterations. Older tests which
  expected summed T-states from one call have been updated.
- A DD/FD prefix ignored by an instruction now executes the
  underlying opcode and adds four T-states. Consecutive DD/FD
  prefixes and DD/FD before ED instructions are supported. The
  already-fetched opcode is passed to the decoder without another
  M1 fetch, preserving the correct R refresh increment.
- R low seven bits advance once for every M1 opcode/prefix fetch;
  DD/FD CB d op refreshes R twice, not once per instruction byte.
  R bit 7 is retained. HALT repeats the opcode fetch without
  advancing PC.
- Z80 IRQ acceptance now observes IFF1 and the instruction delay
  following EI, counts 13 T-states for IM0/IM1 (IM0 assumes an FFh
  interrupt data bus) and 19 for IM2, and exits HALT correctly.
  NMI takes 11 T-states, preserves IFF2, and RETN/RETI restore IFF1.
  IM0/1/2 aliases decode their mode; IM2 forms its vector through I
  and the external bus byte (default FFh).
- Indexed ADD IX/IY and LD H,(IX/IY+d) implementation defects found
  during timing validation were corrected.

Regression tests:

- `mainz80_cycles` remains the existing focused test suite. Its
  LDIR/CPIR/OTDR assertions now use individual 21/16-T-state passes.
- `mainz80_timing_boundaries` validates absolute memory loads,
  ignored and nested prefixes, R increments, interrupt response,
  EI delay, HALT, IM2 and NMI/RETN.
- `mainz80_opcode_timing_coverage` exercises all 252 non-prefix
  base opcodes and 256 each of CB, ED, DD CB, FD CB, DD, and FD
  (1788 total timing executions) under specified initial flags
  and registers, comparing against an independent manual-based
  timing matrix.

```sh
cmake -S . -B build -DFM11_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build -R 'mainz80_|cpu_timing_matrix' --output-on-failure
ctest --test-dir build --output-on-failure
```

Scope and limitations: this is T-state and interrupt-boundary
validation, not a complete semantic conformance suite for every
undocumented IXH/IYL or DD/FD CB register-copy behavior. IM0 uses
the default FFh bus response rather than arbitrary injected opcode
execution; peripheral daisy-chain RETI signalling and hardware
WAIT/BUSREQ subcycles remain outside Stage 1-D. Independent
testing on real hardware would further improve confidence.

## Stage 1-E: Intel 8088 EA and memory transfer timing (initial pass)

The 8088 decoder now records the ModR/M effective-address cost separately:
BX+SI=7, BX+DI=8, BP+SI=8, BP+DI=7, SI/DI/BP/BX=5,
direct disp16=6; disp8/disp16 adds 4 except for the direct case.
Register-only ModR/M operands have no EA charge.

Reference-classified native 8088 MOV and basic binary ALU/CMP ModR/M
instructions now use distinct register, memory-read, and memory
read/modify/write baselines. Each 16-bit memory read or write adds
four clocks for the 8088's 8-bit external bus compared with byte
access; the EA component is charged once. This is instruction-level
accounting, not a model of the BIU instruction-prefetch queue.

Tests:
```sh
cmake -S . -B build -DFM11_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build -R 'main8088_(ea_timing|cycles)' --output-on-failure
ctest --test-dir build --output-on-failure
```

`main8088_ea_timing` covers all eight ModR/M addressing bases,
8- and 16-bit memory transfer cases, displacement costs, register
operands, memory ALU direction, CMP, immediate MOV, and segment
override. Existing `main8088_cycles` expectations for register
and word-memory MOV are updated.

**Remaining Stage 1-E work:** apply reference-backed instruction
baselines to the remaining opcode families (RMW groups, shifts,
stack, calls/returns, strings and I/O); validate taken/not-taken
control flow and REP iterations; model prefetch queue refill,
overlap, and external bus wait states where needed. In particular,
the non-classified instructions retain the older coarse timing
heuristic; V20 uses its separate provisional model and is not
claimed to be Intel 8088 cycle-compatible. Stage 1-E is not yet
complete at whole-CPU coverage level.

## Stage 1-E follow-up: dynamic shift and branch timing

The 8088 timing pass now handles the D0-D3 shift/rotate family by
distinguishing implicit one-bit operations from CL-count forms.
The latter charge 4 clocks per iteration beyond a register or memory
base, and memory forms include EA plus 8 additional clocks for a
16-bit read/modify/write over the 8088's 8-bit external data bus.
The CPU captures the original count before the shift is executed;
the V20 model remains independent.

LOOP, LOOPE, LOOPNE and JCXZ now distinguish taken and untaken:
LOOP 17/5, LOOPE 18/6, LOOPNE 19/7, JCXZ 16/4
clocks. Near relative CALL now charges 23 clocks on the 8088,
including the additional byte-wide stack transfer.

The `main8088_dynamic_timing` CTest covers these cases, along with
both byte and word memory shifts, register shifts and CALL.
These tests validate instruction-level accounting only, not a
prefetch-queue or bus-cycle trace.

```sh
cmake -S . -B build -DFM11_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build -R 'main8088_(cycles|ea_timing|dynamic_timing)' --output-on-failure
ctest --test-dir build --output-on-failure
```

Reference: Intel, *iAPX 86/88 User's Manual*, Instruction Set
Reference Data (execution clock tables):
https://ardent-tool.com/CPU/docs/Intel/808x/manuals/210912-001.pdf

Remaining work includes indirect CALL/JMP, FF/FE and other RMW groups,
RET/IRET/interrupt path, REP-string timing, I/O and prefetch/bus
overlap. Stage 1-E remains incomplete; no claim of physical 8088
cycle-exact timing is made.

## Stage 1-E follow-up: FF/FE groups, stack, I/O and strings

The Intel 8088 timing pass now records the ModR/M extension field.
The same FF opcode is classified separately for INC/DEC, near/far
indirect CALL, near/far indirect JMP, and PUSH. FE INC/DEC byte,
8F POP r/m16, LEA, LDS/LES, TEST and XCHG are also classified by
register versus memory operand and EA cost. The 8088 timing table
for immediate/indirect IN/OUT, PUSHF/POPF, near/far returns,
far immediate CALL and several fixed-clock instructions replaces
the old four-clock fallback where these cases were unclassified.

String timing handles MOVS, CMPS, STOS, LODS and SCAS byte and word
operations. Each REP iteration remains one `executeOne()` invocation:
the first iteration includes the nine-clock setup, subsequent
iterations only the per-element charge. REP CX=0 charges only setup.
The repeating state is discarded on interrupt entry and reset.
The independent NEC V20 timing model is unchanged.

The earlier JCXZ model was corrected from 16/4 to **18/6**
clocks for taken/not taken (8088 reference).

New tests:
- `main8088_group_timing`: FF/FE extension cases, register/memory
  operands, LEA/LES/LDS, TEST/XCHG, PUSHF/POPF, IN/OUT and returns.
- `main8088_string_timing`: all ten string width/opcode combinations,
  REP iteration totals, REP stopping on ZF changes and zero CX.
- `main8088_dynamic_timing`: updated JCXZ 18/6 reference cases.

```sh
cmake -S . -B build -DFM11_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build -R 'main8088_(group_timing|string_timing|dynamic_timing|ea_timing|cycles)' --output-on-failure
ctest --test-dir build --output-on-failure
```

Reference values: Intel iAPX 86/88 instruction timing tables as
reproduced at https://www2.math.uni-wuppertal.de/~fpf/Uebungen/GdR-SS02/opcode_i.html .

**Not modeled:** the hardware BIU prefetch queue, concurrent fetch
and operand bus cycles, DRAM wait states, DMA contention or externally
observed individual bus T-states. Group 3 MUL/DIV operand-dependent
timings, exceptions and externally delivered interrupts still need
further characterization. The scheduler reports best-case
instruction-level estimates, not a bus-cycle-exact trace.

### Stage 1-E fixed Group 1 / Group 3 coverage

The 8088 postinstruction classifier additionally covers 80h-83h
immediate arithmetic (register and memory including read-only CMP)
and F6h/F7h TEST, NOT and NEG. The ModR/M extension decides whether
a memory operation is read-only or read/modify/write. The dedicated
`main8088_alu_group_timing` CTest checks byte/word and
register/memory encodings.

MUL, IMUL, DIV and IDIV remain on the legacy approximate path:
they require operand-dependent timing that cannot be represented
by a single static opcode constant. They are not counted as
cycle-verified instructions.

```sh
ctest --test-dir build -R 'main8088_(alu_group_timing|group_timing|string_timing|dynamic_timing|ea_timing|cycles)' --output-on-failure
```

### Stage 1-E: 8088 interrupt entry and exception timing

The external interrupt entry paths now report the 8088 hardware
response costs independently of the currently decoded opcode:
**61** clocks for PIC-acknowledged INTR and **50** for NMI.
Both values include the reference processor's interrupt-entry
sequence; no second stack/vector transfer charge is added.
The software INTO instruction now accounts for **73** clocks
when overflow is set and **4** when clear.

Existing opcode timing remains 72 for INT3, 71 for INT imm8
and 44 for IRET. The new `main8088_interrupt_cycles` CTest
checks these seven paths, including vector transfer, stack
push/pop, IF clearing and IRET restoration.

```sh
cmake -S . -B build -DFM11_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build -R 'main8088_(interrupt_cycles|alu_group_timing|group_timing|string_timing)' --output-on-failure
ctest --test-dir build --output-on-failure
```

Reference: Intel, *iAPX 86/88 User's Manual*, interrupt timings
and interrupt-response sequence
(https://ardent-tool.com/CPU/docs/Intel/808x/manuals/210912-001.pdf).

These are instruction/event-level reference timing totals. PIC
acknowledge bus-cycle phase, prefetch overlap, external wait-state
timing, DMA HOLD and full TF trap boundaries are not modelled.
MUL/IMUL/DIV/IDIV still lack operand-dependent timing verification.

### Stage 1-E: unsigned MUL microcode-derived variable-clock model

The 8088 `F6 /4` and `F7 /4` (unsigned MUL) paths now
calculate a per-instruction clock estimate from the original
8086/8088 `CORX` conditional-add loop. For the Intel-published
register ranges 70-77 (byte) and 118-133 (word), the current
bounded model adds one clock for each set multiplier bit in the
lower 7 / 15 positions of AL / AX. Memory sources add six clocks
plus the already decoded effective-address cost. The multiplier
is captured *before* AX is overwritten with the product.

This is **not yet a hardware-verified exact timing formula**:
the bounds and branch dependence are grounded in Intel's tables
and the published microcode analysis, but the precise boundary
alignment of the conditional branches and BIU prefetch remains to
be verified with real-hardware cycle traces. The test
`main8088_mul_timing` verifies the implemented model's endpoints,
bit dependence, memory EA, and resulting product; it is a
**model regression**, not an independent measured timing oracle.

IMUL signed sign-adjustment branches and DIV/IDIV quotient and
exception-dependent timings remain separate outstanding tasks.
The existing NEC V20 provisional timing remains unchanged.

References:
- Intel iAPX 86/88 timing tables:
  https://ardent-tool.com/CPU/docs/Intel/808x/manuals/210912-001.pdf
- Ken Shirriff, reverse-engineering the original 8086 MUL microcode:
  https://www.righto.com/2023/03/8086-multiplication-microcode.html

```sh
cmake -S . -B build -DFM11_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build -R 'main8088_(mul_timing|interrupt_cycles|alu_group_timing)' --output-on-failure
ctest --test-dir build --output-on-failure
```

### Stage 1-E: signed multiply and division timing estimates

The Intel 8088 signed `F6/F7 /5` (IMUL), unsigned `/6`
(DIV), and signed `/7` (IDIV) normal-result paths now report
bounded **operand-sensitive estimates** rather than the four-clock
fallback. In IMUL the model uses the absolute accumulator multiplier
(the input to the shared CORX loop), counts conditional additions in
the lower seven/fifteen bits, and adds a small sign-processing
allowance for negative multiplicand, operand and result.
The normal-result estimates remain inside Intel's published
register timing intervals: IMUL8 80–98, IMUL16 128–154.

DIV/IDIV use the number of set quotient bits as a deterministic
proxy for the subtract/no-subtract decisions of the CORD
restoring-division microcode. Signed forms also include a small
sign-normalization allowance. The output falls inside the documented
ranges (DIV8 80–90; DIV16 144–162; IDIV8 101–112;
IDIV16 165–184); memory operands add six clocks plus EA.

**Important:** These are intentionally **heuristic, bounded
estimates**, not datasheet-verified values for each operand and
not a microcode-cycle-exact implementation. The count of subtraction
branches and the exact sign normalization cost must be traced
against real 8088 hardware before those values are considered
verified. Divide-by-zero/overflow still follows the existing INT 0
functional path without a verified exception latency.

`main8088_signed_div_timing` is a regression test for this
interim model, result registers, signs and a divide-by-zero
transition. It is NOT an external timing oracle.

```sh
cmake -S . -B build -DFM11_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build -R 'main8088_(mul_timing|signed_div_timing|interrupt_cycles)' --output-on-failure
ctest --test-dir build --output-on-failure
```

References:
- Intel iAPX 86/88 instruction timing table:
  https://www2.math.uni-wuppertal.de/~fpf/Uebungen/GdR-SS02/opcode_i.html
- Ken Shirriff, original 8086 division microcode:
  https://www.righto.com/2023/04/reverse-engineering-8086-divide-microcode.html
- Ken Shirriff, original 8086 multiplication microcode:
  https://www.righto.com/2023/03/8086-multiplication-microcode.html

Remaining Stage 1-E: real-8088-verified operand timing and exception
microcode, 4-byte prefetch queue/BIU-EU overlap, external WAIT,
and shared-bus DMA contention.

### Stage 1-E: opt-in 8088 BIU prefetch timing shadow (foundation)

Added `Intel8088PrefetchTiming` and integrated it with the 8088
instruction-byte fetch path and postinstruction timing finalizer.
It observes up to **four abstract byte slots**: prefix, opcode,
ModR/M, displacement and immediate fetches all consume slots.
At the end of an instruction a provisional one-byte-per-four-clock
refill budget is applied (capped at four slots). A non-sequential
CS:IP transfer clears the queue. Hardware interrupt entry also
clears it. Snapshot diagnostics expose depth, total instruction
bytes, demand bytes, buffered bytes, provisional refills and
flushes.

**Important limitation:** this is a **passive timing-only shadow**,
not a functioning prefetch FIFO: the CPU continues to read each
instruction byte from the bus as executed; this observer does not
speculatively read or hold the bytes. Thus it does not yet model
self-modifying code hazards, exact BIU/EU overlap, data-transfer
bus contention, WAIT, DMA HOLD, or queue stall additions to CPU
clocks. In particular, its refill grant of `cycles/4` is a
diagnostic placeholder, **not a verified free-bus scheduler**.
Some taken branches returning to their sequential fall-through
address cannot be detected by the CS:IP comparison alone.
All reported `lastExecutionCycles()` values remain unchanged.

The feature is OFF by default. To enable during an isolated test:
```cpp
auto& cpu=static_cast<Main8088Board::Core&>(board.cpu());
cpu.setPrefetchTrackingEnabled(true);
cpu.executeOne();
const auto counters=cpu.prefetchTimingSnapshot();
```

`main8088_prefetch_shadow` validates the opt-in default, length
accounting, bounded refill, jump-induced flush, conformance-state
reset and lack of V20 instrumentation. It does **not** claim
physical cycle exactness.

```sh
cmake -S . -B build -DFM11_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build -R 'main8088_(prefetch_shadow|cycles|interrupt_cycles)' --output-on-failure
ctest --test-dir build --output-on-failure
```

Next: track actual instruction/data bus transfers and prefetch
queue ownership, synchronize wait-states and DMA, and only then
apply starvation/refill timing penalties. Instruction timing
baselines and V20 execution must remain independently testable.

### Stage 1-E: data/IO bus reservations in the passive 8088 shadow

The 8088 core now distinguishes **instruction-byte fetches** from
**explicit operand data transfers** through four CPU-local access
wrappers: `readData8`, `writeData8`, `readPort8`, and
`writePort8`. They still call the unchanged Main8088Bus interfaces,
so guest memory, I/O devices and CPU architectural behavior are not
altered. The instrumentation is opt-in and is disabled for the V20.

The diagnostic snapshot now exposes memory read/write and I/O
read/write byte totals, plus `operandBusCycles`. Every observed
byte reserves a minimum of **4 bus clocks**; a word occupies two
byte transfers on the 8088. For the provisional per-instruction
refill allowance, the shadow subtracts this occupied budget
from `lastExecutionCycles()`, clamps at zero, and grants at most
four queue bytes at one grant per four remaining clocks.
For example, `MOV [BX],AX` takes 18 instruction clocks and
records two memory writes (8 reserved bus clocks), allowing two
provisional refill slots rather than four.

These reservations are **not added to CPU execution clocks**:
the Intel instruction timing baseline already includes data
transfers. Adding them again would double-charge the bus.
The model still assumes that unused instruction time is
available to prefetch, which is not generally true on hardware.
It does not yet resolve exact ordering of instruction fetch and
operand transfer, queue stalls, DMA, WAIT, refresh or bus phases;
the queue is still a passive accounting shadow, not a FIFO
containing real instruction bytes.

The new `main8088_prefetch_bus_budget` CTest covers memory word
read/write, 16-bit IN, 8-bit OUT, cumulative reservations,
bounded refill, disabled tracking and V20 isolation.

```sh
cmake -S . -B build -DFM11_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build -R 'main8088_(prefetch_shadow|prefetch_bus_budget)' --output-on-failure
ctest --test-dir build --output-on-failure
```

**Next:** integrate real byte-addressed instruction prefetch data,
bus-arbitration phase timing and cache invalidation/flush semantics;
only after hardware-equivalent queue stall behavior is verified
should the prefetch model affect reported instruction clocks.

### Stage 1-E: four-byte instruction FIFO (opt-in functional model)

A second, explicitly enabled mode in `Intel8088PrefetchTiming`
now holds **actual instruction bytes** in a circular four-entry
FIFO. The ordinary `fetch8()` implementation returns buffered
bytes without reading guest RAM again, or reads the original
bus on a queue miss. After an instruction, eligible refill slots
are filled with *real* bytes from CS:IP through the normal
8088 memory decoder. The refill is based on the existing
best-effort per-instruction bus budget (instruction cycles less
observed data/IO reservations). On CS:IP discontinuity and
interrupt entry the old FIFO is flushed and refill restarts at
the destination. All offset arithmetic wraps at 16 bits.

Enable separately from passive shadow mode:

```cpp
auto& cpu=static_cast<Main8088Board::Core&>(board.cpu());
cpu.setPrefetchByteQueueEnabled(true); // disabled by default
cpu.executeOne();
// cpu.prefetchTimingSnapshot() reports queue and bus counters
```

The new `main8088_prefetch_fifo` regression exercises
preserved prefetched bytes after later memory modification,
sequential consumption, branch-induced flush, target fetch,
conformance reset, disabled-mode direct fetch, and V20 isolation.
The existing `main8088_prefetch_shadow` and
`main8088_prefetch_bus_budget` tests keep passive mode coverage.

**This is not a cycle-accurate 8088 BIU**. Refill timing uses a
per-instruction estimate, not bus-phase arbitration; demand fetches
do not yet add queue-starvation clocks; the CPU still runs each
instruction atomically and reports its existing timing. The
instruction byte FIFO is optional, rather than enabled by default,
to avoid changing running CP/M and firmware behavior before
more complete validation. No external DMA or WAIT scheduling is
integrated. Read side effects of speculative accesses to
memory-mapped hardware need explicit review before enabling this
mode in the normal machine.

```sh
cmake -S . -B build -DFM11_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build -R 'main8088_(prefetch_fifo|prefetch_shadow|prefetch_bus_budget)' --output-on-failure
ctest --test-dir build --output-on-failure
```

Next step: account for sequential and control-transfer fetch
delays using BIU bus phases, including the exact relative
timing of execution unit accesses, and only then make queue
starvation affect `lastExecutionCycles()`.
