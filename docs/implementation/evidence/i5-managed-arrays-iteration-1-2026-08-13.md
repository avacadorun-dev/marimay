# I5 managed arrays — iteration 1 — 2026-08-13

Gate: I5 managed data and standard core foundation.
Iteration status: complete; milestone gate remains `in-progress`.
Baseline: `dc1b1c5` (`feat: execute deterministic f64 semantics`).
Environment: macOS arm64, AppleClang 17.0.0, CMake 4.4.2.

## Delivered slice

- Accepted D045 fixes exact homogeneous `Array<T>` construction, `int`
  indexing, G4002 identity/span, mutation order, `length`, and failure-atomic
  `push` semantics before implementation chose observable behavior.
- The canonical type table now interns `Array<T>` for every valid element type,
  including nested arrays, with transactional allocation rollback.
- The C17 runtime provides a per-heap non-moving managed arena. Opaque
  provenance/slot/generation references reject foreign, stale, and reused-slot
  values without dereferencing untrusted host pointers.
- Immutable array descriptors distinguish exact scalar and reference layouts.
  Reference descriptors name their exact child array type, so the collector
  supports arbitrary nested and recursive array graphs rather than one runtime
  enum per source `T`.
- Exact shadow-root frames and temporary operation roots feed an iterative
  mark/sweep collector. Only initialized reference cells are traced; scalar
  payloads are never conservatively scanned. Cycles are collectible.
- Construction, indexed reads/writes, and growth use checked size arithmetic,
  configured object/byte/length ceilings, collection-before-limit retry, and
  failure-atomic publication. Live-object and live-byte inspection is bounded
  and deterministic.

This slice is runtime/type infrastructure. It does not yet lower source array
expressions or loops into MIR, serialize array bytecode, execute arrays in the
VM/native emitter, or complete the matrix-multiplication acceptance fixture.

## Verification coverage

- Tests pin cross-heap and stale generations, slot reuse, exact descriptor
  identity, nested reachability, recursive self-cycles, collection of cycles,
  and scalar cells whose bytes resemble a live reference.
- Root tests cover two-frame LIFO enforcement, wrong-heap rejection,
  invalid-root collection atomicity, linked-frame cleanup, and unrooted
  receiver/child preservation during reference-array growth.
- Bounds vectors include negative one and both `INT64` extremes, while failed
  writes and exact-type mismatches leave array contents unchanged.
- Object- and byte-limit fixtures prove an unreachable object is collected and
  its slot reused before reporting a limit. Exact live-byte accounting is
  stable across the retry.
- Fail-every-allocation sweeps cover empty and non-empty construction, slot
  growth, established cell-buffer growth, nested reference construction, and
  disposal with zero live allocations and bytes after every injected failure.

## Commands and exit status

| Command | Exit | Result |
|---|---:|---|
| `cmake --build build/debug --target ruda_c_unit -j2 && ctest --test-dir build/debug -R '^ruda_c_unit$' --output-on-failure` | 0 | Debug warnings-as-errors; 1/1 passed |
| `cmake --build build/release --target ruda_c_unit -j2 && ctest --test-dir build/release -R '^ruda_c_unit$' --output-on-failure` | 0 | optimized build; 1/1 passed |
| `cmake --build build/asan-ubsan --target ruda_c_unit -j2 && ctest --test-dir build/asan-ubsan -R '^ruda_c_unit$' --output-on-failure` | 0 | ASan+UBSan; 1/1 passed without report |
| `cmake --build build/tsan --target ruda_c_unit -j2 && ctest --test-dir build/tsan -R '^ruda_c_unit$' --output-on-failure` | 0 | TSan; 1/1 passed without report |
| strict C17 compilation of `managed.c` and `test_managed.c` with `-Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror` | 0 | warning-free |
| `clang --analyze` over managed runtime and canonical type interner | 0 | no diagnostics |
| `./tools/check-generated`; D0 doc/spec/example/syntax checks; `git diff --check` | 0 | generated artifacts and documentation consistent |

An initial independent audit found that a fixed two-shape runtime did not meet
D045's generic contract. The descriptor redesign removes that P1 gap and adds
the recursive-cycle and resource-retry coverage requested by the audit. This
checkpoint is not evidence for the complete I5 gate or Tier 1 platform matrix.
