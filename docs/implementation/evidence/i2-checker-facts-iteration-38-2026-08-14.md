# I2 checker definition facts — iteration 38 — 2026-08-14

Gate: bounded stable definition facts for successful non-generic packages.
Iteration status: complete; I2 remains `in-progress`.

Baseline: `14c887f` (`feat: add cancellable incremental compiler requests`).
Environment: macOS arm64, AppleClang 17.0.0, CMake 4.4.2.

Authority: the minimum editor-service contract in
[toolchain §6.1](../../spec/toolchain.md), accepted receiver ownership and
free/dot equivalence in [D003](../../design/decisions.md), and the checker owner
in the [compiler architecture](../compiler-architecture.md). This checkpoint
changes no Ruda source semantics and does not accept D003 overloads yet.

## Delivered checkpoint

- Every diagnostic-free supported package publishes one bounded, result-owned
  definition fact for each top-level function, constant, type alias, record,
  and enum. Facts contain kind, visibility, owned name, stable symbol identity,
  exact borrowed declaration-name span, and a result-local type.
- `facts.c` owns name copying, canonical path/span/kind/name/symbol ordering,
  duplicate-ID collision rejection, the by-ID index, and exact half-open
  declaration-name hits. The accessor is deliberately named
  `ruda_checked_symbol_defined_at`; it is not reference go-to-definition.
- Package names are copied under an explicit byte ceiling. Symbol publication
  is capped cumulatively before constant evaluation and body checking. A
  diagnostic-bearing result publishes no semantic facts.
- Stable symbol hashing has length-bearing entry points. Checker call sites
  hash the exact package, source-path, and declaration-name bytes, including a
  path suffix after an embedded NUL.
- A function whose first parameter resolves through an alias to an exact local
  nominal record/enum uses the full receiver symbol in its stable identity.
  Local nominal TypeId-to-symbol lookup is map-backed, avoiding a
  function-by-declaration quadratic scan at package limits.
- Compiler-service body-only overlays preserve symbol identity while facts
  point at the new immutable snapshot. Fact/type lifetimes remain attached to
  one `RudaCheckResult`; a `RudaTypeId` must not be compared across results.

## Verification coverage

- Reversing AST input order produces byte-identical canonical fact order and
  symbol IDs across the five top-level kinds, with exact visibility, type
  mapping, by-ID lookup, and declaration boundary hits.
- Paths that differ only after an embedded NUL produce different checker-owned
  symbols. Alias-to-local-nominal function identity is compared with the exact
  receiver-aware hash and the published MIR symbol; ordinary identity differs.
- Exact and over-limit symbol ceilings, pre-body limit precedence, package-name
  ceiling, semantic diagnostics, and fail-every-allocation publication assert
  transactional teardown with zero tracked live allocations and bytes. The
  failure sweep includes record and enum local-nominal index allocation.
- Incremental compiler-service coverage asserts a body-only edit preserves the
  function symbol, replaces the definition snapshot, and does not mix result
  generations.

## Deliberate boundary and removal gates

- These are definition facts only. Reference/occurrence facts, locals, fields,
  variants, definition-from-reference, package symbol search, and completion
  remain missing.
- The shared package-local non-generic free/dot-call resolver is the immediate
  next checkpoint. Imports, generics, D003 same-name receiver overload
  acceptance, exact cursor-scope free spellings, and incomplete-source facts
  remain later gates.
- No public compiler-service query, LSP transport, or `ruda check` command is
  enabled. This checkpoint is not I2 verification and changes no I3/I4 state.

## Commands and exit status

| Command | Exit | Result |
|---|---:|---|
| Debug build plus full CTest | 0 | definition, identity, incremental, limit, and failure tests passed |
| Release build plus full CTest | 0 | optimized suite passed |
| ASan+UBSan build plus full CTest with halt-on-error and Apple leak detection disabled | 0 | no sanitizer report; no global Apple leak claim |
| strict C17 and simulated `_WIN32` compile of changed production/test translation units | 0 | warnings-as-errors compile-only portability check |
| Clang static analysis of checker facts, result, and type translation units | 0 | no analyzer finding |
| generated/docs/spec/example/memory-policy checks | 0 | repository consistency gates passed |
| `git diff --check` | 0 | clean |
