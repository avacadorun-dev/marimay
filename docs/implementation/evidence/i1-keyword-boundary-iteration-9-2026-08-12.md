# I1 minimal keyword boundary — iteration 9 — 2026-08-12

Iteration status: complete; gate status remains `in-progress`.  
Scope: D038 lexical classification, contextual parsing, ordinary test/fuzz
functions, and shadowable prelude `panic` resolution.

Historical note: iteration 10's exhaustive collision audit supersedes this
iteration's initial 20-hard/9-contextual partition. This file preserves the
commands and evidence that were true for the earlier implementation state.

## Audit result and normative boundary

The audit covered every source word previously emitted as a keyword, every
word used as a baseline grammar discriminator, and common omitted-language
spellings that could otherwise be reserved defensively.

- The hard-keyword set is exactly `break const continue defer enum extern false
  fn for if import interface let match return scope spawn true unsafe while`.
  These spellings determine source structure and remain unavailable as names.
- `as case else from mutable of public type yield` are contextual words. The
  parser recognizes their grammatical role only at an unambiguous construct
  boundary and projects them as ordinary identifier tokens elsewhere.
- `test`, `fuzz`, `panic`, `in`, `any`, `async`, `await`, `class`, `extends`,
  `impl`, and `throws` are ordinary identifiers. Absence of a language feature
  is enforced by the grammar and type system rather than by occupying its
  likely API spelling.
- Only `null` and `undefined` are reserved sentinels. They cannot be introduced
  through ordinary name resolution and therefore cannot create a hidden
  null-like value tier.
- D038 records the accepted boundary and supersedes the affected lexical parts
  of D003a, D036, and the original consistency audit. Language §§2.3, 2.6,
  5.1, and 10.2 specify the same contract.

## Implemented

- The generated token registry distinguishes hard keywords from contextual
  words and no longer allocates token kinds for `test`, `fuzz`, `panic`, or
  `in`.
- Identifier-aware parser primitives accept contextual words in all name
  positions while preserving their original token bytes and lossless CST
  coverage.
- `yield expression;` is a contextual statement form; `yield()`,
  `task.yield()`, and declarations or fields named `yield` remain ordinary
  identifier uses. The checker retains the existing diagnostic when the
  statement form occurs outside a block match arm.
- Special `test` and `fuzz` declarations were removed. `@test` and `@fuzz`
  mark ordinary package-private functions in `_test.ruda` files, with exact
  signature and attribute-argument checks. Unknown function attributes receive
  stable resolver diagnostic `R0007`.
- Unshadowed prelude `fn panic(message: string): never` retains the D036
  intrinsic lowering. A local or package function named `panic` wins normal
  resolution and is called normally.
- The canonical formatter places function attributes on their own line while
  retaining comments attached to an attribute.
- Removing the two special declaration nodes reduces the fully covered
  registry from 76 to 74 syntax kinds and from 117 to 115 grammar productions;
  this is intentional surface reduction, not lost coverage.

## Coverage added or updated

- Lexer tests prove the exact hard/contextual/ordinary/reserved partition and
  stable sentinel diagnostics.
- Parser tests exercise every contextual spelling as an identifier, every
  audited ordinary spelling as a function name, the statement/call distinction
  for `yield`, and rejection of hard-keyword names.
- Checker tests cover valid `@test`/`@fuzz` discovery metadata, invalid
  attribute arguments, conflicting attributes, bad signatures, public
  visibility, wrong file suffixes, unknown attributes, and `panic` shadowing.
- Grammar, formatter, generated-source, example, and production-coverage
  fixtures use ordinary attributed functions instead of removed special
  declarations.

## Verification

| Command | Exit | Result |
|---|---:|---|
| Debug build and `ctest --preset debug --output-on-failure` | 0 | warnings-as-errors C17 build; 1/1 passed |
| Release build and `ctest --preset release --output-on-failure` | 0 | optimized build; 1/1 passed |
| ASan build and strict `ctest --preset asan` | 0 | 1/1 passed without sanitizer report |
| ASan+UBSan build and strict `ctest --preset asan-ubsan` | 0 | 1/1 passed without sanitizer report |
| TSan build and strict `ctest --preset tsan` | 0 | 1/1 passed without sanitizer report |
| `clang --analyze` on parser, formatter, checker, MIR, and affected C-unit sources | 0 | zero findings |
| `./tools/check-generated` | 0 | token, syntax, grammar, and diagnostic artifacts are current |
| `./tools/check-syntax-coverage` | 0 | 74/74 syntax kinds and 115/115 productions covered |
| Doc-link, spec-term, and example-syntax checks | 0 | D001–D038 and public terminology are consistent |
| `build/debug/ruda fmt examples --check` | 0 | all user examples are canonical |

## Deliberate boundary

Ruda 0.1 does not add escaped identifiers. The small hard-keyword set therefore
still cannot be used for generated foreign names; an escaped-name design would
be a separate semantic decision with lexer, formatter, tooling, and FFI tests.
This iteration also does not claim the pending 600 CPU-second I1 fuzz campaigns
or Tier 1 CI matrix, so I1 remains `in-progress`.

The attribute metadata and prelude resolution are covered by the current source
checker and MIR oracle. Production VM/native equivalence remains owned by I3/I4
and is not inferred from frontend tests.
