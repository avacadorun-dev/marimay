# I2 typed core and MIR oracle — iteration 1 — 2026-08-11

Iteration status: complete; gate status is `in-progress`.  
Scope: foundational type identity/inference storage, typed MIR verification and
interpretation, and the first internal source-to-MIR pure-function slice.

## Implemented

- Stable 128-bit symbol identities are derived from length-delimited package,
  source-path, declaration-name, and discriminator parts. The owned type table
  exposes canonical primitive IDs, exact nominal/interface/type-parameter
  descriptors, tuples, function types, and an explicit union-find inference
  context with exact bindings.
- Typed MIR has explicit typed registers, basic blocks, source byte spans,
  instructions, and terminators. Its verifier checks control-flow targets,
  instruction/result types, limits, instruction accounting, and definite
  assignment at joins. Invalid MIR is rejected before execution.
- The unoptimized MIR interpreter covers the initial `i64`/`bool` instruction
  family. Portable checked add/subtract/multiply/divide/remainder/negation
  helpers avoid C undefined behavior and distinguish overflow from division by
  zero.
- The internal checker consumes the trivia-free structural AST and owns its
  diagnostics, canonical type table, function identities, and verified MIR.
  The current complete subset covers primitive function signatures, exact
  `int`/`uint`/`float`/`byte` aliases, parameters, same-block `const`/`let`,
  definite initialization, direct assignment, returns, integer/boolean
  literals, names, parentheses, `!`/`-`/`~`, checked `i64` arithmetic,
  comparisons, and `i64`/`bool` equality.
- A valid baseline construct outside that closed subset returns
  `RUDA_CHECK_INCOMPLETE_LANGUAGE` and disposes the partial result. It does not
  manufacture a source error. Therefore the bootstrap still does not expose
  `ruda check`; the existing unavailable-command response remains honest.
- Resolver/type diagnostics now have committed stable registry entries:
  `R0001`–`R0002` and `T0001`–`T0007`. Tests pin code and exact byte span for
  unknown/duplicate names, unknown types, type mismatches, invalid operators,
  immutable assignment, uninitialized locals, missing returns, and out-of-range
  integer literals.
- Failure injection walks every checker allocation boundary and proves that
  success, allocation failure, explicit implementation limits, and incomplete
  language coverage leave no live allocation or partially initialized result.

The first vertical fixture compiles
`fn calc(a: int, b: int, c: int): int { return a + b * c; }`, verifies its MIR,
and interprets `calc(2, 3, 4)` as `14`. Additional fixtures exercise comparison
lowering, mutable-local initialization/assignment, `INT64_MIN`, and every
primitive signature spelling.

## Verification

| Command | Exit | Result |
|---|---:|---|
| `./tools/check-generated` | 0 | diagnostic, Unicode, token, syntax, and grammar generated artifacts current |
| `./tools/check-doc-links` | 0 | repository-local documentation links valid |
| `./tools/check-spec-terms` | 0 | decisions and normative terminology consistent |
| `./tools/check-example-syntax` | 0 | public example lexical audit green |
| `./tools/check-syntax-coverage` | 0 | 76/76 syntax kinds and 117/117 baseline productions covered |
| Debug build and `ctest --preset debug --output-on-failure` | 0 | warnings-as-errors C17 build; 1/1 test executable passed |
| Release build and `ctest --preset release --output-on-failure` | 0 | optimized build; 1/1 passed |
| ASan build and `ctest --preset asan --output-on-failure` | 0 | 1/1 passed without sanitizer report |
| ASan+UBSan build and `ctest --preset asan-ubsan --output-on-failure` | 0 | 1/1 passed without sanitizer report |
| TSan build and `ctest --preset tsan --output-on-failure` | 0 | 1/1 passed without sanitizer report |
| `clang --analyze` on `bootstrap/src/check/checker.c` | 0 | zero static-analyzer findings |

## Remaining I2 gate work

This iteration does not claim package/import/visibility resolution, constant
evaluation, records, enums, interfaces, generics, closures, dot calls,
`Option`/`Result`/`?`, control-flow lowering, exhaustiveness, effects, compiler
service queries, randomized evaluator equivalence, or an authoritative CLI
checker. No VM/native differential result is applicable yet: both engines begin
in I3/I4, while this iteration establishes the MIR oracle they will compare
against.

The dynamic type interner is currently a bounded canonical linear scan. It is
correct for this small slice but must be replaced by a deterministic canonical
hash index and benchmarked before package-scale generics are accepted. The MIR
verifier's dataflow storage is likewise deliberately unoptimized and remains
subject to adversarial scale tests before I2 verification.
