# I2 `i64` bitwise operators — iteration 11 — 2026-08-11

Iteration status: complete; gate status is `in-progress`.  
Scope: implement the already normative `i64` `&`, `^`, `|` operators and their
compound-assignment forms without prematurely selecting underspecified shift
semantics.

## Implemented

- Typed MIR now has distinct `BITWISE_AND_I64`, `BITWISE_XOR_I64`, and
  `BITWISE_OR_I64` instructions. The verifier requires two definitely assigned
  `i64` inputs and an `i64` target for each instruction.
- Source binary expressions lower `&`, `^`, and `|` to those instructions.
  `&=`, `^=`, and `|=` use the same MIR operations followed by the existing
  assignment copy, preserving visible mutation and left-to-right evaluation.
- The bounded constant evaluator uses the same lowering and MIR interpreter,
  so bitwise constant values cannot drift from executable function behavior.
- Interpreter operations convert Ruda signed values to `uint64_t` bit patterns,
  operate in the unsigned domain, then reconstruct the specified signed
  two's-complement value mathematically. The existing unary `~` now uses the
  same conversion. Portable C17 code therefore does not depend on the host C
  implementation's signed representation or negative bitwise result.
- Non-`i64` integer implementations remain explicit incomplete boundaries.
  Non-integer operands produce `T0003 INVALID_OPERATOR`.

## Coverage added

- A direct MIR test executes all three instructions across a negative/positive
  pair and asserts the two's-complement result. A malformed MIR fixture proves
  that `bool` inputs are rejected by the verifier.
- Checker fixtures execute all compound-assignment forms and unary `~`, and a
  top-level constant combines `~`, `&`, `^`, and `|` with asserted value.
- Negative source fixtures cover `bool & bool`; an `i32` fixture proves that the
  checker does not falsely claim the remaining-width implementation.
- The allocator-failure source now includes a bitwise compound assignment, so
  instruction production and cleanup remain part of the failure-atomic walk.

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
| Doc-link, spec-term, example-syntax, and syntax-coverage checks | 0 | all checks green; 76/76 syntax kinds and 117/117 grammar productions covered |

## Remaining I2 gate work

Other integer widths, unsigned integers, floats, strings, and their complete
operator matrices remain open. Shift syntax is deliberately not implemented in
this iteration: the specification states that an out-of-range count panics but
must first make signed right-shift behavior and whether a representationally
overflowing left shift traps fully explicit. That semantic choice must follow
the decision/spec/test protocol rather than emerge from a C implementation.

The larger nominal/generic/error/match/effect/compiler-service I2 work remains
as listed in the roadmap. No syntax, package, ABI, runtime, trust, capability,
or tooling-policy contract changed here.
