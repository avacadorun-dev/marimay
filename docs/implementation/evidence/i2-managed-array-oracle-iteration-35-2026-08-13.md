# I2 managed-array MIR oracle — iteration 35 — 2026-08-13

Gate: I2 typed core and MIR oracle.
Iteration status: complete; milestone gate remains `in-progress`.
Baseline: `8e034f8` (`docs: define compiler architecture boundaries`).
Authority: accepted [D045](../../design/decisions.md),
[language indexing rules](../../spec/language.md), and
[execution ordering](../../spec/execution.md).
Environment: macOS arm64, AppleClang 17.0.0, CMake 4.4.2.

## Delivered slice

- Typed MIR now owns exact array layouts, construction argument tables, and
  `new`, `length`, checked-index, `get`, and `set` instructions.
- `ARRAY_REQUIRE_INDEX` is an explicit effect-order barrier for indexed
  assignment lowering. It checks the complete access span and publishes the
  array handle only on success, so a following RHS call cannot run after an
  invalid destination.
- The bounded MIR oracle executes mutable, identity-preserving arrays in its
  execution-owned arena. Construction is failure-atomic, empty arrays are
  supported, aliases observe writes, and exact-type managed elements must be
  owned by the same execution.
- Failed reads, writes, and prechecks return `RUDA_MIR_INTERPRET_PANIC` with
  code `G4002`, the locale-independent D045 message, the current symbol, and
  the complete access span. Decimal formatting is host-library independent and
  covers both `INT64` extremes.
- Array count, per-array length, total-element, allocation-size, and verifier
  work are checked before publication. Inspection APIs remain explicitly
  oracle-internal.
- The verifier uses one managed-layout type index for records, enums, and
  arrays. Primitive layout IDs and cross-kind collisions are rejected, and
  indexed lookup removes the prior explicit unaccounted quadratic scan.
- Execution handles carry a fresh execution-provenance identity plus an
  internal managed-kind tag and arena index. Independently verified functions
  sharing an execution cannot reinterpret equal `(type, index)` pairs across
  record, enum, string, and array arenas; populated foreign executions and
  dispose/re-init cycles reject handles from the prior execution identity.
- MIR and the managed runtime now obtain process-unique provenance from one
  portable base primitive: C17 atomics on Clang/GCC and MSVC Interlocked on
  the configured Windows C17 toolchain. Managed values returned by host-call
  handlers are revalidated at the execution boundary before publication.
- Bytecode lowering and the C17 emitter enumerate the new instructions and
  reject them as unsupported. This prevents a partial backend from silently
  accepting array MIR before its representation and panic path exist.

This is an internal typed-MIR checkpoint. It does not yet lower source
`arrayOf`/array literals/indexing, implement `push`, serialize array bytecode,
execute arrays in the VM, emit them as portable C17, or satisfy the matrix
multiplication acceptance program.

## Verification coverage

- Positive execution covers non-empty and typed-empty construction, nested
  managed arrays, `length`, `get`, `set`, stable aliasing, exact `f64` bits,
  same-execution ownership, populated foreign-execution rejection, and stale
  handle rejection after dispose/re-init. A host-call fixture rejects the same
  foreign handle and accepts the corresponding local execution-owned handle.
- Bounds vectors cover `-1`, `length`, `INT64_MIN`, and `INT64_MAX`, with exact
  G4002 bytes, symbol, and source spans. Injected failure of diagnostic-message
  allocation is leak-free, and a no-panic-output run proves status does not
  depend on requesting diagnostic materialization.
- A call-handler fixture proves an invalid indexed assignment precheck invokes
  the RHS zero times; the same MIR with a valid index invokes it once and
  commits exactly one write.
- Negative verifier fixtures cover an uninitialized third operand, invalid
  element descriptors, primitive array type IDs, and within-function
  record/array type-ID collision. A shared-execution regression covers
  cross-function equal-type record/array handles and their distinct kind tags.
- Runtime-limit fixtures cover array length, total elements, and array count.
  A fail-every-allocation sweep reaches success and leaves zero live allocations
  and bytes after every attempt.

## Commands and exit status

| Command | Exit | Result |
|---|---:|---|
| `cmake --build --preset debug --parallel 4 && ctest --preset debug --output-on-failure` | 0 | warnings-as-errors build; 1/1 passed in 19.36 s |
| `cmake --build --preset release --parallel 4 && ctest --preset release --output-on-failure` | 0 | optimized build; 1/1 passed in 54.14 s under concurrent validation load |
| `cmake --build build/asan-ubsan --clean-first --parallel 4 && ctest --preset asan-ubsan --output-on-failure` | 0 | clean ASan+UBSan rebuild; 1/1 passed in 57.90 s without report under concurrent validation load |
| `cmake --build build/tsan --clean-first --parallel 4 && ctest --preset tsan --output-on-failure` | 0 | clean TSan rebuild; 1/1 passed in 59.62 s without report under concurrent validation load |
| AppleClang static analyzer over `bootstrap/src/mir/mir.c` and `bootstrap/src/base/provenance.c` with the debug include/configuration flags | 0 | no diagnostics after making the non-empty construction allocation invariant explicit |
| `./tools/check-generated` | 0 | generated opcode and other artifacts current |
| `./tools/check-doc-links && ./tools/check-spec-terms && ./tools/check-example-syntax && ./tools/check-syntax-coverage` | 0 | documentation checks passed; registry coverage unchanged |
| `git diff --check` | 0 | patch whitespace clean |

No VM/native differential claim is made for arrays in this iteration: both
backends deliberately reject the new MIR surface. I2, I3, I4, and I5 remain
`in-progress`; the next vertical checkpoint must assign verified bytecode
opcodes and VM execution before the C17 emitter can share the same array panic
contract.
