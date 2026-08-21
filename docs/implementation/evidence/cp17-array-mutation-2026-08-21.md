# Post-preview checkpoint 17: executable array mutation — 2026-08-21

Gate: I2 typed core/MIR, I3 bytecode VM, I4 C17 equivalence, and I5 managed
data. Checkpoint status: locally complete. I2, I3, I4, and I5 remain
`in-progress`; the complete Tier 1 matrix is not claimed by this local record.

Baseline commit: `21b2787` (`Restore randomized frontend fuzz campaigns`).

Authority: accepted D003, D040, D043, D045, and new accepted D053, together
with the normative array, managed-memory, diagnostics, typed-MIR, verified
bytecode, and engine-equivalence contracts. D053 fixes the exact signatures,
typed errors, mutation-generation behavior, failure atomicity, reference
release, and iterator invalidation semantics before implementation.

## Delivered slice

- The compiler-known prelude now exposes ordinary free/dot-call `pop`,
  `truncate`, `clear`, and `reserve`, plus the D053 `push` result contract.
  `Option<T>`, `Result<void, ArrayTruncateError>`, and
  `Result<void, ArrayGrowthError>` preserve exact element and named-error
  types. Receiver-directed selection still gives an exact package-owned
  nominal function precedence over compiler-known array operations.
- Empty `pop`, invalid or expanding `truncate`, negative or too-large
  `reserve`, equal-length truncation, empty clearing, and successful mutation
  paths are distinct CFG branches. Receiver and explicit arguments are
  evaluated once from left to right. Backends consume the resulting typed MIR
  and do not reconstruct source error semantics.
- Arrays retain non-observable reserved storage while removed cells leave the
  initialized and precisely traced prefix. `reserve` is failure-atomic and
  guarantees the requested further pushes without exposing capacity.
  Successful initialized-sequence changes increment one non-wrapping unsigned
  mutation generation; no-op, error, and reserve paths do not.
- `for const ... of ...` captures the array generation and checks it before
  each loop test and yield. Reserve remains legal during iteration. A changed
  generation produces exact `G4005`, message
  `array iterator invalidated by mutation`, at the complete loop-header span.
- Typed MIR adds focused mutation/generation instructions and an alias-safe
  capacity model. Bytecode appends opcodes 90–95, verifies operand assignment,
  types, layouts, and generation width, and rejects dynamically impossible raw
  empty-pop execution. The VM and portable generated C17 call the same runtime
  operations and preserve exact state, panic identity, output, and exit code.

## Ownership and scope boundaries

`checker_array_mutation.c`, `execution_array.c`,
`vm_array_mutation.c`, `c_emitter_array_mutation.c`, and
`managed_array_mutation.c` are focused owners. Existing checker, MIR, VM,
native, and runtime files remain coordinators or retain their prior semantic
families. Capacity is not public, no slice/deque surface was added, and the
algorithm-corpus shadow-size cleanup remains checkpoint 20 work.

## Focused coverage

- Runtime tests cover raw, reference, and recursive nested arrays; empty,
  single, partial, and complete removal; below/equal/above reservations;
  no-ops and exact limit statuses; generation increments/exhaustion; alias-safe
  growth; popped-result rooting; post-pop/truncate/clear reference
  reachability; every reachable reserve/reference-push resize failure; atomic
  length/capacity/generation state; and zero live allocation/byte counters.
- Checker tests cover free/dot forms, nominal name collision, exact
  `Option`/`Result` types, all six new MIR operations, complete-loop generation
  instrumentation, negative diagnostics, and deterministic fail-every-
  allocation teardown.
- The focused MIR owner verifies and interprets aliased pop/truncate/clear/
  reserve/push state to exact `i64` value `410`, then checks exact `G4005`
  message and complete `for const value of values` span.
- Bytecode tests cover appended wire IDs, verifier type mutations, malformed
  raw empty pop, encode/decode/verify round trip, successful mutation state,
  and exact `G4005` VM panic.
- The focused native owner emits all mutation and generation runtime calls,
  compiles strict C17, executes to exact `ok:i64:415\n`, and exhausts emitter
  allocations with zero counters. CLI coverage separately executes the native
  invalidation panic.
- The `cp17-array-mutation` corpus covers typed error payloads, empty/nonempty
  pop, nested reference arrays, partial/all removal, reserve during iteration,
  semantic length overflow, aliases, and D040 order. The separate invalidation
  corpus mutates during iteration. The CLI owner checks `fmt --check`, `check`,
  VM `run`, `test --engine both`, native `build`, artifact execution, exact
  stdout/stderr/exit, and exact VM/native `G4005` parity. A dedicated formatter
  pair and `examples/array_mutation.ruda` cover the public spellings.

## Exact observations

| Surface | Exit | Observation |
|---|---:|---|
| `ruda run` success corpus | 0 | stdout `cp17 array mutation ok\n`; empty stderr |
| `ruda test --engine both` | 0 | `1 passed; 0 failed`; matching VM/native observation |
| built native success artifact | 0 | stdout `cp17 array mutation ok\n`; empty stderr |
| VM invalidated-iterator corpus | 1 | exact `panic[G4005]: array iterator invalidated by mutation\n` |
| native invalidated-iterator artifact | 1 | the identical exact `G4005` line |

The existing G4002 bounds matrix remains green, so D053 generation checks do
not change D045 checked-index panic identity or ordering.

## Local validation

Environment: macOS Darwin 25.3.0 arm64; Apple Clang 17.0.0; CMake 4.4.2.

| Command | Exit | Result |
|---|---:|---|
| `build/debug/ruda fmt tests/post-preview/cp17-array-mutation --check` | 0 | corpus canonical |
| `build/debug/ruda check tests/post-preview/cp17-array-mutation` | 0 | complete package accepted |
| `build/debug/ruda run tests/post-preview/cp17-array-mutation` | 0 | exact success stdout |
| `build/debug/ruda test tests/post-preview/cp17-array-mutation --engine both` | 0 | 1/1 VM/native test |
| `build/debug/ruda build tests/post-preview/cp17-array-mutation --output build/debug/cp17-array-mutation` | 0 | Debug-profile native artifact created |
| `build/debug/cp17-array-mutation` | 0 | exact success stdout |
| `build/debug/ruda build tests/post-preview/cp17-array-mutation --release --output build/debug/cp17-array-mutation-release` | 0 | Release-profile native artifact created |
| `build/debug/cp17-array-mutation-release` | 0 | exact success stdout |
| `build/debug/ruda run tests/post-preview/cp17-array-iterator-invalidated` | 1 | exact VM G4005 panic |
| built invalidation artifact | 1 | exact native G4005 panic |
| `cmake --build --preset debug --parallel 4` | 0 | final Debug tree built |
| `ctest --preset debug --output-on-failure` | 0 | 2/2 passed in 212.37 seconds; C-unit 210.21 seconds |
| `cmake --build --preset release --parallel 4` | 0 | final Release tree built |
| `ctest --preset release --output-on-failure` | 0 | 2/2 passed in 199.63 seconds; C-unit 197.82 seconds |
| `cmake --build --preset asan-ubsan --parallel 4` | 0 | final ASan/UBSan tree built |
| `ctest --preset asan-ubsan --output-on-failure` | 0 | 2/2 passed in 331.05 seconds; C-unit 328.15 seconds |
| `./tools/check-architecture` | 0 | owner/dependency/line-budget policy clean |
| `./tools/check-c-memory-policy` | 0 | central allocator policy clean |
| `./tools/check-generated` | 0 | generated bytecode/diagnostic/Unicode/syntax artifacts current |
| `./tools/check-doc-links` | 0 | evidence/spec/roadmap links valid |
| `./tools/check-spec-terms` | 0 | normative terminology and D053 index clean |
| `./tools/check-syntax-coverage` | 0 | all 75 syntax kinds and 115 grammar productions covered |
| `./tools/check-example-syntax` | 0 | all user-facing Ruda examples accepted |
| `git diff --check` | 0 | no whitespace errors |

This local debug/release/sanitizer table closes the checkpoint implementation
and its required local validation tail. Cross-platform Tier 1 CI remains the
governing gate for changing I2–I5 from `in-progress`.
