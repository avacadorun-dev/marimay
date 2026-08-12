# I1 grammar registry — iteration 7 — 2026-08-10

Iteration status: complete; gate status remains `in-progress`.  
Scope: machine-readable source grammar and an independently auditable
production-coverage axis.

## Implemented

- `bootstrap/tables/grammar-productions.tsv` now contains 117 reachable source
  productions spanning files, declarations, types, statements, precedence
  levels, postfix forms, interpolation, and recursive patterns.
- `tools/generate-syntax` validates production names, categories, states,
  quoted terminals, lexer-token references, balanced EBNF delimiters, all
  nonterminal references, and reachability from `file`.
- The committed [generated source grammar](../../generated/source-grammar.md)
  is produced by the same no-network generated-artifact workflow as token and
  syntax-kind references.
- `tests/syntax/parser/production-coverage.tsv` is indexed one-for-one by the
  grammar registry. A covered production must name positive, formatter,
  negative, recovery, and parser-owner evidence. Registry drift, missing
  fixtures, and nonexistent C owners fail `check-syntax-coverage`.
- The coverage check prints both status totals. This iteration records 63 of
  63 syntax kinds covered independently from 0 of 117 fine grammar productions
  covered; a successful structural check is therefore not a syntax-freeze
  claim.
- Language specification §15 now points to the generated reference instead of
  maintaining a second hand-copied, incomplete grammar sketch.

The table has 116 `baseline` rows and one `provisional` row. The provisional
`simple_for_header` preserves the existing unresolved EBNF placeholder rather
than silently turning one parser implementation choice into frozen language
syntax. Generated grammar text becomes normative only after I1 is verified,
all productions are covered, and no provisional row remains.

## Verification

| Command | Exit | Result |
|---|---:|---|
| `./tools/check-generated` | 0 | diagnostic, Unicode, token, syntax-kind, and source-grammar outputs are current |
| `./tools/check-syntax-coverage` | 0 | 63 syntax kinds covered/0 open; 0 grammar productions covered/117 open |
| `./tools/check-doc-links` | 0 | generated grammar and specification links resolve |
| `./tools/check-spec-terms` | 0 | normative terminology remains consistent |
| `./tools/check-example-syntax` | 0 | repository examples still pass the bounded source scan |
| `cmake --build --preset debug` | 0 | bootstrap library, CLI, and C unit executable build |
| `ctest --preset debug --output-on-failure` | 0 | 1/1 test executable passes |

## Deliberately open evidence

This iteration creates the exact work list; it does not relabel broad
syntax-kind tests as production-complete evidence. In particular, the current
parser still groups attribute arguments, enum positional payloads, record
construction fields, closure parameter interiors, and patterns more broadly
than the generated baseline grammar. It also structurally accepts forms that
the specification excludes, including chained assignment and a concise
`return` expression in a match arm. Those must receive grammar-specific
negative/recovery tests and parser corrections before their rows can become
`covered`.

The accepted time-budgeted libFuzzer campaign from iteration 6 also remains
unrun because the installed Apple Command Line Tools Clang lacks the
libFuzzer runtime. Tier 1 CI execution remains external evidence. These three
independent gaps keep I1 `in-progress`; none requires Docker.
