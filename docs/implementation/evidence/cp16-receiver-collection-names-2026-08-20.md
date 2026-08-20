# Post-preview checkpoint 16: receiver-directed collection names — 2026-08-20

Gate: I2 typed core/MIR, I3 bytecode VM, I4 C17 equivalence, and I5 managed
data. Checkpoint status: locally complete. I2, I3, I4, and I5 remain
`in-progress`; the complete Tier 1 matrix is not claimed by this local record.

Baseline commit: `9b7fc51` (`Complete checkpoint 15 imported generic enums`).

Authority: accepted D003 and D043, D045 managed-array semantics, and the
normative first-parameter dot-call, package-import, compiler-service fact, MIR,
and engine-equivalence contracts. This is a conformance repair and normative
clarification, not general overloads, extension methods, a new `Array.pop`, or
a new source construct. No design decision changed.

## Delivered slice

- Receiver-bearing `push` and `length` calls now collect candidates by the
  receiver's exact static type. An exact package-owned nominal receiver selects
  its ordinary D003/D043 function; an exact `Array<T>` receiver selects the
  compiler-known D045 operation. The prelude no longer wins or loses merely
  because a same-named function exists.
- Free and dot forms reuse the same ordinary resolver and lower to one symbol.
  A precompiled receiver value is handed to ordinary or array lowering, so the
  receiver expression is evaluated once and explicit arguments retain source
  order.
- Namespace-qualified calls bypass compiler-known member recognition and stay
  with the qualified import resolver. Named type-only, named function aliases,
  and namespace imports expose the same public owned dot-call set; only the
  available free spelling differs.
- Receiver-aware imported lookup preserves a package-private same-name result
  when no public exact or wrong-nominal candidate exists. Diagnostics therefore
  distinguish hidden, wrong nominal, unknown/approximate, and missing first
  argument cases.
- The existing resolver remains the source for occurrence identity and
  completion publication. Compiler-service completion and
  definition-from-reference queries do not reconstruct a second candidate set.
- Typed MIR remains the backend convergence point. Nominal calls emit ordinary
  call symbols; `Array<T>` operations emit array instructions. VM and native
  backends consume that selection without repairing or duplicating source
  semantics.

## Ownership and scope boundaries

`bootstrap/src/check/checker_receiver_calls.c` is the focused collision owner.
It parses only receiver-bearing `push`/`length`, compiles one receiver, consults
the shared resolver, and dispatches to the existing ordinary-call or array
owner. `checker_expr_calls.c` remains the call coordinator, `resolution.c`
remains the D003/D043 authority, and `array.c` owns only selected D045 lowering.

Checkpoint 17 remains responsible for any later `Array.pop` surface.
Checkpoint 20 remains responsible for removing algorithm-corpus workaround
names after its own fidelity gate.

## Focused coverage

The source corpus is `tests/post-preview/cp16-receiver-names`. Its package-owned
`Stack`, `Queue`, and `Heap` expose ordinary `push`, `pop`, `length`, and
`clear`; their implementations deliberately call compiler-known array
`push`/`length` with the colliding names. Consumers cover same-package behavior,
named type-only imports, aliased named function imports, namespace-qualified
free calls, direct dot calls, and a structural interface requiring the full
collection signature set.

Focused C owners provide:

- same-name nominal/array behavior, free/dot symbol identity, once-only receiver
  evaluation, ordinary-call versus array-instruction MIR assertions, exact MIR
  interpretation, bytecode-program lowering, and native C17 emission;
- wrong-nominal, ambiguous/missing-first-argument, approximate spelling,
  package-private import, and imported wrong-receiver diagnostics;
- compiler-service completion sets and free spellings for type-only, aliased
  named, and namespace imports, plus dot/free/qualified occurrence-to-definition
  identity through both result-scoped and generation-safe service APIs;
- public CLI `fmt --check`, `check`, `run`, `test --engine both`, `build`, and
  native-artifact execution;
- deterministic checker and compiler-service allocation sweeps through the
  first success. Every attempted path tears down fully and asserts both
  `live_allocations == 0` and `live_bytes == 0`.

The user-facing `examples/receiver_collection_names.ruda` independently shows
one `Stack` and one `Array<int>` using identical `push`/`length` spellings in
both free and dot form.

## Exact observations

| Surface | Exit | Observation |
|---|---:|---|
| `ruda run` | 0 | stdout `cp16 receiver names ok\n`; empty stderr |
| `ruda test --engine both` | 0 | `2 passed; 0 failed`; matching VM/native observations |
| built native artifact | 0 | stdout `cp16 receiver names ok\n`; empty stderr |
| wrong nominal receiver C fixture | 0 | exact `T0002` primary receiver span |
| hidden imported receiver function C fixture | 0 | exact `R0011` result |
| approximate collection name C fixture | 0 | exact `R0001` name span |

The focused MIR oracle evaluates to exact `i64` value `11190`. The same
function contains two `RUDA_MIR_ARRAY_PUSH` and two
`RUDA_MIR_ARRAY_LENGTH` instructions while its three Stack push spellings all
reference the one Stack-owned call symbol.

## Local validation

Environment: macOS Darwin 25.3.0 arm64; Apple Clang 17.0.0; CMake 4.4.2.

| Command | Exit | Result |
|---|---:|---|
| `build/debug/ruda fmt tests/post-preview/cp16-receiver-names --check` | 0 | corpus canonical |
| `build/debug/ruda check tests/post-preview/cp16-receiver-names` | 0 | complete package graph accepted |
| `build/debug/ruda run tests/post-preview/cp16-receiver-names` | 0 | exact success stdout |
| `build/debug/ruda test tests/post-preview/cp16-receiver-names --engine both` | 0 | 2/2 VM/native tests |
| `build/debug/ruda build tests/post-preview/cp16-receiver-names --output build/debug/cp16-receiver-names` | 0 | native artifact created |
| `build/debug/cp16-receiver-names` | 0 | exact success stdout |
| `cmake --build --preset debug --parallel` | 0 | final Debug tree built |
| `ctest --preset debug --output-on-failure` | 0 | 2/2 passed in 208.14 seconds; C-unit 206.21 seconds |
| `cmake --build --preset release --parallel` | 0 | final Release tree built |
| `ctest --preset release --output-on-failure` | 0 | 2/2 passed in 206.37 seconds; C-unit 204.56 seconds |
| `cmake --build --preset asan-ubsan --parallel` | 0 | final ASan/UBSan tree built |
| `ctest --preset asan-ubsan --output-on-failure` | 0 | 2/2 passed in 302.94 seconds; C-unit 300.21 seconds |
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
