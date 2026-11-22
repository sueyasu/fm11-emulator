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

## Follow-up: primary ALU direction and width matrix

Compared the 00h–3Bh primary arithmetic/logic encodings
(ADD, OR, ADC, SBB, AND, SUB, XOR and CMP) against the 8088
opcode timing tables, distinguishing register-only operands,
a register destination with a memory source, and a memory
destination that must be read/modified/written.

The earlier implementation used the same base (9+EA) for
register-destination **and** CMP, and 16+EA for modifying
memory destinations. The new 8088 classification uses:

| Form | Base clocks (plus EA) | 16-bit memory access adjustment |
|---|---:|---:|
| register / register | 3 | none |
| register destination, memory source | 13 | +4 |
| memory destination, modifying ALU | 24 | +8 |
| CMP with a memory operand (read only) | 13 | +4 |

The read-only CMP classification is intentional: CMP updates
flags but does not write the memory operand. Exact scheduling
of instruction fetch stalls remains out of scope; the adjustment
is a low-cost, whole-instruction approximation consistent with
the existing word-bus-counting design.

New `main8088_alu_direction_matrix` checks all eight ALU
families across both widths, both direction bits, and
register/memory ModR/M forms (64 cases), as well as a segment
override. The preexisting EA timing test was updated, and
the FE/FF group timing suite now covers additional register
and memory INC/DEC cases and the undocumented FF /7 PUSH
aliases. These tests do not enable experimental FIFO or
T-state execution.

Source: <https://pastraiser.com/cpu/i8088/i8088_opcodes.html>,
plus the Intel 8086/8088 instruction-clock convention for
additional byte-bus transfers. The remaining group-specific
timing rules must be checked individually: do not apply the
general ALU destination rule to all grouped opcodes.

## Follow-up: Group 2 shift/rotate and FF indirect CALL

Cross-checking D0h–D3h against the 8088 opcode timing table
identified a uniform eight-clock undercount in **memory**
shift/rotate operations. The corrected reference calculations are:

| Instruction | Register | Memory |
|---|---:|---:|
| D0/D1 (count 1) | 2 | 23+EA |
| D2/D3 (count CL) | 8+4×CL | 28+EA+4×CL |

In the emulator's existing 8-bit-bus cost model, 16-bit memory
read/modify/write adds 8 clocks on top of these entries. The
count is the original 8088 eight-bit CL (not a 5-bit count
mask); a CL value of zero still uses the variable-count timing
formula. The timing calculation is constant-time; no T-state
simulation or instruction-event allocation has been enabled.

The `main8088_shift_timing_matrix` regression exercises all
eight D0h–D3h ModR/M extensions, register/memory modes,
byte/word widths, CL 0/1/3/31/255 and a segment override.
`main8088_dynamic_timing` has updated memory reference clocks.

The FFh /2 **near indirect CALL** had a register-mode base
of 20 clocks, whereas the 8088 opcode table specifies
24 register clocks, or 29+EA with a memory operand.
Only the register-mode constant changed; other FF group
timings already match their instruction-class references.
`main8088_group_timing` now covers both AX and BX
register targets.

Reference: <https://pastraiser.com/cpu/i8088/i8088_opcodes.html>
(D0–D3 and FFh opcode tables). No extra work is added
to the normal CPU execution path beyond revised arithmetic.

## Follow-up: F6/F7 memory arithmetic and far returns

The Intel 8088 opcode clock table specifies **+6+EA** for
register-to-memory-source forms of MUL/IMUL/DIV/IDIV
relative to the register forms, including 16-bit operands.
This premium is **already part of** the F6/F7 group-specific
clock calculation, and adding another general +4 word-bus
adjustment would double count it. The existing successful-
arithmetic model therefore remains unchanged.

Added successful 16-bit IMUL and IDIV memory timing cases,
and 8-bit DIV memory coverage in
`main8088_signed_div_timing`. These cases compare actual
8088 instruction cycles, AX/DX results, and memory/register
source variants. Divide-error paths (zero divisor or quotient
overflow) remain explicitly approximate and are not presented
as verified hardware cycle timings.

The far-return timing table was found reversed in the 8088
initial opcode classification. Intel's 8088 table specifies
**33 clocks for RETF (CBh)** and **34 clocks for RETF
imm16 (CAh)**. The original 8088's undocumented aliases
C9h and C8h use the matching 33/34 clock counts, while V20
continues to decode them as LEAVE/ENTER.

Updated `main8088_group_timing` and added
`main8088_far_return_timing` to check clocks, returned CS:IP,
SP advancement, C8/C9 aliases, and V20 decode isolation.

No extra allocations, speculative bus reads, or T-state
operations have been added to normal CPU instruction timing.

References: <https://pastraiser.com/cpu/i8088/i8088_opcodes.html>
(F6/F7, C8–CB).

## Correction: primary ALU and Group 1 published 8088 timing

The earlier Stage 1-E notes were based on a **transfer-count
heuristic** which incorrectly added 4 or 8 clocks to some
16-bit-memory ALU forms, and treated the CMP memory
destination as a faster read-only instruction. Although CMP
does not write its operand, the published 8088 timing
classification does **not** support that cycle estimate.

The opcode-by-opcode **Intel 8088** reference table records:

| Family/form | 8088 clocks |
|---|---:|
| Primary ALU 00–3B, register-only | 3 |
| Primary ALU r/m,reg (includes CMP 38/39) | 24+EA |
| Primary ALU reg,r/m (includes CMP 3A/3B) | 13+EA |
| Group 1 immediate ALU 80–83, register | 4 |
| Group 1 immediate ALU 80–83, memory | 23+EA |

These entries are already for the **8088**, rather than the
8086. They do not call for another unverified word-bus
penalty. The C++ timing formulas now use exactly these
reference classes for byte and word forms. The
`main8088_alu_direction_matrix`, `main8088_ea_timing`,
and `main8088_alu_group_timing` expected values were
corrected at the same time; do not regard the older timing
values in the preceding chronological audit sections as
current.

New `main8088_group1_reference_matrix` covers all eight
immediate ALU extensions across 80h, 81h, 82h alias,
and 83h, register and memory modes, and the prefix
surcharge. This work introduces no extra hot-path
allocations or cycle-level bus scheduling.

Primary timing source:
<https://pastraiser.com/cpu/i8088/i8088_opcodes.html>
(00h–3Bh and 80h–83h tables). Another useful
independent Japanese-language 8088 timing discussion is
<https://qiita.com/hdk_2/items/6f8cb8a7c67342e2a32a>,
though published clock listings and modelling assumptions
are not uniform across sources. The changes here follow
the opcode table literally, pending any hardware-based
correction.

## Stage 1-E audit: Group 2/3 extra word penalties corrected

Cross-checking the opcode-level 8088 table revealed that
the earlier byte-transfer heuristic was applied again on top
of some published **8088** instruction clock totals.
This affected Group 3 TEST/NOT/NEG (F6/F7) and Group 2
shift/rotate (D0-D3) word-memory forms. The prior audit
entries that recommend additional +4/+8 clocks for these
specific forms are superseded by the following reference
values:

| Instruction group | Register | Memory |
|---|---|---|
| F6/F7 TEST /0 or /1 | 5 | 11+EA |
| F6/F7 NOT and NEG /2 or /3 | 3 | 24+EA |
| D0/D1 shift/rotate count 1 | 2 | 23+EA |
| D2/D3 shift/rotate count CL | 8+4*CL | 28+EA+4*CL |

For these specific groups, the timing table does **not**
distinguish byte and word widths. Do not add a further
per-transfer charge to these published totals. This is
consistent with the recent ALU correction, without
discarding word-width corrections in *other* instruction
families whose tables explicitly differ.

The production implementation uses constant-time
instruction arithmetic only; it neither models bus
T-states nor performs speculative memory fetches.
Updated `main8088_alu_group_timing`,
`main8088_dynamic_timing` and
`main8088_shift_timing_matrix`. Added
`main8088_group3_reference_matrix` covering all /0-/3
extensions, 8/16-bit widths, register/memory and segment
override. Multiply and divide timing remain on their
existing separately modelled paths.

Source: <https://pastraiser.com/cpu/i8088/i8088_opcodes.html>
(groups D0-D3 and F6-F7). The table values are
best-case instruction timings; they do not characterize
queue starvation or external WAIT.

## Follow-up: MOV ModR/M reference clocks and LOOPNE

An opcode-by-opcode comparison with the published **8088**
clock table corrected the earlier bus-transfer heuristic for
MOV 88h–8Bh and MOV immediate C6h/C7h:

| Instruction form | 8088 reference clocks |
|---|---:|
| MOV r/m8,r8 and MOV r/m16,r16 | 13+EA (memory), 2 (register) |
| MOV r8,r/m8 and MOV r16,r/m16 | 12+EA (memory), 2 (register) |
| MOV r/m8,imm8 and MOV r/m16,imm16 | 14+EA (memory), 4 (register) |

These **8088** values already include the effect of its
8-bit bus, and are not increased for the word form.
The timing finalizer, the optional
`Intel8088MovBusReference` diagnostic validator, and both
MOV regression files now agree on these values.

The same reference gives `LOOPNE` (E0h) 18 clocks
when taken and 6 when not taken, identical to `LOOPE`.
The previous 19/7 clocks have been corrected and covered
by `main8088_dynamic_timing`.

Reference: <https://pastraiser.com/cpu/i8088/i8088_opcodes.html>
(88h–8Bh, C6h–C7h, and E0h–E2h entries).

No new FIFO, bus-event logging, or T-state simulation is
enabled in normal execution.

## Default CPU execution path and optional diagnostics

The instruction-timing audit also identified a low-cost performance
improvement independent of reference clock corrections: the normal
(non-instrumented) CPU previously entered the prefetch tracker and bus
event recorder for every instruction fetch, even though both features
are disabled by default.

`fetch8()` now reads the instruction byte directly from the emulated
bus when **both opt-in diagnostics are disabled**. When either is
enabled, it retains the original prefetch/event path. The per-
instruction event-vector clear and shadow-prefetch setup/finalizer
are skipped in default execution as well. No speculative reads,
extra allocations or instruction-clock changes are introduced.

The `main8088_diagnostic_fastpath` regression executes MOV from
memory, immediate ADD, unconditional JMP and NOP with diagnostics
disabled, event capture enabled, prefetch shadow enabled, and both
enabled; it compares clocks, IP and register effects and checks that
optional counters/events work only when requested. The existing
`main8088_bus_event_order` and `main8088_prefetch_shadow`
regressions remain applicable.

This is a structural reduction of work on the hot path, not a
measured throughput claim. Before Stage 1-E completion, compare
emulated instruction throughput in a repeatable representative
workload.

## Stage 1-E: explicit fixed-clock instruction families

The opcode inventory was cross-checked with the 8088 instruction
clock table for fixed-clock instructions. Previously, the 04h–3Dh
accumulator-immediate ALU forms and A8h/A9h TEST immediate
happened to receive the correct four cycles by inheriting the
generic four-clock default. They are now assigned **explicit**
four-cycle entries in the existing baseline table, at no
additional per-instruction cost.

The new `main8088_fixed_timing_matrix` independently exercises
all eight ALU immediate families in byte and word forms,
TEST accumulator immediate in both widths, all 90h–97h
NOP/XCHG AX encodings, fixed flag/convert/decimal instructions,
AAM/AAD, and prefix overhead. This is a regression and coverage
improvement, **not a claim** that all primary opcodes are now
timing-verified. Grouped ModR/M, exception/fault timing and
memory/BIU contention are separate coverage requirements.

Reference: <https://pastraiser.com/cpu/i8088/i8088_opcodes.html>
(04h–3Dh, 90h–9Fh, A8h/A9h, D4h/D5h and F5h–FDh).

## Intel 8088 undocumented SALC (D6h)

The primary-opcode inventory identified a real 8088 dispatch gap:
D6h SALC (Set AL on Carry). The published original 8088
opcode listing records two clocks. Execution writes AL=FFh
when CF=1, otherwise AL=00h; AH and FLAGS remain unchanged.
A segment prefix is accepted under the existing prefix
overhead convention. The instruction does not use ModR/M.

The 8088-only handler, `main8088_salc_timing` regression,
and opcode inventory were updated together. Tests cover
CF clear/set, unchanged status flags, unchanged AH, IP,
prefix overhead, and isolation from the V20 variant.
No assertion is made about the V20's undocumented D6h
semantics; that CPU remains unsupported for this opcode.
The 8088 fast execution path adds only a single opcode
case and no additional diagnostic work.

Reference: <https://pastraiser.com/cpu/i8088/i8088_opcodes.html>
(D6h *SALC*, two clocks).
