# I2 compiler-service queries — preview checkpoint 3 — 2026-08-17

Gate: internal compiler-service occurrence, navigation, search, and exact
dot-completion foundation. Checkpoint status: locally complete; I2 remains
`in-progress`.

Baseline: `c7da6f0a93f139fde62c4123fff832260e4cd0be`
(`test: compare import-form typed MIR directly`).

Authority: toolchain §6.1; language §3, §5.8, and §6.2; accepted D003, D034,
and D043; the I2 roadmap editor gate; and the compiler architecture ownership
contract. This checkpoint adds an internal service API. It does not enable LSP
transport or public `ruda check`.

## Delivered foundation

- Diagnostic-free checks publish bounded reference occurrences into the same
  result-owned stable symbol universe as definition facts. The implemented
  references cover top-level nominal/alias/interface type uses, constants, and
  resolved direct, free, dot, and qualified calls. Facts are canonicalized by
  exact path/span/symbol and duplicate publications are removed.
- Allocation-free, result-scoped queries provide occurrence lookup,
  definition-from-reference, exact byte-prefix package-symbol search, and
  dot-completion lookup. Definition and completion results expose stable
  `RudaSymbolId` identities; result-local `RudaTypeId` values never cross this
  boundary.
- `resolution.c` publishes completion from the live checker declaration and
  imported-package indexes after signature collection. Every candidate is
  revalidated by the same D003/D043 call resolver used by type checking. The
  compiler service does not infer candidates by filtering
  `checked.functions`.
- Each completion owns the function name, owning package, stable function and
  receiver symbols, source snapshot, and equivalent currently available free
  spelling. Named aliases, namespace qualification, type-only imports, private
  functions, and wrong-receiver overloads retain their exact accepted D043
  behavior.
- The compiler service can check one exact caller-supplied local package graph
  over all current documents. Every document must occur exactly once. Query
  wrappers reject an old result generation and also reject a snapshot that is
  absent from an otherwise current result.
- Occurrence and completion counts have explicit checker limits. Existing
  request cancellation is preserved around the bounded graph-check phase, and
  every new owned byte path participates in transactional teardown.

The completion-equivalence fixture exposed an existing D043 defect in the
second phase of overloaded named-alias free-call resolution. The checker now
retains the named binding's original package function during receiver-based
overload selection. The accepted language contract did not change. The
existing import-form MIR/VM/native differential fixture now uses that aliased
overloaded spelling and still produces the same canonical program and value as
type-only dot and namespace forms.

## Verification coverage

- A four-document, two-package matrix checks named type-only, named
  type-plus-aliased-function, and namespace imports. Each source snapshot gets
  exactly the public owning-package `User` candidates, never the private
  function or the `Order` overload. Owning package and free spelling are exact.
- Every advertised completion symbol equals the definition reached from its
  accepted dot call. Available named-alias and namespace free calls reach that
  same symbol. Reverse package/file order produces identical occurrence and
  completion facts.
- Overlay coverage distinguishes import invalidation, private dependency
  changes, and a public-to-private dependency change. Old raw results remain
  readable while service-scoped queries reject stale results and stale
  snapshots. A diagnostic result publishes no partial definitions,
  occurrences, or completions.
- Graph validation rejects duplicate and missing document assignments.
  Cancellation before the graph check publishes no result. Exact occurrence
  and completion ceilings return `LIMIT_REACHED` without partial publication.
- The full imported completion graph is swept through every deterministic
  allocation-failure index until first success. Each failure and success path
  asserts `live_allocations == 0` and `live_bytes == 0` after complete
  teardown.
- The existing MIR/encoded-VM/generated-C17 import differential now covers an
  overloaded named alias, so the checker fix is exercised through both engines
  rather than only by a service assertion.

## Architecture and scope boundary

`facts.c` owns occurrence storage, canonicalization, and cross-fact
validation. `queries.c` owns immutable result-scoped access. `resolution.c`
owns both call selection and completion publication. `service.c` owns snapshot
lifetimes and package-graph orchestration; `service_queries.c` owns stale
generation/snapshot rejection. The focused test owner is
`test_compiler_service_queries.c`. All owners remain below their architecture
ceilings and the exception registry remains empty.

This checkpoint intentionally does not publish definitions for local bindings,
record fields, or enum variants because those declarations are not yet members
of the stable definition-fact universe. Generic instantiations and generic
completion belong to checkpoint 4. Persistent semantic dependency caching,
effect invalidation, incomplete-source facts, production workspace indexing,
LSP transport, and public `ruda check` remain later gates. The current service
uses a deterministic full semantic recheck, so public/private/import changes
cannot reuse stale semantic facts.

## Local validation

Environment:

- macOS Darwin 25.3.0, arm64;
- Apple Clang 17.0.0;
- CMake 4.4.2.

Commands and results:

| Command | Exit | Result |
|---|---:|---|
| `./tools/check-architecture` | 0 | owner/header/dependency/registration policy clean; largest new production owner 1,054 lines and focused test owner 839 lines |
| `./tools/check-c-memory-policy` | 0 | no production heap call escaped the central allocator |
| `./tools/check-generated` | 0 | bytecode, diagnostic, Unicode, and syntax artifacts current |
| `./tools/check-doc-links` | 0 | documentation links valid |
| `./tools/check-spec-terms` | 0 | normative terminology valid |
| `./tools/check-example-syntax` | 0 | public examples accepted |
| `./tools/check-syntax-coverage` | 0 | 74 syntax kinds and 115 grammar productions covered, 0 open |
| `git diff --check` | 0 | patch whitespace clean |
| debug configure/build/full CTest | 0 | 1/1 passed in 55.91 s, including query failure sweep and import MIR/VM/native differential |
| release configure/build/full CTest | 0 | 1/1 passed in 50.80 s |
| ASan+UBSan configure/build/full CTest with `halt_on_error=1` | 0 | 1/1 passed in 74.56 s without sanitizer report; Apple leak detection disabled, allocator zero-live assertions remained active |

No performance benchmark moved in this internal API checkpoint. Query calls
perform no allocation; their current scans are explicitly bounded by the
published symbol, occurrence, and completion limits. Package-scale indexing
and latency budgets remain an I2/I8 gate rather than an unmeasured claim here.
