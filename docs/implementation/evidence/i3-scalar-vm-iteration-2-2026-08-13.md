# I3 scalar bytecode VM — iteration 2 — 2026-08-13

Gate: I3 — typed bytecode VM vertical slice.
Iteration status: complete; gate status is `in-progress`.
Baseline: `53fec5f` (`feat: establish verified bytecode foundation`).
Environment: macOS Darwin 25.3.0 arm64, AppleClang 17.0.0, CMake 4.4.2.

## Delivered scalar execution slice

- Verified `bool`/`i64`/`void` MIR functions can be lowered through a production
  internal API into the typed bytecode function model. Register and block IDs,
  parameter order, exact scalar types, control-flow edges, and caller-supplied
  source identity plus byte spans are preserved.
- Every currently registered executable opcode has scalar VM dispatch:
  constants, copy, checked `i64` add/subtract/multiply/divide/remainder/negate,
  boolean negation, exact boolean and `i64` equality, `i64` ordering, jump,
  conditional branch, and typed or void return.
- The VM verifies the complete function before frame allocation or dispatch,
  validates exact argument types, publishes a typed result only on success,
  and provides distinct internal outcomes for invalid input, allocation or
  verification limits, invalid bytecode, execution-step limits, overflow, and
  division by zero.
- Per-run and optional shared step budgets count every instruction, including
  terminators. Arithmetic failures and step exhaustion preserve the exact
  bytecode source span in an internal trap observation.
- Checked `i64` arithmetic is now one shared portable C17 implementation used
  by both the MIR oracle and VM. This is a semantic-preserving extraction of
  the existing oracle checks, including the D042 checks before host `/` or `%`.
- The bytecode fuzz harness now executes successfully decoded and verified
  functions with zero-valued exact-type parameters and a 1,024-step limit,
  in addition to canonical round-trip, verifier, and disassembly assertions.

The lowering fails explicitly for unsupported types, instructions, and
terminators. It does not approximate strings, records, enums, bitwise/shift
operations, calls, explicit `panic`, or never-returning calls.

## Execution and differential coverage

- Exact VM result tests cover every current opcode, both conditional branch
  outcomes, jump and void return, `bool` truth tables, `i64` comparison
  boundaries, constants/copy, canonical encode/decode execution, and invalid
  bytecode rejection before dispatch.
- D042 tests cover positive and negative division/remainder truncation, dividend
  remainder sign, zero divisors, `INT64_MIN / -1`, `INT64_MIN % -1`, both-sign
  add/subtract overflow, multiplication overflow, and `INT64_MIN` negation.
- Production-lowered MIR is encoded, decoded, reverified, and then executed by
  the VM alongside the MIR oracle. The differential suite compares exact typed
  scalar results for a fixture containing every supported lowering opcode and
  both branch routes, then
  compares every checked-arithmetic success/error case and VM source trap.
- Exact per-run/shared step boundaries and a non-progressing loop prove bounded
  execution. Deterministic fail-at-each-allocation sweeps cover lowering-owned
  allocations, MIR and bytecode verifier temporaries through each function's
  allocator, and VM frame creation through the run allocator, with no net live
  allocations or bytes after every attempt.

## Commands and exit status

| Command | Exit | Result |
|---|---:|---|
| `cmake --preset debug && cmake --build --preset debug --parallel && ctest --preset debug --output-on-failure` | 0 | warnings-as-errors Debug build; 1/1 CTest executable passed |
| `cmake --preset release && cmake --build --preset release --parallel && ctest --preset release --output-on-failure` | 0 | optimized build; 1/1 passed |
| `cmake --preset asan && cmake --build --preset asan --parallel && ASAN_OPTIONS=halt_on_error=1 ctest --preset asan --output-on-failure` | 0 | AddressSanitizer run; 1/1 passed without report |
| `cmake --preset asan-ubsan && cmake --build --preset asan-ubsan --parallel && ASAN_OPTIONS=halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ctest --preset asan-ubsan --output-on-failure` | 0 | AddressSanitizer and UBSan run; 1/1 passed without report |
| `cmake --preset tsan && cmake --build --preset tsan --parallel && TSAN_OPTIONS=halt_on_error=1 ctest --preset tsan --output-on-failure` | 0 | ThreadSanitizer run; 1/1 passed without report |
| `clang --analyze -std=c17 -Ibootstrap/include -Iruntime/include -Itests/c-unit bootstrap/src/base/checked_i64.c bootstrap/src/bytecode/vm.c bootstrap/src/bytecode/mir_lower.c bootstrap/src/mir/mir.c tests/c-unit/test_bytecode_vm.c tests/c-unit/test_mir_bytecode.c` | 0 | no static-analyzer findings |
| `cc -std=c17 -Wall -Wextra -Wpedantic -Werror -Ibootstrap/include -Iruntime/include -c tests/fuzz/bytecode_fuzz.c -o /tmp/ruda-bytecode-fuzz-iteration2.o` | 0 | extended fuzz target compiles warning-free as C17 |
| `./tools/check-generated` | 0 | committed generated artifacts current |
| `./tools/check-doc-links && ./tools/check-spec-terms && ./tools/check-example-syntax && ./tools/check-syntax-coverage` | 0 | documentation checks passed; 74/74 syntax kinds and 115/115 grammar productions covered |
| `cmake --preset fuzz` | 1 | expected configuration failure: local AppleClang distribution has no libFuzzer runtime |

The ASan result is not an LSan claim; the deterministic failure allocator is
the local ownership evidence. No successful fuzz campaign is claimed.

## Remaining I3 work

I3 remains open for the complete `.rudab` section and integrity format, stable
loader diagnostics, no-partial-runtime-initialization proof, source snapshot
resolution and VM stack traces, direct calls and full program dispatch,
strings/records/enums/managed descriptors/root maps/GC, bitwise and shift
opcodes, explicit `G4000` panic payloads, unsafe/capability restrictions,
VM-backed `ruda run`/`ruda exec`/`ruda test`, cold-start measurement, Tier 1
platform evidence, generated-program differential coverage, and a successful
sanitizer-clean bytecode fuzz campaign. I4 C17 AOT and VM/native equivalence are
not implied by this checkpoint.
