# I2 logical short-circuiting — iteration 9 — 2026-08-11

Iteration status: complete; gate status is `in-progress`.  
Scope: implement the already normative short-circuit semantics of `&&` and
`||` in the current pure `bool`/MIR slice. This does not add syntax or change
the language contract in `docs/spec/language.md`.

## Implemented

- Logical operands are checked as `bool`; invalid operands produce the stable
  `T0003 INVALID_OPERATOR` diagnostic instead of falling through to the
  incomplete-language boundary.
- `&&` and `||` lower to explicit MIR branch, shortcut, right-hand-side, and
  join blocks. The selected branch copies one `bool` value into the result
  register, so the right operand is not executed when the left operand decides
  the result.
- The same lowering is used by functions and bounded top-level constant
  evaluation. A skipped right operand containing division by zero is therefore
  neither executed nor diagnosed, while an evaluated right operand retains
  checked arithmetic behavior.
- Expression lowering now composes control-flow-producing expressions by
  terminating the actual final block of each child expression. `if`, `while`,
  and C-style `for` conditions branch from the expression's final block;
  C-style `for` also connects initializer and step expressions from their
  actual final blocks.
- MIR remains non-SSA at this stage: the logical result register is assigned
  once on each mutually exclusive predecessor. The verifier's definite-
  assignment intersection proves that the register is initialized at the join.

## Coverage added

- Function and constant fixtures prove both skipped and evaluated right
  operands for `&&` and `||`, including division-by-zero sentinels.
- Nested forms cover control-flow generation on both the left and right:
  `true && (false || true)` and `(false || true) && true`.
- Executable MIR fixtures cover logical expressions in `if`, `while`, and all
  expression-bearing portions of C-style `for`: initializer, condition, and
  assignment step. Both true and false inputs are interpreted and asserted.
- The checker allocator-failure walk includes an `if` condition with short-
  circuit CFG lowering and still proves failure atomicity and zero live
  allocations for every injected failure.

## Verification

| Command | Exit | Result |
|---|---:|---|
| Debug build and `ctest --preset debug --output-on-failure` | 0 | warnings-as-errors C17 build; 1/1 test executable passed |
| Release build and `ctest --preset release --output-on-failure` | 0 | optimized build; 1/1 passed |
| ASan build and `ctest --preset asan --output-on-failure` | 0 | 1/1 passed without sanitizer report |
| ASan+UBSan build and `ctest --preset asan-ubsan --output-on-failure` | 0 | 1/1 passed without sanitizer report |
| TSan build and `ctest --preset tsan --output-on-failure` | 0 | 1/1 passed without sanitizer report |
| `clang --analyze` on checker, MIR, type-table, and checker-test sources | 0 | zero static-analyzer findings |
| `./tools/check-generated` | 0 | all committed generated artifacts are current |
| Doc-link, spec-term, example-syntax, and syntax-coverage checks | 0 | all checks green; 76/76 syntax kinds and 117/117 grammar productions covered |

## Remaining I2 gate work

This iteration covers only logical operators over the currently implemented
primitive `bool` core. Records, enums, interfaces, generics, closures,
`Option`, `Result`, postfix `?`, `defer`, exhaustive `match`, imports and
visibility, effect metadata, the compiler service, and public `ruda check`
remain open. No trust, agent provenance, CI policy, runtime sandbox, package
format, ABI, VM/native backend, or capability rule was added; those concerns
remain in their existing architectural layers.
