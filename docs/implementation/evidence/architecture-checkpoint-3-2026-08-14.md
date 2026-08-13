# Stage 0 architecture checkpoint 3 — 2026-08-14

Gate impact: no gate state changed. This is a behavior-neutral checker-result
ownership extraction.

Baseline: `b4c0375` (`refactor: split MIR array tests`).
Environment: macOS arm64, AppleClang 17.0.0, CMake 4.4.2.

## Delivered

- Added the owner-private `checker_internal.h` seam for published-result
  lifecycle and rollback helpers.
- Extracted checker option defaults, nested record/enum published-value
  cleanup, function/constant/type/diagnostic cleanup, function-index reset,
  public result disposal, and const accessors into `result.c`.
- Kept `ruda_check_package` in `checker.c` as the only package-phase
  coordinator and preserved every publication/rollback call site.
- Left the public `ruda/check/checker.h` API and all semantic typing/lowering
  code unchanged.

`checker.c` decreased from 14,559 to 14,299 lines; `result.c` is 263 lines.
This is the first production checker-owner boundary, not completion of checker
decomposition. Declaration, expression, pattern, and statement extraction
remain separately gated by explicit private contexts.

## Verification

| Command | Exit | Result |
|---|---:|---|
| `cmake --build --preset debug --parallel 4 && ctest --preset debug --output-on-failure` | 0 | warnings-as-errors build; 1/1 passed in 12.80 s |
| `cmake --preset release && cmake --build --preset release --parallel 4 && ctest --preset release --output-on-failure` | 0 | 1/1 passed in 11.27 s |
| `cmake --preset asan-ubsan && cmake --build --preset asan-ubsan --parallel 4 && ctest --preset asan-ubsan --output-on-failure` | 0 | 1/1 passed in 12.82 s without report |
| AppleClang static analyzer over `checker.c` and `result.c` with Debug include/configuration flags | 0 | no diagnostics |
| `./tools/check-c-memory-policy` | 0 | production heap boundary clean |
| `./tools/check-generated` | 0 | generated artifacts current |
| `./tools/check-doc-links && ./tools/check-spec-terms && ./tools/check-example-syntax && ./tools/check-syntax-coverage` | 0 | documentation and syntax checks passed |
| `git diff --check` | 0 | patch whitespace clean |

Independent comparison against the baseline found no missing `memset`, nested
cleanup, rollback call, API change, or duplicate symbol. I2 and all later gates
remain unchanged.
