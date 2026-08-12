# I1 lexer iteration 1 evidence — 2026-08-10

Gate: I1 — lossless frontend and canonical formatter.  
Iteration status: complete; gate status is `in-progress`.  
Environment: macOS Darwin 25.3.0 arm64, AppleClang 17.0.0, CMake 4.4.2.  
Applicable contracts: D023, D024, language §§2 and 15, diagnostics §§1–4 and 12, implementation brief §§6.3 and 7.2, test strategy §§2.1–2.2 and 5.

## Normative closure before implementation

The prior baseline required Unicode XID identifiers, malformed-escape diagnostics, and numeric-literal diagnostics without pinning the Unicode data or fully defining literal spelling. Implementing those rules through host libraries would have made acceptance platform-dependent. This iteration therefore recorded:

- D023: Unicode 17.0.0, UAX #31 R1-1 `XID_Start XID_Continue*`, exact source spelling, and standalone `_`;
- D024: closed locale-independent number, escape, string, raw-string, byte-string, rune, and interpolation spelling.

The language specification, requirements index, consistency audit, and decision validator changed in the same iteration as implementation and tests.

## Delivered vertical slice

- the official Unicode 17.0.0 `DerivedCoreProperties.txt`, committed with source URL, terms link, and pinned SHA-256;
- generated, committed XID range tables with binary-search lookup and a Unicode-17 boundary test;
- a machine-readable token registry generating the C token enum, token-name/keyword tables, and public token reference;
- a bounded, failure-atomic lexer over immutable source snapshots;
- exact physical byte spans for every token, including BOM and all trivia, with a zero-width final EOF;
- Unicode XID identifiers, every canonical keyword, targeted reserved-word tokens, and standalone wildcard/discard `_`;
- line/doc-line and nestable block/doc-block comments;
- all punctuation, precedence operators, arrows, shifts, and compound assignments used by the baseline;
- validated base integers, decimal floats/exponents, digit separators, normal/raw/byte strings, runes, and their closed escape sets;
- lossless interpolated-string start/text/expression/end tokens, escaped braces, balanced expression braces, and nested interpolated strings;
- ordered lexical diagnostics for malformed UTF-8, unexpected characters, reserved words, comments, literals, escapes, numbers, runes, byte strings, braces, nesting, and work limits;
- explicit token, diagnostic, comment, and interpolation bounds with a retained partial stream on a limit and no retained partial ownership on OOM.

## Commands and exit status

| Command | Exit | Result |
|---|---:|---|
| `./tools/check-generated` | 0 | diagnostic, Unicode, and syntax artifacts match committed inputs |
| `./tools/check-doc-links` | 0 | all local links/fences valid after D023/D024 and generated references |
| `./tools/check-spec-terms` | 0 | decisions D001–D024, requirements, and canonical terms consistent |
| `./tools/check-example-syntax` | 0 | existing examples remain lexically consistent with the bounded documentation check |
| `cmake --build --preset debug --parallel && ctest --preset debug` | 0 | warnings-as-errors Debug build; 1/1 CTest executable passed |
| `cmake --build --preset release --parallel && ctest --preset release` | 0 | optimized build; 1/1 passed |
| `cmake --build --preset asan --parallel && ctest --preset asan` | 0 | AddressSanitizer build; 1/1 passed |
| `cmake --build --preset asan-ubsan --parallel && ctest --preset asan-ubsan` | 0 | ASan/UBSan build; 1/1 passed with no report |
| `cmake --build --preset tsan --parallel && ctest --preset tsan` | 0 | TSan build; 1/1 passed with no report |
| `clang --analyze` over lexer, token data consumer, Unicode lookup, lexer tests, and fixture support | 0 | zero findings after removing one dead initialization |

The official UCD input hash is
`24c7fed1195c482faaefd5c1e7eb821c5ee1fb6de07ecdbaa64b56a99da22c08`.
The token TSV and generated token enum hashes are
`0f6cacee45adf33efc391fd6a29ae4a1218d00524cb5fa28c97dd511df9c91c7` and
`63254c2380948e8f6963a0849b9bc61e306b88b4eff8a5a457ba4fadf4ea35b3`.

## Test surfaces

- external `.ruda` valid/invalid fixtures with exact token and diagnostic dumps;
- a full mixed-source snapshot covering BOM, CRLF, Unicode, documentation/nested comments, numbers, all literal kinds, interpolation, and trivia;
- every keyword and reserved word, and every fixed punctuation/operator token;
- valid/invalid base digits, separators, exponent forms, suffixes, simple/Unicode/byte escapes, scalar bounds, byte-string ASCII, and rune cardinality;
- nested interpolation and unmatched interpolation text braces;
- exact no-gap/no-overlap reconstruction of every source byte from token spans;
- token and nesting limit results;
- deterministic allocation failure at every lexer-owned allocation with zero live allocations/bytes after each attempt;
- every possible one-byte input plus 100 deterministic arbitrary 256-byte inputs, checked for bounded completion and exact lossless coverage under sanitizer builds.

The arbitrary-byte test is a deterministic robustness sweep, not a substitute for the I1 coverage-guided fuzz campaign.

## Known risks and next slice

I1 is not verified. The lexer diagnostic record is bound to a source snapshot and carries stable code/message/byte span, but source-window human/short/JSON rendering is not yet connected. Unicode normalization-confusable warnings require resolved symbol comparison and are therefore not claimed here. The lossless green tree, recovery parser, AST projection, formatter, grammar-wide fixture matrix, and accepted fuzz campaign remain open.

No Ruda program reaches semantic analysis, MIR, VM, or native execution, so differential evidence is not applicable and no performance budget moved. Tier 1 execution is still unavailable because this workspace has no versioned checkout/remote; the I0 platform gate remains `in-progress` independently of this local I1 progress.

The next highest-risk slice is the immutable lossless green tree plus event-based recovery parser for file/import/declaration/block structure, while preserving these exact token and diagnostic ownership contracts.
