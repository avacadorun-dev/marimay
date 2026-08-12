# I2 public-type visibility — iteration 14 — 2026-08-11

Iteration status: complete; gate status is `in-progress`.  
Scope: enforce language §3's existing rule that a public declaration cannot
expose a package-private type, for the current exact local alias/record subset.

## Existing normative contract implemented

- Top-level declarations and record fields are package-private unless marked
  `public`.
- A public function, constant, alias, or public field of a public record cannot
  name a package-private type in its externally visible signature.
- A private field of a public record may use a private type because that field
  is not part of the package's public surface.
- Visibility is checked against the type spelling as well as canonical type
  identity. A private alias to `int` therefore cannot be used to publish an API
  that external source cannot name.
- This implements language §3 and requirement LANG-002. No decision or source
  semantics changed.

## Implemented

- Internal function, constant, alias, record, and field declarations retain
  their parsed visibility. Checked public metadata now publishes the visibility
  bit for all five declaration/field surfaces.
- Exact type-reference validation distinguishes primitives, public aliases,
  public records, and package-private aliases/records through the checker-owned
  top-level index.
- Public alias targets are checked after bounded alias resolution. Public
  function parameter/return types and explicitly typed public constants are
  checked during staged signature collection. Public record fields are checked
  during the record field-resolution pass.
- Resolver diagnostic `R0003 PRIVATE_TYPE_IN_PUBLIC_API` owns the exact private
  type spelling. It is committed in the generated C and documentation registry.
- Diagnostics remain failure-atomic: no function, constant, alias, or record
  outputs are published when the package has a visibility error.

## Coverage added

- A positive mixed-visibility package proves public aliases, records, fields,
  constants, and functions publish `is_public`, while private counterparts do
  not.
- The same fixture proves a private field of a public record may retain a
  private alias type.
- Exact-span negative fixtures separately cover a public alias target, function
  parameter, function return, public record field, and typed public constant.
- Existing allocator-failure, duplicate-name, record-recursion, canonical
  multi-file, and type-identity suites continue to pass.

## Verification

| Command | Exit | Result |
|---|---:|---|
| Debug build and `ctest --preset debug --output-on-failure` | 0 | warnings-as-errors C17 build; 1/1 test executable passed |
| Release build and `ctest --preset release --output-on-failure` | 0 | optimized build; 1/1 passed |
| ASan build and `ctest --preset asan --output-on-failure` | 0 | 1/1 passed without sanitizer report |
| ASan+UBSan build and `ctest --preset asan-ubsan --output-on-failure` | 0 | 1/1 passed without sanitizer report |
| TSan build and `ctest --preset tsan --output-on-failure` | 0 | 1/1 passed without sanitizer report |
| `clang --analyze` on checker and checker-test sources | 0 | zero static-analyzer findings |
| `./tools/check-generated` | 0 | `R0003` header/reference and all generated artifacts are current |
| Doc-link, spec-term, example-syntax, and syntax-coverage checks | 0 | D001–D035 consistent; 76/76 syntax kinds and 117/117 grammar productions covered |

## Remaining I2 work

This slice has no import graph, re-export, generic argument, interface,
dot-call, or overload visibility semantics. Those remain behind the roadmap's
complete package/import/visibility index removal gate. Inferred public constant
types are currently limited to the primitive constant subset.

Record construction/projection, field access checks, MIR record operations,
managed allocation, GC, identity, and D007 performance evidence remain open.
The visibility bit is a compiler fact for tools and package interfaces; it does
not encode trust, authorship, review policy, or runtime sandbox grants.
