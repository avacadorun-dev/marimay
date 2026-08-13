# I4 strings, panic, and `never` — iteration 2 — 2026-08-13

Gate: I4 — C17 AOT and engine equivalence.  
Iteration status: complete; gate status is `in-progress`.  
Baseline: `6bb1496` (`feat: emit verified scalar MIR as C17`).  
Environment: macOS Darwin 25.3.0 arm64, AppleClang 17.0.0, CMake 4.4.2.

## Delivered slice

- Generated C17 now represents immutable strings as counted byte slices. MIR
  constants, copies, parameters, direct-call results, entry results, and exact
  string equality preserve embedded NUL and UTF-8 bytes without using C-string
  length or comparison semantics.
- MIR `PANIC` emits an explicit `G4000` outcome containing the exact message
  bytes, primary stable symbol, complete source span, and source-first frame
  sequence. The standalone protocol hex-encodes message and symbol bytes, so
  diagnostics remain deterministic and lossless.
- MIR `CALL_NEVER` has a separate generated edge with no result destination or
  normal successor. A normal return from a declared-`never` callee is reported
  as an invalid generated artifact instead of fabricating a value or
  continuing execution.
- The iterative call-cycle validator includes both value calls and
  terminating `never` calls. Panic frame storage is statically allocated and
  bounded by the verified, configured function count, avoiding both recursive
  compiler traversal and a proportional native C stack allocation.
- Existing public emitter API and Ruda semantics are unchanged. The generated
  string/panic helpers remain compiler-owned implementation details.

This implements the existing D036 and D037 native contracts for the current
constant-string, nonrecursive direct-call slice. It does not introduce dynamic
string construction, GC-backed managed data, cleanup unwinding, or a public
native runtime ABI.

## Verification coverage

- Embedded-NUL Unicode string return is compared with VM bytes and exact native
  hex output. Independently materialized equal constants exercise counted
  `memcmp` equality rather than reference or NUL-terminated equality.
- A two-function MIR program uses `CALL_NEVER` to enter a `PANIC` leaf. MIR
  validates the leaf identity, VM verifies result nonpublication and exact
  `G4000` message/symbol/span/two-frame order, and generated native C prints
  the same primary identity and source-first frames.
- Generated source is byte-identical across repeated emission and unordered
  input functions. Limits and deterministic allocator failure preserve zero
  live allocations and publish no partial C source.
- Independent review found no remaining P0/P1 production defect after
  `CALL_NEVER` was added to cycle discovery and generated panic-frame storage
  was moved out of automatic stack storage.

## Commands and exit status

| Command | Exit | Result |
|---|---:|---|
| `cmake --build --preset debug --parallel 4 && ctest --preset debug --output-on-failure -R '^ruda_c_unit$'` | 0 | Debug build; 1/1 passed; generated strings/panic C ran at `-O0` and `-O2` |
| `cmake --build --preset release --parallel 4 && ctest --preset release --output-on-failure -R '^ruda_c_unit$'` | 0 | Release build; 1/1 passed; generated strings/panic C ran at `-O0` and `-O2` |
| `cmake --build --preset asan-ubsan --parallel 4 && ASAN_OPTIONS=halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ctest --preset asan-ubsan --output-on-failure -R '^ruda_c_unit$'` | 0 | parent and generated child programs passed ASan+UBSan without report |
| `cmake --build --preset tsan --parallel 4 && TSAN_OPTIONS=halt_on_error=1 ctest --preset tsan --output-on-failure -R '^ruda_c_unit$'` | 0 | ThreadSanitizer run passed without report |
| `clang --analyze -std=c17 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror -Ibootstrap/include -Iruntime/include -Itests/c-unit -Itests/fuzz bootstrap/src/native/c_emitter.c tests/c-unit/test_native_c_emitter.c` | 0 | no static-analyzer findings |
| `cc -std=c17 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror -D_WIN32 -Ibootstrap/include -Iruntime/include -Itests/c-unit -Itests/fuzz -c tests/c-unit/test_native_c_emitter.c -o /tmp/ruda-native-c-emitter-cp7-win.o` | 0 | Windows-guarded test translation unit warning-clean; this is not MSVC generated-C evidence |
| `git diff --check` | 0 | clean |

The local compiler is AppleClang. GCC/Clang CI and MSVC generated-C execution
are not claimed until those remote jobs run; Windows still skips the child
compiler harness. MIR's standalone panic observation has no caller-frame
vector, so the exact nested frame differential currently uses the verified VM
program as the program-level oracle alongside native output.

## Remaining I4 work

I4 remains open for MSVC generated-C execution, full EXEC-020 identifier
coverage, dynamic/GC-owned strings, records/enums and managed runtime linkage,
cleanup-aware panic propagation, recursive and resumable calls, generated
program fuzzing/minimization, deterministic record/replay, D030 toolchain
discovery, object/build caching, native CLI commands and source-oriented
diagnostics, Tier 1 evidence, and compile-time/binary-size baselines.
