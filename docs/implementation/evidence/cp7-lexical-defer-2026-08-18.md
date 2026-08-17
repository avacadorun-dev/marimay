# Lexical `defer` — preview checkpoint 7 — 2026-08-18

Checkpoint status: locally complete. I2–I5 remain `in-progress`; I6 remains
`not-started`. This is the D041 cleanup slice available before task runtime,
not scheduler, cancellation-runtime, Tier 1, or public-release evidence.

Authority: accepted D041; language §10.3; diagnostics §9; the compiler
architecture contract; and checkpoint 7 in `checkpoints.md`.

## Delivered vertical slice

- The existing lossless `defer` statement is checker-backed. It accepts only a
  statically resolved direct or structural-interface call returning `void`.
  Non-void and `Result` calls receive a deterministic type diagnostic rather
  than an implicit discard or unwrap.
- Callee receiver and arguments are evaluated and copied when registration is
  reached. Lexical-scope lowering emits exact LIFO cleanup on fallthrough,
  `return`, postfix `?`, `break`, and `continue`; panic and declared-`never`
  paths retain registrations for engine unwinding.
- MIR and bytecode have explicit register/call cleanup operations. Their
  bounded dataflow verifiers require identical incoming cleanup stacks at CFG
  merges, exact LIFO call identity, and an empty stack at ordinary return.
- The MIR oracle and encoded program VM run remaining cleanups exactly once on
  abnormal exit. The native backend emits the same state-machine cleanup,
  including interface dispatch, and rejects direct defer-call cycles under the
  same no-recursive-native-call boundary as ordinary calls.
- D041 panic precedence is explicit: the first cleanup panic replaces a
  non-panic outcome; an existing panic stays primary; later cleanups still run.
  Suppressed cleanup failures retain code, message, symbol, span, and frames in
  observation order. VM and generated native diagnostics use the same shape.
- Cleanup owners were split into focused MIR, bytecode VM/verifier, checker,
  and native-emitter translation units. The architecture gate is green without
  exceptions or handwritten include shards.

## Requirements-to-tests coverage

| Checkpoint-7 claim | Executable evidence |
|---|---|
| fallthrough, return, `break`/`continue`, nested scopes, LIFO, exactly once | one source acceptance trace in `examples/defer_cp7.ruda`, executed by MIR, encoded VM, and native C17 |
| panic and explicit failure policy | body-primary, cleanup-primary, two-cleanup primary/suppressed, and remaining-cleanup cases compare exact panic identity and suppressed records |
| reject non-void/`Result` cleanup | focused checker diagnostics for scalar and generic `Result` calls |
| verifier CFG invariants | MIR and bytecode negatives for wrong LIFO identity, ordinary return with a residual stack, and unequal stacks at a branch merge |
| same VM/native outcome | acceptance result and every panic-policy case execute in both engines; native C is compiled/run at O0 and O2 |
| parser/checker/formatter | parser positive/recovery, checker source fixture and negatives, canonical all-productions entry, and dedicated input→expected defer formatter fixture |
| fail allocation and ownership | MIR/bytecode verifiers, normal MIR/VM execution, and panic/suppression paths exhaust deterministic failure indices and assert both live counters are zero |
| pre-task boundary | no scheduler/task/scope/cancellation runtime was added; I6 remains `not-started` |

- `examples/defer_cp7.ruda` is the D041 acceptance fixture. Its single
  differential trace covers normal fallthrough, early return, nested scopes,
  LIFO, exactly-once loop fallthrough/continue/break, registration-time
  argument capture, structural interface dispatch, and postfix-`?` Err exit.
- `test_defer_cp7.c` runs that source through checker and verified MIR, encoded
  bytecode round trip, VM, and generated native C17 at O0/O2. Every engine
  produces the same trace value.
- Focused panic cases prove cleanup-panic replacement, body-panic precedence,
  first-cleanup primary plus later-cleanup suppression, suppressed failure
  contents, remaining-cleanup execution, and equal VM/native diagnostics. A
  direct MIR callback case independently verifies residual LIFO execution
  after a cleanup panic.
- Checker negatives reject both scalar non-void and generic `Result` cleanup
  calls. MIR and bytecode corruption tests reject wrong-LIFO, residual-stack
  return, and unequal branch-merge stack paths.
- MIR verifier, bytecode verifier, normal MIR/VM execution, and
  panic/suppression execution run deterministic fail-every-allocation sweeps
  and assert both `live_allocations == 0` and `live_bytes == 0` after teardown.
- Parser positive/recovery tests cover the statement form; the canonical
  all-productions formatter fixture contains `defer`; a dedicated unformatted
  input/expected pair checks normalization and idempotence; the formatter
  example corpus includes the public checkpoint fixture.

## Scope boundary

No scheduler, task, `scope`, runtime cancellation request, or cancellation
mask implementation is introduced. D041's cleanup ordering and panic policy
are available now; the task-cancellation edge remains part of I6, which stays
`not-started`.

## Local validation

Environment: macOS Darwin 25.3.0, arm64; Apple Clang 17.0.0; CMake 4.4.2.

| Command | Exit | Result |
|---|---:|---|
| `./tools/check-architecture` | 0 | bounded ownership and line budgets |
| `./tools/check-c-memory-policy` | 0 | central allocator policy |
| `./tools/check-generated` | 0 | generated syntax/opcode artifacts |
| `./tools/check-doc-links` | 0 | documentation links |
| `./tools/check-spec-terms` | 0 | normative terminology |
| `./tools/check-example-syntax` | 0 | public example syntax |
| `./tools/check-syntax-coverage` | 0 | 75/75 syntax kinds and 115/115 grammar productions covered |
| `git diff --check` | 0 | whitespace validation |
| `ctest --preset debug --output-on-failure` | 0 | full CTest, 195.75 seconds |
| `ctest --preset release --output-on-failure` | 0 | full CTest, 195.52 seconds |
| `ctest --preset asan-ubsan --output-on-failure` | 0 | sanitizer CTest, 248.17 seconds |
