# I2 type aliases — iteration 10 — 2026-08-11

Iteration status: complete; gate status is `in-progress`.  
Scope: implement non-generic aliases whose targets resolve to the current exact
type subset, preserving the normative rule that an alias is another spelling
of the same type.

## Implemented

- Package checking now has a deterministic type-name prepass before function
  signatures, constants, or bodies. Alias declarations are collected from all
  canonically ordered source files and may reference later aliases in the same
  or another file.
- Alias expansion is recursive, bounded by explicit count and depth options,
  and detects dependency cycles. Cycles produce the stable generated
  `T0011 TYPE_ALIAS_CYCLE` diagnostic; unknown targets retain `T0001`.
- Primitive names cannot be redeclared by an alias. Aliases participate in the
  current exact package-level duplicate-name index alongside functions and
  constants, so declaration-order changes cannot change which symbol wins.
- Alias use resolves to the exact target `RudaTypeId`; it does not manufacture
  nominal identity. Function parameters/results, local annotations, and
  top-level constant declarations therefore consume aliases without MIR or
  runtime representation changes.
- Successful results publish owned `RudaCheckedTypeAlias` entries containing
  exact names, path/name-derived stable symbol IDs, and target types. Any
  package diagnostic clears aliases, constants, functions, and the executable
  function index rather than exposing a partially authoritative package.

## Coverage added

- A forward alias chain is used by a constant and by both sides of a function
  signature; the signature canonicalizes to `i64` and executable MIR preserves
  its argument.
- Cross-file functions and constants use aliases declared in a later canonical
  source file. Reversing caller AST order preserves alias/function/constant
  symbols and the interpreted package result.
- Exact negative fixtures cover alias cycles, unknown targets, primitive-name
  redefinition, and a function/alias duplicate.
- Count and expansion-depth limits return a failure-atomic limit status.
  Generic aliases remain an explicit `RUDA_CHECK_INCOMPLETE_LANGUAGE` boundary.
- Single- and multi-source allocator-failure walks include alias collection,
  indexing, recursive resolution, publication, and cleanup. Every injected
  failure ends with zero live allocations and bytes.

## Verification

| Command | Exit | Result |
|---|---:|---|
| Debug build and `ctest --preset debug --output-on-failure` | 0 | warnings-as-errors C17 build; 1/1 test executable passed |
| Release build and `ctest --preset release --output-on-failure` | 0 | optimized build; 1/1 passed |
| ASan build and `ctest --preset asan --output-on-failure` | 0 | 1/1 passed without sanitizer report |
| ASan+UBSan build and `ctest --preset asan-ubsan --output-on-failure` | 0 | 1/1 passed without sanitizer report |
| TSan build and `ctest --preset tsan --output-on-failure` | 0 | 1/1 passed without sanitizer report |
| `clang --analyze` on checker, MIR, type-table, and checker-test sources | 0 | zero static-analyzer findings |
| `./tools/check-generated` | 0 | `T0011` header/reference and all generated artifacts are current |
| Doc-link, spec-term, example-syntax, and syntax-coverage checks | 0 | all checks green; 76/76 syntax kinds and 117/117 grammar productions covered |

## Remaining I2 gate work

Generic and compound aliases, imports, visibility, nominal records/enums,
interfaces, generics, closures, dot-call sets, effects, `Option`, `Result`,
postfix `?`, `defer`, exhaustive `match`, the compiler service, and public
`ruda check` remain open. The current package namespace/index is still the
documented flat exact-name bootstrap and must be replaced by the complete I2
visibility/import/eligible-overload model before verification.

No source syntax, alias semantics, package format, MIR instruction, ABI,
runtime behavior, capability policy, trust policy, or tooling responsibility
changed in this iteration; it implements an already normative pure type rule.
