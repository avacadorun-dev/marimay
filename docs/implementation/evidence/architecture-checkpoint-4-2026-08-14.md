# Architecture checkpoint 4 — checker sibling-module boundary — 2026-08-14

Gate: behavior-preserving Stage 0 compiler decomposition. Checkpoint status:
complete; no roadmap delivery gate changes state.

Baseline: `7cc9d76` (`test: prove loop-based f64 matrix equivalence`).
Environment: macOS arm64, AppleClang 17.0.0, CMake 4.4.2.

Authority: the implementation-only
[Stage 0 compiler architecture](../compiler-architecture.md). No language,
MIR, diagnostic, runtime, or CLI semantic changed.

## Change

- The owner-private `RudaChecker`, `RudaFunctionChecker`, expression value, and
  source-name contexts moved verbatim from `checker.c` to
  `checker_internal.h`.
- The minimal existing helpers required by future expression sibling modules
  now have owner-private external linkage and declarations in that header:
  checker failure/diagnostic reporting, expression compilation and never-state
  observation, register/instruction emission, and compound-assignment opcode
  selection.
- Public headers and every public function signature remain unchanged.
  `ruda_check_package` is still the sole package coordinator; the private
  header is included only inside the checker owner.
- This checkpoint deliberately adds no array or other source feature. It lands
  the internal ownership boundary separately so the next vertical slice can
  place array expression lowering in a focused sibling translation unit.

## Commands and exit status

| Command | Exit | Result |
|---|---:|---|
| `cmake --build build/debug -j 2 && ctest --test-dir build/debug --output-on-failure` | 0 | Debug warnings-as-errors; 1/1 passed |
| `cmake --build build/release -j 2 && ctest --test-dir build/release --output-on-failure` | 0 | Release; 1/1 passed |
| `cmake --build build/asan-ubsan -j 2 && ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir build/asan-ubsan --output-on-failure` | 0 | ASan+UBSan; 1/1 passed without report |
| `./tools/check-generated && ./tools/check-c-memory-policy` | 0 | generated artifacts and heap policy clean |
| `./tools/check-doc-links && ./tools/check-spec-terms && ./tools/check-example-syntax` | 0 | documentation checks passed |
| `git diff --check` | 0 | clean |

The next checkpoint uses this private boundary for source array typing and
lowering. That semantic work remains separate from this extraction.
