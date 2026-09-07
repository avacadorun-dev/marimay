# CP42: rune equality coherence — 2026-09-07

Scope: the rune-Eq defect recorded by the collection optimization investigation,
on parent `267d81a`. This is a correctness repair under D080, not evidence that
the remaining Map/Set/rune-frequency performance obligation or CP42 is complete.

## Reproduction and repair

Before the repair, the expanded CP36 source command
`build/release/mmay test tests/public-alpha/cp36-map-set --engine vm` exited 1
with T0003 at `expect.equal(values[index], values[index])` and at
`expect.equal(Some(values[2]), Some(values[8]))`. `rune` was absent from the
scalar/structural equality eligibility checks despite being an allowed D079
Map/Set key. Ordinary `mmay check` excludes `_test.my` sources and is not
evidence for this reproduction.

The scalar checker now emits the existing equality MIR operation for exact
rune operands. The structural checker uses that operation for rune fields and
enum payloads. MIR and wire verification admit rune only for equality, retaining
the exact operand types and bool result; integer arithmetic and ordering are
not enabled. The MIR oracle and VM compare the full scalar bits explicitly,
without falling back to integer-kind inference. Native C already emits the
typed comparison. Wire opcode 11 keeps its ID and operand encoding; the source
contract, D080 clarification, execution contract and generated opcode reference
document its admitted operand types.

Direct rune equality has one instruction and one block, checked by a structural
MIR assertion. It adds no managed allocation, normalization or source conversion.
No benchmark speedup is claimed for this repair. The unchanged algorithm
benchmark workloads have not been rewritten to avoid their collection costs.

## Focused coverage

- `test_checker_equality.c`: 81 ordered scalar pairs for each of `==`, `!=`,
  and composed Option/derived-record equality, through the MIR oracle and
  encoded/decoded bytecode VM. Cases include NUL, ASCII, lambda, both surrogate
  boundaries, U+FFFF, U+10041, an emoji and U+10FFFF; U+0041/U+10041 catch
  accidental 16-bit truncation.
- Hostile MIR/wire edits reject rune ordered comparisons and mixed rune/u32
  equality. Source diagnostics reject mixed rune/int/string/bool operands,
  integer/rune equality, rune ordering and arithmetic at the exact operator.
- Deterministic checker failure sweeps visit every allocation index through
  first complete success for both the valid scalar/structural source and an
  unsupported derived-field diagnostic. Every teardown checks both live
  allocation and byte counts are zero.
- `rune_equality_test.my`: two new CP36 tests, bringing the corpus to 12.
  They cover full scalar comparisons, left-to-right exactly-once evaluation
  for operators/expectations, Option and Result payloads, imported derived
  rune keys, duplicate Set insertion and Map get/remove rune values.
- `examples/collections.my` demonstrates ordinary array search using rune
  equality. No new syntax is introduced; the existing formatter formats and
  checks the added source corpus and example.

## Local verification

On macOS arm64 / AppleClang 21:

- Release checker suite: passed.
- Complete Release CTest suite: 68/68 passed, including all eight CP31
  allocation shards and native/differential owners (259.93 seconds; this is
  validation runtime, not a performance benchmark).
- Debug checker, bytecode and full CP36 source acceptance: 3/3 passed.
- ASan+UBSan checker, bytecode and full CP36 source acceptance: 3/3 passed.
- Direct `mmay test ... --engine both`: 12 passed, 0 failed.
- Architecture, C-memory policy, roadmap, document links, generated artifacts,
  specification terms, example syntax, syntax coverage and release-contract
  gates: passed. Release `mmay doctor`, formatting and `git diff --check` passed.

The separate older `build/asan` configuration passed checker and bytecode,
but its CP36 native test preflight failed B0002. Its generated configuration
still expects AppleClang 17.0.0.17000604; `/usr/bin/cc --version` reports
AppleClang 21.0.0.21000101. `mmay doctor` confirms that mismatch. The current
`build/asan-ubsan` configuration expects Clang 21 and passes the same source
acceptance. This old-build failure is retained, not counted as a green run.

Independent Linux validation uses the existing Ubuntu 24.04 amd64 / Clang
18.1.3 diagnostic image, with ASan, UBSan and `detect_leaks=1`, network disabled
and the language checkout read-only. Checker and bytecode suites: 2/2 passed.
The exact current CP36 source was copied without tool-owned caches to a fresh
writable diagnostic fixture. Its unmodified CMake acceptance driver passed
format, check, VM run, all 12 both-engine tests, Release native build and native
execution under the same sanitizer environment.
This local emulated environment is not the original failing Gitea runner and
does not close its unresolved CI failures or supply a complete Tier 1 matrix.

Reproduction commands:

```sh
cmake --build build/release --parallel 2
ctest --test-dir build/release --output-on-failure --parallel 2
ctest --test-dir build/debug -R '^(mmay_c_unit_checker|mmay_c_unit_bytecode|mmay_cp36_map_set)$' --output-on-failure
ctest --test-dir build/asan-ubsan -R '^(mmay_c_unit_checker|mmay_c_unit_bytecode|mmay_cp36_map_set)$' --output-on-failure
build/release/mmay test tests/public-alpha/cp36-map-set --engine both
build/release/mmay fmt examples/collections.my --check
build/release/mmay check examples/collections.my
```

## Remaining obligations

The collection hot-path optimization, tuple-binding parser recovery, remaining
CP42 ergonomics and ten-utility portfolio stay open. Gitea SSH at
ci.example:2222 still timed out during a bounded remote-ref check; no remote
publication is claimed for this working change or parent `267d81a`.
