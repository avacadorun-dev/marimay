# I1 formatter CLI iteration 5 evidence — 2026-08-10

Gate: I1 — lossless frontend and canonical formatter.  
Iteration status: complete; gate status remains `in-progress`.  
Environment: macOS Darwin arm64, AppleClang 17.0.0, CMake 4.4.2.  
Applicable contracts: D001, D015, D026–D028, D031, toolchain §4, implementation brief §§5 and 7.4, test strategy §§2.1–2.2.

## Delivered vertical slice

- `ruda fmt` now supports stdin, an explicit `.ruda`/`ruda.mod`/`ruda.work` file, an explicit directory, and no-target module/workspace discovery;
- `--check` and `--diff` are deterministic non-writing modes, while in-place source updates use a flushed, permission-preserving sibling temporary and atomic replacement;
- directory selection is bounded, sorted, no-follow, excludes `.git`/`.ruda`, and stops at nested `ruda.mod` boundaries under D028;
- the D027 bounded manifest parser/formatter canonicalizes module and workspace manifests and participates in the same CLI traversal;
- D031 braced import lists, record construction/patterns, and named enum payloads use the D026 width oracle with canonical compact/broken trailing separators;
- contiguous comment-free imports are grouped standard/current-module/third-party and sorted by decoded path; a comment pins its adjacent boundary so sorting never moves declarations across it;
- formatter semantic-equivalence tests compare root import declarations as an order-insensitive set while retaining strict order for all non-import AST nodes;
- formatter-owned import decoding, ordering, token rebuilding, and rendering are covered by fail-at-every-allocation cleanup tests;
- the public executable, internal namespace, examples, manifests, and source suffix use the accepted Ruda identity; `.ma` is an explicit rejected formatter target, not a compatibility alias.

## Commands and exit status

| Command | Exit | Result |
|---|---:|---|
| `./tools/check-generated` | 0 | committed diagnostic, Unicode, token, and syntax artifacts match generators |
| `./tools/check-doc-links` | 0 | local documentation links and fences valid |
| `./tools/check-spec-terms` | 0 | accepted decisions and normative term checks pass |
| `./tools/check-example-syntax` | 0 | all `.ruda` examples pass the independent lexical audit |
| `cmake --build --preset debug --parallel && ctest --preset debug` | 0 | warnings-as-errors Debug build; C unit, frontend, formatter, CLI, snapshot, and fixture cases pass |
| equivalent `release`, `asan`, `asan-ubsan`, and `tsan` preset build/test commands | 0 | 1/1 CTest executable passed in every maintained local preset with no sanitizer report |
| `clang --analyze -std=c17 ...` over bootstrap/common POSIX runtime/C tests | 0 | zero static-analyzer findings |
| `build/debug/ruda --help` and `build/debug/ruda --version` | 0 | current Ruda CLI identity and build metadata render successfully |
| `build/debug/ruda fmt examples --check` | 0 | repository examples are canonical |

## Test surfaces

- exact golden files cover braced fit/break behavior, named enum payload semicolons, import grouping/sorting by decoded path, comment barriers, broad grammar, and comment placement;
- CLI tests cover stdin, explicit file, directory tree, no-target module/workspace discovery, check/diff, invalid input, symlinks, nested modules, traversal limits, transactional replacement failures, and rejection of the former `.ma` suffix;
- parse/format/parse checks preserve comments and semantic AST, treating only top-level import order as semantically irrelevant;
- deterministic whitespace generation, example-corpus idempotence, defensive limits, and allocation/file-I/O failure injection remain green.

## Remaining I1 work

I1 is not verified. A grammar-production-indexed positive/negative/recovery fixture manifest, canonical ordinary-string spelling, incremental green-tree reuse, and an accepted time-budgeted parser/formatter fuzz campaign remain open. Tier 1 evidence also requires the configured CI matrix to run on its claimed hosts rather than relying on this macOS arm64 result.
