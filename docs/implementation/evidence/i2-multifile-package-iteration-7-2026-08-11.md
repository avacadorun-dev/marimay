# I2 multi-file package checking — iteration 7 — 2026-08-11

Iteration status: complete; gate status is `in-progress`.  
Scope: make staged signature collection and the typed MIR oracle operate over
an ordered set of package source files without claiming import or visibility
semantics that are not implemented yet.

## Implemented

- The checker exposes `ruda_check_package` for a non-empty borrowed set of ASTs;
  the existing `ruda_check_ast` API is a one-source convenience wrapper.
- Package sources are ordered by exact source-path bytes before checking.
  Function order, diagnostic phase order, and path-derived stable symbol IDs are
  therefore independent of caller input order. Duplicate source paths are
  rejected as an invalid package input rather than checked twice.
- A bounded `max_source_files` option prevents an untrusted input list from
  creating an unbounded pointer-index allocation before semantic checking.
- Top-level names and exact function signatures are collected across every
  ordered source before any function body is resolved. Direct forward calls
  between files execute through the same verified MIR program oracle as
  same-file calls.
- The current flat function namespace has a bounded exact-byte name index.
  Signature duplicate detection and body call resolution no longer scan the
  declaration vector; map entries store stable vector indices and are rechecked
  against the declaration before use. This is deliberately a single-symbol
  index, not the future overload/import resolver.
- Every declaration and source-backed name retains its borrowed source
  snapshot. Cross-file name comparison reads the corresponding immutable
  source bytes, and every semantic diagnostic identifies the exact snapshot
  that owns its byte span. `RudaCheckResult.snapshot` remains a borrowed
  compatibility field and identifies the first source in canonical path order.
- Body lowering restores the declaration's source context before inspecting
  types, names, spans, or emitting diagnostics. A semantic error anywhere in
  the package still clears the entire checked-function set and exact symbol
  index.
- Temporary source ordering and every package-check result remain
  allocation-failure atomic. No AST or source snapshot ownership moved into the
  checker.

## Coverage added

- A function in `a_entry.ruda` calls a function in `z_helper.ruda`; the checked
  program oracle executes the cross-file call and returns the expected value.
- Reversing the input AST array preserves canonical function order and both
  128-bit symbol IDs.
- A duplicate top-level name across two files produces one stable diagnostic
  owned by the later canonical source, with no partial MIR functions or symbol
  index entries.
- Duplicate source paths are rejected, and a two-source package exceeding
  `max_source_files` returns `RUDA_CHECK_LIMIT_REACHED` without an initialized
  result.
- The diagnostic helper now verifies snapshot identity for every existing
  single-source semantic diagnostic.
- A dedicated two-file allocator-failure walk covers source ordering,
  cross-source signatures, cross-file call lowering, and cleanup. Every failed
  and successful attempt ends with zero live allocations and bytes.
- A 257-function fixture forces repeated declaration-index growth, resolves a
  forward call to the last function, and detects a duplicate inserted after the
  growth sequence.

## Verification

| Command | Exit | Result |
|---|---:|---|
| Debug build and `ctest --preset debug --output-on-failure` | 0 | warnings-as-errors C17 build; 1/1 test executable passed |
| Release build and `ctest --preset release --output-on-failure` | 0 | optimized build; 1/1 passed |
| ASan build and `ctest --preset asan --output-on-failure` | 0 | 1/1 passed without sanitizer report |
| ASan+UBSan build and `ctest --preset asan-ubsan --output-on-failure` | 0 | 1/1 passed without sanitizer report |
| TSan build and `ctest --preset tsan --output-on-failure` | 0 | 1/1 passed without sanitizer report |
| `clang --analyze` on checker, MIR, type-table, and checker-test sources | 0 | zero static-analyzer findings |
| `./tools/check-generated` | 0 | committed generated artifacts are current |
| Doc-link, spec-term, example-syntax, and syntax-coverage checks | 0 | all checks green; 76/76 syntax kinds and 117/117 grammar productions covered |

## Remaining I2 gate work

This iteration establishes a deterministic multi-file compilation boundary; it
does not implement Ruda's package semantics. The current namespace is a flat
set of exact unqualified function names. Imports, visibility, qualified lookup,
overload-set-aware first-parameter dot calls, constants, nominal types,
generics, interfaces, closures, effects, and incremental dependency
invalidation remain open. The exact single-symbol name index must evolve into
the authoritative package/symbol and overload-set index used by the minimum
compiler service.

The checker still accepts only the documented pure-primitive subset and keeps
`RUDA_CHECK_INCOMPLETE_LANGUAGE` for the remaining baseline constructs. The
public `ruda check` command remains unavailable until the complete I2 source
service exists. No source syntax, language semantics, package format, ABI, VM,
native behavior, or trust-policy construct changed in this iteration.
