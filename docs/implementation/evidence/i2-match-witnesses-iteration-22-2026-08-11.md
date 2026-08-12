# I2 match witnesses — iteration 22 — 2026-08-11

Iteration status: complete; gate status is `in-progress`.  
Scope: render concrete, bounded missing-pattern witnesses for the checked simple
enum and finite-boolean match domains from iterations 18–21.

## Existing normative contract implemented

- A non-exhaustive diagnostic names concrete missing witness patterns rather
  than only saying that coverage is incomplete.
- Large missing sets use a bounded deterministic representative prefix and an
  exact remaining count/ellipsis instead of dumping the complete space.
- When a guarded pattern matches a missing witness, the diagnostic explicitly
  states that guarded patterns do not contribute to exhaustiveness.
- Diagnostics retain the match-expression source span and stable
  `T0013 NON_EXHAUSTIVE_MATCH` code. Witness rendering changes no source or MIR
  semantics.
- This implements the applicable part of diagnostics specification section 6
  for the current D005 subset; nested/generic pattern-space reporting remains a
  later I2 gate.

## Implemented

- `RudaCheckDiagnostic` can now optionally own a zero-terminated message while
  preserving the existing `const char *message` consumer API. Static messages
  allocate nothing; generated messages are disposed with the check result.
- Enum witnesses follow declaration order and distinguish unit, positional,
  and named-payload syntax: `Quit`, `Write(_, _)`, and
  `Move { x: _, y: _ }`.
- Rendering is bounded to four missing variants, four payload positions/fields,
  and 64 bytes per identifier. Larger sets append exact `+N more`; larger
  payload shapes use an ellipsis rather than unbounded output.
- Boolean diagnostics name `false`, `true`, or both in domain order. Empty
  boolean and enum matches use the same witness path as partial matches.
- The guard note is relevance-aware: it appears only when a guarded alternative
  matches one of the still-missing witnesses, not merely because some unrelated
  arm has a guard.
- Declaration names are read from their own source snapshots. A function whose
  alias and match live in one package file correctly renders a missing variant
  declared in another file.

## Coverage added

- Exact message tests cover unit, positional, named-payload, multi-witness, and
  `+N more` rendering in declaration order.
- A five-item positional payload proves bounded payload rendering; empty enum
  and boolean matches list their entire current finite witness set.
- Guarded missing cases for enum and boolean include the non-covering note.
  Separate fixtures prove that an unrelated or subsequently unguarded guard
  does not add that note.
- A multi-file type-alias fixture omits `Peer(Node)` and renders `Peer(_)` from
  the declaration snapshot.
- A fail-allocator sweep exercises every owned-message construction/push and
  disposal point, accepts only `ALLOCATION_FAILED` before eventual success,
  and observes zero live allocations/bytes after every attempt.

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

Witnesses currently describe only the complete simple enum and boolean domains
implemented by the oracle. Nested, literal, record, tuple, generic, and imported
pattern spaces still need their own usefulness/witness representation. T0014
still identifies the redundant pattern span but does not yet attach the earlier
covering-pattern location required by the complete diagnostic contract.

Block arms and `yield`, `never` paths, generic/prelude enums, constant
matching/construction, imported and package-qualified resolution/visibility,
derives, hardened serialized-MIR tag dominance, and VM/native differential
coverage remain open. This tooling-facing diagnostic ownership does not change
language meaning and, in accordance with D034, adds no authorship, agent-trust,
CI, sandbox, or release-policy semantics to Ruda.
