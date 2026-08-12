# I1 parser iteration 2 evidence — 2026-08-10

Gate: I1 — lossless frontend and canonical formatter.  
Iteration status: complete; gate status is `in-progress`.  
Environment: macOS Darwin 25.3.0 arm64, AppleClang 17.0.0, CMake 4.4.2.  
Applicable contracts: D022–D025, language §§2, 5.5, and 15–16, implementation brief §§6.3–6.4 and 7.2, and test strategy §§2.1–2.2 and 5.

## Normative closure before implementation

Parser fixtures exposed a conflict between the grammar/record declarations and named enum payloads in standard-library and tour examples. D025 resolves it: payload declaration fields use `name: Type;`, and the optional comma after the closing brace separates variants. The language and standard-library specifications, requirements index, consistency audit, examples, decision validator, and a formatter fixture changed in the same iteration.

The formatter fixture records the canonical spelling but is not executable evidence yet because the formatter has not landed.

## Delivered vertical slice

- an event-based parser over the complete lossless lexer stream;
- an arena-owned immutable green tree whose elements are child nodes or physical/synthetic tokens;
- exact trivia, comment, BOM, malformed-token, and EOF retention without copying or normalizing source bytes;
- a recursive losslessness validator that proves physical token spans cover the source exactly once and synthetic tokens are zero-width at the current cursor;
- structural parsing for imports, attributes, aliases, records, enums and payloads, interfaces, functions, externs, constants, tests/fuzz declarations, blocks, bindings, returns, defer, conditionals, loops, scope, loop control, yield, interpolation, and balanced groups;
- grammar-local synthetic-token recovery plus error nodes for invalid top-level declarations and mismatched/unterminated delimiters;
- stable parser diagnostics P0001–P0006 generated from the shared diagnostic registry;
- configurable event, diagnostic, and nesting budgets, checked size arithmetic, forward-progress guards, and failure-atomic ownership on allocation or limit failure;
- parser-tree snapshot and external source fixtures integrated into the C unit executable.

This is deliberately a structural parser iteration. Expressions are retained losslessly in generic expression/group nodes rather than parsed with the required Pratt precedence grammar, types are only structurally grouped, and there is no semantic AST projection.

## Commands and exit status

| Command | Exit | Result |
|---|---:|---|
| `./tools/check-generated` | 0 | diagnostics, Unicode, and syntax-token artifacts match committed inputs |
| `./tools/check-doc-links` | 0 | local links and fences valid after D025 and this evidence entry |
| `./tools/check-spec-terms` | 0 | decisions D001–D025, requirements, and canonical terms consistent |
| `./tools/check-example-syntax` | 0 | corrected examples pass the bounded documentation checker |
| `cmake --build --preset debug --parallel && ctest --preset debug` | 0 | warnings-as-errors Debug build; 1/1 CTest executable passed |
| `cmake --build --preset release --parallel && ctest --preset release` | 0 | optimized build; 1/1 passed |
| `cmake --build --preset asan --parallel && ctest --preset asan` | 0 | AddressSanitizer build; 1/1 passed with no report |
| `cmake --build --preset asan-ubsan --parallel && ctest --preset asan-ubsan` | 0 | ASan/UBSan build; 1/1 passed with no report |
| `cmake --build --preset tsan --parallel && ctest --preset tsan` | 0 | TSan build; 1/1 passed with no report |
| `clang --analyze` over parser, green tree, and parser tests | 0 | zero findings |

## Test surfaces

- exact green-tree snapshot for a mixed BOM/CRLF/Unicode/comment/literal/interpolation source;
- every repository example parses with zero lexer/parser diagnostics and exact byte coverage;
- a declaration corpus covers imports, aliases, records, generic enums with named payloads, interfaces, externs, constants, fuzz declarations, and tests;
- missing names, colons, semicolons, and declaration starters recover with synthetic tokens/error nodes while retaining all bytes;
- mismatched and unterminated nested delimiters emit targeted P0002/P0003 diagnostics and remain lossless;
- empty input and all 256 possible one-byte inputs complete with a lossless root;
- event, nesting, and diagnostic ceilings return `RUDA_PARSE_LIMIT_REACHED` without a partial owned result;
- allocation fails deterministically at every parser-owned allocation until the first successful run, with zero live allocations and a lossless success tree.

The diagnostic-ceiling test found a recovery-loop stall: after the ceiling set the limit state, invalid-declaration recovery continued polling without consuming. Every recovery loop now exits on allocation or configured-limit failure, and the regression is exercised under every sanitizer preset. The all-byte sweep is deterministic robustness coverage, not the accepted I1 coverage-guided fuzz campaign.

## Known risks and next slice

I1 is not verified. The syntax-kind list and grammar are not generated; Pratt/precedence expression parsing, full statement/pattern grammar, AST projection, grammar-wide positive/negative/recovery fixtures, formatter document model and CLI, parse/format/parse equivalence, incremental parsing, and the accepted fuzz campaign remain open. The parser library is not wired into `ruda check` or any source command.

No Ruda program reaches semantic analysis, MIR, VM, or native execution, so VM/native differential evidence is not applicable and no performance budget moved. Tier 1 execution is still unavailable because this workspace has no versioned checkout/remote; I0 remains `in-progress` independently of this local I1 progress.

The next slice is a machine-readable syntax-kind/grammar registry plus Pratt expression nodes and a trivia-free AST projection, keeping the same lossless source ownership and bounded-recovery contracts.
