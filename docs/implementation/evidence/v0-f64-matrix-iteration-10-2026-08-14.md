# V0 loop-based f64 matrix — iteration 10 — 2026-08-14

Gate: the internal typed-MIR to encoded-VM to generated-C17
engine-equivalence spine. Iteration status: complete; I2, I3, and I4 remain
`in-progress`.

Baseline: `b95c19b` (`feat: execute managed arrays in generated C17`).
Environment: macOS arm64, AppleClang 17.0.0, CMake 4.4.2.

Authority: accepted [D045](../../design/decisions.md), the normative
[language](../../spec/language.md), [execution](../../spec/execution.md), and
[diagnostic](../../spec/diagnostics.md) contracts. No source semantic changed.

## Delivered checkpoint

- A verified typed-MIR fixture constructs heap-managed matrices with the exact
  bounded backend layouts `Array<f64>` and `Array<Array<f64>>`.
- The fixture multiplies a 2-by-3 matrix by a 3-by-2 matrix using real `i`,
  `j`, and `k` CFG loops, indexed reads, an explicit D045 checked-index
  barrier, f64 multiplication/addition, and indexed mutation of the result.
- The expected result is `[[58, 64], [139, 154]]`. The fixture reads all four
  result cells and returns their deterministic f64 checksum, `415.0`, whose
  exact representation is `0x4079f00000000000`.
- The unoptimized MIR oracle, serialized-and-decoded verified bytecode VM, and
  generated C17 agree on those exact bits. VM execution and generated native
  children force managed collection before allocation, exercising precise
  roots while nested input and result arrays remain live.
- The generated child is compiled and executed at both `-O0` and `-O2` with
  strict floating-point flags and prints
  `ok:f64:4079f00000000000`.

## Deliberate boundary

- This is an internal engine checkpoint, not the final matrix acceptance
  program. It begins from hand-built typed MIR; source array construction,
  indexing, array types, and public command wiring are not delivered here.
- Returning a checksum proves that every expected result cell was observed and
  that all three engines agree, but the final source fixture must print the
  user-facing matrix representation and pass `fmt --check`, `check`, `run`,
  `test`, `build`, and `test --engine both` from a clean checkout.
- The two exact backend layouts remain a documented preview shortcut. This
  checkpoint does not claim general `Array<T>`, array growth, or iteration.
- Generated matrix C is not compiled or executed with MSVC by the current
  POSIX child harness. The `_WIN32` test translation unit is warning-clean,
  but no Tier 1 or I4 verification claim follows from that compile-only check.
- Existing differential bounds and arithmetic panic fixtures remain the
  semantic evidence for failure identity; this successful matrix fixture does
  not replace the final source-level failing acceptance cases.

## Commands and exit status

| Command | Exit | Result |
|---|---:|---|
| `cmake --build build/debug -j 2 && ctest --test-dir build/debug --output-on-failure` | 0 | Debug warnings-as-errors; 1/1 passed; matrix generated C ran at `-O0` and `-O2` |
| `cmake --build build/release -j 2 && ctest --test-dir build/release --output-on-failure` | 0 | optimized parent build; 1/1 passed |
| `cmake --build build/asan-ubsan -j 2 && ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir build/asan-ubsan --output-on-failure` | 0 | ASan+UBSan parent and generated children passed without report |
| `cmake --build build/tsan -j 2 && TSAN_OPTIONS=halt_on_error=1 ctest --test-dir build/tsan --output-on-failure` | 0 | TSan passed without report |
| `clang --analyze` over `test_native_matrix.c` | 0 | no diagnostics |
| strict C17 `_WIN32` compile of `test_native_matrix.c` | 0 | no warning; not generated-program MSVC evidence |
| `./tools/check-generated && ./tools/check-c-memory-policy` | 0 | generated registries and production heap policy clean |
| `./tools/check-doc-links && ./tools/check-spec-terms && ./tools/check-example-syntax` | 0 | documentation checks passed |
| `git diff --check` | 0 | clean |

The nearest product blocker is now source-level array type/expression/index
lowering and public CLI execution of this same computation. Until that path is
complete, the repository has engine evidence for the matrix kernel but does
not meet the clean-checkout user acceptance condition.
