# I2 block match arms and `yield` — iteration 24 — 2026-08-11

Iteration status: complete; gate status is `in-progress`.  
Scope: implement the existing language-section-9 contract for multi-statement
`match` arms whose continuing paths produce the match value with explicit
`yield`, across the current enum, finite-boolean, and `int` match oracles.

## Existing normative contract implemented

- An expression arm produces its expression value. A block arm produces its
  value through `yield expression;` and does not acquire a different match
  result-type rule.
- Every continuing path in the implemented block subset must yield. A false
  branch, loop exit, or ordinary fallthrough that reaches the closing brace is
  rejected.
- `yield` targets the innermost block match arm. It is not a function return,
  loop control statement, or scheduler operation.
- All non-`never` arms retain one exact result type. Guards, OR-pattern
  bindings, exhaustiveness, redundancy, and selected-path evaluation keep the
  behavior established by iterations 19–23.
- This is checker/MIR implementation of accepted specification text; it adds
  no syntax or language decision.

## Implemented

- Enum, boolean, and `i64` arm analysis now records either the arm expression or
  parsed block while preserving the guarded-arm expression index.
- A transient, allocation-free checker context carries the current match join
  block, inferred/required result type, and shared result register. Nested block
  matches save and restore this context, so an inner `yield` cannot escape into
  its enclosing match.
- `yield` compiles its expression under the current exact match type, lazily
  creates the shared result register, copies the value, terminates the current
  CFG path with a jump to the match join, and marks the path terminated.
- Existing `if`/loop termination analysis detects block fallthrough. A block
  where one branch yields and another continues must yield later on the
  continuing path; a loop-contained yield alone is insufficient because the
  loop may not execute.
- Expression and block arms use one result-target helper, removing three
  duplicated result-type/copy/join lowering sequences.
- Stable `T0015 YIELD_OUTSIDE_MATCH_ARM` reports a source `yield` without an
  active block arm. Stable `T0016 MISSING_MATCH_YIELD` reports the complete
  block span when a reachable path falls through.
- Generated diagnostic header/reference artifacts were regenerated from the
  canonical table.

## Coverage added

- Runtime fixtures execute enum block arms with payload bindings, guards,
  locals, and an `if` path that either yields immediately or continues to a
  later yield.
- Boolean OR patterns exercise an `if` whose two branches both yield. Integer
  OR literals and catch bindings exercise block-local computation and yield.
- A nested block match proves innermost-context ownership and restoration; a
  surrounding arm uses the nested match value in a later outer yield.
- Mixed expression/block arms share one typed target. Mixed `return`/`yield`
  control flow proves that `return` still exits the enclosing function.
- A division-by-zero call in a block arm is skipped when the arm is not selected
  and traps when it is selected.
- Exact-span negative fixtures cover `yield` outside a match arm, direct block
  fallthrough, conditional fallthrough, and a yielded value with the wrong
  type.
- The single-file fail-allocator corpus now lowers guarded/OR enum block arms,
  proving failure-atomic cleanup under every injected allocation failure until
  success.

## Verification

| Command | Exit | Result |
|---|---:|---|
| Debug build and `ctest --preset debug --output-on-failure` | 0 | warnings-as-errors C17 build; 1/1 test executable passed |
| Release build and `ctest --preset release --output-on-failure` | 0 | optimized build; 1/1 passed |
| ASan build and `ctest --preset asan --output-on-failure` | 0 | 1/1 passed without sanitizer report |
| ASan+UBSan build and `ctest --preset asan-ubsan --output-on-failure` | 0 | 1/1 passed without sanitizer report |
| TSan build and `ctest --preset tsan --output-on-failure` | 0 | 1/1 passed without sanitizer report |
| `clang --analyze` on MIR, checker, and their C-unit sources | 0 | zero findings |
| `./tools/check-generated` | 0 | diagnostic, Unicode, and syntax artifacts are current |
| Doc-link, spec-term, example-syntax, and syntax-coverage checks | 0 | D001–D035 consistent; 76/76 syntax kinds and 117/117 grammar productions covered |

## Deliberate boundary and remaining I2 work

The complete specification also admits a block arm whose every path returns,
panics, diverges, or otherwise has type `never`. This iteration accepts mixed
`return`/`yield` paths but deliberately leaves return-only arms, panic, general
`never` typing, and all-`never` match result inference behind the explicit
incomplete-language boundary. Those paths require the common bottom-type CFG
model rather than a `yield`-specific exception.

Generic/prelude enums, constant construction/matching, imported and
package-qualified resolution/visibility, nested payload/record/tuple/string and
other literal patterns, nested/generic witnesses, derives,
earlier-covering-pattern related locations, hardened serialized-MIR tag
dominance, and VM/native differential coverage also remain open. This
iteration changes no ABI/runtime policy and, under D034, adds no authorship,
agent-trust, CI, sandbox, or release-policy semantics to Ruda.
