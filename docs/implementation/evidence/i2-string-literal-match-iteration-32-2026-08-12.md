# I2 exact string literal match — iteration 32 — 2026-08-12

Iteration status: complete; gate status remains `in-progress`.  
Scope: string literal-pattern analysis, typed MIR equality, exact oracle
execution, and bounded missing-witness diagnostics.

## Normative contract implemented

- Language §9 and D005/D032 define string literal patterns over the complete
  decoded UTF-8 byte sequence. Embedded zero bytes participate in comparison.
- Ordinary, raw, and escaped spellings that decode to identical bytes denote
  the same pattern and are redundant when both are reachable.
- Matching applies no NUL termination, locale collation, case folding, or
  Unicode normalization. Canonically composed and decomposed byte sequences
  remain distinct values and patterns.
- The string domain is not finite. Literal arms alone cannot exhaust it; an
  unguarded wildcard or name binding must cover the remainder. A guarded
  catch-all still does not contribute to exhaustiveness.
- LANG-029 records the same contract for I2/I4, and execution §13.1 makes exact
  pattern selection part of VM/native observable equivalence.

## Implemented

- Added typed MIR instruction `RUDA_MIR_EQUAL_STRING`, requiring two assigned
  `string` registers and producing `bool`.
- The MIR verifier rejects wrong operand/result types and uninitialized or
  invalid registers. The oracle resolves both execution-owned references and
  compares their full length plus bytes with `memcmp`; it never uses a C string
  function.
- Interpreter entry now validates string parameters against the selected
  execution object graph, so forged and cross-context handles fail before
  execution rather than being accepted as well-typed values.
- The checker decodes every string pattern through the shared D032 decoder,
  stores the exact value in the bounded MIR constant pool, and lowers each
  reachable literal test to `CONSTANT_STRING` plus `EQUAL_STRING`.
- OR-pattern and cross-arm redundancy uses decoded bytes, so `"a"`, `r"a"`,
  and `"\u{61}"` collide as required. Catch bindings retain the exact string
  value and type.
- Non-exhaustive string matches produce a deterministic bounded witness
  `"ruda#N"`. Candidate search is capped by the number of distinct unguarded
  literals and map lookup is keyed by full bytes, including zero-length keys.

## Coverage added

- Checker execution fixtures cover empty, ordinary, raw, Unicode, embedded-zero,
  composed/decomposed, guarded, OR, catch-binding, wildcard, and block-`yield`
  arms through the MIR oracle.
- Negative fixtures cover missing catch-all, guarded-only coverage, literal
  type mismatch, duplicate decoded values across ordinary/raw/escaped spelling,
  and duplicate values inside one OR pattern.
- Diagnostic fixtures assert exact deterministic string witnesses and the
  guard caveat; fail-allocator campaigns cover both pattern decoding and owned
  witness construction without leaks.
- MIR adversarial fixtures cover equal and unequal embedded-zero strings,
  missing execution context, wrong operand type, uninitialized operand, and
  invalid managed references.
- Limits tests prove string-pattern constants respect both constant-count and
  total-byte ceilings. Grammar and formatter corpora include a canonical string
  literal match.

## Verification

| Command | Exit | Result |
|---|---:|---|
| Debug build and `ctest --preset debug --output-on-failure` | 0 | warnings-as-errors C17 build; 1/1 passed |
| Release build and `ctest --preset release --output-on-failure` | 0 | optimized build; 1/1 passed |
| ASan build and strict `ctest --preset asan` | 0 | 1/1 passed without sanitizer report |
| ASan+UBSan build and strict `ctest --preset asan-ubsan` | 0 | 1/1 passed without sanitizer report |
| TSan build and strict `ctest --preset tsan` | 0 | 1/1 passed without sanitizer report |
| `clang --analyze` on checker, MIR, and affected C-unit sources | 0 | zero findings |
| `./tools/check-generated` | 0 | generated diagnostic, Unicode, and syntax artifacts are current |
| `./tools/check-syntax-coverage` | 0 | 74/74 syntax kinds and 115/115 grammar productions covered |
| Doc-link, spec-term, and example-syntax checks | 0 | public contracts and examples are consistent |
| Canonical formatter checks for examples, grammar golden, and all-productions corpus | 0 | all checked sources require no rewrite |

## Deliberate boundary and remaining work

This iteration implements top-level scalar string literal patterns only. It
does not yet provide nested string patterns inside enum, record, or tuple
constructors; record/tuple pattern lowering; string constants in constant
evaluation; the full constructor-matrix usefulness algorithm; or generic
pattern products.

Strings are currently execution-owned oracle objects, not production GC or ABI
values. I3/I4 must encode `EQUAL_STRING`, reject invalid bundle operands, and
prove exact VM/native behavior—including embedded zero and non-normalized
Unicode—before LANG-029 or I2 is fully verified.
