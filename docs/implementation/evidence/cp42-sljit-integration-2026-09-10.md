# CP42: optional shared JIT integrated into the C17 compiler

Date: 2026-09-10. Status: **implemented opt-in; passed local validation;
not enabled by default and not Tier 1/parity acceptance**.
Source parent: `496872b52123b6dd1a83adb93f9b9616132fd962` plus the production
inputs recorded in the [validation record](cp42-sljit-integration-validation-2026-09-10.json).
The measured compiler embeds that parent build identity; its exact binary hash
and indexed input hashes identify this integration candidate.
Control: production VM at `f43647eeef4728eed96ab4855eb7ce02549dae7a`.
Host: Apple M2/macOS, `local-unverified`.

## Change

[D111](../vm-jit.md) moves the reused prototype into independently compiled
production owners behind `MMAY_ENABLE_SLJIT=ON`. The common C adapter uses SLJIT
for machine instructions on ARM64 and x86-64. No handwritten CPU emitter or
source/native-executable substitution is introduced. The default OFF build
compiles out JIT state and calls and requires no third-party dependency.

CMake accepts an explicit local dependency matching the 35-file pinned
manifest, applies the verified ARM64 minimum-immediate patch and installs owned
allocation hooks. The preparer has no network path. The code-page owner is part
of the central allocation boundary, with an 8 MiB page-rounded budget, immutable
W^X publication and complete teardown. Both temporary allocations and code
mappings have deterministic failure coverage. Enabled installations preserve
the upstream BSD-2-Clause license.

The coordinator uses a narrow tier-entry result; it does not inspect compiled
plans. Cold calls, logical frames, roots, return checks, capabilities and panic
handling retain their existing owners. Exact finite/shared fuel, invocation
selected unmetered mode and D109 composed loops are carried forward.

Production conformance found a real prototype defect: I64-named wire opcodes
also carry narrower and unsigned integer types. Emitting signed i64 operations
for them produced noncanonical i8 bits and a spurious integer-overflow panic in
`numeric_cp6` policy checks. The corrected plan admits exact i64 operands only;
other integer widths use their existing typed VM rules. The initial 89/90 test
failure is retained in the validation record alongside the passing final run.

Record field projections now read the authoritative original signed operand,
and enum reads retain their original full-width selector. Focused tests cover
the second record field and a variant-1 selector exceeding UINT32_MAX. Existing
uncommitted execution-image edits in the user's main checkout were preserved;
this change does not depend on them.

## Validation

The [validation record](cp42-sljit-integration-validation-2026-09-10.json) contains
compiler hashes, build/test commands, flags, dependency digests, logs and all
three 32-workload correctness replays:

| Configuration | Tests | Frozen workload correctness |
|---|---:|---:|
| macOS ARM64 Release, JIT ON | 90/90 full CTest | 32/32 |
| macOS ARM64 Release, JIT OFF | 86/86 full CTest | independent interpreter control |
| macOS ARM64 ASan/UBSan, JIT ON | 6/6 focused families including bytecode/runtime | 32/32 |
| macOS x86-64 Release under Rosetta, JIT ON | 5/5 focused families including bytecode | 32/32 |

Verified fixtures assert actual JIT admission before comparing scalar results,
source traps and every finite/shared-fuel boundary with independent single-
function dispatch. They cover signed i64 extrema, zero divisors, comparisons,
negation and all other integer widths' fallback. Nominal tests run with forced
collection. Bytecode/runtime suites retain VM/native differential coverage.

Code-memory tests exhaust 5 allocator and 2 platform failure indices. Two-
function preparation exhausts 12 allocator and 4 platform failure indices in
each fuel mode, including failures after the first publication. All teardown
paths assert zero live allocations/bytes and mappings/bytes. Publication tests
also exercise relocated calls/branches, INT64_MIN and denied writes to code.

Six pinned-input tests cover exact patch bytes, repeat/recovery, changed input,
changed patch/result digest, hook shadowing, foreign output and path escape.
The final preparer writes/validates bytes without Windows newline translation;
rebuilding at the same macOS path leaves all five tested executables byte-
identical. This does not claim path-independent binary reproduction. Missing
local dependency configuration fails explicitly. Installed license bytes match
upstream, and symbol inspection finds no JIT/SLJIT code in the OFF compiler.

Architecture, central-memory, generated-artifact, roadmap, specification-term
and documentation-link gates pass. The decision index also now includes the
previously accepted D110 alongside D111. Linux Release and ASan/UBSan/LSan JIT
jobs are configured in CI; their execution has not been observed. macOS ASan
uses `detect_leaks=0`. Rosetta is functional evidence, not physical x86 timing.
Linux/Windows execution, full Tier 1 acceptance and default enablement remain
open.

## Paired performance

The [raw full matrix](cp42-sljit-integration-paired-2026-09-10.json) contains 896
fresh processes: 32 unchanged workloads × two pinned compilers × (3 warmups +
11 samples). Participant order alternates and workload order rotates. All 114
source/harness hashes, compiler hashes and expected outputs passed. Source
loading, checking and lowering remain inside `mmay run`, with the unchanged
67,108,864-byte managed-heap limit. Builds/tests were finished before timing.

The sum of VM medians changes from **27475.64 to 14704.54 ms
(-46.5%)**. The geometric mean of per-workload control/candidate
ratios is **1.366×**. No median regression exceeds 2%; small movements within
noise are not individual speedup claims. Go, Rust, Bun and native Marimay were
not remeasured in this two-compiler comparison, so this is not parity evidence.

| Workload | Production VM, ms | Integrated JIT VM, ms | Candidate/control |
|---|---:|---:|---:|
| `title-search-10k` | 146.96 | 124.05 | 0.844× |
| `title-search-100k` | 913.40 | 670.69 | 0.734× |
| `normalize-title` | 214.01 | 208.45 | 0.974× |
| `word-frequency` | 410.57 | 350.02 | 0.853× |
| `report-builder` | 139.20 | 135.88 | 0.976× |
| `report-concat` | 220.63 | 218.40 | 0.990× |
| `map-int` | 106.69 | 91.46 | 0.857× |
| `map-string` | 135.59 | 118.97 | 0.877× |
| `map-string-book` | 134.20 | 132.94 | 0.991× |
| `set-string-churn` | 150.82 | 151.23 | 1.003× |
| `catalog-match` | 270.52 | 235.96 | 0.872× |
| `catalog-dotcall` | 197.80 | 180.90 | 0.915× |
| `catalog-interface` | 257.75 | 242.39 | 0.940× |
| `record-reference-copy` | 181.92 | 164.91 | 0.906× |
| `record-create` | 219.08 | 201.58 | 0.920× |
| `array-grow` | 203.42 | 171.57 | 0.843× |
| `array-reserved` | 204.56 | 171.21 | 0.837× |
| `bfs-int` | 219.56 | 140.57 | 0.640× |
| `bfs-record` | 237.38 | 156.08 | 0.658× |
| `catalog-pipeline` | 549.89 | 446.48 | 0.812× |
| `parse-result` | 121.33 | 116.17 | 0.957× |
| `insertion` | 232.38 | 234.75 | 1.010× |
| `heap-sort` | 1670.72 | 432.12 | 0.259× |
| `stack` | 794.62 | 748.44 | 0.942× |
| `queue` | 895.91 | 865.12 | 0.966× |
| `min-heap` | 5632.05 | 1131.67 | 0.201× |
| `dsu` | 3879.26 | 2658.48 | 0.685× |
| `bfs` | 5620.75 | 3112.13 | 0.554× |
| `dijkstra` | 2417.62 | 268.49 | 0.111× |
| `frequency-map` | 484.97 | 360.31 | 0.743× |
| `unique-set` | 361.87 | 268.01 | 0.741× |
| `rune-frequency` | 250.19 | 195.10 | 0.780× |

Candidate SHA-256: `83564606c695677e4c1ac88528bf7a80ce291a65d84d9b3303e76b33e57ea07d`.

## Reproduction and next boundary

Follow the [current CMake instructions](../vm-jit.md). Reproduce the frozen
comparison with `tools/performance/compare-vm-preparation.py`, the recorded
32-workload reference and pinned before/after toolchains, without workload
filters or changed arguments. The historical adapter remains reproducible from
its recorded commits; its source-hash guards intentionally reject current
production coordinators.

Ordinary activation/call overhead remains large, especially DSU, stack and
BFS. The next JIT layer must retain explicit logical frames and reconstruct the
actual callee PC/registers on a cold exit; replaying an already mutating callee
from its call site would violate semantics. Native call support/inlining is
not implemented by this integration. Native text/runtime costs and Go/Rust
parity across all 32 workloads remain open.
