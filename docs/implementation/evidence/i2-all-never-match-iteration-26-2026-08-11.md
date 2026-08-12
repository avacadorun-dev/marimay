# I2 all-`never` match propagation — iteration 26 — 2026-08-11

Iteration status: complete; gate status is `in-progress`.  
Scope: give an exhaustive match whose every arm exits through `return` a real
bottom-value contract, without adding a dummy result register, dead return, or
new MIR terminator.

## Existing normative contract implemented

- `never` denotes an expression that cannot complete normally. Match result
  equality applies only to non-`never` arms, and `never` is compatible with
  every return position.
- If every selected arm returns from the enclosing function, the match itself
  cannot produce a runtime value and no continuation reaches its consumer.
- A `return` inside a block arm remains an enclosing-function return. This
  iteration changes neither return semantics nor the observable order of any
  expression that can complete.
- This is implementation of language sections 4.2, 5.6, and 9; no syntax,
  source ABI, or runtime policy changed.

## Implemented

- The checker now represents a terminated expression as `RUDA_TYPE_NEVER`, a
  valid expression result with `RUDA_MIR_NO_REGISTER`, paired with a terminated
  current CFG path.
- Match join blocks are allocated lazily on the first value-producing
  expression or `yield`. An all-return match allocates no join, so the MIR
  verifier never sees an unterminated dead block.
- Boolean, `i64`, and nominal-enum match lowerings return the bottom expression
  when no arm reaches a value target. Mixed value/return matches keep their
  ordinary typed join from iteration 25.
- A nested all-return match used as an arm expression or `yield` operand
  propagates termination to the enclosing arm instead of copying
  `RUDA_MIR_NO_REGISTER`.
- The current executable expression consumers propagate a terminated operand
  without emitting an instruction: return positions (including `void`),
  bindings, unary/binary operators, direct-call arguments, record and enum
  constructor arguments, member receivers, and assignments.
- Short-circuit lowering handles a never-completing right operand specially:
  the selected right path terminates, while the shortcut path alone reaches the
  existing boolean join. A never-completing left operand terminates the whole
  expression before the right operand is compiled.
- No MIR kind or verifier exception was added; existing typed register and
  terminator invariants remain unchanged.

## Coverage added

- Runtime interpretation covers all-return boolean matches directly in
  `return`, an inferred binding initializer, unary and binary operands, and a
  direct-call argument. The enclosing operator/call is demonstrably skipped.
- Right- and left-side logical expressions cover bottom propagation with and
  without a surviving shortcut path.
- `void` return positions accept an all-return match and produce the ordinary
  void function result.
- Record-field and enum-payload constructors receive an all-return match;
  construction is skipped and the inner returns determine the function result.
- Assignment RHS and nested all-return matches propagate termination without a
  copy or join.
- `i64` catch and nominal-enum variant matches compile through the same
  all-return endpoint; the integer fixture also executes in the MIR oracle.
- Every produced function passes MIR verification. The simplest boolean
  all-return function contains no result `COPY` for the match.
- The fail-allocator corpus contains an all-return match and reaches success
  after every preceding injected allocation failure without leaks.

## Verification

| Command | Exit | Result |
|---|---:|---|
| Debug build and `ctest --preset debug --output-on-failure` | 0 | warnings-as-errors C17 build; 1/1 test executable passed |
| Release build and `ctest --preset release --output-on-failure` | 0 | optimized build; 1/1 passed |
| ASan build and `ctest --preset asan --output-on-failure` | 0 | 1/1 passed without sanitizer report |
| ASan+UBSan build and `ctest --preset asan-ubsan --output-on-failure` | 0 | 1/1 passed without sanitizer report |
| TSan build and `ctest --preset tsan --output-on-failure` | 0 | 1/1 passed without sanitizer report |
| `clang --analyze` on checker and checker C-unit source | 0 | zero findings |
| `./tools/check-generated` | 0 | diagnostic, Unicode, and syntax artifacts are current |
| Doc-link, spec-term, example-syntax, and syntax-coverage checks | 0 | D001–D035 consistent; 76/76 syntax kinds and 117/117 grammar productions covered |

## Deliberate boundary and remaining I2 work

This slice models bottom values created by the implemented match/return CFG.
Panic expressions, declared never-returning calls, divergence, never-completing
match guards, and loop-condition bottom propagation remain open with their
own control-flow lowering. Unreachable-statement diagnostics/recovery also
remain the pre-existing explicit incomplete boundary.

Generic/prelude enums, constant construction/matching, imported and
package-qualified resolution/visibility, nested payload/record/tuple/string and
other literal patterns, nested/generic witnesses, derives,
earlier-covering-pattern related locations, hardened serialized-MIR tag
dominance, and VM/native differential coverage also remain open. This
iteration changes no ABI/runtime policy and, under D034, adds no authorship,
agent-trust, CI, sandbox, or release-policy semantics to Ruda.
