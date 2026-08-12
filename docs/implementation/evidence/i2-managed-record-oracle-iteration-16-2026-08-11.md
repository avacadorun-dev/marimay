# I2 executable managed-record MIR oracle — iteration 16 — 2026-08-11

Historical boundary note: iteration 34 subsequently implements and verifies
the local `identity.same` operation that this iteration listed as open. Its
production-GC, imported-name, and D007 performance boundaries remain current.

Iteration status: complete; gate status is `in-progress`.  
Scope: implement the already accepted D004/D007 nominal-record construction,
projection, mutability, and managed-reference alias behavior in the local I2
source subset and its verified MIR oracle.

## Existing normative contract implemented

- A record declaration creates a nominal managed-reference type. Copying a
  record value copies its reference identity rather than its fields.
- Construction supplies every field exactly once. Source field order is
  semantically accepted in any order, shorthand `{ value }` reads the visible
  local `value`, and there are no implicit defaults.
- Fields are immutable after construction unless declared `mutable`.
  `const`/`let` controls binding rebinding and does not change deep field
  mutability.
- Expression evaluation order remains left to right even though construction
  arguments are stored in declaration-field order in typed MIR.
- This implements existing language specification §5.4 and execution
  specification §2. It does not amend source semantics or add a decision.

## Implemented

- Typed MIR now owns bounded record-layout and construction metadata plus
  verified `RECORD_NEW`, `RECORD_GET`, and `RECORD_SET` operations.
- The verifier checks unique nominal layouts, exact field types and mutability,
  construction arity/order, initialized argument registers, result/receiver
  types, immutable writes, metadata ranges, and configured work/storage limits.
- `RudaMirExecution` owns a context-bounded object graph. A `RudaMirValue`
  carries an opaque record handle; copied handles alias the same object until
  the execution is disposed. Object descriptors retain the exact field schema,
  so a conflicting cross-function type or mutability layout is rejected rather
  than silently reinterpreted.
- Object count, fields per object, and total field count are bounded. Allocation
  and metadata insertion are failure-atomic, and disposal releases every object
  retained by the oracle execution.
- Source lowering accepts local record names and aliases to records, explicit
  and shorthand fields, arbitrary source field order, projection, simple
  mutable assignment, and the current `i64` compound-assignment set. A member
  receiver is evaluated once; compound assignment reads the old field before
  evaluating the right side.
- Stable diagnostics now distinguish unknown fields (`R0004`), duplicate
  construction fields (`R0005`), and missing required fields (`T0012`). `T0004`
  now explicitly covers immutable bindings and record fields.
- Checked-program interpretation shares one execution through nested direct
  calls. A temporary execution safely owns records that do not cross the public
  interpreter boundary; callers must provide an initialized execution when a
  record is an argument or result, preventing a returned dangling handle.

## Coverage added

- Out-of-declaration-order construction and shorthand fields lower to the exact
  declaration layout; a division-by-zero/overflow pair proves source-order
  evaluation is preserved.
- Copy then mutation is observed through the alias, including mutation in a
  nested direct call, proving reference rather than value-copy behavior.
- A returned record remains usable by another checked function only with the
  same caller-owned execution. Missing execution and forged handles are
  rejected.
- Malformed MIR covers duplicate nominal layouts, wrong result types,
  uninitialized construction arguments, immutable writes, invalid references,
  and a cross-function layout that changes field mutability.
- Exact-span source diagnostics cover unknown, duplicate, missing, wrong-type,
  immutable, unknown-projection, and missing-shorthand cases.
- Cross-file checking proves a function can construct and project a record
  declared in another file of the same package independent of input AST order.
- MIR metadata limits, object limits, and allocator-failure loops prove cleanup
  and failure atomicity for construction, execution, and package publication.

## Verification

| Command | Exit | Result |
|---|---:|---|
| Debug build and `ctest --preset debug --output-on-failure` | 0 | warnings-as-errors C17 build; 1/1 test executable passed |
| Release build and `ctest --preset release --output-on-failure` | 0 | optimized build; 1/1 passed |
| ASan build and `ctest --preset asan --output-on-failure` | 0 | 1/1 passed without sanitizer report |
| ASan+UBSan build and `ctest --preset asan-ubsan --output-on-failure` | 0 | 1/1 passed without sanitizer report |
| TSan build and `ctest --preset tsan --output-on-failure` | 0 | 1/1 passed without sanitizer report |
| `clang --analyze` on MIR, checker, and their C-unit sources | 0 | zero static-analyzer findings after making zero-field allocation preconditions explicit |
| `./tools/check-generated` | 0 | diagnostic header/reference and all generated artifacts are current |
| Doc-link, spec-term, example-syntax, and syntax-coverage checks | 0 | D001–D035 consistent; 76/76 syntax kinds and 117/117 grammar productions covered |

## Deliberate boundary and remaining I2 work

`RudaMirExecution` is a bounded oracle arena that retains objects until explicit
disposal. It is not the production generational/tracing GC, a frozen object ABI,
an escape-analysis implementation, a native/VM differential result, or evidence
that D007's allocation cost is acceptable. D007 remains open for real DTO,
JSON, UI-tree, request/response, and collection-heavy measurements with object
count, allocated/retained bytes, and GC work before the 1.0 semantics freeze.

Qualified imported construction, cross-package field access, `identity.same`,
derived equality, record patterns, and production GC/root handling remain open.
The object graph and exact layout checks are language/runtime semantics only;
they do not encode authorship, agent trust, CI policy, sandbox permission, or
release approval.
