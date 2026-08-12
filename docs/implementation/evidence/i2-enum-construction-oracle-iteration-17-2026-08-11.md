# I2 algebraic-enum construction oracle — iteration 17 — 2026-08-11

Iteration status: complete; gate status is `in-progress`.  
Scope: implement the already accepted D004/D005 qualified construction forms
for the current non-generic I2 source subset and a bounded typed-MIR execution
oracle, without claiming pattern matching or a production enum representation.

## Existing normative contract implemented

- Enums are closed nominal sums with unit, positional, and named payload
  variants.
- Outside patterns, non-prelude construction is qualified as `Message.Quit`,
  `Message.Write(value)`, or `Message.Move { x, y }`.
- Positional payloads are ordered. Named payload fields may be supplied in any
  source order and use the same visible-local shorthand as record construction;
  unknown, duplicate, and missing fields are errors.
- Enums are immutable, the compiler owns their tag layout, and safe source
  cannot inspect or forge an integer tag.
- This implements part of language specification §5.5 and the enum-construction
  instruction family required by execution specification §3. It does not amend
  source semantics or add a design decision.

## Implemented

- Typed MIR now owns bounded nominal enum layouts, variant kinds, ordered
  payload-type descriptors, construction argument tables, and verified
  `ENUM_NEW` instructions.
- Layout insertion is exact and idempotent for one nominal type. The verifier
  rejects inconsistent duplicate layouts, record/enum nominal collisions,
  malformed ranges and arities, invalid payload types, wrong result types,
  uninitialized argument registers, and configured storage/work overrun.
- `RudaMirExecution` retains immutable enum oracle values separately from
  managed records. Each value retains its exact full layout descriptor, selected
  variant, and selected payload values, so calls cannot silently reinterpret a
  handle under a conflicting layout.
- Internal C-only inspection exposes the selected variant and payloads to the
  conformance oracle. It has no safe-source equivalent and does not expose a
  forgeable numeric tag.
- Source lowering accepts direct enum names and same-package type aliases for
  all three construction forms. Positional arguments and named fields are
  evaluated left to right in source order, while named payload values are
  stored in declaration order.
- Checked-program interpretation shares one execution through nested calls.
  Enum arguments and results require an initialized caller-owned execution, and
  every incoming managed handle is validated against that execution and nominal
  type before the function runs.
- Stable diagnostic `R0006 UNKNOWN_VARIANT` now distinguishes a missing enum
  variant. Existing field and arity summaries now cover named/positional enum
  construction as well as records and ordinary calls.

## Coverage added

- Unit, positional, and named construction execute through checked source and
  expose exact variant/payload results through the internal oracle.
- Alias-qualified named construction proves arbitrary source-field order and
  shorthand preserve declaration-order payload layout.
- A nested direct call returns an enum in the same execution. A pass-through
  function proves immutable values do not allocate again, while missing
  executions and forged handles are rejected at the interpreter boundary.
- Division-by-zero followed by overflow in positional and named payloads proves
  left-to-right evaluation and proves no enum value is retained before all
  payload expressions complete.
- Exact-span diagnostics cover unknown variants, every wrong constructor shape,
  positional arity, unknown/duplicate/missing named fields, wrong payload types,
  missing shorthand locals, and the required qualification of ordinary
  variants.
- Multi-file checking constructs an enum declared in another file of the same
  package independent of input AST order.
- MIR malformed-input tests cover inconsistent layouts, wrong target types,
  uninitialized arguments, object limits, invalid handles, and allocator
  failure. Checker tests cover every enum MIR metadata limit, single- and
  multi-file allocator-failure cleanup, and failure atomicity.

## Verification

| Command | Exit | Result |
|---|---:|---|
| Debug build and `ctest --preset debug --output-on-failure` | 0 | warnings-as-errors C17 build; 1/1 test executable passed |
| Release build and `ctest --preset release --output-on-failure` | 0 | optimized build; 1/1 passed |
| ASan build and `ctest --preset asan --output-on-failure` | 0 | 1/1 passed without sanitizer report |
| ASan+UBSan build and `ctest --preset asan-ubsan --output-on-failure` | 0 | 1/1 passed without sanitizer report |
| TSan build and `ctest --preset tsan --output-on-failure` | 0 | 1/1 passed without sanitizer report |
| `clang --analyze` on MIR, checker, and their C-unit sources | 0 | zero static-analyzer findings |
| `./tools/check-generated` | 0 | diagnostic header/reference and all generated artifacts are current |
| Doc-link, spec-term, example-syntax, and syntax-coverage checks | 0 | D001–D035 consistent; 76/76 syntax kinds and 117/117 grammar productions covered |

## Deliberate boundary and remaining I2 work

The execution representation is a bounded conformance oracle, not a frozen
runtime tag/payload ABI, VM value format, native layout, production GC decision,
or evidence about allocation cost. Copying its internal handle is unobservable
in safe Ruda: enum values remain immutable and have no specified reference
identity.

Generic enum instantiation, the unqualified prelude constructors `Some`,
`None`, `Ok`, and `Err`, constant enum construction, imported/package-qualified
resolution and visibility, checked tag edges and payload projection, patterns,
guards, usefulness/redundancy analysis, exhaustive `match`, derives, and
VM/native differential coverage remain open. No syntax was added for compiler,
tooling, agent trust, CI policy, sandbox permission, or release approval.
