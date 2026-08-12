# I2 checked `i64` shifts — iteration 12 — 2026-08-11

Iteration status: complete; gate status is `in-progress`.  
Scope: close the previous shift-semantics ambiguity through D035 and implement
that contract for the current `i64` typed-MIR slice.

## Decision and normative contract

- Accepted D035 requires exact same-type integer operands and a count in
  `0 <= count < bitWidth`.
- Left shift is checked multiplication by `2^count`; an unrepresentable result
  panics instead of discarding high bits.
- Signed right shift explicitly copies the sign bit. It does not inherit C17's
  implementation-defined signed-right-shift behavior.
- Unsigned right shift will zero-fill when the remaining integer widths land.
  Compound shift assignments use the same operation before assignment.
- The language and execution specifications, requirement LANG-025, decision
  registry check, and consistency audit now carry the same contract.

## Implemented

- Typed MIR adds `SHIFT_LEFT_CHECKED_I64` and `SHIFT_RIGHT_I64`. The verifier
  requires two definitely assigned `i64` inputs and an `i64` result.
- The portable oracle validates the signed count before conversion. Left shift
  operates in `uint64_t`, reconstructs the Ruda two's-complement value, and
  proves exactness by a portable arithmetic-right-shift round trip. Right shift
  performs explicit unsigned shift plus sign fill. No negative signed C shift
  or signed left-shift overflow is executed.
- Runtime oracle results distinguish checked overflow from invalid shift count
  through `RUDA_MIR_INTERPRET_OVERFLOW` and
  `RUDA_MIR_INTERPRET_INVALID_SHIFT`.
- Source `<<`, `>>`, `<<=`, and `>>=` lower to the new MIR. The bounded constant
  evaluator uses the same instructions and reports `T0010` when a required
  constant would panic.
- Non-`i64` integer shifts remain explicit incomplete boundaries; non-integer
  operands receive `T0003`.

## Coverage added

- MIR execution covers valid negative left shifts reaching `INT64_MIN`, zero,
  positive overflow, negative overflow, negative and width-sized counts,
  positive right shift, sign-extending `-7 >> 1`, and `INT64_MIN >> 63`.
- A malformed-MIR fixture proves `bool` shift operands are rejected.
- Checker fixtures cover constants, precedence, direct and compound shifts,
  runtime overflow/invalid-count propagation, non-integer diagnostics, and the
  remaining-`i32` incomplete boundary.
- Constant negative fixtures cover overflowing left shift, negative count, and
  count equal to the width.
- The allocator-failure program includes `<<= 0`. The canonical formatter
  corpus now contains binary and compound shift spellings, while the existing
  complete parser grammar already covers both forms.

## Verification

| Command | Exit | Result |
|---|---:|---|
| Debug build and `ctest --preset debug --output-on-failure` | 0 | warnings-as-errors C17 build; 1/1 test executable passed |
| Release build and `ctest --preset release --output-on-failure` | 0 | optimized build; 1/1 passed |
| ASan build and `ctest --preset asan --output-on-failure` | 0 | 1/1 passed without sanitizer report |
| ASan+UBSan build and `ctest --preset asan-ubsan --output-on-failure` | 0 | 1/1 passed without sanitizer report |
| TSan build and `ctest --preset tsan --output-on-failure` | 0 | 1/1 passed without sanitizer report |
| `clang --analyze` on checker, MIR, type-table, checker-test, and MIR-test sources | 0 | zero static-analyzer findings |
| `./tools/check-generated` | 0 | all committed generated artifacts are current |
| Doc-link, spec-term, example-syntax, and syntax-coverage checks | 0 | D001–D035 consistent; 76/76 syntax kinds and 117/117 grammar productions covered |

## Remaining I2/I4 work

D035 still requires implementations for `i8`/`i16`/`i32`, unsigned widths,
contextual integer-literal typing across those widths, and eventual VM/native
differential tests. Only the unoptimized I2 MIR oracle exists today, so this
iteration makes no backend-equivalence claim.

The larger nominal/generic/error/match/effect/compiler-service I2 work remains
open. No package, ABI, concurrency, capability, trust, or tooling-policy
contract changed beyond the arithmetic semantics explicitly recorded in D035.
