# I2 checked enum-match oracle — iteration 18 — 2026-08-11

Iteration status: complete; gate status is `in-progress`.  
Scope: add checked enum tag/payload operations and an executable exhaustive
`match` subset over the non-generic enum values introduced in iterations 15
and 17, without claiming the complete pattern language.

## Existing normative contract implemented

- `match` is an expression. Its scrutinee is evaluated once, only the selected
  arm is evaluated, and all admitted non-`never` arms have one exact result
  type.
- A closed-enum match must cover every variant unless an unguarded wildcard or
  binding catches the remainder. Duplicate variants and arms after a catch-all
  are unreachable.
- Unit, positional, and named enum patterns bind immutable payload values in an
  arm-local scope. `_` discards a value; a bare non-variant name is a catch-all
  binding.
- Payload access follows a checked tag edge. Safe source cannot inspect or
  forge the runtime integer tag.
- This implements a strict subset of D005 and language specification §9 using
  the execution-specification §2/§5 checked-enum requirements. It does not
  amend source semantics or add a design decision.

## Implemented

- Typed MIR now has `ENUM_IS_VARIANT`, `ENUM_REQUIRE_VARIANT`, and
  `ENUM_GET_PAYLOAD`. Verification checks exact nominal layout, variant and
  payload ranges, receiver/result types, and definite assignment.
- Execution validates the context-bound handle, compares the complete stored
  layout with the consuming function's layout, dynamically requires the
  selected variant, and checks payload bounds/type before reading. A forged
  handle, wrong selected variant, or inconsistent cross-function layout cannot
  reinterpret payload memory.
- Source lowering accepts expression arms with unguarded unit patterns,
  positional simple bindings/wildcards, complete named payload patterns with
  shorthand or renamed simple bindings, and wildcard/binding catch-alls.
- The scrutinee is lowered once. Variant tests form an explicit CFG; every arm
  begins with a dynamic variant requirement, payload bindings are projected
  only inside that checked arm, and all arm results copy into one typed join
  register.
- Coverage analysis rejects a missing variant and rejects duplicate or
  unreachable arms before CFG publication. Stable diagnostics are
  `T0013 NON_EXHAUSTIVE_MATCH` and `T0014 REDUNDANT_PATTERN`; existing exact
  type, variant, field, arity, and duplicate-name diagnostics cover malformed
  patterns.
- Arm bindings have lexical arm scope and cannot collide with one another.
  Outer bindings remain visible and may be shadowed by a new pattern binding.

## Coverage added

- One source fixture constructs and matches unit, positional, and named values;
  it exercises shorthand, renamed fields, discards, and a catch-all binding.
- Quit/Write/Move values take different CFG paths and yield `0`, a projected
  payload, or a sum of named payloads. Arms containing division by zero and
  checked overflow prove that only the selected arm executes.
- Missing execution contexts and forged enum handles are rejected before a
  checked match function runs.
- Exact-span negative tests cover non-exhaustiveness, duplicate variants, arms
  after wildcard, wrong unit/payload shapes, unknown variants, positional
  arity, duplicate bindings, mismatched arm result types, and
  unknown/duplicate/missing named fields.
- Same-package multi-file checking lowers a match whose enum and record payload
  are declared in another file, independent of input AST order.
- Malformed MIR covers wrong variants, out-of-range payloads, forged handles,
  and a second function that assigns a conflicting payload type to the same
  nominal enum.
- Dedicated limits stop match CFG block growth and arm-local binding growth;
  single- and multi-file allocator-failure loops include both enum construction
  and match lowering and release every partial allocation.

## Verification

| Command | Exit | Result |
|---|---:|---|
| Debug build and `ctest --preset debug --output-on-failure` | 0 | warnings-as-errors C17 build; 1/1 test executable passed |
| Release build and `ctest --preset release --output-on-failure` | 0 | optimized build; 1/1 passed |
| ASan build and `ctest --preset asan --output-on-failure` | 0 | 1/1 passed without sanitizer report |
| ASan+UBSan build and `ctest --preset asan-ubsan --output-on-failure` | 0 | 1/1 passed without sanitizer report |
| TSan build and `ctest --preset tsan --output-on-failure` | 0 | 1/1 passed without sanitizer report |
| `clang --analyze` on MIR, checker, and their C-unit sources | 0 | zero findings after making the zero-variant coverage precondition explicit |
| `./tools/check-generated` | 0 | diagnostic header/reference and all generated artifacts are current |
| Doc-link, spec-term, example-syntax, and syntax-coverage checks | 0 | D001–D035 consistent; 76/76 syntax kinds and 117/117 grammar productions covered |

## Deliberate boundary and remaining I2 work

This is not the complete D005 pattern checker. Guards, OR patterns, nested
patterns, literal/record/tuple patterns, block arms and `yield`, `never` paths,
finite-boolean matching, and uncovered-witness rendering remain open. Guards
currently remain an explicit `INCOMPLETE_LANGUAGE` boundary and therefore are
not incorrectly counted toward exhaustiveness.

Generic/prelude enums, constant matching/construction, imported and
package-qualified resolution/visibility, derives, and VM/native differential
coverage also remain open. The current verifier and runtime both check every
payload projection dynamically; a future hardened MIR verifier still needs an
explicit dominance/refinement proof for the checked tag edge before accepting
untrusted serialized MIR. The oracle does not freeze a tag ABI, payload layout,
allocation strategy, GC policy, or backend representation, and it contains no
authorship, agent-trust, CI, sandbox, or release-policy semantics.
