# I1 exhaustive keyword boundary — iteration 10 — 2026-08-12

Iteration status: complete; gate status remains `in-progress`.  
Scope: exhaustive D038 reserved-spelling audit, bounded contextual
disambiguation, formatter/CST preservation, and checker/MIR symbol use.

## Audit result

The audit started from every spelling in the generated token registry, every
quoted word in the baseline grammar, every previously ordinary omitted-feature
spelling, and the two sentinel-reserved words. The resulting 0.1 partition is:

| Class | Count | Exact spellings |
|---|---:|---|
| hard keywords | 8 | `defer false if match return spawn true while` |
| contextual grammar words | 21 | `as break case const continue else enum extern fn for from import interface let mutable of public scope type unsafe yield` |
| reserved sentinels | 2 | `null undefined` |
| audited ordinary names | 11 | `test fuzz panic in any async await class extends impl throws` |

The eight hard words either have an unavoidable valid-expression/control
ambiguity or would require unbounded delimiter search/speculation at a common
expression boundary. Making `true` or `false` names would collide exactly with
literals; `return(value);`, `defer(call());`, and `spawn(call())` are already
complete valid constructs byte-for-byte. Distinguishing calls named `if`,
`while`, or `match` from their expression-plus-block constructs would require
looking beyond an arbitrary nested expression; the bounded linear parser keeps
those spellings hard. Ruda has no escaped-identifier syntax in 0.1.

The other 21 grammar words have a bounded discriminating form. The parser now
projects them to `IDENTIFIER` at every name site and retains their construct
token only at an unambiguous boundary. Important adversarial pairs include
`break;`/`break()`, `continue;`/`continue()`, binding-leading
`const name`/the expression `const()`, `for const ...`/`for()`, and statement
blocks `scope { ... }`/`unsafe { ... }` versus record expressions in an
established expression context such as `const value = scope {};`. Lookahead is
limited to at most the next two significant tokens and does not inspect
semantic types.

D038, language specification section 2.3, LANG-028, the consistency audit, and
the generated token reference record this partition. This iteration supersedes
iteration 9's initial 20-hard/9-contextual partition without changing any
construct's syntax or meaning.

## Implementation and coverage

- Twelve former hard tokens move to the generated `contextual` category; the
  parser's identifier projection accepts all 21 contextual token kinds.
- Statement dispatch uses bounded lookahead for bindings, loops, loop control,
  `scope`, `unsafe`, `else`, and `yield`, while retaining structural recovery
  for incomplete constructs.
- The parser collision fixture uses every contextual spelling as a record
  field, uses the newly contextual words as declaration and parameter names,
  covers call/arithmetic/record-expression collisions, and rejects all eight
  hard words as function names.
- The checker declares all 21 contextual spellings as record fields and
  functions, resolves all calls through the ordinary symbol index, lowers the
  calls to verified typed MIR, and executes their sum as `231` in the MIR
  oracle.
- A second checker record uses every C17 keyword that is a lexically eligible
  Ruda name and not one of Ruda's eight hard words, including `struct`, `union`,
  `typedef`, `static`, `register`, `sizeof`, `int`, and `void`. Leading-underscore
  C17 keywords remain unavailable because Ruda identifiers independently
  cannot begin with `_`; `if`, `return`, and `while` are Ruda hard words.
- The canonical grammar formatter fixture contains contextual parameter and
  function names and a flat nominal record pattern.
- Lexer tests preserve the exact token-kind partition and the two stable
  reserved-sentinel diagnostics; generated artifact checks make token category
  drift fail the repository checks.

## Verification

| Command | Exit | Result |
|---|---:|---|
| Debug build and `ctest --preset debug --output-on-failure` | 0 | warnings-as-errors C17 build; 1/1 passed |
| Release build and `ctest --preset release --output-on-failure` | 0 | optimized build; 1/1 passed |
| ASan build and strict `ctest --preset asan` | 0 | 1/1 passed without sanitizer report |
| ASan+UBSan build and strict `ctest --preset asan-ubsan` | 0 | 1/1 passed without sanitizer report |
| TSan build and strict `ctest --preset tsan` | 0 | 1/1 passed without sanitizer report |
| `clang --analyze` on parser, formatter, checker, MIR, and affected C-unit sources | 0 | zero findings |
| Generated, syntax-coverage, doc-link, spec-term, and example-syntax checks | 0 | generated references current; 74/74 syntax kinds and 115/115 productions covered; D001–D039 documentation consistent |
| `build/debug/ruda fmt examples --check` | 0 | all user examples canonical |

## Deliberate boundary

The grammar still uses distinct internal token kinds for contextual construct
recognition; their CST projection to `IDENTIFIER` at a name site is normative
and lossless. This iteration does not add escaped identifiers and therefore
does not make the eight hard spellings available to generated bindings.

The future C17 AOT backend is a separate implementation boundary. D039 requires
it to use
injective compiler-owned names rather than copying Ruda identifiers into C,
including for Ruda names that happen to be C keywords or implementation-
reserved C identifiers. I4 owns executable generated-C evidence; no native
backend is claimed here.
