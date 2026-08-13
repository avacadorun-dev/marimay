# I4 native managed arrays — iteration 9 — 2026-08-14

Gate: I4 portable-C AOT and engine-equivalence vertical slice, using the I3
iteration 9 bytecode subset and the I5 managed heap. Iteration status:
complete; I2, I3, I4, and I5 remain `in-progress`.

Baseline: `e3d742c` (`feat: execute managed arrays in bytecode VM`).
Environment: macOS arm64, AppleClang 17.0.0, CMake 4.4.2.

Authority: accepted [D045](../../design/decisions.md), the normative
[language](../../spec/language.md), [execution](../../spec/execution.md), and
[diagnostic](../../spec/diagnostics.md) contracts. No source semantic changed.

## Delivered slice

- The C17 emitter accepts the same two matrix-critical exact layouts as the VM:
  `Array<f64>` and `Array<Array<f64>>`. Every other dynamic type remains an
  explicit unsupported-emission result. This is not a general `Array<T>` ABI.
- Generated code uses the shared `RudaManagedHeap`, canonical descriptors, and
  provenance-bearing `RudaManagedReference`; it does not introduce a native
  arena, serialize MIR type identifiers, or expose object pointers.
- Every generated function receives one execution context. Array parameters
  and locals are registered in a precise root frame before an allocation or
  call. Locals begin as invalid references, call results publish into an
  already-rooted caller register, and every success, trap, and panic path
  reaches one strict-LIFO cleanup epilogue.
- Construction cells and root-location tables use checked, allocator-owned
  storage rather than VLAs. The shared managed runtime retains reference-valued
  construction cells across collection and publishes arrays transactionally.
- `array_length`, `array_require_index`, `array_get`, and `array_set` validate
  provenance and exact descriptors before observation. The store retains its
  defensive runtime bounds check after verifier proof and publishes its MIR
  result only after a successful mutation.
- Native `G4002` uses allocation-free locale-independent signed/unsigned
  decimal formatting into panic-owned bounded storage. Code, message, complete
  access span, primary symbol, and source-first caller frames use the existing
  deterministic native panic protocol.
- Generated main initializes and disposes the managed heap on every result,
  trap, and panic path. Internal compile-time test seams force collection before
  allocation, a chosen allocation failure, or a smaller object ceiling without
  changing default generated-program behavior.

## Verification coverage

- A flat array fixture constructs two f64 values, observes length, checks an
  index, mutates through an alias-capable handle, reads the result, and agrees
  across MIR, encoded/decoded VM, and generated native output at `-O0` and
  `-O2`.
- Bounds cases cover `-1`, the upper bound, `INT64_MIN`, and `INT64_MAX`.
  MIR and VM agree on exact message bytes and access span; native output pins
  the complete `G4002` code/message/symbol/span/frame serialization.
- A D045 ordering fixture places an explicit-panic direct callee after
  `array_require_index`. The invalid destination reports `G4002` without
  reaching the `G4000` callee, while the valid destination reaches that callee.
- A nested array fixture allocates `Array<f64>` then
  `Array<Array<f64>>`, collects before every allocation, reads through both
  descriptors, and agrees bit-for-bit across all three engines.
- A direct callee returns a managed array into a rooted caller register; the
  caller then allocates another array under collect-before-allocation and still
  reads the returned value. This pins publication-before-callee-root-pop.
- Generated execution failure injection covers root/construction allocation
  failure as native allocation trap 5. A one-object heap ceiling makes the
  second nested construction report native limit trap 6. ASan/UBSan covers
  the cleanup paths.
- The POSIX child compiler links the actual allocator, provenance, managed,
  checked-integer, and f64 runtime sources. Strict warnings and floating-point
  flags apply to both optimized and unoptimized generated programs.

## Deliberate boundary and remaining work

- I4 is not verified. Generated array C has not yet compiled or executed under
  MSVC; the Windows test translation unit is warning-clean, but the child
  compiler harness remains POSIX-only. GCC/Clang CI and every Tier 1 platform
  still require external clean-checkout evidence.
- The first rectangular loop-based f64 matrix fixture is not delivered by this
  checkpoint. Source array construction/index lowering and public
  `ruda check/run/test/build` integration remain the next acceptance blockers.
- Replace the two fixed backend array kinds with a canonical serialized program
  layout table and execution-interned descriptors before claiming general
  D045 arrays or verifying I3/I5. Array entry results, growth/push, recursive
  calls, records/enums in native managed storage, and resumable calls remain
  outside this bounded slice.
- Allocation and limit traps are internal checkpoint protocol values, not a
  frozen public process-output ABI. The release CLI must map resource failures
  through its accepted diagnostic contract.

## Commands and exit status

| Command | Exit | Result |
|---|---:|---|
| `cmake --build build/debug -j 2 && ctest --test-dir build/debug --output-on-failure` | 0 | Debug warnings-as-errors; 1/1 passed; generated array C ran at `-O0` and `-O2` |
| `cmake --build build/release -j 2 && ctest --test-dir build/release --output-on-failure` | 0 | optimized parent build; 1/1 passed |
| `cmake --build build/asan-ubsan -j 2 && ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir build/asan-ubsan --output-on-failure` | 0 | ASan+UBSan parent and generated children passed without report |
| `cmake --build build/tsan -j 2 && TSAN_OPTIONS=halt_on_error=1 ctest --test-dir build/tsan --output-on-failure` | 0 | TSan passed without report |
| `clang --analyze` over the emitter and native tests | 0 | no diagnostics |
| Windows-guarded strict C17 compile of `test_native_c_emitter.c` | 0 | no unused or conversion warning; not MSVC generated-program evidence |
| `./tools/check-generated && ./tools/check-c-memory-policy` | 0 | generated registries and production heap policy clean |
| `./tools/check-doc-links && ./tools/check-spec-terms && ./tools/check-example-syntax` | 0 | documentation checks passed |
| `git diff --check` | 0 | clean |

The final independent read-only audit found no remaining P0/P1 defect in the
stabilized generated roots, cleanup, direct-call ownership, exact descriptors,
provenance checks, or `G4002` paths. The nearest product risk is now the
loop-based matrix spine and its missing source/CLI integration, not a second
array memory model.
