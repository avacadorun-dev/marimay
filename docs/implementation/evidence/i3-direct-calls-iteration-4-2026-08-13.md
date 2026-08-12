# I3 verified direct-call programs — iteration 4 — 2026-08-13

Gate: I3 — typed bytecode VM vertical slice.
Iteration status: complete; gate status is `in-progress`.
Baseline: `3ca1f8e` (`feat: execute portable i64 shifts`).
Environment: macOS Darwin 26.3.1 arm64, AppleClang 17.0.0, CMake 4.4.2.

## Delivered direct-call slice

- The generated internal opcode registry appends stable wire IDs 25 and 26 for
  typed result-bearing and `void` direct calls. Existing wire IDs remain
  unchanged. Per-function call descriptors own stable callee symbols, exact
  result types, and ordered typed register arguments.
- Function verification bounds every call table and flat argument slice,
  requires initialized exact-typed arguments, and distinguishes result-bearing
  calls from `void` calls. No-call function encodings remain byte-identical;
  call-bearing functions use one exact internal feature bit and append a
  canonical call table.
- A bounded owning program registry orders functions by raw 128-bit symbol ID,
  verifies every body including non-entry functions, resolves every callee, and
  requires exact arity, ordered parameter types, and result type before
  execution. Aggregate function, register, block, instruction, call, argument,
  encoded-byte, and verification-work limits are enforced. Function verifier
  data-flow work is debited from the program's shared verification budget, so
  composing many individually bounded functions cannot bypass the aggregate
  cap.
- The internal `RUDABCP\0` envelope length-delimits canonical encoded functions
  and authenticates neither authors nor publishers. Its SHA-256 digest is an
  integrity check only. Decode validates the complete envelope boundary and
  digest before program allocation, then transactionally decodes and verifies
  the complete program before publishing it.
- Program execution uses an explicit heap frame stack rather than C recursion.
  The root is depth one; each call instruction consumes a caller step; each
  frame has an independent step limit; an optional shared budget covers every
  caller and callee instruction. Destinations and the entry result are published
  only after successful returns. Depth and step failures retain the next/call
  span, while arithmetic and shift failures retain the exact callee span.
- Production MIR lowering copies verified call descriptors, preserves source
  spans and register order, lowers scalar result calls and `void` calls, sorts
  an unordered function set into stable symbol order, resolves the selected
  entry, and verifies the completed program.
- The bytecode fuzz harness now admits both individual `RUDABCF` functions and
  whole `RUDABCP` programs, requires canonical decode/re-encode identity, and
  bounds program execution, call depth, and all decode/verification resources.

This implements the existing direct-call bytecode verification contract. It
does not change Ruda source semantics, define the public `.rudab` bundle format,
or claim cryptographic signing/authenticity.

## Verification coverage

- Registry and disassembly tests pin both new numeric IDs, operand layouts,
  flags, and `cN` rendering. Function codec tests cover exact no-call bytes,
  call-table round trips, every-prefix truncation, unsupported/canonical feature
  handling, corrupt call IDs/registers/types/slices, and trailing data.
- Program tests cover stable ordering and entry lookup, duplicate/out-of-order
  symbols, unknown callees, arity/parameter/result mismatch, malformed
  non-entry bodies, deterministic encode/decode/re-encode identity, every-prefix
  truncation, corrupt magic/version/features/payload/digest, and trailing data.
  An exact aggregate verification-work boundary pins both the last rejected and
  first accepted budget values.
- VM tests cover ordered `i64` arguments, `bool`, `void`, three-frame nesting,
  self recursion with a real base case, exact root-depth behavior, per-frame and
  shared step boundaries, callee division/overflow/invalid-shift spans, result
  nonpublication, standalone-call rejection, and deterministic fail-at-each-
  allocation cleanup.
- MIR tests lower unordered multi-function input through program encode/decode
  and VM execution, pin exact `CALL_DIRECT` and `CALL_DIRECT_VOID` mappings, and
  retain explicit rejection of unsupported string/managed/never surfaces.
- Construction, function/program encode/decode/verify, and nested VM execution
  participate in bounded allocation-failure sweeps with zero live allocations
  after every attempt.

## Commands and exit status

| Command | Exit | Result |
|---|---:|---|
| `cmake --build --preset debug --parallel && ctest --preset debug --output-on-failure` | 0 | warnings-as-errors Debug build; 1/1 passed |
| `cmake --build --preset release --parallel && ctest --preset release --output-on-failure` | 0 | optimized build; 1/1 passed |
| `cmake --build --preset asan --parallel && ASAN_OPTIONS=halt_on_error=1 ctest --preset asan --output-on-failure` | 0 | AddressSanitizer run; 1/1 passed without report |
| `cmake --build --preset asan-ubsan --parallel && ASAN_OPTIONS=halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ctest --preset asan-ubsan --output-on-failure` | 0 | AddressSanitizer and UBSan run; 1/1 passed without report |
| `cmake --build --preset tsan --parallel && TSAN_OPTIONS=halt_on_error=1 ctest --preset tsan --output-on-failure` | 0 | ThreadSanitizer run; 1/1 passed without report |
| `clang --analyze -std=c17 -Ibootstrap/include -Iruntime/include -Itests/c-unit bootstrap/src/base/checked_i64.c bootstrap/src/bytecode/bytecode.c bootstrap/src/bytecode/program.c bootstrap/src/bytecode/vm.c bootstrap/src/bytecode/mir_lower.c bootstrap/src/mir/mir.c tests/c-unit/test_bytecode.c tests/c-unit/test_bytecode_vm.c tests/c-unit/test_mir_bytecode.c tests/fuzz/bytecode_fuzz.c` | 0 | no static-analyzer findings |
| `cc -std=c17 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror -Ibootstrap/include -Iruntime/include -c tests/fuzz/bytecode_fuzz.c -o /tmp/ruda-bytecode-fuzz-iteration4.o` | 0 | program-aware fuzz target compiles warning-free as C17 |
| `./tools/check-generated` | 0 | opcode and all other generated artifacts current |
| `./tools/check-doc-links && ./tools/check-spec-terms && ./tools/check-example-syntax && ./tools/check-syntax-coverage` | 0 | documentation checks passed; 74/74 syntax kinds and 115/115 grammar productions covered |
| `cmake --preset fuzz` | 1 | expected configuration failure: local AppleClang distribution has no libFuzzer runtime |

The ASan result is not an LSan claim. Deterministic failure allocators provide
the local ownership evidence. No successful fuzz campaign is claimed.

## Remaining I3 work

I3 remains open for the public `.rudab` section/signature policy, stable loader
diagnostics, canonical valid fuzz seeds and a sanitizer-clean fuzz campaign,
source snapshot resolution and VM stack traces, strings/records/enums/managed
descriptors/root maps/GC, explicit `G4000` panic payloads and terminating
`never` calls, unsafe/capability restrictions, VM-backed commands, cold-start
measurement, Tier 1 platform evidence, and generated-program differential
coverage. I4 C17 AOT and VM/native equivalence are not implied by this
checkpoint.
