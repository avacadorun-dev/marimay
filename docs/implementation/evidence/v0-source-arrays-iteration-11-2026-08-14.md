# V0 source managed arrays — iteration 11 — 2026-08-14

Gate: source-to-native engine-equivalence spine for the matrix-critical D045
subset. Iteration status: complete; I2, I3, and I4 remain `in-progress`.

Baseline: `ccfa291` (`refactor: expose checker expression boundary`).
Environment: macOS arm64, AppleClang 17.0.0, CMake 4.4.2.

Authority: accepted [D045](../../design/decisions.md), the normative
[language](../../spec/language.md), [execution](../../spec/execution.md),
[diagnostic](../../spec/diagnostics.md), and
[standard-library](../../spec/standard-library.md) contracts. This checkpoint
implements an already accepted subset; it does not amend source semantics.

## Delivered checkpoint

- The checker resolves exact recursive `Array<T>` types, infers homogeneous
  non-empty literals, accepts contextual empty literals, and lowers indexing,
  indexed `=`, indexed compound assignment, and unshadowed free/dot `length`
  calls to verified typed MIR.
- `bootstrap/src/check/array.c` owns the feature lowering behind the existing
  owner-private checker boundary instead of adding another expression domain
  to the already large coordinator. Array layouts are canonical per function,
  recursively seeded for parameters/results/registers, and bounded before
  allocation or child lowering with a second cumulative nested-literal check.
- Indexed writes preserve D045 order: receiver, exact `i64` index, full-span
  bounds preflight, then RHS and publication. Compound writes additionally
  read the prior element before the RHS. Invalid `values[3] = panic("rhs")`
  therefore produces exact `G4002`, not `G4000`, with canonical message,
  function symbol, and complete access span.
- The source matrix fixture constructs dynamic nested f64 arrays and computes
  a real 2-by-3 times 3-by-2 multiplication with three C-style loops. The
  checker/MIR oracle, serialized-and-decoded verified bytecode VM, and emitted
  C17 agree on checksum `415.0`, exact bits `0x4079f00000000000`. VM and native
  child execution force collection before allocation; native children run at
  `-O0` and `-O2` under strict floating-point flags.
- A second source entry executes the out-of-bounds write in the encoded VM and
  generated native program. Native output is constructed from the VM panic
  observation and compared byte-for-byte, including code, message, symbol,
  span, and source-first frame.
- The source feature suite covers malformed/heterogeneous literals and types,
  non-`i64` indices, wrong assigned types, `length` shadowing, array/type/MIR
  limits, and fail-every-allocation cleanup with both allocator counters at
  zero. Formatter input/expected fixtures cover nested literals, chained
  indexing, indexed `=`/`+=`, and loops.
- The new source package exposed and pinned two portable emitter cases: a
  typed function whose every path panics must not trigger an unused out
  parameter warning, and legitimate unreachable package functions must not
  fail strict generated-C compilation.
- `examples/matrix.ruda` provides the canonical source-checkable kernel while
  honestly withholding public executable/CLI claims.

## Deliberate boundary and removal gates

- This is not complete D045. Unshadowed `arrayOf`, `arrayOf<T>`, free/dot
  `push`, and array iteration are not implemented. The first three are
  explicitly classified as `RUDA_CHECK_INCOMPLETE_LANGUAGE`, rather than
  misleading unknown-name diagnostics. They remain mandatory before preview.
- Source checking can represent general exact `Array<T>` layouts in MIR, but
  the serialized VM and generated-C checkpoint intentionally accept only
  `Array<f64>` and `Array<Array<f64>>`. Replacing those fixed backend tags with
  a canonical serialized program type table remains an I3/I5 removal gate.
- `ruda check`, `ruda run`, `ruda test`, `ruda test --engine both`, and
  `ruda build` are not wired to this checker/program path. The source fixture
  runs through the internal conformance harness, not the public CLI.
- The kernel returns an internal f64 checksum. Authoritative user-facing output
  and the final printed matrix acceptance require the shared compiler service,
  a normative output surface, and CLI/native driver wiring.
- Generated source is warning-checked under a simulated `_WIN32` translation
  unit, but the generated child is not compiled or executed by MSVC. This is
  not Tier 1 or I2/I3/I4 verification evidence.

## Commands and exit status

| Command | Exit | Result |
|---|---:|---|
| `cmake --build build/debug -j 2 && ctest --test-dir build/debug --output-on-failure` | 0 | Debug warnings-as-errors; full C suite passed; source matrix/G4002 generated C ran at `-O0` and `-O2` |
| `cmake --build build/release -j 2 && ctest --test-dir build/release --output-on-failure` | 0 | optimized parent build and generated children passed |
| `cmake --build build/asan-ubsan -j 2` plus CTest with `detect_leaks=0` and halt-on-error sanitizer settings | 0 | ASan+UBSan parent/generated-child paths passed without report; Apple leak detection is not claimed |
| strict C17 compile of `array.c`, `checker.c`, and source-array/native test TUs | 0 | warnings-as-errors clean |
| strict `_WIN32` C17 compile of `array.c` and `test_native_c_emitter.c` | 0 | compile-only portability check; not MSVC generated-program evidence |
| `clang --analyze` over changed production and test TUs | 0 | no diagnostics |
| formatter stdin/canonical comparisons and `ruda fmt --check` over the fixture/example | 0 | canonical and idempotent |
| `./tools/check-generated && ./tools/check-c-memory-policy` | 0 | generated registries and production heap policy clean |
| `./tools/check-doc-links && ./tools/check-spec-terms && ./tools/check-example-syntax` | 0 | documentation and example syntax checks passed |
| `git diff --check` | 0 | clean |

An independent read-only audit found no remaining P0/P1 after early resource
preflight, register-limit precedence, and deferred D045 builtin classification
were corrected. The nearest product blocker is now the authoritative compiler
service and public CLI path for this already differential source program, not a
second hand-built matrix implementation.
