# I1 grammar completion — iteration 8 — 2026-08-11

Iteration status: complete; gate status remains `in-progress`.  
Scope: exact baseline source grammar, parser enforcement, recovery, formatter
coverage, and production-level evidence.

## Implemented

- The 117-entry machine-readable source grammar now has 117 `baseline`
  productions and no provisional production. Accepted D033 freezes the two
  loop headers already shown by the language specification:
  `for const name of expression` and
  `for let name = initializer; condition; step` with no omitted clauses or
  mixed binding forms.
- `tests/syntax/parser/production-coverage.tsv` has 117 `covered` rows and no
  open row. Every row identifies a positive source fixture, canonical
  formatter fixture, negative owner, recovery owner, and parser owner. The
  independent syntax-kind matrix has 76 covered rows and no open row.
- The parser now represents recursive patterns, record fields, enum payloads,
  attribute/call arguments, index expressions, closure parameters,
  declarations, statements, and expression precedence with their exact grammar
  rather than a generic delimiter scanner.
- Expression enforcement rejects chained assignment, comparison chains, empty
  or tuple-like parenthesized expressions, `_` as a value, non-call `spawn`,
  concise `return` match arms, and record construction after a non-identifier
  primary such as `panic`.
- D033 recovery preserves following declarations after invalid arbitrary,
  mixed, underscored, or incomplete loop headers. Its syntactic contract has a
  decision, normative specification text, parser positive/negative/recovery
  tests, and a canonical formatter fixture. VM/native differential evidence is
  not applicable to this syntax-only Stage 0 change; observable loop semantics
  remain unchanged and are still gated by I2–I4.
- `tests/syntax/parser/all-productions.ruda` is the canonical positive corpus
  for all syntax kinds and now exercises every assignment operator, every
  precedence family, every literal family, postfix forms, both closure bodies,
  both loop headers, all statement families, and recursive match patterns.

## Verification

| Command | Exit | Result |
|---|---:|---|
| `./tools/check-generated` | 0 | diagnostic, Unicode, token, syntax-kind, and source-grammar artifacts are current |
| `./tools/check-syntax-coverage` | 0 | 76/76 syntax kinds and 117/117 grammar productions covered; zero open rows |
| `./tools/check-doc-links` | 0 | repository-local links and Markdown fences valid |
| `./tools/check-spec-terms` | 0 | D001–D033 and normative terminology consistent |
| `./tools/check-example-syntax` | 0 | public examples pass the independent lexical audit |
| Debug configure/build and `ctest --preset debug` | 0 | warnings-as-errors build; 1/1 test executable passes |
| Release configure/build and `ctest --preset release` | 0 | optimized build; 1/1 passes |
| ASan configure/build and `ASAN_OPTIONS=halt_on_error=1 ctest --preset asan` | 0 | 1/1 passes without an AddressSanitizer report |
| ASan+UBSan configure/build and strict sanitizer `ctest` | 0 | 1/1 passes without sanitizer reports |
| TSan configure/build and `TSAN_OPTIONS=halt_on_error=1 ctest --preset tsan` | 0 | 1/1 passes without a ThreadSanitizer report |
| `build/debug/ruda fmt examples --check` | 0 | all user examples are canonical |
| `ruda fmt --check` for every `tests/format/*.expected.ruda` and `all-productions.ruda` | 0 | every canonical golden/grammar fixture requires no rewrite |
| `build/debug/ruda --help` and `build/debug/ruda --version` | 0 | Stage 0 Ruda identity and command surface render successfully |

An exploratory `ruda fmt tests --check` is intentionally not a gate and
returns nonzero: that tree contains non-canonical `.input.ruda` golden inputs
and deliberately malformed lexer/recovery/fuzz corpus files. The C fixture
runner verifies each input against its paired expected output and verifies that
unrecoverably invalid inputs are not rewritten; the canonical-only commands
above separately prove that expected outputs are stable.

## Remaining I1 gate evidence

The required 600 CPU-second libFuzzer campaigns for both persistent I1 targets
remain unrun. On this host, `cmake --preset fuzz` exits during preflight with
`RUDA_BUILD_FUZZERS requires a Clang distribution with the libFuzzer runtime`.
The detected compiler is Apple Clang 17.0.0 from
`/Library/Developer/CommandLineTools/usr/bin`; that distribution does not ship
the required macOS libFuzzer runtime. This is a host-toolchain limitation, not
a parser failure, and Docker is not required for ordinary implementation or
the completed local checks.

Tier 1 CI execution is also external evidence. Until both accepted fuzz
campaigns and the required platform matrix are recorded, I1 remains
`in-progress` even though its local grammar registry, parser, formatter, unit,
and sanitizer evidence are complete.
