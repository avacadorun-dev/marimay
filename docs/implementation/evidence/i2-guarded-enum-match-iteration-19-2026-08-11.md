# I2 guarded enum match — iteration 19 — 2026-08-11

Iteration status: complete; gate status is `in-progress`.  
Scope: extend the iteration-18 non-generic enum-match oracle with guarded
simple patterns, guard-aware coverage, and explicit false-edge continuation,
without claiming the complete D005 pattern language.

## Existing normative contract implemented

- A guard is evaluated only after its pattern matches. Pattern payload
  bindings are therefore available to the guard and the selected arm body.
- A false guard continues matching at the next arm; it does not select the arm
  and does not re-evaluate the scrutinee.
- Guards do not contribute to exhaustiveness because they may be false. An
  unguarded variant or catch-all remains necessary to close the corresponding
  finite space.
- A guard has exact type `bool`. Guard evaluation preserves the language's
  existing checked arithmetic and selected-path execution semantics.
- This implements the guard part of D005 and language specification section 9
  over the strict enum-pattern subset from iteration 18. It adds no new source
  semantics or design decision.

## Implemented

- Match-arm analysis records the optional guard separately from the arm body.
  For a guarded expression arm the first direct expression is the guard and
  the second is the body; block arms remain an explicit incomplete-language
  boundary.
- Coverage records only unguarded arms. Repeated guarded arms for one variant
  are admitted until an unguarded arm covers that variant, while an arm after
  an earlier unguarded variant or catch-all is still rejected as redundant.
- Lowering tests or dynamically requires the pattern first, projects and binds
  payloads inside that checked path, then evaluates the guard. A true edge
  enters the body; a false edge joins the same next-arm block as a failed
  variant test.
- Guard and body share the arm-local lexical bindings. Those bindings do not
  escape to later arms, and the scrutinee remains evaluated exactly once.
- Guard expressions are checked without imposing a contextual type on their
  literals and are then required to have exact `bool` type. This preserves the
  ordinary expression diagnostic rather than misclassifying a non-boolean
  guard as unsupported syntax.
- Existing stable diagnostics cover this slice: `T0013 NON_EXHAUSTIVE_MATCH`
  for guarded-only coverage, `T0014 REDUNDANT_PATTERN` after an unguarded
  covering arm, and `T0002 TYPE_MISMATCH` for a non-boolean guard.

## Coverage added

- Unit, positional, and named enum arms use guards that read their pattern
  bindings and either select the guarded arm or continue to an unguarded
  fallback for the same variant.
- A guarded catch binding falls through to a later wildcard and proves that a
  guarded catch-all does not close exhaustiveness.
- A division-by-zero guard is attached to one variant. A different input
  variant skips that guard, while the matching variant executes it and reports
  the checked arithmetic failure.
- Negative fixtures cover a match whose only arm is guarded, a non-boolean
  guard, and a guarded arm made unreachable by an earlier unguarded arm, with
  exact diagnostic spans.
- Single- and multi-file allocator-failure loops now include guarded enum
  matches and release all partial analysis/lowering state.

## Verification

| Command | Exit | Result |
|---|---:|---|
| Debug build and `ctest --preset debug --output-on-failure` | 0 | warnings-as-errors C17 build; 1/1 test executable passed |
| Release build and `ctest --preset release --output-on-failure` | 0 | optimized build; 1/1 passed |
| ASan build and `ctest --preset asan --output-on-failure` | 0 | 1/1 passed without sanitizer report |
| ASan+UBSan build and `ctest --preset asan-ubsan --output-on-failure` | 0 | 1/1 passed without sanitizer report |
| TSan build and `ctest --preset tsan --output-on-failure` | 0 | 1/1 passed without sanitizer report |
| `clang --analyze` on MIR, checker, and their C-unit sources | 0 | zero findings |
| `./tools/check-generated` | 0 | all committed generated artifacts are current |
| Doc-link, spec-term, example-syntax, and syntax-coverage checks | 0 | D001–D035 consistent; 76/76 syntax kinds and 117/117 grammar productions covered |

## Deliberate boundary and remaining I2 work

This remains a deliberately narrow checked oracle, not the complete D005
pattern checker. OR patterns, nested patterns, literal/record/tuple patterns,
block arms and `yield`, `never` paths, finite-boolean matching, and concrete
uncovered-witness rendering remain open.

Generic/prelude enums, constant matching/construction, imported and
package-qualified resolution/visibility, derives, and VM/native differential
coverage also remain open. The verifier and runtime dynamically check each
payload projection; hardened acceptance of untrusted serialized MIR still
needs an explicit dominance/refinement proof for the checked tag edge. This
iteration does not freeze a tag ABI, payload layout, allocation strategy, GC
policy, or backend representation. In accordance with D034, it adds no
authorship, agent-trust, CI, sandbox, or release-policy semantics to Ruda.
