# I2 flat nominal record patterns — iteration 33 — 2026-08-12

Iteration status: complete; gate status remains `in-progress`.  
Scope: same-package flat record destructuring, exact nominal/field checking,
guard coverage, typed MIR field projections, and managed-reference oracle use.

## Normative basis

D004 makes records nominal, D007 gives them managed reference identity, and
D005 plus language section 9 require exhaustive patterns. LANG-030 records the
implemented contract: a record pattern resolves to the exact nominal
scrutinee type or its alias, lists every field exactly once, and a flat
binding/wildcard form is irrefutable unless guarded. This is implementation of
the accepted pattern model, not a new syntax or a representation change.

## Implemented

- The match dispatcher recognizes local record and alias scrutinees after the
  already implemented bool, integer, and string domains.
- `Point { x, y }`, reordered/renamed fields, `_` field discards, whole-record
  binding, empty records, guards, block arms with `yield`, and alias constructor
  spellings are checked and lowered.
- Every bound field emits the existing verified `RECORD_GET` operation with
  its declaration ordinal and exact field type. A whole-record catch binding
  copies the managed reference and therefore preserves identity.
- The checker rejects unknown, duplicate, and missing fields, duplicate local
  bindings, positional record patterns, non-record patterns, and structurally
  identical but nominally different record types.
- An unguarded flat record pattern covers the one-constructor domain and makes
  later alternatives/arms redundant. Guarded patterns do not cover; the
  missing witness is deterministic (`Point { x: _, y: _ }` or `Empty {}`).
- All-return/never and ordinary value-producing match paths reuse the common
  match body lowering without fabricating a value.

## Coverage

The checker fixture verifies all produced MIR functions, asserts exact
`RECORD_GET` counts for bound versus discarded fields, and executes values
through one shared checked execution arena. It observes reordered binding,
guard true/false paths, empty-record matching, block `yield`, whole-reference
identity, and rejection of a forged record reference.

Negative fixtures cover field completeness, unknown/duplicate fields,
duplicate bindings, exact nominality, redundant OR alternatives, guard-only
non-exhaustiveness, wrong positional shape, and alias-to-non-record misuse.
Limit fixtures exercise locals, record fields, and register budgets. The
allocation-failure sweep includes record-match collection/lowering, and a
dedicated witness allocation sweep asserts cleanup and exact diagnostics.
Parser and formatter grammar fixtures cover the public named-pattern form.

## Verification

| Command | Exit | Result |
|---|---:|---|
| Debug build and `ctest --preset debug --output-on-failure` | 0 | warnings-as-errors C17 build; 1/1 passed |
| Release build and `ctest --preset release --output-on-failure` | 0 | optimized build; 1/1 passed |
| ASan build and strict `ctest --preset asan` | 0 | 1/1 passed without sanitizer report |
| ASan+UBSan build and strict `ctest --preset asan-ubsan` | 0 | 1/1 passed without sanitizer report |
| TSan build and strict `ctest --preset tsan` | 0 | 1/1 passed without sanitizer report |
| `clang --analyze` on checker, MIR, and affected C-unit sources | 0 | zero findings |
| Generated, syntax-coverage, doc-link, spec-term, and example-syntax checks | 0 | generated artifacts and D001–D039/LANG-030 contracts consistent |

## Deliberate boundary

This slice does not implement nested/refutable record field patterns,
qualified imported record resolution, cross-package field visibility,
production GC/layout, derived equality, or native/VM representation. It reuses
the execution-owned managed-reference oracle arena from iteration 16; D007's
allocation/performance gate remains open. Production bytecode and generated-C
differential evidence remains owned by I3/I4 and is not inferred from the MIR
interpreter.
