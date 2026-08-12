# I2 staged signatures and loop control — iteration 4 — 2026-08-11

Iteration status: complete; gate status is `in-progress`.  
Scope: establish the required declaration/signature-before-body ordering and
complete nearest-`while` control for the current internal source slice.

## Implemented

- The checker now collects all visible top-level function names, exact
  parameter/result types, stable symbols, and canonical function-type IDs
  before lowering any body. Duplicate declaration and signature diagnostics
  therefore do not depend on source body order.
- Function signatures are canonical entries in the owned type table. Two
  distinct declarations with the same exact parameter/result types share one
  function type while retaining distinct stable declaration symbols.
- Parameter-name duplication is diagnosed during the signature phase. If any
  signature is invalid, body lowering is skipped; no partially checked
  functions are presented as an authoritative result.
- `break` and `continue` now target the nearest active `while` through an
  explicit per-function loop-context stack. The former branches to the loop
  exit and the latter to condition reevaluation. Both compose with nested
  blocks and conditional paths.
- `T0008 LOOP_CONTROL_OUTSIDE_LOOP` is a generated stable diagnostic, and the
  normative language specification now explicitly states that loop control
  outside a loop is a compile error.

## Coverage added

- A function descriptor is checked for exact kind, arity, parameter types, and
  result type; two equal signatures prove canonical type identity while their
  declaration symbols remain unequal.
- A duplicate declaration is found before an earlier unsupported call body can
  enter lowering, proving phase ordering rather than merely refactoring names.
- Duplicate parameters pin the second declaration span.
- The MIR oracle executes `continue` and `break` inside conditional paths of a
  loop and produces the expected result. Outside-loop forms pin `T0008` to the
  keyword span.
- Checker fault injection continues to traverse a function containing both a
  branch and loop, now also covering signature ownership, function-type
  interning, and loop-stack allocation.

## Verification

| Command | Exit | Result |
|---|---:|---|
| Debug build and `ctest --preset debug --output-on-failure` | 0 | warnings-as-errors C17 build; 1/1 test executable passed |
| Release build and `ctest --preset release --output-on-failure` | 0 | optimized build; 1/1 passed |
| ASan build and `ctest --preset asan --output-on-failure` | 0 | 1/1 passed without sanitizer report |
| ASan+UBSan build and `ctest --preset asan-ubsan --output-on-failure` | 0 | 1/1 passed without sanitizer report |
| TSan build and `ctest --preset tsan --output-on-failure` | 0 | 1/1 passed without sanitizer report |
| `clang --analyze` on checker, MIR, and type-table sources | 0 | zero static-analyzer findings |
| Generated/doc/term/example/syntax checks | 0 | generated `T0008` artifacts current; links, terminology, examples, 76/76 syntax kinds, and 117/117 productions green |

## Remaining I2 gate work

This is the first two phases of the required package resolver, not the complete
resolver. The API still accepts one AST/source snapshot; imports, visibility,
multi-file package ownership, nominal declarations, eligible overload sets,
calls, and serialized interfaces remain absent. C-style and iterable `for`
loops, `defer`, and unreachable/shadow warnings are also outside this slice.

The internal checker remains intentionally unavailable through the CLI. No
package format, ABI, external command behavior, or previously accepted source
semantics changed. VM/native differential evidence becomes applicable only
after I3/I4; current executions use the verified unoptimized MIR oracle.
