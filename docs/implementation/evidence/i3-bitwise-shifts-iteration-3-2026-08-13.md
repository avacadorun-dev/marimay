# I3 portable bitwise and shift bytecode — iteration 3 — 2026-08-13

Gate: I3 — typed bytecode VM vertical slice.
Iteration status: complete; gate status is `in-progress`.
Baseline: `2ea0819` (`feat: execute scalar typed bytecode`).
Environment: macOS Darwin 25.3.0 arm64, AppleClang 17.0.0, CMake 4.4.2.

## Delivered D035 slice

- The generated internal opcode registry appends stable wire IDs 19 through 24
  for `i64` bitwise complement/AND/XOR/OR, checked left shift, and arithmetic
  right shift. Existing IDs are unchanged. Both shifts carry `may_panic`.
- A shared portable C17 numeric core now owns explicit two's-complement
  reconstruction and bitwise operations. It validates a signed shift count
  before every host shift, performs shifts only in the unsigned domain, checks
  left-shift representability, and implements negative right-shift sign
  extension without implementation-defined signed `>>`.
- The MIR oracle uses that shared implementation, preserving its accepted D035
  behavior while removing a second backend-specific copy.
- The bytecode verifier requires exact initialized `i64` target and operands for
  all six opcodes. Production MIR lowering preserves register operands and
  source spans; the VM reports overflow separately from invalid shift and
  publishes no result on either failure.
- The bytecode fuzz target accepts the new bounded invalid-shift outcome after
  successful decode and verification. A successful libFuzzer campaign and
  canonical valid shift corpus seeds remain open platform evidence.

This implements existing normative language text and accepted decision D035;
it does not change source semantics or the public `.rudab` compatibility
contract.

## Verification coverage

- Registry tests assert every numeric ID, mnemonic, operand layout, and flag.
  The all-opcode fixture verifies, canonically encodes/decodes, re-encodes, and
  exactly disassembles every registered opcode.
- Verifier negatives cover wrong target/count types and every input position of
  all six opcodes reading an uninitialized register. VM admission rejects a new
  opcode mutation before allocating its execution frame.
- Direct VM and production MIR-to-bytecode-to-codec-to-VM differentials cover
  two's-complement negative bitwise values; counts `-1`, `0`, `1`, `62`, `63`,
  and `64`; `INT64_MIN`/`INT64_MAX` counts; positive, negative, zero, and extreme
  checked-left boundaries; arithmetic-right sign extension; exact trap spans;
  and unchanged result outputs on failure.
- The every-supported-opcode lowering fixture participates in deterministic
  fail-at-each-allocation sweeps. Both branch paths retain exact per-run and
  shared step accounting after the six added instructions.

## Commands and exit status

| Command | Exit | Result |
|---|---:|---|
| `cmake --build --preset debug --parallel && ctest --preset debug --output-on-failure` | 0 | warnings-as-errors Debug build; 1/1 passed |
| `cmake --build --preset release --parallel && ctest --preset release --output-on-failure` | 0 | optimized build; 1/1 passed |
| `cmake --build --preset asan --parallel && ASAN_OPTIONS=halt_on_error=1 ctest --preset asan --output-on-failure` | 0 | AddressSanitizer run; 1/1 passed without report |
| `cmake --build --preset asan-ubsan --parallel && ASAN_OPTIONS=halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ctest --preset asan-ubsan --output-on-failure` | 0 | AddressSanitizer and UBSan run; 1/1 passed without report |
| `cmake --build --preset tsan --parallel && TSAN_OPTIONS=halt_on_error=1 ctest --preset tsan --output-on-failure` | 0 | ThreadSanitizer run; 1/1 passed without report |
| `clang --analyze -std=c17 -Ibootstrap/include -Iruntime/include -Itests/c-unit bootstrap/src/base/checked_i64.c bootstrap/src/bytecode/bytecode.c bootstrap/src/bytecode/vm.c bootstrap/src/bytecode/mir_lower.c bootstrap/src/mir/mir.c tests/c-unit/test_bytecode.c tests/c-unit/test_bytecode_vm.c tests/c-unit/test_mir_bytecode.c` | 0 | no static-analyzer findings |
| `cc -std=c17 -Wall -Wextra -Wpedantic -Werror -Ibootstrap/include -Iruntime/include -c tests/fuzz/bytecode_fuzz.c -o /tmp/ruda-bytecode-fuzz-iteration3.o` | 0 | extended fuzz target compiles warning-free as C17 |
| `./tools/check-generated` | 0 | opcode and all other generated artifacts current |
| `./tools/check-doc-links && ./tools/check-spec-terms && ./tools/check-example-syntax && ./tools/check-syntax-coverage` | 0 | documentation checks passed; 74/74 syntax kinds and 115/115 grammar productions covered |
| `cmake --preset fuzz` | 1 | expected configuration failure: local AppleClang distribution has no libFuzzer runtime |

The ASan result is not an LSan claim. Deterministic failure allocators provide
the local ownership evidence. No successful fuzz campaign is claimed.

## Remaining I3 work

I3 remains open for the complete `.rudab` section and integrity format, stable
loader diagnostics, canonical valid fuzz seeds and a sanitizer-clean fuzz
campaign, source snapshot resolution and VM stack traces, direct calls and full
program dispatch, strings/records/enums/managed descriptors/root maps/GC,
explicit `G4000` panic payloads, unsafe/capability restrictions, VM-backed
commands, cold-start measurement, Tier 1 platform evidence, and generated-
program differential coverage. I4 C17 AOT and VM/native equivalence are not
implied by this checkpoint.
