# I2 compiler-service incremental requests — iteration 37 — 2026-08-14

Gate: request-local parser reuse and cooperative compiler-request cancellation.
Iteration status: complete; I2 remains `in-progress`.

Baseline: `b035621` (`feat: establish versioned compiler service`).
Environment: macOS arm64, AppleClang 17.0.0, CMake 4.4.2.

Authority: the existing compiler-service contract in
[toolchain §6.1](../../spec/toolchain.md), the I2 invalidation gate in the
[roadmap](../roadmap.md), and the immutable incremental-parser contract already
delivered by I1. This checkpoint changes no Ruda source semantics.

## Delivered checkpoint

- `ruda_compiler_service_overlay_edit` accepts a complete new immutable
  snapshot together with one exact base-version edit. Range, unchanged-prefix,
  and unchanged-suffix proofs are checked before mutation. An ordinary full
  overlay explicitly clears incremental metadata.
- `ruda_compiler_service_check_request` optionally consumes one prior result.
  It reuses parse storage only when file identity, base version, and previous
  snapshot identity all match. A skipped intermediate version deterministically
  falls back to a full parse.
- Reuse is request-local. There is no persistent semantic cache or hidden
  process-global state. Every successful request projects fresh ASTs and runs
  the whole package checker, so a declaration removal in one file invalidates
  a consumer in another file.
- Cancellation is cooperative and is polled at the request boundary and
  between bounded lex/parse/AST/check phases. A phase failure takes precedence
  over a cancellation observed only after that phase. Callbacks may not mutate
  the service or results and provide their own cross-thread synchronization.
- Result publication remains transactional when `previous == result` and when
  previous/output are distinct. Cancellation, allocation failure, invalid edit,
  and resource-limit paths leave the prior result and caller stats unchanged.
- Result currentness is scoped to exact service snapshots, not a numeric
  generation alone; two live services at the same generation cannot accept
  each other's result as current.

## Verification coverage

- Same-length and shifting edits prove top-level green-node identity reuse and
  exact interpreted values; insertions and deletions cover empty edit sides.
- Invalid base/range/prefix/suffix matrices, chained-edit fallback, and
  allocation/limit failure assert version, snapshot, edit metadata, aggregate
  counters, and generation remain unchanged.
- A two-file rename proves the unchanged consumer is fully rechecked, and a
  fresh full compilation produces byte-equivalent diagnostic identity, message,
  snapshot, and span.
- Cancellation is swept across phase boundaries after a real incremental edit;
  the old result remains runnable, stats remain byte-unchanged, and allocator
  live allocation/byte counters return to zero.
- Incremental allocation failure is swept through successful completion with
  the prior result preserved at every failed allocation index.

## Deliberate boundary and removal gates

- This is parser reuse plus full semantic recheck, not an incremental semantic
  dependency graph. The I2 public/private/effect invalidation matrix remains
  open, as do imports, visibility, overloads, and stable package/item IDs.
- Definition/reference facts, symbol search, and exact checker-owned completion
  remain missing. `ruda check` therefore remains unavailable; I2 is not
  `verified`.
- Cancellation latency is bounded only by the current phase limits. This is not
  a production LSP transport, concurrent service API, or task-runtime
  cancellation guarantee.

## Commands and exit status

| Command | Exit | Result |
|---|---:|---|
| Debug build plus full CTest | 0 | request reuse, cross-file recheck, cancellation, provenance, and failure tests passed |
| Release build plus full CTest | 0 | optimized suite passed |
| ASan+UBSan build plus full CTest with halt-on-error and Apple leak detection disabled | 0 | no sanitizer report; no global Apple leak claim |
| strict C17 and simulated `_WIN32` compile of service and incremental-test translation units | 0 | warnings-as-errors compile-only portability check |
| Clang static analysis of service and incremental-test translation units | 0 | no analyzer finding |
| generated/docs/spec/example/memory-policy checks | 0 | repository consistency gates passed |
| `git diff --check` | 0 | clean |

The next checkpoint is a package-local, non-generic checker-facts and shared
free/dot-call resolver foundation. It must publish stable symbol identity once
and serve both type/lowering and later completion queries; the compiler service
must not infer semantic candidates by rescanning syntax or checked outputs.
