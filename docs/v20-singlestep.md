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
