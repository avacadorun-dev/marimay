# I2 i64 literal match — iteration 23 — 2026-08-11

Iteration status: complete; gate status is `in-progress`.  
Scope: add top-level non-negative `int` literal patterns to the checked scalar
match oracle, with guards, OR patterns, catch bindings, and concrete missing
witnesses.

## Existing normative contract implemented

- An integer literal pattern matches exactly its value and must have the same
  type as the `int` scrutinee. A wildcard or bare binding catches the remaining
  infinite domain.
- A finite list of integer literals is never exhaustive. Only an unguarded
  wildcard/binding closes the domain; guarded catch patterns remain
  non-covering.
- Duplicate literals and catch-all-subsumed alternatives are redundant errors.
  Top-level OR alternatives preserve the existing exact binding-set rule.
- The scrutinee is evaluated once, guard/body execution is selected-path only,
  and all reachable arm results have one exact type.
- This implements the current parser's non-negative `int` literal-pattern
  subset of D005/language specification section 9 without extending the pattern
  grammar or changing arithmetic semantics.

## Implemented

- Match dispatch routes an `int` (`i64` in the current oracle) scrutinee to a
  dedicated scalar analysis before nominal-enum resolution.
- Literal magnitude uses the existing checked integer parser and rejects values
  above `INT64_MAX` with stable `T0007 INTEGER_LITERAL_RANGE`. A boolean literal
  against `int` produces `T0002 TYPE_MISMATCH`.
- Unguarded literal usefulness uses a seeded `RudaMap` keyed by the exact
  `int64_t` value, avoiding quadratic duplicate checks across arms. Same-OR
  redundancy remains source-local and deterministic.
- Lowering uses existing typed `CONSTANT_I64`, `EQUAL_I64`, and branch MIR.
  Successful alternatives converge before one guard/body block; a catch
  binding receives the original `i64` value.
- Non-exhaustive integer diagnostics choose the smallest uncovered
  non-negative literal, so `0`, `1`, `2` yield concrete witness `3`. A relevant
  guarded catch/literal adds the non-covering guard note.
- Empty integer matches use witness `0`; literal range, map, arm, CFG, and owned
  diagnostic allocations are failure-atomic.

## Coverage added

- Runtime fixtures distinguish `0`, the OR union `1 | 2`, and a catch binding;
  they also exercise maximum `INT64_MAX` matching.
- Guards fall through from the OR union to an unguarded union. A guarded catch
  binding is visible in its guard/body and negative runtime inputs reach the
  wildcard fallback.
- A division-by-zero guard is skipped for a different literal and executed for
  its selected literal. A scrutinee call appears exactly once in MIR.
- Exact-span negative fixtures cover duplicates across arms and within OR,
  arms after catch, catch subsumption, wrong literal type, out-of-range literal,
  non-boolean guard, and mismatched arm result type.
- Witness tests cover consecutive literals, guarded infinite-domain coverage,
  and the empty match.
- Single- and multi-file allocation-failure sweeps now contain valid integer
  literal matches and exercise map growth/disposal together with scalar CFG
  lowering.

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

This slice follows the current pattern grammar: negative literal spelling is
not admitted, and other integer widths, strings, nested literal patterns,
record/tuple patterns, and imported/generic pattern spaces remain open. Integer
matching is currently the `i64` oracle, not a claim that D035 or all numeric
backends are complete.

Block arms and `yield`, `never` paths, generic/prelude enums, constant
matching/construction, imported and package-qualified resolution/visibility,
derives, earlier-covering-pattern related locations, hardened serialized-MIR
tag dominance, and VM/native differential coverage also remain open. This
iteration changes no ABI/runtime policy and, under D034, adds no authorship,
agent-trust, CI, sandbox, or release-policy semantics to Ruda.
