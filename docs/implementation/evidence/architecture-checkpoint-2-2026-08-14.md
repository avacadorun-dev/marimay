# Stage 0 architecture checkpoint 2 — 2026-08-14

Gate impact: no gate state changed. This is a behavior-neutral test-owner
extraction supporting the active managed-array I2–I5 vertical work.

Baseline: `570a1f8` (`ci: enforce bounded memory safety gates`).
Environment: macOS arm64, AppleClang 17.0.0, CMake 4.4.2.

## Delivered

- Moved the complete managed-array MIR oracle suite from `test_mir.c` to the
  new CMake-owned `test_mir_arrays.c` translation unit.
- Kept `ruda_test_mir` as the only externally registered MIR suite and retained
  the array suite's assertion order through one explicit entrypoint.
- Kept array RHS and managed-call callback fixtures private to their owner.
  Common scalar helper shapes are duplicated locally rather than introducing a
  premature cross-suite support API.
- Reduced `test_mir.c` from 4,612 to 3,396 lines. The new focused owner is
  1,251 lines and is the natural test home for the next bytecode/VM/native
  array vertical slice.

No source syntax, specification, public C header, MIR representation,
diagnostic, runtime behavior, bytecode, native emission, or CLI behavior
changed in this extraction.

## Verification

| Command | Exit | Result |
|---|---:|---|
| `cmake --preset debug && cmake --build --preset debug --parallel 4` | 0 | warnings-as-errors build including both MIR test owners |
| `ctest --preset debug --output-on-failure` | 0 | 1/1 passed in 14.17 s |
| `cmake --preset release && cmake --build --preset release --parallel 4 && ctest --preset release --output-on-failure` | 0 | 1/1 passed in 8.78 s |
| `cmake --preset asan-ubsan && cmake --build --preset asan-ubsan --parallel 4 && ctest --preset asan-ubsan --output-on-failure` | 0 | 1/1 passed in 15.06 s without report |
| `./tools/check-c-memory-policy` | 0 | production heap boundary clean |
| `./tools/check-generated` | 0 | generated artifacts current |
| `./tools/check-doc-links && ./tools/check-spec-terms && ./tools/check-example-syntax && ./tools/check-syntax-coverage` | 0 | documentation and syntax checks passed |
| `git diff --check` | 0 | patch whitespace clean |

An independent mechanical review found no assertion loss, duplicate
registration, linkage conflict, or semantic change before commit. I2, I3, I4,
and I5 remain `in-progress`.
