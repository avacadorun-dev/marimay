# I3 bytecode foundation — iteration 1 — 2026-08-12

Gate: I3 — typed bytecode VM vertical slice.
Iteration status: complete; gate status is `in-progress`.
Environment: macOS Darwin 25.3.0 arm64, AppleClang 17.0.0, CMake 4.4.2.

## Delivered internal foundation

- A generated, contiguous wire-opcode registry for the current `bool`/`i64`
  function subset, with explicit operand schemas, terminator/branch flags, and
  checked-arithmetic classification. The TSV is the source for the generated C
  enum/metadata and the generated Markdown reference.
- A versioned, bounded internal function codec with canonical variable-length
  integer encodings, exact decode/encode round trips, source spans, and
  rejection of non-canonical, truncated, overflowing, unknown, unsupported,
  and trailing encodings.
- A typed function verifier covering block termination/control-flow targets,
  register bounds and definite assignment, exact operand/result types, and
  configured structural and verification-work limits.
- A deterministic internal disassembler, including an injected writer-failure
  test.
- C-unit allocation-failure sweeps over construction, encoding, decoding, and
  verifier work; every attempted failure path releases all tracked allocations
  and bytes before returning.

This is an internal function codec/verifier foundation only. It is not the
complete `.rudab` bundle format, loader integrity contract, VM, VM execution,
`ruda run`, `ruda exec`, root-map implementation, public bytecode ABI, or a
VM/native differential result. The status values used by this internal API are
not public diagnostics; no `Bxxxx` diagnostic was allocated or claimed.

## Test surfaces

- Registry metadata exactness for every current wire ID and generated-artifact
  freshness.
- Valid multi-block `bool`/`i64` function construction, verification,
  encoding/decoding, canonical re-encoding, and disassembly, plus void return.
- Malformed magic, truncated header, non-canonical and overflowing varints,
  unsupported format/features, invalid types/opcodes/booleans, malformed
  operands, malformed spans, and trailing data.
- Invalid/missing terminators and branch targets, invalid/type-mismatched and
  unassigned registers, invalid result shape, and verifier work-limit
  exhaustion.
- Deterministic allocation failure through successful completion for function
  construction, encoding, decoding, and verifier temporary state, checking
  `live_allocations == 0` and `live_bytes == 0` after each run.

The current instruction table is verifier/codec coverage, not evidence of VM
execution for every opcode. Execution coverage remains an explicit I3 gate
requirement under the test strategy and is not satisfied here.

## Commands and exit status

| Command | Exit | Result |
|---|---:|---|
| `./tools/check-generated` | 0 | opcode, diagnostic, Unicode, and syntax generated artifacts current |
| `cmake --preset debug && cmake --build --preset debug --parallel && ctest --preset debug --output-on-failure` | 0 | warnings-as-errors Debug build; 1/1 CTest executable passed |
| `cmake --preset release && cmake --build --preset release --parallel && ctest --preset release --output-on-failure` | 0 | optimized build; 1/1 passed |
| `cmake --preset asan && cmake --build --preset asan --parallel && ASAN_OPTIONS=halt_on_error=1 ctest --preset asan --output-on-failure` | 0 | AddressSanitizer run; 1/1 passed without report |
| `cmake --preset asan-ubsan && cmake --build --preset asan-ubsan --parallel && ASAN_OPTIONS=halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ctest --preset asan-ubsan --output-on-failure` | 0 | AddressSanitizer and UBSan run; 1/1 passed without report |
| `cmake --preset tsan && cmake --build --preset tsan --parallel && TSAN_OPTIONS=halt_on_error=1 ctest --preset tsan --output-on-failure` | 0 | ThreadSanitizer run; 1/1 passed without report |
| `./tools/check-doc-links && ./tools/check-spec-terms && ./tools/check-example-syntax && ./tools/check-syntax-coverage` | 0 | documentation checks passed; 74/74 syntax kinds and 115/115 grammar productions covered |
| `clang --analyze -std=c17 -Ibootstrap/include -Iruntime/include bootstrap/src/bytecode/bytecode.c tests/c-unit/test_bytecode.c` | 0 | no static-analyzer findings |
| `cc -std=c17 -Wall -Wextra -Wpedantic -Werror -Ibootstrap/include -Iruntime/include -c tests/fuzz/bytecode_fuzz.c -o /tmp/ruda-bytecode-fuzz.o` | 0 | fuzz target compiles warning-free as an ordinary C17 translation unit |
| `cmake --preset fuzz` | 1 | expected configuration failure: local AppleClang distribution has no libFuzzer runtime |

The local ASan result is not an LSan claim: this macOS toolchain aborts before
test execution when `detect_leaks=1` is requested. The deterministic
failure-injected allocator sweeps above are separate ownership evidence.

## Remaining I3 work

The I3 gate remains open for the complete `.rudab` section/integrity format,
loader rejection with no partial runtime initialization, root maps, managed
object descriptors, VM dispatch and execution tests for every opcode, VM source
stacks and deterministic runtime limits, VM-backed CLI/test behavior, MIR/VM
agreement, startup measurement, and a successful sanitizer-clean bytecode
libFuzzer campaign with committed/minimized corpus evidence. I4's portable C17
backend and engine-equivalence work is also not implied by this checkpoint.
