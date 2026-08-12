# I1 formatter library iteration 4 evidence — 2026-08-10

Gate: I1 — lossless frontend and canonical formatter.  
Iteration status: complete; gate status is `in-progress`.  
Environment: macOS Darwin 25.3.0 arm64, AppleClang 17.0.0, CMake 4.4.2.  
Applicable contracts: D015, D022–D026, toolchain §4, implementation brief §7.4, and test strategy §§2.1–2.2.

## Delivered vertical slice

- a bounded C17 document model implements text, hard and literal lines, soft line/soft break, groups, indentation, and text emitted only when a group breaks;
- group fitting uses the committed Unicode 17.0.0 D026 scalar-width tables, a fixed 100-column target, checked column arithmetic, and explicit scan/nesting budgets rather than host `wcwidth` or locale behavior;
- the formatter library consumes the immutable lossless syntax tree, refuses every lexer/parser diagnostic or synthetic recovery token, preserves significant token spelling, and returns an owned failure-atomic result without writing files;
- canonical output uses two-space indentation, LF, one final newline, spaced binary operators, attached postfix/generic punctuation, multiline braces, grouped parentheses/brackets, one-item-per-line broken lists, and required trailing commas for broken lists and multiline record construction;
- comments retain kind, normalized text, order, documentation attachment, blank documentation lines, and same-line placement after attributes, fields, statements, and block closes;
- top-level attributes, nested blocks, generic angle tokens including `>>`, `for` headers, interpolation boundaries, and inline/multiline comments have explicit formatting rules;
- formatter/document APIs carry token, document-part, output-byte, nesting, and render-work limits; allocation and writer failures leave no partial owned result.

The D026 input added during this iteration is the official Unicode 17.0.0 East Asian Width file, hash-pinned beside the existing Derived Core Properties input. Generated width tables classify `Grapheme_Extend` and `Default_Ignorable_Code_Point` as zero columns, remaining `W`/`F` scalars as two, and all other valid scalars as one.

## Commands and exit status

| Command | Exit | Result |
|---|---:|---|
| `./tools/check-generated` | 0 | diagnostic, Unicode width/XID, token, and syntax-kind artifacts match their committed sources |
| `./tools/check-doc-links` | 0 | repository-local links and fences valid |
| `./tools/check-spec-terms` | 0 | decisions D001–D026 and requirement terms consistent |
| `./tools/check-example-syntax` | 0 | documentation example lexical audit passed |
| `cmake --build --preset debug --parallel && ctest --preset debug` | 0 | warnings-as-errors Debug build; 1/1 CTest executable passed |
| `cmake --build --preset release --parallel && ctest --preset release` | 0 | optimized build; 1/1 passed |
| `cmake --build --preset asan --parallel && ctest --preset asan` | 0 | AddressSanitizer build; 1/1 passed with no report |
| `cmake --build --preset asan-ubsan --parallel && ctest --preset asan-ubsan` | 0 | ASan/UBSan build; 1/1 passed with no report |
| `cmake --build --preset tsan --parallel && ctest --preset tsan` | 0 | TSan build; 1/1 passed with no report |
| `clang --analyze` over document/formatter sources and tests | 0 | zero findings |

## Test surfaces

- document tests prove Unicode-width fit decisions at the boundary, flat versus broken groups, lazy indentation, literal-line behavior, malformed nesting rejection, writer failure, defensive limits, and fail-at-every-allocation cleanup;
- a CRLF-heavy round trip proves parse/format/parse, byte-idempotence, semantic AST equivalence, generic and postfix attachment, nested block layout, and normalized block-comment text;
- golden fixtures cover named enum payloads, the declaration and expression grammar surface, control-flow forms, record trailing commas, and documentation/trailing comment placement;
- a long-call fixture proves the 100-column break path, one argument per line, conditional trailing-comma insertion, and second-pass stability;
- all four repository examples have zero lexer/parser diagnostics after formatting, preserve semantic AST and comment streams, and are byte-stable on the second formatting pass;
- 128 deterministic whitespace variants of the broad grammar fixture vary spaces, tabs, LF, and CRLF between every token and converge to one expected output;
- invalid recovery trees, every public limit, internal output rejection, and fail-at-every-allocation formatting are covered without leaks or partial results.

## Known risks and next slice

I1 is not verified. The formatter is currently a library: public `ruda fmt`, `--check`, `--diff`, stdin, module/tree discovery, transactional permission-preserving write-back, and source-span diagnostic rendering remain open. Repository examples pass the formatting invariants but have not yet been rewritten to their final canonical bytes. Import sorting, canonical ordinary-string selection, a grammar-production-complete positive/negative/recovery fixture manifest, incremental green-tree reuse, and the accepted time-budgeted parser/formatter fuzz campaign also remain open.

No Ruda source reaches resolution, typed MIR, VM, or native execution, so VM/native differential evidence is not applicable to this slice. Tier 1 execution remains unavailable because this workspace is not a versioned checkout with a configured remote; I0 remains `in-progress` independently of this local I1 progress.
