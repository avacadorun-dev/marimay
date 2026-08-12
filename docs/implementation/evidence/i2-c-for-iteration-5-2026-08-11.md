# I2 C-style `for` lowering — iteration 5 — 2026-08-11

Iteration status: complete; gate status is `in-progress`.  
Scope: lower the accepted D033 C-style loop header and the primitive compound
assignments needed by ordinary counter loops.

## Implemented

- `for let name = initializer; condition; step { ... }` lowers into explicit
  initializer, condition, body, step, and exit MIR blocks. The condition is
  exact `bool`, the body back-edge enters the step, and the step returns to
  condition evaluation.
- The header binding is mutable, initialized exactly once, visible to the
  condition/body/step, and removed from lexical lookup at loop exit. Its scope
  is distinct from both the enclosing block and the nested body block.
- `continue` in a C-style loop targets the step block, while `break` targets the
  exit. The existing loop-context stack therefore preserves nearest-loop
  behavior for future nested C-style/`while` combinations.
- Primitive `int` `+=`, `-=`, `*=`, `/=`, and `%=` lower through the existing
  checked i64 MIR operations followed by an explicit binding copy. They require
  an initialized mutable binding and retain the interpreter's overflow and
  division-by-zero behavior.

## Coverage added

- The MIR oracle executes a counter loop containing compound addition,
  conditional `continue`, and conditional `break`; the test pins the result
  after both control edges and several step executions.
- Access to the header binding after loop exit produces `R0001` at the exact
  use span.
- The existing mutable-local test now executes `+=`, while fault injection and
  sanitizer runs cover the added CFG blocks, loop context, scoped binding, and
  compound-operation temporaries.
- A syntactically valid iterable `for const name of expression` still returns
  `RUDA_CHECK_INCOMPLETE_LANGUAGE`; the checker does not fake an iterator
  protocol before nominal types, calls, cleanup, and standard iterators exist.

## Verification

| Command | Exit | Result |
|---|---:|---|
| Debug build and `ctest --preset debug --output-on-failure` | 0 | warnings-as-errors C17 build; 1/1 test executable passed |
| Release build and `ctest --preset release --output-on-failure` | 0 | optimized build; 1/1 passed |
| ASan build and `ctest --preset asan --output-on-failure` | 0 | 1/1 passed without sanitizer report |
| ASan+UBSan build and `ctest --preset asan-ubsan --output-on-failure` | 0 | 1/1 passed without sanitizer report |
| TSan build and `ctest --preset tsan --output-on-failure` | 0 | 1/1 passed without sanitizer report |
| `clang --analyze` on checker, MIR, and type-table sources | 0 | zero static-analyzer findings |

## Remaining I2 gate work

Iterable `for` remains dependent on the first-party iterator/resource protocol.
Compound operations outside the current exact `int` slice remain explicit
incomplete rather than silently adopting C promotions. `defer`, unreachable and
shadow warnings, calls, nominal data, match, `?`, effects, multi-file package
resolution, service queries, and randomized evaluator comparison are still open.

The public CLI checker remains unavailable, and this iteration changes no
package format, ABI, or external tool behavior. VM/native differential evidence
remains pending the I3/I4 engines.
