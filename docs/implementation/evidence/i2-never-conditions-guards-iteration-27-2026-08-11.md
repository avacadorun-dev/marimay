# I2 `never` conditions and match guards — iteration 27 — 2026-08-11

Iteration status: complete; gate status is `in-progress`.  
Scope: extend iteration-26 bottom propagation to ordinary `if` conditions and
selected match guards while preserving pattern-miss continuation.

## Existing normative contract implemented

- A `never` expression cannot complete normally, so a condition that evaluates
  through such an expression does not require or produce a boolean register.
- A match guard is evaluated only after its pattern matches. If that guard
  exits through `return`, only the selected-pattern path terminates; a
  nonmatching pattern still continues to the next arm.
- Guard coverage remains unchanged: syntactically guarded patterns do not
  contribute to exhaustiveness even when the current guard implementation
  happens not to complete.
- This iteration changes no source syntax, match usefulness rule, or observable
  evaluation order.

## Implemented

- A common guard compiler distinguishes an ordinary exact-`bool` result from a
  terminated `never` result. Existing non-boolean guards retain
  `T0002 TYPE_MISMATCH` and their exact spans.
- Boolean, `i64`, and nominal-enum lowering treat a bottom-valued guard as a
  completed selected arm path. They emit no branch using
  `RUDA_MIR_NO_REGISTER` and continue compiling the already-created
  pattern-miss block for later arms.
- `if` condition lowering recognizes a bottom result before allocating true and
  false blocks. The enclosing function remains terminated by the condition's
  returns and the MIR contains no dead unterminated branch blocks.
- `while` and C-style `for` currently allocate loop structure before compiling
  their condition. They now detect a bottom condition and stop at the explicit
  incomplete-language boundary instead of attempting to emit an invalid MIR
  branch. Their full lazy-block refactor remains separate work.

## Coverage added

- An `if` whose boolean match condition returns from every arm executes the two
  inner return paths; its nominal body is never entered.
- Integer literal matching evaluates a bottom-valued guard only for literal
  `0`; another integer follows the pattern-miss edge to the wildcard result.
- Boolean matching skips the guard for `false` and executes both inner return
  paths for `true`.
- Nominal-enum matching executes the guard for variant `A`, preserves both
  decision outcomes, and reaches the ordinary `B` arm without evaluating the
  guard.
- All generated functions pass MIR verification and runtime interpretation
  proves selected-path behavior.
- A dedicated fixture asserts the loop-condition incomplete boundary.
- The fail-allocator corpus contains a bottom-valued integer guard and remains
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

Bottom-valued loop conditions, panic expressions, declared never-returning
calls, divergence, and unreachable-statement diagnostics/recovery remain open.
The checker also still needs a semantic-only pass for diagnostics inside code
that bottom-valued conditions make unreachable; the current executable oracle
does not claim that complete unreachable-code analysis.

Generic/prelude enums, constant construction/matching, imported and
package-qualified resolution/visibility, nested payload/record/tuple/string and
other literal patterns, nested/generic witnesses, derives,
earlier-covering-pattern related locations, hardened serialized-MIR tag
dominance, and VM/native differential coverage also remain open. This
iteration changes no ABI/runtime policy and, under D034, adds no authorship,
agent-trust, CI, sandbox, or release-policy semantics to Ruda.
