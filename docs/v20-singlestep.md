# NEC V20 instruction conformance tests

Use hardware-generated test cases from
[SingleStepTests/v20](https://github.com/SingleStepTests/v20) (V20 native V1,
currently 1.0.4). This is a separate reference CPU from the Intel 8088.
The external data is not vendored into this repository.

## Acquire the test data

```sh
git clone --filter=blob:none --sparse https://github.com/SingleStepTests/v20 external/v20-tests
git -C external/v20-tests sparse-checkout set v1_native
```

## Build and run

```sh
cmake --build build -j$(nproc)
ctest --test-dir build -R mainv20_singlestep_smoke --output-on-failure

# First inspect basic instructions and V20 extensions separately:
python3 tests/run_v20_singlestep.py --opcode 00 --limit 100
python3 tests/run_v20_singlestep.py --opcode 0F10 --opcode 0F31 --limit 100
python3 tests/run_v20_singlestep.py --opcode C8 --opcode C9 --limit 100

# Survey all opcode files, including grouped opcodes:
python3 tests/run_v20_singlestep.py --all --limit 100 > /tmp/v20-test-results.log 2>&1
tail -30 /tmp/v20-test-results.log
```

The driver also accepts `--data-dir`, `--runner`, `--fail-fast`, and
`--limit 0` (all cases). The C++ runner accepts `--cpu v20`; without
a CPU selector it continues to run the original Intel 8088 core, preserving
existing test workflows.

V20 native extensions are not Intel 8088 aliases. In particular,
`0Fxx` extension opcodes, `C0/C1` immediate shifts,
`C8/C9` ENTER/LEAVE and REPC/REPNC prefixes have V20-specific
semantics. Only metadata-marked undefined, undocumented, FPU and
prefix entries are automatically skipped, as in the existing 8088 driver.

Scope: instruction-level final CPU and RAM state, not cycle-accurate
prefetch/bus timing. This is an initial compatibility survey. Passing
it does not yet validate the V20's 8080 emulation mode; the upstream
`v1_emulation` dataset is a separate workstream.

## ROL4/ROR4 undefined AL upper nibble

The NEC instruction manual explicitly states that the upper four bits of
AL are not guaranteed after `0F 28` (ROL4) or `0F 2A` (ROR4). Hardware
test cases nevertheless record those bits, producing apparent register
mismatches that cannot be treated as conformance failures. For these two
instructions only, the V20 driver sends an AX comparison mask of `FF0Fh`
to the native adapter; AX bits 15:8 and 3:0, all other registers, RAM,
IP and documented FLAGS remain checked. The Intel 8088 driver uses
`FFFFh` for all AX comparisons. The offline regression
`mainv20_nibble_flag_mask` covers the masking rules.

## Phase 2: V20 arithmetic and bit-field corrections

The `mainv20_extended_arithmetic` regression covers these native V20
differences from Intel 8088:

- `D5` (AAD) uses a radix of 10 even when the encoded immediate differs;
  Intel 8088 still uses the encoded immediate.
- `0F18/0F19` (immediate TEST1) update SF, ZF and PF from the masked
  result and clear CF/OF; `0F10/0F11` retain their previous behavior.
- `0F31/33/39/3B` (INS/EXT) accept immediate encodings with any
  ModR/M.reg field. Bit position uses four bits; the SI/DI byte index
  advances by two on crossing a 16-bit word boundary.
- `0F20/22/26` (ADD4S/SUB4S/CMP4S) perform 8-bit binary ADC/SBB and
  DAA/DAS-style decimal correction for each packed-BCD byte. This
  matters when the hardware test suite includes arbitrary nibbles rather
  than only 0–9. The destination remains unchanged for CMP4S.

Run the focused regression and reference tests:

```sh
ctest --test-dir build -R mainv20_extended_arithmetic --output-on-failure
python3 tests/run_v20_singlestep.py --opcode 0F18 --opcode 0F19 --opcode 0F20 --opcode 0F22 --opcode 0F26 --opcode 0F31 --opcode 0F33 --opcode 0F39 --opcode 0F3B --opcode D5 --limit 100
```

## Phase 3: V20 immediate shifts (C0/C1)

On the NEC V20, `C0 /0..7 ib` and `C1 /0..7 ib` are the
immediate-count forms of the 8-bit and 16-bit Group 2 rotate/shift
instructions. The byte following ModR/M (and any displacement) is the
shift count; a count of zero leaves the destination and FLAGS intact.
The implementation shares the proven D0-D3 shift/rotate logic.

On the original Intel 8088 these same opcodes are undocumented RET
aliases: C0 means RET near with imm16 stack adjustment, and C1 is RET
near. Those behaviors remain unchanged.

The `mainv20_immediate_shift` CTest exercises register/memory
forms, all defined operation subgroups, and zero, one, and multi-bit
counts. The hardware dataset intentionally skips undocumented /6.

```sh
ctest --test-dir build -R 'mainv20_immediate_shift|main8088_undocumented_ret' --output-on-failure
python3 tests/run_v20_singlestep.py --opcode C0 --opcode C1 --limit 100
```

## Phase 4: carry-conditional repeat prefixes and REP IDIV

V20 has two additional repeat prefixes: `64h` (REPNC, repeat while
carry clear) and `65h` (REPC, repeat while carry set). They are
accepted with all supported string operations, not just CMPS/SCAS:
MOVS, STOS, LODS, INS and OUTS also qualify. Repeat processing
executes one element per `executeOne()`; CX decrements and IP returns
to the prefix while the carry condition and CX permit another iteration.
The ordinary 8088 prefix handling is unchanged.

On both Intel 8088 and NEC V20, an F2h/F3h-prefixed F6/F7 signed
division negates the resulting quotient without repetition. The V20
division-overflow bounds still differ from the original 8088, which
rejects the minimum negative signed quotient.

```sh
ctest --test-dir build -R 'mainv20_rep_prefixes|main8088_divide_quirks' --output-on-failure
python3 tests/run_v20_singlestep.py --opcode 6C --opcode 6D --opcode 6E --opcode 6F --opcode A4 --opcode A5 --opcode AA --opcode AB --opcode AC --opcode AD --opcode F6.7 --opcode F7.7 --limit 100
```
