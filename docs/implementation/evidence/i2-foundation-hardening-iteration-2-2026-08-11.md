# I2 foundation hardening — iteration 2 — 2026-08-11

Iteration status: complete; gate status is `in-progress`.  
Scope: remove the first package-scale type-identity bottleneck and make MIR
verification explicitly bounded and failure-atomic.

## Implemented

- The dynamic type table now uses a deterministic canonical index instead of a
  linear descriptor scan. Its key is a SHA-256 digest over a fixed-width,
  little-endian encoding of type kind, nominal symbol, function parameter
  count, and canonical argument IDs. A lookup verifies the complete descriptor
  before returning an ID, so a digest collision or corrupt index cannot return
  the wrong type.
- Type insertion is failure-atomic across descriptor ownership, descriptor
  vector growth, and canonical-map insertion. If the index insertion fails,
  the vector length is rolled back and the new descriptor is released while
  every earlier canonical entry remains usable.
- The MIR verifier builds a compact predecessor index once rather than scanning
  every block for predecessors during every dataflow pass. It uses one reusable
  register-fact scratch buffer and a monotone definite-assignment fixed point.
- `max_verify_work` is now an explicit nonzero per-function limit. Structural
  validation, predecessor construction, intersections, transfers, comparisons,
  and final use validation all consume the budget and return
  `RUDA_MIR_VERIFY_LIMIT_REACHED` before continuing past it.
- Every verifier allocation size is checked and retained for exact allocator
  accounting. The MIR interpreter also validates instruction/register inputs at
  its internal execution boundary instead of depending on an implicit pointer
  precondition.

## Coverage added

- 512 distinct nominal types are interned, then re-interned in reverse order;
  IDs remain canonical and neither the descriptor table nor the index grows on
  the second pass.
- Failure injection at the type-index insertion boundary proves rollback, zero
  leaks, and continued canonical lookup of an existing type.
- A 256-block linear CFG carrying one assigned register verifies within a
  10,000-unit work budget.
- A valid function with a one-unit budget terminates with the documented limit
  status.
- Failure injection walks every verifier allocation boundary and checks both
  live allocation count and byte count against the pre-verification baseline,
  then against zero after function disposal.

## Verification

| Command | Exit | Result |
|---|---:|---|
| Debug build and `ctest --preset debug --output-on-failure` | 0 | warnings-as-errors C17 build; 1/1 test executable passed |
| Release build and `ctest --preset release --output-on-failure` | 0 | optimized build; 1/1 passed |
| ASan build and `ctest --preset asan --output-on-failure` | 0 | 1/1 passed without sanitizer report |
| ASan+UBSan build and `ctest --preset asan-ubsan --output-on-failure` | 0 | 1/1 passed without sanitizer report |
| TSan build and `ctest --preset tsan --output-on-failure` | 0 | 1/1 passed without sanitizer report |
| `clang --analyze` on the type table, MIR, and checker sources | 0 | zero static-analyzer findings after removing a dead union-find assignment and making interpreter operand checks explicit |

## Remaining I2 gate work

This iteration removes the linear interner implementation and the verifier's
repeated predecessor scans; it does not satisfy the complete I2 language,
service, or `ruda check` gate. Package-scale interner time/memory measurements,
randomized malformed-CFG generation, and broader graph shapes remain required
before these components can be accepted for untrusted package and MIR input.

No VM/native differential result applies yet: the I2 MIR interpreter remains
the semantic oracle for the engines introduced in I3 and I4. No source syntax,
language semantics, package format, ABI, or user-visible toolchain behavior
changed in this hardening iteration.
