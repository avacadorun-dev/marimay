# I2 declared-`never` calls — iteration 31 — 2026-08-12

Iteration status: complete; gate status remains `in-progress`.  
Scope: D037 declared bottom-returning functions across source checking, typed
MIR verification, and the executable MIR oracle.

## Normative contract fixed and implemented

- D037 defines `never` as a non-returning call contract. Every reachable path
  in a function declared `-> never` must terminate without a normal return.
- Arguments are evaluated left to right before the terminating call edge.
- A resolved call to a declared-`never` signature has type `never`, no result
  register, and no normal MIR successor. It cannot fabricate `void`, a sentinel
  value, or a backend-specific unreachable value.
- Language §§5.1 and 6.1, execution §13.1, and LANG-027 record the same
  observable contract.

## Implemented

- Added the typed `RUDA_MIR_CALL_NEVER` terminator with callee and argument
  operands but no result register or normal target.
- The source checker lowers direct, selected, nested, and argument-position
  calls to declared-`never` functions after normal left-to-right argument
  evaluation and propagates bottom through the existing expression CFG.
- Declared-`never` bodies reject fallthrough and every normal `return`, whether
  bare or value-producing.
- The MIR verifier validates the callee result type, argument count/ranges,
  exact argument types, definite assignment, terminator shape, and absence of
  a normal return path. A regular call instruction targeting a `never` result
  is malformed MIR.
- The executable oracle invokes the selected callback or MIR callee and
  propagates its defined abnormal result. If a host callback or malformed
  callee returns normally despite the contract, execution reports invalid MIR
  rather than continuing with a fabricated value.

## Coverage added

- Source conformance covers direct, selected, nested, and argument-position
  calls, result-type propagation, evaluation order, and runtime panic
  observation through a declared-`never` callee.
- Source negatives cover an empty body, bare `return`, and value `return` in a
  declared-`never` function.
- MIR adversarial tests cover a callback that panics, a callback that violates
  the contract by returning success, allocator failure, result-type tampering,
  invalid and unassigned arguments, absence of the required call edge, a
  normal return in a declared-`never` function, and a regular call instruction
  incorrectly targeting `never`.
- Checker and MIR fail-allocator campaigns include the new lowering and
  verifier/executor ownership paths.

## Verification

| Command | Exit | Result |
|---|---:|---|
| Debug build and `ctest --preset debug --output-on-failure` | 0 | warnings-as-errors C17 build; 1/1 passed |
| Release build and `ctest --preset release --output-on-failure` | 0 | optimized build; 1/1 passed |
| ASan build and strict `ctest --preset asan` | 0 | 1/1 passed without sanitizer report |
| ASan+UBSan build and strict `ctest --preset asan-ubsan` | 0 | 1/1 passed without sanitizer report |
| TSan build and strict `ctest --preset tsan` | 0 | 1/1 passed without sanitizer report |
| `clang --analyze` on checker, MIR, and affected C-unit sources | 0 | zero findings |
| Generated, doc-link, spec-term, example-syntax, and syntax-coverage checks | 0 | generated artifacts and D001–D038 contracts are consistent |

## Deliberate boundary and remaining work

The oracle enforces the terminating edge and detects a violated callback
contract, but it does not prove arbitrary semantic divergence. Interprocedural
termination proofs are not required: a declared-`never` implementation must
instead have no reachable normal completion under the typed CFG rules.

Production bytecode and native engines do not yet exist. D037 therefore still
requires I3/I4 execution and differential evidence, including recursive,
panic, cancellation, and invalid-bundle cases, before LANG-027 or I2 can be
considered fully verified.
