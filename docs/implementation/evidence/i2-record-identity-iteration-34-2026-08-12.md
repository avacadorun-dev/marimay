# I2 stable nominal record identity — iteration 34 — 2026-08-12

Iteration status: complete; gate status remains `in-progress`.  
Scope: same-package `identity.same`, exact nominal typing, verified typed MIR,
and stable managed-reference identity in the execution oracle.

## Normative basis

D007 and language section 5.4 already require managed reference identity for
every nominal record. LANG-031 makes the operation's boundary explicit:
`identity.same(a, b)` accepts exactly two values of the same nominal record
type after alias resolution, evaluates them once from left to right, and
observes object identity independently of fields or backend addresses. This
iteration implements that existing contract; it does not change record
representation or accept D007's provisional production-performance gate.

## Implemented

- The checker recognizes the unshadowed prelude spelling `identity.same`,
  rejects type arguments and every non-record domain, resolves aliases, and
  requires the second operand to have the exact first operand's nominal type.
  `identity` remains an ordinary lexically shadowable name, not a keyword.
- Both arguments lower once in source order. A terminating first or second
  operand propagates normally; no speculative or duplicate evaluation is
  introduced.
- Typed MIR adds `RECORD_IDENTITY_SAME` with a nominal record-layout operand,
  two exact record registers, and a `bool` result. Definite-assignment and type
  verification cover the operation with the same bounded dataflow as all
  other MIR instructions.
- The execution oracle compares opaque execution-owned record handles. It does
  not compare C pointers, field contents, or mutable state. This supplies a
  stable semantic model for a future moving collector, whose implementation
  must preserve the same logical handle identity.
- Runtime execution validates both handles and their stored type/field layout
  against the verified function layout before observing identity. Forged,
  missing, wrong-type, and incompatible-layout values fail rather than being
  reinterpreted.

## Coverage

The checker suite proves that one object and an alias compare `true`, including
after mutation, while two allocations with equal fields compare `false`.
Aliases to the same record type are accepted. Different nominal records,
integers, strings, enums, one/three arguments, type-argument and constant
contexts are rejected. A division-by-zero first operand followed by an
overflowing second operand proves left-to-right evaluation.

The direct MIR suite constructs two objects in one execution, compares
same/different handles, and covers invalid references. Verifier mutations cover
uninitialized operands, wrong operand/result types, and missing layout IDs; a
runtime layout mutation proves descriptor validation. Register limits and the
checker allocation-failure sweep include identity lowering.

## Verification

| Command | Exit | Result |
|---|---:|---|
| Debug build and `ctest --test-dir build/debug --output-on-failure` | 0 | warnings-as-errors C17 build; 1/1 passed |
| Release build and `ctest --test-dir build/release --output-on-failure` | 0 | optimized build; 1/1 passed |
| ASan build and strict `ASAN_OPTIONS=halt_on_error=1` test | 0 | 1/1 passed without sanitizer report |
| ASan+UBSan build and strict halt-on-error test | 0 | 1/1 passed without sanitizer report |
| TSan build and strict `TSAN_OPTIONS=halt_on_error=1` test | 0 | 1/1 passed without sanitizer report |
| `clang --analyze` on checker, MIR, and affected C-unit sources | 0 | zero output/findings |
| Generated, syntax-coverage, doc-link, spec-term, example-syntax, and example-format checks | 0 | generated artifacts current; 74/74 syntax kinds and 115/115 productions covered; docs/examples consistent |

This macOS AddressSanitizer runtime aborts before test execution when
`detect_leaks=1` is requested because integrated LeakSanitizer is unsupported;
the successful ASan row therefore does not claim an LSan run. Failure-injected
allocator sweeps separately assert zero live allocations and live bytes.

## Deliberate boundary

This is the local checker/MIR semantic oracle. Qualified imported resolution,
production bytecode encoding, the C17 backend, VM/native differential tests,
moving/tracing/generational GC, root maps, escape/scalar replacement, and D007
workload measurements remain owned by I3–I5. Stable opaque handles demonstrate
the required observable result; they do not freeze the production object ABI
or prove that managed-reference allocation cost is acceptable.
