# Post-preview checkpoint 14: imported managed arrays — 2026-08-19

Gate: I2 typed core/MIR, I3 bytecode VM, I4 C17 equivalence, and I5 managed
data. Checkpoint status: locally complete. I2, I3, I4, and I5 remain
`in-progress`; the complete Tier 1 matrix is not claimed by this local record.

Authority: D002, D013, D043, D045, the normative package/type rules, typed MIR,
and the generic bytecode array-type table. This is a conformance repair, not a
new source-language feature. D045 and the execution specification now state
the already implemented built-in-string table convention explicitly:
`Array<string>` uses the canonical string descriptor with `NO_LAYOUT`, while
array and nominal reference elements carry their exact layout-table index.

## Delivered slice

- Public signatures, concrete public aliases, and forwarding wrappers preserve
  the same fully qualified instantiated `Array<T>` type across package edges.
  There is no package conversion. The checker publishes generic nominal arity,
  interns imported concrete arguments, substitutes imported record fields, and
  materializes the transitive array/nominal layout closure even when a function
  only forwards a value.
- MIR lowering transports the exact element type and managed layout. The
  bytecode table accepts scalar, string, nested-array, record, enum, and
  interface descriptor forms; whole-program call verification recursively
  compares array rows across function-local tables.
- The codec reserves array-table storage transactionally, round-trips exact
  rows, and reports deterministic allocation failure. Verification rejects a
  missing or out-of-range managed element layout and a nested row that does not
  reference an earlier row.
- The VM owns a dedicated array-descriptor cache. It composes array descriptors
  with canonical nominal/string descriptors, preserves recursive descriptor
  graphs, roots temporary reference cells during construction, and converts
  bytecode strings to and from managed string cells.
- Generated C emits exact scalar/string/array/record/enum/interface element
  descriptors with nominal forward declarations. VM and native retain the same
  alias-visible mutations, output, failure exit, and G4002 text.

## Scope boundaries

- `public import` is not Ruda syntax. Re-export coverage uses a concrete public
  alias plus ordinary public wrappers.
- Imported generic enum constructors, patterns, returned `Option`/`Result`
  values, and postfix `?` remain checkpoint 15. CP14 transports a non-generic
  enum array and an imported generic record element without claiming that
  constructor/result surface.
- Compact array spelling, new Array APIs, mutation-during-iteration policy, and
  optimization are unchanged.

## Focused coverage

The source corpus is `tests/post-preview/cp14-imported-arrays`. It covers
scalars, strings, records, a non-generic enum, `Box<int>`, nested arrays,
named/namespace/aliased imports, public alias/wrapper relay, same short names
from different packages, package-private functions, length/index/push/for-of,
and mutation observed through aliases. Separate corpora own cross-package
private rejection and bounds failure.

Focused C owners provide:

- checker metadata/TypeId/generic substitution and fail-every-allocation;
- forwarding-only MIR closure, checked-index ordering, exact MIR-oracle
  execution, and fail-every-allocation teardown;
- MIR→bytecode lowering, encode→decode→verify→execute, corrupt row/envelope
  rejection, VM collect-before-allocation, and lower/codec/VM allocation
  sweeps;
- generated-C descriptor assertions, native execution, and emitter allocation
  sweeps;
- real CLI fmt/check/run/test-both/build/artifact, R0011, and VM/native G4002.

Every deterministic allocation sweep advances one failure index at a time
until the first complete success. Every attempted path tears down fully and
asserts both `live_allocations == 0` and `live_bytes == 0`.

## Exact observations

| Surface | Exit | Observation |
|---|---:|---|
| `ruda run` | 0 | stdout `cp14 imported arrays ok\n`; empty stderr |
| `ruda test --engine both` | 0 | `6 passed; 0 failed`, including producer-private test |
| built native artifact | 0 | stdout `cp14 imported arrays ok\n`; empty stderr |
| private cross-package check | 1 | `R0011` on `privateAdjust` |
| bounds VM | 1 | `panic[G4002]: array index 2 is outside length 2\n` |
| bounds native artifact | 1 | identical G4002 text and empty stdout |

The D045 differential owner runs in the same C-unit suite and also compares the
VM/native panic symbol and exact source span (`805..806`) for G4002. The CP14
package corpus adds the cross-package path, exact public diagnostic text, exit,
and empty-stdout assertions without changing that span contract.

The bytecode owner also mutates a record-element row to `NO_LAYOUT` and an
out-of-range layout, then flips a byte in the encoded integrity envelope. The
verifier/decoder reject every mutation before execution. The successful
decoded program returns exact `i64` value `51` in the VM; generated C returns
the same value.

## Local validation

Environment: macOS Darwin 25.3.0 arm64; Apple Clang 17.0.0; CMake 4.4.2.

| Command | Exit | Result |
|---|---:|---|
| `build/debug/ruda fmt tests/post-preview/cp14-imported-arrays --check` | 0 | corpus canonical |
| `build/debug/ruda check tests/post-preview/cp14-imported-arrays` | 0 | complete package graph accepted |
| `build/debug/ruda run tests/post-preview/cp14-imported-arrays` | 0 | exact success stdout |
| `build/debug/ruda test tests/post-preview/cp14-imported-arrays --engine both` | 0 | 6/6 VM/native tests |
| `build/debug/ruda build tests/post-preview/cp14-imported-arrays --output build/debug/cp14-imported-arrays` | 0 | native artifact created |
| `build/debug/cp14-imported-arrays` | 0 | exact success stdout |
| `cmake --build --preset debug --parallel` | 0 | final Debug tree built |
| `ctest --preset debug --output-on-failure` | 0 | 2/2 passed in 173.15 seconds; C-unit 171.34 seconds |
| `cmake --build --preset release --parallel` | 0 | final Release tree built |
| `ctest --preset release --output-on-failure` | 0 | 2/2 passed in 198.39 seconds; C-unit 196.16 seconds |
| `cmake --build --preset asan-ubsan --parallel` | 0 | final ASan/UBSan tree built |
| `ctest --preset asan-ubsan --output-on-failure` | 0 | 2/2 passed in 290.67 seconds; C-unit 287.89 seconds |
| `./tools/check-architecture` | 0 | owner/dependency/line-budget policy clean |
| `./tools/check-c-memory-policy` | 0 | central allocator policy clean |
| `./tools/check-generated` | 0 | generated artifacts current |
| `./tools/check-doc-links` | 0 | evidence/spec/roadmap links valid |
| `./tools/check-spec-terms` | 0 | normative terminology clean |
| `./tools/check-syntax-coverage` | 0 | all 75 syntax kinds and 115 grammar productions covered |
| `git diff --check` | 0 | no whitespace errors |

This local debug/release/sanitizer table closes the checkpoint implementation
and its required local validation tail. Cross-platform Tier 1 CI remains the
governing gate for changing I2–I5 from `in-progress`.
