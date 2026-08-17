# Preview numeric widths — checkpoint 6 — 2026-08-17

Checkpoint status: locally complete after a requirements-to-tests re-audit on
2026-08-18. I2–I5 remain `in-progress`; I6 remains `not-started`. This does
not claim a Tier 1 release or public 0.1.0.

## Authority and scope

The slice follows accepted D035, D042, D044, D046, and D047; language §5.1;
and standard-library §3. D047 was added during the audit because the earlier
documents required policy operations and loss-detecting conversions without
fixing their complete callable signatures or conversion syntax.

`f32` remains a raw-binary32 formatter input only. This checkpoint does not
add f32 literals, arithmetic, NaN canonicalization, stdout, `println`, or a
public execution command.

## Delivered vertical slice

- The C17 checked-integer owner implements all eight widths, checked,
  wrapping, and saturating arithmetic, D035 shifts, D042 division/remainder,
  negate, bitwise operations, comparisons, and range-aware conversion.
- The checker recognizes exact-width literals/constants/operators, the closed
  D047 policy-function family, and `value as Target`. Lossy conversions return
  the exact canonical `Option<Target>` nominal type; no implicit conversion or
  integer/float conversion was added.
- Typed MIR, bytecode metadata/codec/verifier, encoded VM execution, and the
  generated strict-C17 backend carry exact integer bits. Signed values are
  reconstructed without host signed-overflow or implementation-defined shift
  behavior.
- `std/convert/convert.ruda` publishes `formatF32` and `formatF64`. Imported
  calls lower to formatter MIR operations rather than unresolved extern calls.
  D046 formatting uses no locale, host `printf`, or production heap call.
- The original checkpoint commit was not accepted on green-suite status alone.
  The re-audit closed missing source policy/conversion lowering, canonical
  numeric `Option` identity, public `std/convert` wiring, signed native result
  canonicalization, all 24 public policy-name checks, registered formatter
  fixtures, requirements traceability, and executable formatter coverage.

## Requirements-to-tests coverage

| Checkpoint-6 claim | Executable evidence |
|---|---|
| eight integer widths; checked/wrapping/saturating operations | `test_checked_integer.c` runs every operation family for every width; `test_checker_numeric_policy.c` resolves all 24 D047 public names and proves ordinary declaration shadowing; `examples/numeric_cp6.ruda` carries every width through source lowering |
| explicit conversions | the source fixture exercises widening, narrowing, and signedness loss; checker negatives reject an unhandled lossy result; central conversion tests cover signed/unsigned extrema |
| D035 shifts and D042 division/remainder | all-width central cases plus all-width MIR/VM/native ordinary results and exact overflow/divide-by-zero/invalid-shift identities in `test_native_c_emitter_scalar.c` |
| accepted D044 `f64`, strings, D046 formatters | existing f64 and counted-string engine suites plus MIR/VM/native `formatF64(415.0)` and raw-binary32 `formatF32(0x3f800000)` byte comparisons |
| VM/native equality and strict C17 O0/O2 | encoded-program acceptance and panic differentials plus generated-C child compilation/execution at both optimization levels |
| negatives, corruptions, formatter coverage | conversion/type negatives, policy panic entries, numeric bytecode mutations and common codec corruptions, and registered numeric input/expected formatter fixtures |

- `test_checked_integer.c` covers every operation for every width, conversion
  positives/negatives, `MIN/-1`, zero divisors, invalid and boundary shift
  counts, malformed bit patterns, and checked/wrapping/saturating outcomes.
- `test_checker_numeric_policy.c`, `test_checker_operators.c`, and
  `examples/numeric_cp6.ruda` cover source policy calls, explicit conversions,
  every public policy operation name, all width types, iterative
  Fibonacci/factorial, boundary results, and negative type diagnostics.
- `test_numeric_cp6_differential.c` checks the acceptance program through the
  MIR oracle, verified encoded/decode bytecode, VM, and generated C17 at O0/O2.
  Separate source entries compare wrapping zero-divisor and saturating invalid
  shift identities. D046 f64 and raw-f32-bit formatter operations execute in
  MIR, VM, and generated native C17; the native raw-bit harness does not add
  source `f32` literals or arithmetic. Formatter allocation failures are
  exhausted with zero live allocations and bytes after teardown.
- `test_native_c_emitter_scalar.c` covers all-width result bits, signed and
  unsigned comparisons, ordinary overflow/division/shift traps, and signed
  `MIN/-1` division and remainder across MIR, VM, and native execution.
- `test_bytecode_model.c` covers all-width constant codec round trips,
  generated opcode metadata, malformed type/register rejection, plus the
  common truncated, invalid-encoding, and verifier-failed codec corruptions.
- `test_number_format.c` and `tests/format/numeric-widths.*` cover shortest
  round trips, plain/scientific thresholds, signed zero, infinities, NaNs,
  buffer failures, raw binary32, and canonical source formatting.
- `test_mir_scalar.c`, `test_bytecode_vm_scalar.c`,
  `test_native_c_emitter_strings.c`, and `test_managed_objects.c` retain exact
  string bytes/identity/panic and allocation ownership coverage in full CTest.

## Local validation

Environment: macOS Darwin 25.3.0, arm64; Apple Clang 17.0.0; CMake 4.4.2.

The checkpoint-7 evidence records the final shared validation run because the
re-audit fixes and lexical-defer work are delivered together. The required
architecture, memory-policy, generated-artifact, documentation, syntax,
Debug, Release, and ASan/UBSan commands are listed there. The full C-unit
target contains every focused checkpoint-6 owner named above.
