# I1 frontend iteration 3 evidence — 2026-08-10

Gate: I1 — lossless frontend and canonical formatter.  
Iteration status: complete; gate status is `in-progress`.  
Environment: macOS Darwin 25.3.0 arm64, AppleClang 17.0.0, CMake 4.4.2.  
Applicable contracts: D022–D025, language §§2, 7–11, and 15–16, implementation brief §§6.2–6.4 and 7.2–7.4, and test strategy §§2.1–2.2 and 5.

## Delivered vertical slice

- one machine-readable syntax-kind registry now generates the C enum, name table, and public syntax-node reference alongside the existing token registry;
- completed-marker/forward-parent events allow immutable green nodes to wrap already-parsed left operands without mutating tree nodes;
- Pratt expression parsing implements the specified precedence tiers, left-associative binary operators, right-associative assignment, prefix unary operators, member/call/index/postfix `?`, generic calls, record/array construction, closures, `match`, control-return arms, and `spawn`;
- `match` arms retain structural pattern nodes and optional guards, while expressions inside interpolations use the same Pratt parser;
- structural statement parsing now distinguishes both `for const item of value` and C-style `for let i = 0; ...` headers, nested `if`/`else`, `while`, `scope`, `unsafe`, and bare blocks;
- nested generic type scanning handles adjacent `>>` lexer tokens without consuming the rest of the file;
- an arena-owned immutable AST projection removes BOM/whitespace/comments/EOF while retaining significant tokens, synthetic recovery tokens, exact source spans, and syntax-node structure;
- AST projection has explicit node/depth budgets, checked allocation sizes, source-order validation, and failure-atomic ownership.

The AST is intentionally a trivia-free structural projection, not resolved or typed HIR. Pattern and type interiors remain structurally grouped in this iteration.

## Commands and exit status

| Command | Exit | Result |
|---|---:|---|
| `./tools/check-generated` | 0 | diagnostic, Unicode, token, and syntax-kind outputs match their committed sources |
| `./tools/check-doc-links` | 0 | local links and fences valid |
| `./tools/check-spec-terms` | 0 | accepted decisions and requirements remain consistent |
| `./tools/check-example-syntax` | 0 | documentation examples remain lexically consistent |
| `cmake --build --preset debug --parallel && ctest --preset debug` | 0 | warnings-as-errors Debug build; 1/1 CTest executable passed |
| `cmake --build --preset release --parallel && ctest --preset release` | 0 | optimized build; 1/1 passed |
| `cmake --build --preset asan --parallel && ctest --preset asan` | 0 | AddressSanitizer build; 1/1 passed with no report |
| `cmake --build --preset asan-ubsan --parallel && ctest --preset asan-ubsan` | 0 | ASan/UBSan build; 1/1 passed with no report |
| `cmake --build --preset tsan --parallel && ctest --preset tsan` | 0 | TSan build; 1/1 passed with no report |
| `clang --analyze` over lexer/parser green tree, AST projection, and their tests | 0 | zero findings |

## Test surfaces

- operator-tree assertions prove multiplication nests under addition, assignment nests on the right, and prefix/postfix/member/call/index/generic-call nodes are distinct;
- record, array, closure, match/guard/arm, spawn, and postfix `?` forms parse without diagnostics and retain every byte;
- statement fixtures cover both loop headers, nested `else if`, `continue`, `break`, `while`, `unsafe`, bare blocks, and triple-closing nested generic types;
- every repository example still has zero lexer/parser diagnostics;
- AST golden output proves that a comment and surrounding whitespace disappear while `a + b * c` retains the precedence tree and exact significant-token spans;
- AST node/depth limits return `RUDA_AST_LIMIT_REACHED` with no partial result;
- fail-at-every-allocation AST projection reaches success with zero live allocations after every failed/successful attempt;
- the prior parser event/nesting/diagnostic limits, all-byte sweep, malformed recovery, and fail-at-every-allocation tests continue under every preset.

## Known risks and next slice

I1 is not verified. The syntax-kind registry is generated, but a complete generated grammar is not yet normative. Type and pattern interiors need dedicated nodes and negative/recovery fixture families; comparison-chaining rejection belongs to semantic checking. The structural AST is not resolved/typed HIR. Incremental tree reuse is not implemented.

Most importantly, there is no formatter library or `ruda fmt`; therefore idempotence, comment ownership through formatting, parse/format/parse equivalence, transactional write-back, canonical repository formatting, and the accepted parser/formatter fuzz campaign remain open. No source reaches MIR, VM, or native execution, so VM/native differential evidence is not applicable.

The next slice is the canonical document model (`group`, `indent`, `softline`) and formatter library over the lossless tree, starting with idempotent external fixtures and AST equality before exposing any write-capable CLI mode.
