# I2 `never` loop conditions — iteration 28 — 2026-08-11

Iteration status: complete; gate status is `in-progress`.  
Scope: complete lazy control-flow lowering for bottom-valued `while` conditions
and C-style `for` initializers and conditions.

## Existing normative contract implemented

- A `never` expression cannot complete normally and therefore needs neither a
  value register nor a successor block that assumes a value was produced.
- A loop condition is required to have exact type `bool` only on paths where
  evaluating it completes normally.
- C-style `for` initialization precedes condition evaluation and enters the
  loop-local scope. If initialization exits the function, no loop local or
  condition/body/step/exit control-flow structure becomes observable.
- This iteration changes no syntax, type relation, evaluation order, or loop
  ownership rule.

## Implemented

- `while` now creates only its condition block before lowering the condition.
  Body and exit blocks are allocated lazily after an ordinary boolean result.
- An all-bottom `while` condition leaves the selected return paths terminated
  and emits no branch using `RUDA_MIR_NO_REGISTER` and no dead loop blocks.
- C-style `for` lowers its initializer before creating the loop local. A
  bottom-valued initializer completes the enclosing path without a dummy local,
  copy, or loop CFG.
- C-style `for` initially creates only its condition block. Body, step, and exit
  blocks are allocated after the condition produces an ordinary boolean.
- Existing snapshot and scope cleanup remains active on every early-success and
  failure path; allocation-failure injection exercises the lazy loop path.

## Coverage added

- An all-bottom boolean match in a `while` condition returns the expected value
  from both match arms.
- A mixed `while` condition returns on the selected `true` path and yields
  `false` on the other path, which reaches code after the loop.
- Equivalent all-bottom and mixed cases cover C-style `for` conditions.
- An all-bottom match in a C-style `for` initializer returns before creating the
  loop local or entering the condition.
- Every added function passes MIR verification and runtime interpretation proves
  all ten selected outcomes.
- The fail-allocator corpus uses a bottom-valued `while` condition and remains
  leak-free through every injected failure until success.

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

Panic expressions, declared never-returning calls, divergence, bottom-valued
C-style `for` steps, and unreachable-statement diagnostics/recovery remain open.
The checker also still needs a semantic-only pass for diagnostics inside code
made unreachable by bottom-valued control-flow expressions; the executable
oracle does not claim complete unreachable-code analysis.

Generic/prelude enums, constant construction/matching, imported and
package-qualified resolution/visibility, nested payload/record/tuple/string and
other literal patterns, nested/generic witnesses, derives,
earlier-covering-pattern related locations, hardened serialized-MIR tag
dominance, and VM/native differential coverage also remain open. This iteration
changes no ABI/runtime policy and, under D034, adds no authorship, agent-trust,
CI, sandbox, or release-policy semantics to Ruda.
