# I2 enum OR patterns — iteration 20 — 2026-08-11

Iteration status: complete; gate status is `in-progress`.  
Scope: extend the checked non-generic enum-match oracle with top-level OR
patterns over the simple unit, positional, named-payload, wildcard, and binding
patterns admitted by iterations 18–19.

## Existing normative contract implemented

- An OR pattern denotes the union of its alternatives. Every alternative must
  bind the same names with identical resolved types; source order of those
  bindings is irrelevant.
- Exhaustiveness uses the variants covered by every unguarded alternative. A
  guard applies once to the complete OR pattern and still contributes no
  coverage because it may be false.
- Redundant alternatives are rejected. A duplicate variant adds no case, and a
  catch-all subsumes every other alternative in the same union.
- The scrutinee is evaluated once. A selected alternative establishes its
  checked tag edge and payload bindings before the shared guard and arm body.
- This implements the top-level OR-pattern rule from D005 and language
  specification section 9 over the existing strict enum-pattern subset. It
  does not amend source semantics or add a design decision.

## Implemented

- Match analysis flattens the parser's left-associated OR tree in source order
  with an explicit bounded vector rather than recursive C traversal.
- Each alternative is resolved and shape-checked independently. Its binding
  set is collected with exact resolved types; later alternatives compare by
  name rather than field/source order.
- Guard-aware usefulness rejects alternatives already covered by an earlier
  unguarded arm, duplicate variants within one OR pattern, alternatives after a
  catch-all, and alternatives subsumed by a later catch-all.
- Lowering emits a tag-test chain whose successful edges dynamically require
  the selected variant and project payloads. Every alternative assigns the
  same arm-local binding registers and converges at one selected-pattern block.
- The optional guard and arm body are lowered exactly once after that merge. A
  false guard continues at the next arm, never at another alternative of the
  already-matched OR pattern.
- Stable diagnostics remain sufficient for this slice: `T0002 TYPE_MISMATCH`
  reports unequal binding names/types, `T0014 REDUNDANT_PATTERN` reports
  redundant alternatives, and `T0013 NON_EXHAUSTIVE_MATCH` reports a
  guarded-only union.

## Coverage added

- A three-alternative unit union proves left-associated flattening and complete
  coverage; two-alternative positional and named unions exercise all admitted
  enum payload shapes.
- Positional alternatives bind one shared value. Named alternatives spell
  fields in different orders and rename them to the same two bindings, proving
  set-based rather than ordinal compatibility.
- A guarded positional union tests true selection, false fallthrough to an
  unguarded union, and binding visibility in the shared guard/body.
- A division-by-zero guard is lowered to exactly one MIR call instruction. Unit
  inputs skip it, while either payload alternative executes it, proving that
  guard/body lowering is shared without changing selected-path behavior.
- Exact-span negative fixtures cover unequal names, unequal types, a missing
  binding, duplicate variants, a variant covered by an earlier arm, catch-all
  subsumption in both source orders, and guarded-only non-exhaustiveness.
- The deterministic two-file package fixture matches two record-payload enum
  variants declared in the other source file with one shared binding.
- Single- and multi-file allocation-failure loops now contain OR patterns. TSan
  exposed an uninitialized-vector cleanup path when the arm array allocation
  succeeded and the following coverage allocation failed; zero-initializing the
  arm array immediately after allocation makes that path failure-atomic.

## Verification

| Command | Exit | Result |
|---|---:|---|
| Debug build and `ctest --preset debug --output-on-failure` | 0 | warnings-as-errors C17 build; 1/1 test executable passed |
| Release build and `ctest --preset release --output-on-failure` | 0 | optimized build; 1/1 passed |
| ASan build and `ctest --preset asan --output-on-failure` | 0 | 1/1 passed without sanitizer report |
| ASan+UBSan build and `ctest --preset asan-ubsan --output-on-failure` | 0 | 1/1 passed without sanitizer report |
| TSan build and `ctest --preset tsan --output-on-failure` | 0 | 1/1 passed after the allocation-failure cleanup fix, without sanitizer report |
| `clang --analyze` on MIR, checker, and their C-unit sources | 0 | zero findings after explicit enum-coverage bounds/null validation |
| `./tools/check-generated` | 0 | all committed generated artifacts are current |
| Doc-link, spec-term, example-syntax, and syntax-coverage checks | 0 | D001–D035 consistent; 76/76 syntax kinds and 117/117 grammar productions covered |

## Deliberate boundary and remaining I2 work

This iteration admits only top-level unions of the already checked simple enum
patterns. Nested patterns, including OR patterns inside payload patterns,
literal/record/tuple patterns, block arms and `yield`, `never` paths,
finite-boolean matching, and concrete uncovered-witness rendering remain open.

Generic/prelude enums, constant matching/construction, imported and
package-qualified resolution/visibility, derives, and VM/native differential
coverage also remain open. The verifier and runtime dynamically check each
payload projection; hardened acceptance of untrusted serialized MIR still
needs an explicit dominance/refinement proof for the checked tag edge. This
iteration does not freeze a tag ABI, payload layout, allocation strategy, GC
policy, or backend representation. In accordance with D034, it adds no
authorship, agent-trust, CI, sandbox, or release-policy semantics to Ruda.
