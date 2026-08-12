# I2 `never` C-style `for` steps — iteration 29 — 2026-08-12

Iteration status: complete; gate status is `in-progress`.  
Scope: complete bottom propagation through the assignment-only step of the
current C-style `for` slice.

## Existing normative contract implemented

- A `never` expression cannot complete normally, including when it appears as
  the right-hand side of an assignment.
- The step of a C-style `for` runs only after a body path completes normally.
  A step path that exits the function must not receive a synthetic back-edge to
  the condition.
- The condition's false edge remains an independent normal exit from the loop,
  even when every path selected inside the step terminates.
- This iteration changes no syntax, assignment typing, evaluation order, or
  loop-control semantics.

## Implemented

- C-style `for` lowering checks whether step assignment lowering terminated the
  active path before emitting the condition back-edge.
- A terminated step retains the MIR terminator created by its bottom-valued
  right-hand side; an ordinary step still emits the existing back-edge.
- Initialization-state restoration and loop-scope cleanup execute for both
  cases, while the separately allocated condition false edge remains available
  as the continuation after the loop.

## Coverage added

- An all-bottom boolean match on the step assignment's right-hand side returns
  from either selected arm when the condition enters the loop.
- The same function proves that an initially false condition skips the step and
  reaches the return after the loop.
- A mixed step returns on one selected path and yields the next index on the
  other, proving that the ordinary path retains its back-edge and exits through
  the condition on the next iteration.
- All seven loop-bottom fixtures pass MIR verification; runtime interpretation
  covers the five earlier initializer/condition functions and all five new
  step outcomes.
- The fail-allocator corpus includes an all-bottom step and remains leak-free
  through every injected failure until success.

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

Panic expressions, declared never-returning calls, divergence, and
unreachable-statement diagnostics/recovery remain open. The checker also still
needs a semantic-only pass for diagnostics inside code made unreachable by
bottom-valued control-flow expressions; the executable oracle does not claim
complete unreachable-code analysis.

Generic/prelude enums, constant construction/matching, imported and
package-qualified resolution/visibility, nested payload/record/tuple/string and
other literal patterns, nested/generic witnesses, derives,
earlier-covering-pattern related locations, hardened serialized-MIR tag
dominance, and VM/native differential coverage also remain open. This iteration
changes no ABI/runtime policy and, under D034, adds no authorship, agent-trust,
CI, sandbox, or release-policy semantics to Ruda.
