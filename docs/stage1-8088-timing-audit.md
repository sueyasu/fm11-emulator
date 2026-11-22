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
| MOV moffs ↔ accumulator (A0–A3) | Fixed 14 clocks (both widths on 8088) | `main8088_missing_timing` | Corrected this audit |
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

## Follow-up: instruction clock table cross-check

A subsequent check against the 8088 instruction clock table found
additional differences. The previous A0/A2 byte MOV estimate of
10 clocks was an incorrect carry-over of the 8086-oriented timing:
on the **8088**, all A0-A3 direct accumulator memory MOV encodings
are recorded as **14 clocks**. Both production constants and the
preceding regression have been corrected.

The main string-op timing table also previously conflated the
byte and word timings for CMPS, LODS and SCAS. The updated
non-REP values are:

| Instruction | Byte | Word |
|---|---:|---:|
| MOVS | 18 | 26 |
| CMPS | 22 | 30 |
| STOS | 11 | 15 |
| LODS | 12 | 16 |
| SCAS | 15 | 19 |

The REP iteration values are also distinct where the reference
specifies different timings: CMPS byte/word 22/30; LODS
byte/word 13/17. Existing tests now check those distinctions.
The documented setup cost remains charged once per REP
instruction, consistent with the current one-iteration-per-
`executeOne()` implementation.

ESC D8–DF was still using a generic four-clock fallback.
The Intel 8088 reference gives **2 clocks for a register
ModR/M operand** and **12+EA for memory ModR/M operands**;
the new low-cost correction uses the already decoded
`timingModRmMemory_` and `timingEaCycles_`.
`main8088_esc_timing` tests all eight ESC opcode values
with register, [BX], direct, displacement and prefix cases.
The FM-11 emulator still has no 8087 coprocessor.

Source used for this pass: the Intel 8088 instruction timing
opcode table at https://pastraiser.com/cpu/i8088/i8088_opcodes.html,
cross-checked against Intel's 8086/8088 manual clock
conventions. These values exclude dynamic instruction queue
starvation and external wait states.

These changes deliberately require neither the optional FIFO
nor a per-T-state simulator in normal CPU execution.

## Follow-up: 8088 Group 1/3 memory-word transfers

The instruction-cycle audit found that the immediate Group 1
(80h/81h/82h/83h) and unary/test Group 3 (F6h/F7h)
timing overrides applied identical memory-cycle totals to
byte and word operands. On an 8088 byte-wide bus, each
additional 16-bit memory read or write requires four clocks.
These formulas now distinguish word operands without enabling
prefetch or a per-cycle event scheduler:

| Family | Byte-memory formula | Word-memory formula |
|---|---|---|
| Group 1 CMP immediate (read-only) | 14 + EA | 18 + EA |
| Group 1 modifying ALU (RMW) | 23 + EA | 31 + EA |
| Group 3 TEST immediate | 11 + EA | 15 + EA |
| Group 3 NOT/NEG (RMW) | 24 + EA | 32 + EA |

The existing test constants use [BX] with EA=5, so 8088
word Group 1 ADD expects 36 clocks, CMP expects 23,
word TEST expects 20, and NOT/NEG expect 37. Further
cases cover short sign-extended immediate (83h) and
Group 3 TEST alias /1.

The conditional jump JCXZ now explicitly invalidates its
optional diagnostic prefetch queue when *taken*, even when
the relative displacement is zero. Reference instruction
clocks (18 taken, 6 untaken) are unchanged.

Reference clock methodology: Intel *8086 Family User's
Manual*, instruction-clock tables and 8088 word-transfer
note (four clocks for each 16-bit memory access).
<https://www.inf.pucrs.br/~calazans/undergrad/orgcomp_EC/mat_microproc/intel-8086_family_Users_Manual.pdf>

Normal CPU execution still performs only a few small integer
operations. These changes do not allocate event buffers
or perform speculative bus reads.
