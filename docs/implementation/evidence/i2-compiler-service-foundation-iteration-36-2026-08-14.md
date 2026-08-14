# I2 compiler-service foundation — iteration 36 — 2026-08-14

Gate: reusable compiler-service diagnostics and ownership foundation.
Iteration status: complete; I2 remains `in-progress`.

Baseline: `7e58f37` (`feat: lower source arrays through native matrix`).
Environment: macOS arm64, AppleClang 17.0.0, CMake 4.4.2.

Authority: the existing compiler-service and `ruda check` contract in
[toolchain §6.1](../../spec/toolchain.md), with editor coverage requirements in
the [test strategy](../test-strategy.md). This checkpoint implements an
internal owner boundary and changes no Ruda source semantics.

## Delivered checkpoint

- `RudaCompilerService` owns one source manager and a bounded set of documents.
  Initial source bytes and every overlay are retained as immutable snapshots;
  each overlay must carry a strictly increasing caller version, and a stale
  edit cannot change the current snapshot.
- Per-document limits are backed by checked aggregate caps on retained
  snapshot count and source bytes. Defaults cap the service at 4,096 retained
  snapshots and 256 MiB of source payload; adversarial overlays cannot multiply
  independent maxima into an effectively unbounded retained heap.
- One service check captures current documents, sorts them by exact path, and
  owns the complete lex, parse, AST, and package-check lifetime. Older results
  remain readable after later overlays because the service retains prior
  snapshots.
- Lexical and syntax source errors remain ordinary owned diagnostics with an
  overall `RUDA_COMPILER_OK`; they suppress AST/check publication so recovery
  state is not exposed as typed MIR. Type diagnostics come from the existing
  package checker rather than a second semantic pipeline.
- Resource, allocation, incomplete-language, and internal failures are kept
  distinct. Failure disposal is ordered check → AST → parse → lex, and the
  fail-at-every-allocation sweep proves both live allocation counters return
  to zero.
- Service initialization and check results support transactional reuse.
  Invalid options and allocation failure preserve the live owner; successful
  replacement disposes it before publication. Shared public phase-option
  validators keep service initialization aligned with lexer, parser, AST, and
  checker entry points.
- Focused tests cover canonical multi-file order, cross-file calls, versioned
  overlays, stale and unknown documents, retained old results, frontend and
  type diagnostics, duplicate/maximum document boundaries, and allocation
  failure through successful completion.

## Deliberate boundary and removal gates

- This is not the complete normative I2 compiler-service gate. Checker-owned
  go-to-definition/reference facts, symbol search, exact dot/free-call
  completion, cancellation, and invalidation are still missing.
- `ruda check` remains unavailable. Enabling a matrix-only partial command
  would contradict the roadmap and would create a second, weaker meaning of
  the public command.
- No package/module disk discovery or CLI target loading is included. The
  service accepts already selected versioned documents and has no ambient file
  or network access.
- I2 is not `verified`; full compile-pass/fail, completion equivalence,
  incremental invalidation, sanitizer, and Tier 1 gates remain open.

## Commands and exit status

| Command | Exit | Result |
|---|---:|---|
| `cmake --build build/debug --target ruda_c_unit -j 2` | 0 | warnings-as-errors build passed |
| `ctest --test-dir build/debug -R '^ruda_c_unit$' --output-on-failure` | 0 | full C suite passed, including compiler-service allocation sweep |
| Release build plus full CTest | 0 | optimized service and full suite passed |
| ASan+UBSan build plus full CTest with halt-on-error, Apple leak detection disabled | 0 | no sanitizer report; no global Apple leak claim |
| strict C17 and simulated `_WIN32` compile of service/test translation units | 0 | warnings-as-errors compile-only portability check |
| `./tools/check-doc-links && ./tools/check-spec-terms && ./tools/check-c-memory-policy` | 0 | docs and production heap policy passed |
| `git diff --check` | 0 | clean |

The next vertical checkpoint is to publish stable checker facts once and use
them for definition, symbol search, and completion. The compiler service must
not reconstruct semantic lookup from syntax, and public `ruda check` remains
closed until that minimum owner gate is verified.
