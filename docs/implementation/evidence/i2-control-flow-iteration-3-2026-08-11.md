# I2 control-flow lowering — iteration 3 — 2026-08-11

Iteration status: complete; gate status is `in-progress`.  
Scope: the internal typed checker now lowers its first structured source control
flow to verified MIR and preserves lexical/definite-assignment semantics.

## Implemented

- `if`, `if`/`else`, `else if`, `while`, and explicit nested statement blocks
  lower to typed MIR blocks with branch, back-edge, join, and return
  terminators. Conditions must have exact type `bool`; there is no truthiness.
- Nested blocks have explicit lexical depth. Bindings leave the lookup set at
  block exit while their MIR registers remain owned by the function artifact.
- Definite initialization is snapshotted at a branch point and intersected over
  every continuing branch. A branch that returns does not weaken the state of
  the surviving path. An `if` without `else` includes the unchanged false path.
- A `while` body cannot establish definite initialization after the loop,
  because the condition may be false before the first iteration. Mutations of
  already initialized locals remain valid.
- Fully returning `if`/`else` constructs terminate the enclosing path without
  manufacturing an unreachable join. Non-void missing-return diagnostics are
  now issued only after the top-level function body has been lowered.
- Control-flow lowering uses the checked MIR block limit and owns every
  initialization snapshot through allocation failure and diagnostic exits.

## Coverage added

- The MIR oracle executes true/false one-arm `if`, both arms of an assigning
  `if`/`else`, a fully returning `if`/`else`, a nested `else if`, a lexical
  statement block, and a counter loop with a back-edge.
- Compile diagnostics pin the exact spans for an integer condition, use after
  initialization on only one branch, use after initialization only in a loop,
  and access to a binding after its nested scope ends.
- Failure injection now walks the complete checker allocation surface on a
  function containing both an assigning branch and a loop, asserting zero live
  allocations and bytes after every failed attempt and after success disposal.

## Verification

| Command | Exit | Result |
|---|---:|---|
| Debug build and `ctest --preset debug --output-on-failure` | 0 | warnings-as-errors C17 build; 1/1 test executable passed |
| Release build and `ctest --preset release --output-on-failure` | 0 | optimized build; 1/1 passed |
| ASan build and `ctest --preset asan --output-on-failure` | 0 | 1/1 passed without sanitizer report |
| ASan+UBSan build and `ctest --preset asan-ubsan --output-on-failure` | 0 | 1/1 passed without sanitizer report |
| TSan build and `ctest --preset tsan --output-on-failure` | 0 | 1/1 passed without sanitizer report |
| `clang --analyze` on checker, MIR, and type-table sources | 0 | zero static-analyzer findings |

## Remaining I2 gate work

The authoritative public checker remains unavailable. This source slice does
not yet implement `break`, `continue`, `for`, `defer`, expression-valued control
flow, unreachable-code warnings, or the required warning for nested shadowing.
It also does not replace the pending package/import/signature phases, nominal
types, calls, generics, matching, effects, service queries, or randomized
reference-evaluator comparison.

The accepted language semantics were already normative; this iteration changes
no syntax, package format, ABI, or external tool behavior. VM/native
differential evidence remains inapplicable until the I3 and I4 engines exist;
the verified unoptimized MIR interpreter is still their future oracle.
