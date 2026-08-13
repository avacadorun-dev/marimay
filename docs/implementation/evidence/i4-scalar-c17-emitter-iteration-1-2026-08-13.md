# I4 scalar C17 emitter — iteration 1 — 2026-08-13

Gate: I4 — C17 AOT and engine equivalence.  
Iteration status: complete; gate status is `in-progress`.  
Baseline: `d994b3b` (`feat: execute string panics and never calls`).  
Environment: macOS Darwin 25.3.0 arm64, AppleClang 17.0.0, CMake 4.4.2.

## Delivered slice

- A new internal `ruda/native/c_emitter.h` API accepts an unordered borrowed
  set of verified MIR functions plus an explicit entry symbol and publishes
  owned standalone C17 source transactionally.
- The emitter supports scalar `bool`, `i64`, and `void` results, scalar CFG,
  exact direct non-`never` calls, comparison, bitwise operations, and all
  existing checked `i64` arithmetic and shifts. Checked operations use the
  shared UB-free helpers required by the execution specification, and native
  traps preserve the exact originating instruction span.
- Functions are sorted deterministically by their raw stable symbol ID and
  receive compiler-owned `ruda_g_` identifiers. Duplicate or unknown symbols,
  recursion, malformed MIR, unsupported types/instructions/terminators/calls,
  resource limits, and allocation failures are rejected before output is
  published.
- Call-cycle validation uses an allocator-owned iterative DFS bounded by the
  already bounded function count; it does not recurse on the host C stack.
- Generated C is compiled without a shell, with the CMake-selected compiler,
  warnings as errors, and both `-O0` and `-O2`. Sanitizer presets propagate
  their ASan/UBSan instrumentation into these generated child programs.

This is the first narrow V0 native slice under existing decisions D035, D039,
D042 and the normative generated-C execution contract. It does not change
Ruda source semantics or define a public native object, runtime ABI, or CLI.

## Verification coverage

- The same scalar MIR fixtures are evaluated by the MIR interpreter, lowered
  to bytecode and run by the VM, and, for representative cases, emitted,
  compiled, and run as native C. Coverage includes both branch paths, exact
  direct-call results, result nonpublication on failure, bool operations,
  comparisons, bitwise operations, checked add/subtract/multiply/negate,
  signed divide/remainder signs and exceptional boundaries, and D035 shift
  counts `-1`, `0`, `1`, `62`, `63`, and `64` including signed right extension
  and checked left overflow.
- Generated source is byte-identical under input-function permutation. Stable
  128-bit symbols generate hexadecimal compiler-owned identifiers; no source
  identifier is interpolated.
- Negative tests cover null/empty input, function and generated-byte limits,
  unknown calls, malformed instructions, direct recursion, and deterministic
  allocation failure with zero live allocations and no partial output.
- Independent review found no remaining P0 production defect. The recursive
  call-graph walk, direct-call status conversion warnings, and child-process
  argument termination found during review were fixed before this evidence was
  recorded.

## Commands and exit status

| Command | Exit | Result |
|---|---:|---|
| `cmake --preset debug && cmake --build --preset debug --parallel 4 && ctest --preset debug --output-on-failure -R '^ruda_c_unit$'` | 0 | warnings-as-errors Debug build; 1/1 passed; generated AppleClang C ran at `-O0` and `-O2` |
| `cmake --build --preset release --parallel 4 && ctest --preset release --output-on-failure -R '^ruda_c_unit$'` | 0 | optimized parent build; 1/1 passed; generated AppleClang C ran at `-O0` and `-O2` |
| `cmake --preset asan-ubsan && cmake --build --preset asan-ubsan --parallel 4 && ASAN_OPTIONS=halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ctest --preset asan-ubsan -R '^ruda_c_unit$' --output-on-failure` | 0 | parent and generated child programs passed ASan+UBSan without report |
| `cmake --preset tsan && cmake --build --preset tsan --parallel 4 && TSAN_OPTIONS=halt_on_error=1 ctest --preset tsan -R '^ruda_c_unit$' --output-on-failure` | 0 | ThreadSanitizer run passed without report |
| `clang --analyze -std=c17 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror -Ibootstrap/include -Iruntime/include -Itests/c-unit -Itests/fuzz bootstrap/src/native/c_emitter.c tests/c-unit/test_native_c_emitter.c` | 0 | no static-analyzer findings |
| `cc -std=c17 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror -D_WIN32 -Ibootstrap/include -Iruntime/include -Itests/c-unit -Itests/fuzz -c tests/c-unit/test_native_c_emitter.c -o /tmp/ruda-native-c-emitter-win-preprocess.o` | 0 | Windows-guarded test translation unit is warning-clean; this is not MSVC execution evidence |
| `./tools/check-generated && ./tools/check-doc-links && ./tools/check-spec-terms && ./tools/check-example-syntax && ./tools/check-syntax-coverage` | 0 | generated/docs checks passed; 74/74 syntax kinds and 115/115 grammar productions covered |

The local compiler is AppleClang. The configured CI matrix will exercise the
same generated-C child harness with GCC and Clang on Unix, but no unexecuted CI
result is claimed here. On Windows the current test intentionally skips child
compilation, so generated-C compilation and execution under MSVC remain open.

## Remaining I4 work

I4 remains open for MSVC generated-C execution, full EXEC-020 adversarial
identifier coverage, strings and `G4000` native stack frames, declared-`never`
calls, records/enums and managed runtime linkage, loops and generated-program
differential fuzzing, deterministic record/replay and minimization, native
toolchain discovery under D030, object/build caching, `ruda build`, native
`ruda run`, `ruda test --engine native|both`, source-oriented stack traces,
Tier 1 evidence, and compile-time/binary-size baselines.
