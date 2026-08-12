# I2 typed direct calls — iteration 6 — 2026-08-11

Iteration status: complete; gate status is `in-progress`.  
Scope: connect the staged signature table to typed MIR calls and extend the
unoptimized oracle from isolated functions to a checked function set.

## Implemented

- Direct calls to package functions resolve against the complete signature
  collection, so forward calls and recursion work without declaration-order
  exceptions. Arguments are lowered left to right against exact parameter
  types; arity mismatch has stable diagnostic `T0009 CALL_ARITY`.
- MIR owns bounded call-site descriptors containing the stable callee symbol,
  result type, and a flat typed argument-register slice. Function limits now
  separately bound call count and aggregate call arguments.
- The MIR verifier checks call IDs, argument-slice arithmetic, register ranges,
  exact stored argument/result types, definite assignment, and work charged per
  argument. Malformed calls cannot reach the interpreter.
- The MIR interpreter accepts an explicit call resolver rather than hidden
  global program state. The checked-program oracle resolves stable symbols,
  supports recursive calls, and applies per-function step, shared total-step,
  and call-depth limits.
- The owned checked result indexes every emitted function by its exact 128-bit
  symbol ID. Interpreter call dispatch is therefore deterministic indexed
  lookup rather than a scan over the package function list; the resolved entry
  is rechecked against the requested symbol before execution.
- Call-expression statements support `void` functions. Any semantic error in a
  source unit clears all emitted function artifacts, preventing callers from
  treating a partially checked set as authoritative.
- Call-site construction and interpretation are allocation-failure safe. Flat
  call vectors keep their observable lengths unchanged on failed insertion;
  temporary value arrays are released on every interpreter result.

## Coverage added

- A forward caller invokes two later declarations and returns the combined
  result; a recursive factorial function executes through the same program
  oracle; a `void` call executes as a statement. Tests pin one symbol-index
  entry per accepted function and an empty index whenever diagnostics clear the
  function set.
- Exact fixtures cover wrong arity, wrong argument type, and an unknown callee.
  The recursive fixture separately proves call-depth and shared-total-step
  termination.
- MIR unit tests execute a call through an injected resolver, reject a missing
  resolver, wrong stored argument type, uninitialized call argument, and a call
  exceeding verifier work.
- Failure injection covers both call-site vector growth and interpreter
  register/argument allocations with zero live allocation count and bytes after
  every attempt. The complete checker failure walk now includes a forward call.

## Verification

| Command | Exit | Result |
|---|---:|---|
| Debug build and `ctest --preset debug --output-on-failure` | 0 | warnings-as-errors C17 build; 1/1 test executable passed |
| Release build and `ctest --preset release --output-on-failure` | 0 | optimized build; 1/1 passed |
| ASan build and `ctest --preset asan --output-on-failure` | 0 | 1/1 passed without sanitizer report |
| ASan+UBSan build and `ctest --preset asan-ubsan --output-on-failure` | 0 | 1/1 passed without sanitizer report |
| TSan build and `ctest --preset tsan --output-on-failure` | 0 | 1/1 passed without sanitizer report |
| `clang --analyze` on checker, MIR, and type-table sources | 0 | zero static-analyzer findings |
| Generated/doc/term/example/syntax checks | 0 | generated `T0009` artifacts current; all documentation and syntax matrices green |

## Remaining I2 gate work

Call resolution remains single-source and package-local. Imported, qualified,
dot-call, generic, interface, closure/function-value, intrinsic, and suspending
calls are not implemented. Source-name lookup still uses the temporary
declaration vector and must become an overload-set-aware package index when
nominal first-parameter dot calls land. The complete resolver, nominal
types/data, match, `Option`/`Result`/`?`, effects, service queries, and
randomized reference-evaluator comparison remain open.

The public CLI checker remains unavailable. No package format, ABI, native/VM
contract, or external command behavior changed; the I2 interpreter remains the
oracle for differential evidence beginning in I3/I4.
