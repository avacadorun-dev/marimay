# I2 finite-boolean match — iteration 21 — 2026-08-11

Iteration status: complete; gate status is `in-progress`.  
Scope: add the finite `bool` match domain to the checked source-to-MIR oracle,
including guards, catch bindings, and top-level OR patterns, without admitting
literal patterns for other source types.

## Existing normative contract implemented

- A match over `bool` is exhaustive only when both `false` and `true` are
  covered, or an unguarded wildcard/binding catches the remainder.
- Literal patterns `false` and `true` select their exact values. A bare name is
  a new immutable binding with type `bool`; `_` discards the value.
- Guards run only after their pattern and bindings succeed, may fall through to
  the next arm, and do not contribute to exhaustiveness.
- Top-level OR alternatives use the same exact binding-set and redundancy rules
  as the enum slice. The scrutinee and the selected arm body are evaluated once.
- This implements the finite-boolean requirement from D005 and language
  specification section 9. It adds no syntax, semantic decision, or backend
  contract.

## Implemented

- Match dispatch now recognizes a checked `bool` scrutinee before nominal-enum
  resolution and routes it to a dedicated two-witness analysis.
- Analysis accepts only `false`, `true`, wildcard, and new-binding patterns plus
  their top-level OR combinations. Integer/string and nested literal-pattern
  work remains an explicit incomplete-language boundary.
- Guard-aware coverage records the two boolean witnesses only for unguarded
  alternatives. Duplicate literals, arms after a catch-all, and catch-all
  subsumption inside an OR pattern use `T0014 REDUNDANT_PATTERN`; missing values
  use `T0013 NON_EXHAUSTIVE_MATCH`.
- Lowering uses existing typed `CONSTANT_BOOL`, `EQUAL_BOOL`, and branch MIR.
  Successful alternatives converge before one guard/body block, and a catch
  binding copies the original scrutinee into one arm-local `bool` register.
- A final proven remaining literal may take the fallthrough edge directly; the
  accepted source coverage proof and MIR verifier still guarantee a complete
  terminating CFG.
- Scrutinee calls are lowered once before dispatch. Guard calls are emitted only
  on the selected literal path.

## Coverage added

- Both literal orders return the expected values for `false` and `true`; every
  generated boolean-match MIR function passes the verifier.
- A catch binding returns the matched boolean. A guarded catch binding exposes
  that value to its guard and falls through correctly when it is false.
- `true | false` proves complete OR coverage. A separate scrutinee-call fixture
  contains exactly one MIR call instruction and returns the correct arm value.
- A division-by-zero guard is skipped for `false` and executed for `true`,
  proving selected-path guard evaluation.
- Exact-span negative fixtures cover one missing witness, guarded-only missing
  coverage, duplicate literals across arms and inside OR, catch-all
  subsumption, a non-boolean guard, and mismatched arm result types.
- An integer pattern on a boolean scrutinee remains explicitly
  `INCOMPLETE_LANGUAGE`, so this slice does not accidentally claim general
  literal-pattern support.
- Single- and multi-file allocation-failure loops include finite-boolean match
  analysis/lowering and release all partial arm, alternative, and CFG state.

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

This is the complete finite `bool` domain for the current expression-arm
oracle, not general literal matching. Integer/string literals, nested patterns,
OR patterns inside payload patterns, record/tuple patterns, block arms and
`yield`, `never` paths, and concrete uncovered-witness rendering remain open.

Generic/prelude enums, constant matching/construction, imported and
package-qualified resolution/visibility, derives, and VM/native differential
coverage also remain open. The enum verifier/runtime boundary from iterations
17–20 is unchanged. This iteration freezes no ABI, backend representation, or
runtime policy and, in accordance with D034, adds no authorship, agent-trust,
CI, sandbox, or release-policy semantics to Ruda.
