# Intel 8088 hardware-generated single-step conformance tests

The executable `fm11-8088-singlestep` uses the actual
`Main8088Board::Core::executeOne()` implementation. It is driven by the
hardware reference data from [SingleStepTests/8088](https://github.com/SingleStepTests/8088)
(V2, MIT license). Test data are **not** copied into this repository.

## Set up once

From the `fm11-emulator` repository root:

```sh
git pull
cmake -S . -B build
cmake --build build -j"$(nproc)"
ctest --test-dir build -R main8088_singlestep_smoke --output-on-failure

git clone --filter=blob:none --sparse https://github.com/SingleStepTests/8088 external/8088-tests
git -C external/8088-tests sparse-checkout set v2
```

Existing local data can be passed via `--data-dir`; the sparse clone is
optional if you already have a copy of the upstream V2 folder.

## Run

Test 100 cases from ADD opcode `00` (default):

```sh
python3 tests/run_8088_singlestep.py --opcode 00
```

Test all V2 files, up to 100 cases per opcode/subopcode file:

```sh
python3 tests/run_8088_singlestep.py --all --limit 100
```

Test all cases from selected opcodes, including group subcodes:

```sh
python3 tests/run_8088_singlestep.py --opcode 80 --opcode FF.6 --limit 0
```

`--opcode 80` includes `80.0` through `80.7`, wherever the corresponding
files exist; `--all` includes every `*.json.gz` file. `--limit 0`
performs all cases. `--fail-fast` stops after the first failing file.

Output includes per-file `PASS`, `FAIL`, or `SKIP`, followed by
`TOTAL=...`, `PASS=...`, `FAIL=...`, `UNSUPPORTED=...` for executed
files. An unsupported instruction is reported separately from a
completed instruction that returned incorrect state. Exit status is
nonzero on any failure/unsupported result.

## Comparison model and limitations

* Initialize all eight 16-bit general registers, ES/CS/SS/DS, IP, FLAGS,
  and all RAM entries in the upstream `initial` state. Memory is reset to
  flat 1 MiB RAM for every test (including FF000h-FFFFFh, normally ROM).
* Compare all final registers (the V2 `final.regs` object holds **only
  changed** registers), defined FLAGS bits, and all memory entries listed
  under `final.ram`.
* Use `metadata.json` for opcode/subopcode status and undefined flag
  masks. The native comparison applies a further architecturally relevant
  FLAG mask; reserved status bits are not treated as failures.
* For `F6.6`/`F6.7` and `F7.6`/`F7.7`, a divide error
  produces a type-0 interrupt with an ordinary three-word stack frame.
  The 8088's saved IP points *after* the faulting instruction. The
  arithmetic status bits of FLAGS are undefined after DIV/IDIV, including
  in the copy saved on the exception stack. The comparison therefore
  masks only the two bytes of the saved FLAGS word when an INT0 frame
  was actually pushed (SP decreased by six), using the same metadata
  flag mask; it still checks saved IP/CS and other RAM exactly.
* REP/REPNE preceding IDIV is a documented-by-testing 8088 quirk:
  it negates the quotient without changing the remainder. The 8088
  also faults at the most negative signed quotient (-128/-32768),
  unlike later x86 CPUs. These are exercised by
  `main8088_divide_quirks` and `main8088_singlestep_divide_stack`.
* For `D2h`/`D3h` with `CL > 1`, **OF is undefined** on the 8088.
  The driver removes bit `0800h` from the comparison mask for those cases
  individually; it still compares OF for `CL = 0` and `CL = 1`.
  This avoids approximately half the cases being reported as false
  failures when the emulator preserves OF and the measured CPU leaves
  an arbitrary value. This is documented in the upstream test README.
* REP string instructions can be executed in multiple calls to
  `executeOne()`, since this emulator runs one iteration per call.
* Bus cycle timing and prefetch queue state are **not** compared.
  The emulator does not implement the real 8088 instruction prefetch queue.
  Tests that rely on self-modifying prefetched code may therefore fail
  despite otherwise correct instruction semantics.
* Unexpected RAM writes outside the expected changed-memory addresses
  are not currently detected. The listed changed addresses are compared.
* Instructions that rely on interrupt or external I/O environment may
  require a dedicated harness; this runner targets single-instruction
  CPU semantics, not the machine's peripheral behavior.
* V2 is a reference for **8088**, not NEC V20; use the existing V20
  regressions for NEC-specific instructions.

The offline `main8088_singlestep_smoke` test verifies only that the
protocol and one MOV-immediate CPU case work. It does not claim any
upstream V2 conformance results. Run against the full upstream dataset
to discover missing opcodes and behavioral mismatches.
