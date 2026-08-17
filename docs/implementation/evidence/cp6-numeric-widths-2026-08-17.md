# Preview numeric widths — checkpoint 6 — 2026-08-17

Checkpoint status: locally complete for the numeric/text preview slice. This
evidence does not mark I2–I5 verified, does not start I6, and does not claim a
Tier 1 release or public 0.1.0.

## Authority and scope

The implementation follows the already accepted D035 shift rules, D042
division/remainder rules, D044 binary64 rules, and D046 formatter contract,
plus `docs/spec/language.md` §5.1 and `docs/spec/standard-library.md` §3.
No new source-language decision was needed: the normative documents already
require all integer widths, explicit conversions, checked overflow, and the
raw-binary32 limitation for `formatF32`.

## Delivered slice

- Added a central C17 checked-integer core for `i8/i16/i32/i64` and
  `u8/u16/u32/u64`, including checked, wrapping, saturating, arithmetic,
  bitwise, shifts, division/remainder, negate, and loss-detecting explicit
  conversions.
- Extended checker literals, typed top-level constants, compound assignments,
  MIR constants, verification, and lowering for every integer width. The
  legacy `i64` bytecode/type values remain stable; new scalar tags are
  appended and version-safe.
- Extended the shared MIR oracle, encoded bytecode VM, and generated C17
  backend with exact-width integer bits, signed/unsigned interpretation,
  D035/D042 traps, and identical result/trap spans.
- Added D046 `formatF64` and raw-binary32 `formatF32` helpers without locale,
  `printf`, or production heap use. The implementation covers shortest
  round-trip text, plain/scientific thresholds, signed zero, infinities, and
  NaN sign spelling.
- Kept string values on the existing immutable UTF-8 MIR/VM/native path and
  added numeric-width formatter/source fixtures. `std/io`, `println`, and
  user stdout remain later checkpoint work.

## Test coverage

- `test_checked_integer.c`: all eight widths, cross-width conversions and
  negative conversion cases, checked/wrapping/saturating arithmetic, shifts,
  division/remainder, `MIN/-1`, zero divisors, invalid counts, and malformed
  values.
- `test_checker_operators.c`: typed literals, typed top-level constants,
  boundary values, all-width compound arithmetic/bitwise/shift expressions,
  and MIR interpretation.
- `test_bytecode_model.c`: all-width `CONST_INTEGER` encode/decode round trips
  plus malformed register-type verifier rejection.
- `test_native_c_emitter_scalar.c`: all-width MIR→VM→native differential
  results, O0/O2 generated-C execution, overflow/div-zero/invalid-shift
  traps, signed `MIN/-1` division/remainder identity, and signed/unsigned
  comparison behavior.
- `test_number_format.c` and `tests/format/numeric-widths.*`: D046 f64/f32
  boundary spellings, shortest round trips, NaN/infinity/signed-zero cases,
  and formatter syntax fixtures. `examples/numeric_widths.ruda` exercises
  the public numeric surface without stdout.
- Existing string MIR/VM/native tests remain part of the full suite.

## Local validation

Environment: macOS Darwin 25.3.0, arm64; Apple Clang 17.0.0; CMake 4.4.2.

| Command | Result |
|---|---|
| `./tools/check-architecture` | exit 0 |
| `./tools/check-c-memory-policy` | exit 0 |
| `./tools/check-generated` | exit 0 |
| `./tools/check-doc-links` | exit 0 |
| `./tools/check-spec-terms` | exit 0 |
| `./tools/check-example-syntax` | exit 0 |
| `git diff --check` | exit 0 |
| `cmake --build --preset debug --parallel` | exit 0 |
| `ctest --preset debug --output-on-failure` | 1/1 passed, 125.01 s |
| `cmake --build --preset release --parallel` | exit 0 |
| `ctest --preset release --output-on-failure` | 1/1 passed, 131.96 s |
| `cmake --build --preset asan-ubsan --parallel` | exit 0 |
| `ctest --preset asan-ubsan --output-on-failure` | 1/1 passed, 190.61 s; no sanitizer report |
| `build/debug/ruda fmt tests/format/numeric-widths.expected.ruda --check` | exit 0 |
| `build/debug/ruda fmt examples/numeric_widths.ruda --check` | exit 0 |

All three configurations use the same full C-unit acceptance target.

## Explicit limitations

`f32` arithmetic, f32 literals, and NaN canonicalization are intentionally not
implemented from D044. `formatF32` formats raw binary32 bits only, exactly as
D046 states. Public `std/convert` wiring, `std/io`, `println`, `ruda run`,
and user stdout are not claimed here; they belong to the later execution/tool
checkpoints.
