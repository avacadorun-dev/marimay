# A0 bounded architecture checkpoint 0 — 2026-08-14

Status: local verification complete; Tier 1 push/PR CI pending.

Base commit: `0d08dd28d3d446b15723f70de34beac563e01ccf`.

## Scope

This checkpoint is a behavior-preserving physical decomposition of the C17
bootstrap and its C unit suites. It introduces no Ruda language, MIR,
bytecode, runtime, diagnostic, formatter, or CLI semantic change.

The blocking problem was that one context packet required loading files from
1,545 to 15,112 lines. The extraction creates independently compiled semantic
owners for checker phases, MIR model/verification/oracle execution, bytecode
model/codec/verification/VM execution, parser grammar families, formatter
collection/rendering, CLI formatting/target discovery, native validation and
emission, lexer scanning, manifest parsing/formatting, and the corresponding
test families.

## Enforced contract

`AGENTS.md` and `compiler-architecture.md` now require:

- a 500–1,200 line target and 1,500-line hard ceiling for handwritten C;
- a 400-line hard ceiling for private headers and 160 for owner API headers;
- tighter path ceilings for stable coordinators and the checker shared
  contract;
- independently compiled translation units, with no handwritten `.inc`, C
  inclusion, or include facade;
- CMake registration for every production and test C source;
- coarse owner dependency direction;
- `tools/check-architecture` after every feature and checkpoint and on every
  push/pull request.

The exception registry is empty. Existing hotspots were not grandfathered.

## Inventory

| Owner family | Largest before | Largest after |
|---|---:|---:|
| checker | 15,112 | 1,137 |
| checker tests | 8,271 | 1,175 |
| MIR | 5,373 | 1,108 |
| MIR tests | 3,396 | 1,089 |
| bytecode + VM | 3,363 | 1,017 |
| bytecode + VM tests | 3,152 | 1,072 |
| parser | 2,992 | 1,150 |
| parser tests | 2,304 | 918 |
| native emitter | 1,343 | 525 |
| native emitter tests | 2,737 | 867 |
| CLI | 2,248 | 826 |
| formatter | 1,596 | 1,115 |
| lexer | 1,231 | 1,161 |
| manifest | 1,284 | 698 |

Across the resulting local tree, the largest handwritten production/test file
is 1,175 lines, the largest owner-private header is 221 lines, and the largest
checker owner API is 139 lines. There are 94 production/platform and 82
unit/fuzz C translation units.

## Post-audit public-header closure — 2026-08-15

The first version of `tools/check-architecture` discovered all handwritten C
translation units but applied the 160-line public owner-header ceiling only to
the bytecode include subtree. The corrected gate recursively scans both
`bootstrap/include/ruda/` and `runtime/include/ruda/`, excluding generated
artifacts only. Its built-in policy regression proves discovery of checker and
runtime public headers and proves the exact 160/161 acceptance boundary.

The former aggregate bytecode, MIR, checker/service, type-system, and managed
runtime headers are now backwards-compatible umbrellas over self-contained
semantic owner headers. The largest handwritten public owner header is
`bootstrap/include/ruda/bytecode/vm.h` at exactly 160 lines. The exception
registry remains empty.

## Semantic ownership review

The first mechanical checker split was rejected even though it passed the line
gate: the record owner still contained the global expression dispatcher, owner
API headers redeclared dependency bundles, and the shared internal header
contained package/function/match mutable state. The accepted split gives the
dispatcher its own owner, leaves record and nominal/enum lowering with their
respective owners, separates package/function/match context contracts, reduces
`checker_internal.h` to 146 lines, and reduces every checker owner API to at
most 139 lines.

The original checker, parser, formatter, bytecode, VM, MIR, MIR-bytecode, and
native-emitter test aggregator call sequences were preserved. Focused suites
share fixture owners instead of duplicating fixtures or assertions.

## Local validation

Environment:

- macOS Darwin 25.3.0, arm64;
- Apple Clang 17.0.0;
- CMake 4.4.2.

Commands and results:

```text
./tools/check-architecture                         ok
./tools/check-c-memory-policy                      ok
./tools/check-generated                            ok
./tools/check-doc-links                            ok
./tools/check-spec-terms                           ok
./tools/check-example-syntax                       ok
./tools/check-syntax-coverage                      ok; 74 syntax kinds and 115 productions covered
git diff --check                                   ok
cmake --build --preset debug --parallel 4          ok
ctest --preset debug --output-on-failure           1/1 passed; 23.90 s
cmake --build --preset release --parallel 4        ok
ctest --preset release --output-on-failure         1/1 passed; 21.56 s
cmake --build --preset asan-ubsan --parallel 4     ok
ctest --preset asan-ubsan --output-on-failure      1/1 passed; 34.78 s
```

## Remaining verification

The pushed architecture commit passed the complete configured Gitea Linux
x86_64 matrix, including Clang/GCC Debug and Release, ASan/UBSan/LSan, TSan,
and documentation/architecture invariants in
[Gitea run 26](http://ci.example:3000/Marimay/marimay/actions/runs/26). A0 remains
`in-progress`: Linux arm64, macOS arm64/x86_64, and Windows x86_64 Tier 1 jobs
still require linked GitHub Actions evidence, and the public-header closure
must pass the CI run for its containing pushed checkpoint commit.
