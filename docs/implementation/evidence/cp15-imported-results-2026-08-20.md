# Post-preview checkpoint 15: imported generic enums and results — 2026-08-20

Gate: I2 typed core/MIR, I3 bytecode VM, I4 C17 equivalence, and I5 managed
data. Checkpoint status: locally complete. I2, I3, I4, and I5 remain
`in-progress`; the complete Tier 1 matrix is not claimed by this local record.

Authority: D004, D005, D006, D013, the normative package/import, generic-enum,
match, `Option`/`Result`, postfix `?`, typed MIR, and bytecode verification
contracts. This is a conformance repair, not a source-language extension. The
existing specifications already require canonical nominal identity and
cross-package generic arguments, so no decision or normative syntax change was
needed.

## Delivered slice

- Selective and namespace imports now resolve ordinary public generic enum
  types for unit, positional, and named construction. Expected concrete types
  retain the declaration symbol and exact generic arguments. Concrete public
  aliases expose another spelling of the same type for construction and
  pattern matching; no wrapper enum or conversion is introduced.
- The checker materializes a bounded, cached declaration view for an imported
  enum specialization. The view borrows immutable published names and owns
  only its type-parameter, variant, and payload vectors. Teardown therefore
  neither leaks the adaptation nor frees package metadata twice.
- Imported payload types are substituted against the exact concrete nominal
  arguments. Returned enum values, nested `Result<Option<Array<int>>, E>`,
  `mapError`, and both the success and error paths of postfix `?` flow through
  package calls without local compatibility enums.
- Qualified patterns validate that their namespace actually exports the enum
  being matched. Selective, namespace, and re-export-alias patterns share exact
  variant lookup, payload binding, nested patterns, and exhaustiveness. Missing
  witnesses render published imported names, including `Value(_)`.
- Existing typed MIR enum layouts, bytecode nominal tables/provenance, VM enum
  operations, and native C17 descriptors already preserved the required
  canonical identity once frontend metadata was supplied. No backend repair or
  duplicate source semantics were added.
- The formatter exposed a parser boundary bug by canonically joining nested
  generic closers as `>>`. Type-list parsing now preserves a pending outer `>`
  before considering the following comma. A focused parser owner and formatter
  fixture cover `Result<Choice<Array<int>>, Problem>` and sibling arguments.

## Scope boundaries

- Generic specialization strategy, enum syntax, prelude ownership, and
  `Option`/`Result` semantics are unchanged.
- `public import` is not Ruda syntax. Re-export coverage uses a concrete public
  alias and an ordinary public forwarding function.
- Checkpoint 20 remains responsible for removing algorithm-corpus
  `IntOption`/`SearchResult` workarounds after its own fidelity gate.

## Focused coverage

The source corpus is `tests/post-preview/cp15-imported-results`. It covers
explicit and inferred `Some`/`None`/`Ok`/`Err`, selective and namespace
ordinary enum construction, imported returns, nested
`Result<Option<Array<int>>, Problem>`, payload patterns, exhaustive matching,
`mapError`, success/error postfix `?`, a concrete public alias relay, VM run,
namespace-vs-local short-name shadowing, both-engine tests, and native
build/run. Separate corpora own package-private
import rejection and imported non-exhaustive witness diagnostics.

Focused C owners provide:

- checker metadata, exact payload substitution, public/private, wrong arity,
  wrong payload type, import ambiguity, wrong pattern namespace,
  non-exhaustiveness, and fail-every-allocation checking;
- canonical `Choice<Array<int>>` TypeId identity in producer, consumer, and
  relay MIR, verified construction/match instructions, exact MIR-oracle
  execution, and interpreter allocation exhaustion;
- MIR-to-bytecode lowering, nominal-provenance mutation rejection,
  encode/decode/verify/VM round trip, corrupt-envelope rejection, and
  lower/encode/decode/VM allocation exhaustion;
- generated-C enum/array descriptor assertions, exact native execution,
  deterministic native allocation traps, and emitter allocation exhaustion;
- real CLI fmt/check/run/test-both/build/artifact plus exact `R0011` and
  `T0013` negative observations;
- parser losslessness and formatter parse/format/parse idempotence for a
  nested generic closer immediately followed by an outer comma.

Every deterministic allocation sweep advances one failure index at a time
until the first complete success. Every attempted path tears down fully and
asserts both `live_allocations == 0` and `live_bytes == 0`.

## Exact observations

| Surface | Exit | Observation |
|---|---:|---|
| `ruda run` | 0 | stdout `cp15 imported results ok\n`; empty stderr |
| `ruda test --engine both` | 0 | `5 passed; 0 failed`, including propagated imported error and namespace shadowing |
| built native artifact | 0 | stdout `cp15 imported results ok\n`; empty stderr |
| private cross-package check | 1 | `R0011` on `Hidden` |
| non-exhaustive imported match | 1 | `T0013` with witness `Value(_)` |

The focused MIR oracle, decoded bytecode VM, and generated C17 program all
produce exact `i64` value `119`; the native harness observes
`ok:i64:119\n`. Native allocation failures at indices 0 and 1 produce exact
`trap:5:0:0\n` and exit 1. The bytecode owner mutates an imported enum symbol
and receives `SIGNATURE_MISMATCH`, then restores, encodes, decodes, verifies,
and executes the program. A final encoded-byte mutation is rejected before
execution.

## Local validation

Environment: macOS Darwin 25.3.0 arm64; Apple Clang 17.0.0; CMake 4.4.2.

| Command | Exit | Result |
|---|---:|---|
| `build/debug/ruda fmt tests/post-preview/cp15-imported-results --check` | 0 | corpus canonical |
| `build/debug/ruda check tests/post-preview/cp15-imported-results` | 0 | complete package graph accepted |
| `build/debug/ruda run tests/post-preview/cp15-imported-results` | 0 | exact success stdout |
| `build/debug/ruda test tests/post-preview/cp15-imported-results --engine both` | 0 | 5/5 VM/native tests |
| `build/debug/ruda build tests/post-preview/cp15-imported-results --output build/debug/cp15-imported-results` | 0 | native artifact created |
| `build/debug/cp15-imported-results` | 0 | exact success stdout |
| `cmake --build --preset debug --parallel` | 0 | final Debug tree built |
| `ctest --preset debug --output-on-failure` | 0 | 2/2 passed in 204.26 seconds; C-unit 202.10 seconds |
| `cmake --build --preset release --parallel` | 0 | final Release tree built |
| `ctest --preset release --output-on-failure` | 0 | 2/2 passed in 1078.41 seconds; C-unit 1076.42 seconds |
| `cmake --build --preset asan-ubsan --parallel` | 0 | final ASan/UBSan tree built |
| `ctest --preset asan-ubsan --output-on-failure` | 0 | 2/2 passed in 300.24 seconds; C-unit 297.35 seconds |

| `./tools/check-architecture` | 0 | owner/dependency/line-budget policy clean |
| `./tools/check-c-memory-policy` | 0 | central allocator policy clean |
| `./tools/check-generated` | 0 | generated bytecode/diagnostic/Unicode/syntax artifacts current |
| `./tools/check-doc-links` | 0 | evidence/spec/roadmap links valid |
| `./tools/check-spec-terms` | 0 | normative terminology clean |
| `./tools/check-syntax-coverage` | 0 | all 75 syntax kinds and 115 grammar productions covered |
| `git diff --check` | 0 | no whitespace errors |

This local debug/release/sanitizer table closes the checkpoint implementation
and its required local validation tail. Cross-platform Tier 1 CI remains the
governing gate for changing I2–I5 from `in-progress`.
