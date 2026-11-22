# Stage 1-E — 8088 instruction timing audit

## Objective

Count 8088 instruction cycles at roughly the fidelity and runtime
cost of the other FM-11 CPU cores. Do **not** target pin-level bus
accuracy or require per-byte speculative prefetch, T-state arbitration,
or full microcode reconstruction for ordinary execution.

Normal execution uses a static 256-entry opcode estimate, fixed
instruction-family corrections, and a small number of dynamic
counters. The existing prefetch/FIFO/T-state/event diagnostic
experiments must remain opt-in; they must not become the Stage 1-E
completion gate or run in the normal machine.

## Source paths examined

- `Main8088Board::Core::executeOne()`: `k8088Cycles` baseline table;
  instruction timing overrides before dispatch; `TimingFinalize`
  opcode-group calculation; explicit per-opcode dynamic adjustments
- `decodeModRm()`: effective-address clock calculation
- `Intel8088PrefetchTiming`, `Intel8088BusTStates`,
  `Intel8088BusEvents`: optional diagnostics, not production
  timing prerequisites
- `tests/main8088_*timing.cpp`: instruction-family regression
  coverage and operand-dependent timing

## Classification

| Family | Current timing approach | Existing regression coverage | Status / follow-up |
|---|---|---|---|
| ALU r/m↔reg | ModR/M, effective-address clocks, width, destination direction | `main8088_alu_group_timing`, `main8088_ea_timing` | Modelled; complete direction/width/EA matrix still needed |
| Group 1 immediate ALU | Group extension, read/modify/write, EA | `main8088_alu_group_timing` | Modelled; test CMP/read-only and all subops |
| MOV reg↔r/m; segment; immediate | Direction/EA/width corrections | `main8088_mov_bus_reference`, `main8088_ea_timing` | Modelled; ModR/M aliases tested separately |
| MOV moffs ↔ accumulator (A0–A3) | Fixed 10/14 clocks | `main8088_missing_timing` | Corrected this audit |
| XLAT (D7) | Fixed 11 clocks | `main8088_missing_timing` | Corrected this audit |
| PUSH/POP registers, segment, flags | Opcode/group and direct corrections | `main8088_group_timing` | POP CS (0F) corrected this audit |
| Calls, returns, jumps, interrupt entry | Opcode/group and explicit adjustments | `main8088_group_timing`, `main8088_interrupt_cycles`, `main8088_dynamic_timing` | Further branch/return variant matrix needed |
| Jcc, LOOP, JCXZ | Taken/untaken timing | `main8088_dynamic_timing`, `main8088_cycles` | Undocumented 60–6F aliases also flush on taken branch |
| String and REP | Per-iteration and repeat-entry overhead | `main8088_string_timing` | Representative tested; need DF, early termination, word cases |
| Shift/rotate | Count-dependent corrections, EA/width | `main8088_dynamic_timing` | Boundary count and zero-count variants remain |
| Group 3 MUL/IMUL/DIV/IDIV | Operand-dependent microcode-inspired count | `main8088_mul_timing`, `main8088_signed_div_timing` | Modelled; exception path still approximate |
| XCHG, LEA, LES/LDS, TEST, Group 4/5 | Group-specific timing | `main8088_group_timing` | Representative forms tested |
| Decimal adjustment and miscellaneous | Some explicit fixed corrections, others use baseline fallback | `main8088_cycles` and semantic tests | **Audit remaining implemented opcodes** against Intel clocks |
| ESC/coprocessor and unusual undocumented encodings | Baseline estimate or special handling | Decoder inventory and semantic tests | Need explicit classification of timing confidence |
| NEC V20 native/8080 emulation | Separate approximate model | Other V20 tests | Outside Intel 8088 cycle calibration; preserve behavior |

A decoder recognizes all primary opcode bytes through a mix of
ranges, individual cases and prefixes, but this **does not** mean
all opcode variants have verified timing. The static dispatch inventory
is `main8088_opcode_inventory`; its results should be cross-checked
with ModR/M extensions and accepted undocumented aliases.

## Regressions fixed in this pass

1. POP CS (0Fh) was executing correctly but retained a 4-clock
   generic fallback instead of the 12-clock segment-pop estimate.
2. Direct accumulator memory MOV (A0–A3) retained the 4-clock
   fallback. Explicit 10-clock byte and 14-clock 8088 word cases
   have been added.
3. XLAT (D7h) retained the 4-clock fallback instead of 11 clocks.
4. Taken undocumented Jcc aliases (60h–6Fh) did not flush
   the optional prefetch instrumentation when the target was the
   sequential fall-through, unlike documented Jcc.

The new `main8088_missing_timing` regression covers all four
corrections without enabling any production bus modelling.

## Remaining work, prioritized

1. Audit every actually accepted **8088** instruction family
   against a reference clock category; distinguish deliberate
   approximation from accidental 4-clock fallbacks.
2. Expand systematic tests for ModR/M EA modes, memory word
   transfers, branches, shifts, REP and ALU directions. Avoid
   tests that merely duplicate a formula without an external
   reference or a separate established invariant.
3. Keep the hot path light: constant-time primitive arithmetic
   and table lookups; no per-instruction heap allocations, event
   vectors, speculative memory reads, or T-state simulations.
4. Run the full 8088/whole-machine regression suite and assess
   instruction throughput before declaring Stage 1-E complete.

Reference: Intel *iAPX 86/88 User's Manual* (1981), instruction
clock tables. Published clock totals are minimum/nominal instruction
times; variable BIU queue stalls are not exhaustively represented.
