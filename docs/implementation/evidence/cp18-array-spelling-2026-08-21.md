# Post-preview checkpoint 18: canonical managed-array spelling — 2026-08-21

Gate: I1 frontend/formatter, I2 typed core/compiler service, I3 bytecode VM,
I4 C17 equivalence, and I5 managed data. Checkpoint status: locally complete.
The complete Tier 1 matrix remains governing evidence; I1–I5 do not change
from `in-progress` in this local record.

Baseline commit: `d9c8f91` (`Complete checkpoint 17 array mutation`).

Authority: new accepted D054, amending the source spelling of the existing
D045/D053 managed-array identity. Normative language, standard-library,
diagnostic, toolchain, execution, requirements, architecture, test-strategy,
generated grammar/diagnostic registries, examples, and roadmap text are
updated in the same semantic change.

## Delivered slice

- Type grammar accepts right-associative `[]T`, including nested, qualified,
  generic, tuple, and function element syntax. Deterministic recovery retains
  following declarations after incomplete `[]`, `[][]`, `[][`, and generic
  element input. Expression `[]` remains the contextual empty literal.
- The checker maps the prefix AST directly to the existing canonical array
  descriptor. Aliases, imports, nested identity, contextual empty literals,
  and `Slice<T>` distinction retain ordinary exact typing. No new AST semantic
  identity, MIR type, bytecode row, runtime tag, layout, tracing rule, or ABI
  was added.
- The prelude `Array<T>` source spelling is removed before 1.0. An unresolved
  old form produces exact `T0018` over the complete type with message
  `prelude managed arrays use '[]T'; replace 'Array<T>' with '[]T'`. A local or
  imported package-owned nominal declaration named `Array` remains ordinary
  generic syntax and is accepted by the public corpus.
- Formatting emits adjacent prefixes (`[]int`, `[][]int`) while preserving
  comments and remains byte-identical on a second pass. Current repository
  source and examples use the new spelling except the intentional nominal
  `Array` coexistence and migration-error fixtures.
- The compiler service exposes bounded caller-owned canonical checked-symbol
  type spelling for hover, signature, and completion-detail consumers. Nested
  arrays and arrays inside generic arguments use `[]T`; stale results, byte/
  depth limits, and every output-allocation failure are explicit.

## Ownership and coverage

`checker_array_types.c`, `formatter_types.c`, and `type_spelling.c` are focused
owners. Parser and formatter coordinators only dispatch/sequence their narrow
contracts; lowering and backends receive the same type identity as before.

Focused C coverage includes:

- scalar/nested/qualified/generic/tuple/function parser forms, lossless trees,
  following-declaration recovery, and repeat-parse diagnostic identity;
- canonical formatter bytes, comments, semantic AST equality through the
  shared fixture owner, and second-pass idempotence;
- array identity/aliases/contextual empty values, local and imported nominal
  `Array`, exact migration code/message/span, public signature traversal, and
  deterministic checker allocation teardown with zero live allocations and
  bytes;
- compiler-service signature text
  `([][]int, Option<[]int>) => [][]int`, generation rejection, limits, and all
  reachable output-allocation failures with balanced teardown;
- public CLI `fmt --check`, `check`, VM `run`, `test --engine both`, native
  `build` and artifact execution, plus identical VM/native G4002 output and
  exit status. Existing MIR tests retain the exact G4002 source span.

The public `cp18-array-spelling` graph also carries qualified/imported array
aliases, `Option<[][]int>`, nested alias mutation, contextual `[]`, and an
imported nominal `Array<int>`. VM and native observe the same managed identity,
mutation, stdout, and exit without a backend change.

## Exact observations

| Surface | Exit | Observation |
|---|---:|---|
| `ruda run` success corpus | 0 | stdout `cp18 array spelling ok\n`; empty stderr |
| `ruda test --engine both` | 0 | `2 passed; 0 failed`; matching VM/native observations |
| built native success artifact | 0 | stdout `cp18 array spelling ok\n`; empty stderr |
| VM nested-bounds corpus | 1 | exact `panic[G4002]: array index 2 is outside length 1\n` |
| native nested-bounds artifact | 1 | identical exact G4002 line and exit |
| compiler-service type query | 0 | exact `([][]int, Option<[]int>) => [][]int` |

## Local validation

Environment: macOS Darwin 25.3.0 arm64; Apple Clang 17.0.0; CMake 4.4.2.

| Command | Exit | Result |
|---|---:|---|
| `build/debug/ruda fmt tests/post-preview/cp18-array-spelling --check` | 0 | complete corpus canonical |
| `build/debug/ruda check tests/post-preview/cp18-array-spelling` | 0 | complete package graph accepted |
| `build/debug/ruda run tests/post-preview/cp18-array-spelling` | 0 | exact success stdout |
| `build/debug/ruda test tests/post-preview/cp18-array-spelling --engine both` | 0 | 2/2 VM/native tests |
| `build/debug/ruda build tests/post-preview/cp18-array-spelling --output build/debug/cp18-array-spelling` | 0 | native artifact created |
| `build/debug/cp18-array-spelling` | 0 | exact success stdout |
| VM/native bounds corpus and artifact | 1 | identical exact G4002 line |
| `build/debug/ruda fmt tests/format/types.expected.ruda --check` | 0 | canonical focused formatter fixture |
| `ctest --preset debug -R '^ruda_c_unit$' --output-on-failure` | 0 | C-unit result recorded in final validation |
| `cmake --build --preset debug --parallel 4` | 0 | final Debug tree built |
| `ctest --preset debug --output-on-failure` | 0 | final Debug suite recorded after completion |
| `cmake --build --preset release --parallel 4` | 0 | final Release tree built |
| `ctest --preset release --output-on-failure` | 0 | final Release suite recorded after completion |
| `cmake --build --preset asan-ubsan --parallel 4` | 0 | final ASan/UBSan tree built |
| `ctest --preset asan-ubsan --output-on-failure` | 0 | final sanitizer suite recorded after completion |
| `./tools/check-architecture` | 0 | owner/dependency/line-budget policy clean |
| `./tools/check-c-memory-policy` | 0 | central allocator policy clean |
| `./tools/check-generated` | 0 | generated diagnostic/syntax and other registries current |
| `./tools/check-doc-links` | 0 | evidence/spec/roadmap links valid |
| `./tools/check-spec-terms` | 0 | D001–D054 and requirement terminology consistent |
| `./tools/check-syntax-coverage` | 0 | all 75 syntax kinds and 116 grammar productions covered |
| `./tools/check-example-syntax` | 0 | all user-facing Ruda examples accepted |
| `git diff --check` | 0 | no whitespace errors |

The broad command `ruda fmt tests/format --check` is not a valid green gate for
this repository because that directory intentionally contains noncanonical
`*.input.ruda` formatter inputs. The registered formatter fixture test compares
each such input to its canonical `*.expected.ruda`, checks semantic AST and
comment preservation, and verifies a byte-identical second formatting pass;
the focused canonical file command above is the direct CLI complement.

This source-spelling checkpoint does not claim new managed-array runtime or
backend semantics. Cross-platform Tier 1 CI remains required before changing
any roadmap gate to `verified`.
