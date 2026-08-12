# I2 return-only match arms — iteration 25 — 2026-08-11

Iteration status: complete; gate status is `in-progress`.  
Scope: extend the iteration-24 block-arm CFG so an individual arm may have no
`yield` when every path exits the enclosing function through `return`, while
keeping an all-`never` match behind the explicit incomplete boundary.

## Existing normative contract implemented

- Language section 9 permits every reachable block-arm path to `yield`,
  `return`, panic, or otherwise have type `never`.
- `return` exits the enclosing function; it does not write the match result
  register and does not jump to the match join.
- Only non-`never` arms participate in the exact match result-type constraint.
  A return-only arm therefore does not invent a type or force an unused value.
- This iteration implements the return-only subset of the accepted contract;
  it introduces no syntax, diagnostic, or runtime-policy change.

## Implemented

- A terminated block arm no longer requires a syntactic/compiled `yield` of its
  own. Existing CFG analysis still rejects any block with a continuing
  fallthrough path using `T0016 MISSING_MATCH_YIELD`.
- Return-only arms leave the shared match result register untouched. Other
  expression/yield arms infer or satisfy the exact result type and provide all
  predecessors that reach the join.
- The rule is independent of arm order: a return-only arm may precede type
  inference or follow an already allocated result target.
- Enum, boolean, and `i64` match lowerings now explicitly reject a completed
  match with no value-producing predecessor. This preserves the all-`never`
  boundary instead of exposing a valid-looking result with
  `RUDA_MIR_NO_REGISTER`.

## Coverage added

- Enum matching returns directly for a unit variant and yields a payload for
  the value variant.
- Boolean fixtures cover return-only arms both before and after a yielding arm.
- A return-only block containing `if/else` proves both terminating branches are
  accepted without a yield.
- A match used to initialize a local proves the return path exits the function
  while the value path reaches the join and the following consumer.
- Runtime interpretation checks every path and MIR verification checks all
  produced functions.
- The fail-allocator corpus now contains a return-only enum OR arm next to
  guarded yielding arms.
- A match whose every arm returns remains an asserted
  `RUDA_CHECK_INCOMPLETE_LANGUAGE` boundary until bottom-type result propagation
  is implemented.

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

All-arm `return`, panic/divergence, general `never` expression propagation, and
bottom-type compatibility in every expression consumer remain open. They need
one common unreachable-value model rather than a match-local dummy register.
Iteration 25 only removes the false requirement that each individual block arm
must contain a yield when another arm supplies the match value.

Generic/prelude enums, constant construction/matching, imported and
package-qualified resolution/visibility, nested payload/record/tuple/string and
other literal patterns, nested/generic witnesses, derives,
earlier-covering-pattern related locations, hardened serialized-MIR tag
dominance, and VM/native differential coverage also remain open. This
iteration changes no ABI/runtime policy and, under D034, adds no authorship,
agent-trust, CI, sandbox, or release-policy semantics to Ruda.
