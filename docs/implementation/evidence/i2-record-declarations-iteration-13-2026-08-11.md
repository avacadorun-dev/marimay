# I2 nominal record declarations — iteration 13 — 2026-08-11

Iteration status: complete; gate status is `in-progress`.  
Scope: implement deterministic, bounded declaration checking and compiler-owned
metadata for the existing D004/D007 nominal-record contract. This is a
declaration foundation, not record construction or managed-runtime execution.

## Existing normative contract implemented

- D004 and language §5.4 make every `type Name { ... }` declaration a distinct
  nominal type even when two declarations have identical fields.
- D007 makes records managed references and fields immutable unless explicitly
  `mutable`; that runtime and identity behavior remains gated work.
- Existing syntax permits package-private/public records and fields, with
  `public mutable` represented as two independent properties.
- This iteration changes no syntax or language semantics and therefore adds no
  decision record. It implements the already accepted contract without moving
  trust, CI policy, sandboxing, or agent provenance into the language.

## Implemented

- The checker predeclares every supported non-generic record in canonical
  source-path/declaration order before resolving aliases, fields, functions, or
  constants. Each record receives a stable declaration symbol and a canonical
  `RUDA_TYPE_KIND_NOMINAL` identity.
- Record and alias names share the current exact top-level namespace with
  functions and constants. Primitive redefinition and duplicate declarations
  retain `R0002`; unknown field types retain `T0001`.
- Alias targets may name later records. Record fields may name aliases, later
  records, the containing record, or mutually recursive records without
  collapsing nominal identity.
- The checked result owns bounded record/field metadata: exact name bytes,
  nominal type and symbol, field types, and public/mutable flags. The AST and
  snapshots remain borrowed under the existing checker contract.
- `max_records` and `max_fields_per_record` bound work and publication. Every
  initialization, interning, map insertion, nested field allocation, transfer,
  and cleanup path is failure-atomic under allocator injection.
- Generic records and declaration attributes remain explicit
  `RUDA_CHECK_INCOMPLETE_LANGUAGE` boundaries rather than being accepted with
  partial semantics.

## Coverage added

- Same-shape declarations prove distinct symbols and type identities.
- Self-recursive and mutually recursive field graphs prove two-phase nominal
  resolution. A forward alias and reversed caller-supplied multi-file order
  prove canonical cross-file behavior and stable identities.
- Function signatures consume record types while the runtime value path remains
  intentionally unused.
- Metadata assertions cover record/field visibility, field mutability, alias
  field types, and nominal descriptors.
- Negative fixtures cover duplicate fields, duplicate top-level names,
  primitive redefinition, unknown field types, generic declarations,
  attributes, record limits, and per-record field limits.
- Both single-file and package allocator-failure loops now include recursive or
  aliased record declarations and require zero live allocations after every
  injected failure.

## Verification

| Command | Exit | Result |
|---|---:|---|
| Debug build and `ctest --preset debug --output-on-failure` | 0 | warnings-as-errors C17 build; 1/1 test executable passed |
| Release build and `ctest --preset release --output-on-failure` | 0 | optimized build; 1/1 passed |
| ASan build and `ctest --preset asan --output-on-failure` | 0 | 1/1 passed without sanitizer report |
| ASan+UBSan build and `ctest --preset asan-ubsan --output-on-failure` | 0 | 1/1 passed without sanitizer report |
| TSan build and `ctest --preset tsan --output-on-failure` | 0 | 1/1 passed without sanitizer report |
| `clang --analyze` on checker and checker-test sources | 0 | zero static-analyzer findings |
| `./tools/check-generated` | 0 | all committed generated artifacts are current |
| Doc-link, spec-term, example-syntax, and syntax-coverage checks | 0 | D001–D035 consistent; 76/76 syntax kinds and 117/117 grammar productions covered |

## Remaining I2/I3/I5 work

The checker does not yet accept record construction, field projection or field
assignment. It does not enforce public-signature visibility or derive rules and
does not lower record operations to MIR. Those source semantics remain I2 work.

No object is allocated by this iteration. Record layout descriptors, bytecode
operations, managed allocation, tracing, stable reference identity, mutable
field execution, escape analysis, and D007 allocation measurements remain
I3/I5 gates. Publication of declaration metadata is not evidence that GC-backed
records are usable at runtime.
