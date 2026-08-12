# I1 frontend iteration 6 evidence — 2026-08-10

Gate: I1 — lossless frontend and canonical formatter.  
Iteration status: complete; gate status remains `in-progress`.  
Environment: macOS Darwin arm64, AppleClang 17.0.0, CMake 4.4.2.  
Applicable contracts: D015, D022–D026, D031–D032, toolchain §4,
implementation brief §§6–7.4, and test strategy §§2.1–2.2, 2.9, 4–5.

## Delivered vertical slice

- `tests/syntax/parser/all-productions.ruda` is a diagnostic-free canonical
  source containing every generated non-error syntax kind; its parser test
  iterates `RUDA_SYNTAX_KIND_COUNT` and fails if any registered kind disappears;
- `tests/syntax/parser/coverage.tsv` has exactly one structurally checked row
  for all 63 registered kinds, naming positive and formatter fixtures plus
  concrete negative and recovery C test owners;
- `check-syntax-coverage` validates registry drift, duplicate/missing rows,
  fixture paths, owner symbols, and the completeness claim for every `covered`
  row, and runs in the documentation CI job;
- recursive type parsing now validates qualified names, nested generic types,
  tuple/function types, multi-constraint parameters, empty/missing types, and
  lexical `>>` splitting without accepting unmatched closers;
- declaration recovery synchronizes on every top-level starter, including
  attributes, `public`, `unsafe`, `extern`, `const`, `test`, and `fuzz`, so a
  malformed predecessor cannot consume a valid following declaration;
- table-driven declaration, statement, expression, delimiter, type, and match
  recovery tests preserve a lossless tree, assert targeted diagnostic classes,
  and retain following sentinel declarations; an empty match pattern now
  receives a synthetic expected-pattern token rather than a silent empty node;
- accepted D032 replaces the ambiguous raw-string formatter rule with a decoded
  value contract; golden and semantic-AST tests cover normal/raw selection,
  simple and Unicode controls, quotes/apostrophes, non-ASCII scalars, and
  grammar-required ordinary strings;
- formatter-owned decoded string storage remains alive through document render
  and is covered by fail-at-every-allocation cleanup tests;
- `ruda_parse_file_incremental` verifies overlay edit ranges and reuses actual
  immutable top-level green-node pointers for unchanged prefixes and
  same-offset suffixes; reference-counted arena chains keep reused nodes alive
  when the previous parse result is disposed first;
- incremental tests cover same-length and shifting edits, pointer identity,
  losslessness against the new snapshot, invalid ranges, and every allocation
  failure index;
- persistent `ruda_fuzz_frontend` and `ruda_fuzz_formatter` libFuzzer targets
  are configured for Clang ASan+UBSan. The formatter harness checks semantic AST
  equality (including import reordering, trailing separators, and decoded
  strings) plus byte-exact idempotence; that oracle also runs over ordinary C
  unit formatter fixtures and examples.

## Commands and exit status

| Command | Exit | Result |
|---|---:|---|
| `./tools/check-generated` | 0 | diagnostic, Unicode, token, and syntax artifacts match their generators |
| `./tools/check-doc-links` | 0 | repository-local links and Markdown fences valid |
| `./tools/check-spec-terms` | 0 | D001–D032 and canonical normative terms consistent |
| `./tools/check-example-syntax` | 0 | public examples pass the independent lexical audit |
| `./tools/check-syntax-coverage` | 0 | 63 covered rows, zero open/unknown/duplicate rows |
| Debug configure/build plus `ctest --preset debug` | 0 | warnings-as-errors C unit, fixture, semantic oracle, incremental, formatter, CLI, and allocation tests pass |
| equivalent Release, ASan, ASan+UBSan, and TSan preset build/test commands | 0 | 1/1 CTest executable passed in every maintained local preset with no sanitizer report |
| `clang --analyze -std=c17 ...` over bootstrap, common/POSIX runtime, C unit, and fuzz sources | 0 | zero static-analyzer findings |
| `clang ... -fsyntax-only tests/fuzz/*.c` with the project warning profile | 0 | both harnesses and semantic oracle compile cleanly independent of libFuzzer linking |
| `build/debug/ruda fmt examples --check` and the D032 golden check | 0 | examples and canonical string fixture require no rewrite |
| `cmake --preset fuzz` with Apple Command Line Tools Clang | 1 (expected preflight) | compiler distribution lacks `libclang_rt.fuzzer_osx.a`; configure reports the missing libFuzzer prerequisite before build/link |

## Remaining I1 work

I1 is not verified. The local Apple Command Line Tools installation does not
ship libFuzzer, so the accepted 600-CPU-second ASan+UBSan campaign for each
persistent target has not run and no coverage/corpus-growth evidence is claimed.
A full LLVM Clang is an optional development prerequisite for that campaign;
Docker is not required and no compiler package was installed by this change.

The 63-row matrix is complete for the generated syntax-kind registry, not yet
for a generated normative production grammar. The current informative EBNF
still contains finer productions (notably detailed pattern and record field
forms) represented inside broader `PATTERN`/braced nodes. Those productions
need a machine-readable registry and production-indexed cases before the exact
roadmap wording “every grammar production” can be verified. Tier 1 evidence
also requires the configured CI matrix to run on its claimed hosts.
