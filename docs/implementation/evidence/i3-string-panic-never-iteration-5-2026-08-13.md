# I3 string panic and `never` calls — iteration 5 — 2026-08-13

Gate: I3 — typed bytecode VM vertical slice.
Iteration status: complete; gate status is `in-progress`.
Baseline: `68010e1` (`feat: execute verified direct calls`).
Environment: macOS Darwin 26.3.1 arm64, AppleClang 17.0.0, CMake 4.4.2.

## Delivered slice

- The generated internal opcode registry appends stable wire IDs 27–30 for
  immutable UTF-8 string constants, exact byte-string equality, explicit
  `G4000` panic, and declared-`never` direct calls. Existing wire IDs and
  feature-zero encodings remain unchanged.
- Function bytecode owns a bounded, length-delimited UTF-8 string pool. The
  codec preserves embedded NUL bytes, rejects malformed UTF-8 and noncanonical
  feature use, and enforces per-function string count, byte, encoded-size, and
  verification-work limits before publishing decoded output. Program decoding
  and verification additionally enforce aggregate string count and byte caps.
- Verification admits `string` registers and parameters and `never` results,
  while excluding `void` and `never` from runtime value registers. It checks
  exact string operand types, definite assignment, constant IDs, terminating
  panic placement, and the distinct no-normal-successor shape of
  `CALL_DIRECT_NEVER`.
- The VM execution context owns and exactly interns immutable string bytes
  behind opaque references. Foreign and forged references are rejected before
  frame allocation. String results remain valid for the lifetime of their
  execution context.
- Explicit panic returns status `PANIC` and optionally publishes code `G4000`,
  exact message bytes, the primary function symbol and complete call span, and
  source-first frames from the failing function through its callers. A
  standalone function has no program symbol and therefore uses the all-zero
  symbol sentinel; program execution records exact stable symbols.
- `CALL_DIRECT_NEVER` uses the existing explicit heap frame stack and never
  creates a result destination or normal successor. A normally returning body
  cannot verify as `never`; the VM also rejects a normal return from a frame
  entered through a never call. Panic and all failures preserve caller
  destinations and the root result.
- MIR lowering now copies string pools and lowers `CONSTANT_STRING`,
  `EQUAL_STRING`, `PANIC`, and `CALL_NEVER`, preserving stable symbols and
  source spans. A full MIR panic/never program is lowered, encoded, decoded,
  and executed by the VM with exact outcome comparison to the MIR oracle.
- The bytecode fuzz target accepts the new run statuses, supplies bounded
  execution contexts for string-bearing functions/programs, and retains
  canonical decode/re-encode checks.

This implements the existing D036 explicit-panic and D037 declared-`never`
execution decisions. It does not change Ruda source semantics or define the
public `.rudab` bundle, managed-object, GC, or native stack-layout contracts.

## Verification coverage

- Registry, disassembly, and codec tests pin all four opcode IDs, operand
  layouts, flags, string rendering, exact feature-string bytes, canonical
  decode/re-encode identity, embedded NUL and Unicode payloads, every-prefix
  truncation, invalid UTF-8, corrupt string IDs, unsupported features, limits,
  and transactional allocation failures.
- Verifier tests reject wrong and uninitialized string operands, nonterminal or
  wrong-typed panic, invalid never-call descriptors/result types, and invalid
  constant IDs.
- VM tests cover literal materialization, equal and unequal exact strings,
  direct standalone panic, a caller-to-never-callee panic, exact two-frame
  symbol/span order, result nonpublication, bounded string interning, forged
  and cross-execution references, and deterministic allocation cleanup.
- MIR differential tests compare the direct MIR panic oracle with an unordered
  two-function MIR program lowered through program encode/decode to VM
  execution, including exact embedded-NUL message bytes, `G4000`, symbols,
  spans, frame order, `CALL_DIRECT_NEVER`, and nonpublication.
- Independent production and coverage reviews found no P0/P1 implementation
  defect after the negative and cross-layer tests above were added. Static
  analysis findings on analyzer-only nullable allocation paths were made
  explicit and the analyzer then completed without findings.

## Commands and exit status

| Command | Exit | Result |
|---|---:|---|
| `cmake --build --preset debug --clean-first --parallel 4 && ctest --preset debug --output-on-failure` | 0 | clean warnings-as-errors Debug build; 1/1 passed |
| `cmake --build --preset release --parallel 4 && ctest --preset release --output-on-failure` | 0 | optimized build; 1/1 passed |
| `cmake --build --preset asan --parallel 4 && ASAN_OPTIONS=halt_on_error=1 ctest --preset asan --output-on-failure` | 0 | AddressSanitizer run; 1/1 passed without report |
| `cmake --build --preset asan-ubsan --parallel 4 && ASAN_OPTIONS=halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ctest --preset asan-ubsan --output-on-failure` | 0 | AddressSanitizer and UBSan run; 1/1 passed without report |
| `cmake --build --preset tsan --parallel 4 && TSAN_OPTIONS=halt_on_error=1 ctest --preset tsan --output-on-failure` | 0 | ThreadSanitizer run; 1/1 passed without report |
| `clang --analyze -std=c17 -Ibootstrap/include -Iruntime/include -Itests/c-unit bootstrap/src/base/checked_i64.c bootstrap/src/bytecode/bytecode.c bootstrap/src/bytecode/program.c bootstrap/src/bytecode/vm.c bootstrap/src/bytecode/mir_lower.c bootstrap/src/mir/mir.c tests/c-unit/test_bytecode.c tests/c-unit/test_bytecode_vm.c tests/c-unit/test_mir_bytecode.c tests/fuzz/bytecode_fuzz.c` | 0 | no static-analyzer findings |
| `cc -std=c17 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror -Ibootstrap/include -Iruntime/include -c tests/fuzz/bytecode_fuzz.c -o /tmp/ruda-bytecode-fuzz-iteration5.o` | 0 | string-aware fuzz target compiles warning-free as C17 |
| `./tools/check-generated` | 0 | generated opcode and other artifacts current |
| `./tools/check-doc-links && ./tools/check-spec-terms && ./tools/check-example-syntax && ./tools/check-syntax-coverage` | 0 | documentation checks passed; 74/74 syntax kinds and 115/115 grammar productions covered |
| `cmake --preset fuzz` | 1 | expected configuration failure: local AppleClang distribution has no libFuzzer runtime |

The ASan result is not an LSan claim. Deterministic failure allocators provide
the local ownership evidence. No successful fuzz campaign is claimed.

## Remaining I3 work

I3 remains open for the public `.rudab` section/signature policy, stable loader
diagnostics, representative binary fuzz seeds and a sanitizer-clean fuzz
campaign, source-snapshot resolution, recursive/task-aware stack traces,
records/enums/closures and their managed descriptors/root maps/GC,
unsafe/capability restrictions, VM-backed commands, cold-start measurement,
Tier 1 platform evidence, and generated-program differential coverage. I4 C17
AOT and VM/native equivalence remain separate open gates.
