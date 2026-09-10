# CP42: shared JIT memory ownership and composed loops

Date: 2026-09-10. Status: **experimental; excluded from production CMake**.
Production source base: `f43647eeef4728eed96ab4855eb7ce02549dae7a`.
Previous experiment: `0145177bd4c6366128a2039fd094ba1fede30f3a`.
SLJIT: `d6ec3d07dcdbf2256ccae2b84d0d8a2d019f392e`, with the recorded local
ARM64 immediate-selection patch. Host: Apple M2/macOS, `local-unverified`.

## What changed

The shared generator now uses the central Marimay allocator for compiler
arenas and staging. A separate owner reserves page-rounded code mappings and
publishes relocated instructions once. macOS ARM64 uses MAP_JIT with an
allowlisted write callback; macOS x86/Linux use RW-to-RX mappings; Windows has
VirtualAlloc/VirtualProtect paths. Only macOS paths have execution evidence here.
Published code is immutable; no generic RWX fallback is permitted.

The code limit counts complete mapped pages, including rounding. Code-budget
exhaustion skips a function. Unsupported executable memory falls back to the
interpreter after releasing all compiled functions. Actual allocation failure
remains observable. Failed release of an owned mapping is a fatal ownership
violation, never a successful zero-ledger teardown. Managed heap accounting,
source semantics and the 67,108,864-byte benchmark heap limit are unchanged.

D109's existing nonallocating array-shift owner executes before generic JIT
segment reservation. Its exact source prefix and fuel are returned to the VM,
and binding epochs expire after progress. This retains the composed loop that
the earlier generic JIT displaced. No workload-specific source detection or
benchmark change was introduced.

ASan/UBSan found two ARM64 immediate-selection paths in upstream SLJIT that
could negate INT64_MIN in signed C arithmetic. The small recorded patch falls
back to existing register operands for that immediate. The upstream checkout
is unchanged; the builder patches a pinned copy. Boundary comparison and
addition are included in the focused memory test. No sanitizer suppression or
wrapping-arithmetic compiler flag was added.

## Correctness and ownership evidence

The [durable validation record](cp42-sljit-owned-memory-validation-2026-09-10.json)
contains compile/link commands, all source/archive/compiler hashes, exact test
logs and all 32 source-run commands/results for each configuration:

| Configuration | Bytecode suite | Unchanged workloads | Focused ownership |
|---|---|---|---|
| macOS ARM64 Release | passed | 32/32 | passed |
| macOS x86-64 Release under Rosetta | passed | 32/32 | passed |
| macOS ARM64 ASan/UBSan | passed | 32/32 | passed |

The existing bytecode suite includes finite frame/shared-fuel differential
checks, composed-loop side exits, frame reuse, managed-reference/GC and
allocation tests. Its production native oracle remains intact. These tests
exercise the experimental VM but do not establish exhaustive coverage of every
JIT operation and transition.

The focused memory test exhausts 5 reachable central-allocation failure indices
and 2 platform failure indices, plus success after the last failure. It checks
alignment, staging-length rejection, page/ledger overflow, code-budget rejection,
relocated calls/branches, minimum signed values, duplicate publication and
write protection. A child process must receive a protection fault when writing
published code on the tested macOS hosts.

The focused two-function JIT test exhausts 12 central-allocation failure indices
and 4 platform failure indices, including failures after a previous function
has published code. It checks the complete fallback/error result, exact
source-PC/frame/shared-fuel exit and idempotent teardown. Every observed teardown
asserts `live_allocations == 0`, `live_bytes == 0`, zero mapped pages and zero
mapped bytes. This is bounded ownership evidence, not a universal leak claim.

ASan/UBSan instrument the new VM adapter, both memory owners and SLJIT itself.
macOS used `ASAN_OPTIONS=detect_leaks=0:halt_on_error=1` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`; Linux LeakSanitizer remains
required. Rosetta is actual execution of the complete x86-64 VM corpus, but is
neither physical x86-64 performance evidence nor Linux/Windows validation.

## Performance

The [paired full-matrix record](cp42-sljit-owned-memory-paired-2026-09-10.json)
contains 896 fresh-process observations: 32 workloads × two pinned compilers
× (3 warmups + 11 samples), with alternating participants and rotating workload
order. All checksums passed. All 114 input/harness hashes and pinned
compiler/runtime/header hashes matched before and after. Source loading,
checking, lowering and verification are included in every VM observation.

Sum of medians: **27,246.42 → 14,674.18 ms**, a 46.1% reduction.
The geometric mean of control/candidate ratios is 1.357×. Small differences
are not asserted statistically significant: insertion is +0.2% and
set-string-churn +1.0%; no median regression exceeds 2%. This is a local
paired comparison, not a Tier 1 or Go/Rust parity result.

| Workload | Control, ms | Owned-memory JIT, ms | Control/JIT |
|---|---:|---:|---:|
| `title-search-10k` | 144.32 | 122.92 | 1.17× |
| `title-search-100k` | 894.02 | 673.33 | 1.33× |
| `normalize-title` | 212.04 | 204.98 | 1.03× |
| `word-frequency` | 406.95 | 346.26 | 1.18× |
| `report-builder` | 138.20 | 132.89 | 1.04× |
| `report-concat` | 218.20 | 215.01 | 1.01× |
| `map-int` | 106.14 | 90.48 | 1.17× |
| `map-string` | 133.87 | 117.60 | 1.14× |
| `map-string-book` | 133.18 | 130.50 | 1.02× |
| `set-string-churn` | 148.12 | 149.53 | 0.99× |
| `catalog-match` | 271.33 | 236.03 | 1.15× |
| `catalog-dotcall` | 197.45 | 179.09 | 1.10× |
| `catalog-interface` | 256.66 | 238.32 | 1.08× |
| `record-reference-copy` | 180.05 | 164.42 | 1.10× |
| `record-create` | 217.36 | 199.30 | 1.09× |
| `array-grow` | 202.87 | 173.26 | 1.17× |
| `array-reserved` | 202.55 | 172.10 | 1.18× |
| `bfs-int` | 218.37 | 141.37 | 1.54× |
| `bfs-record` | 235.30 | 155.84 | 1.51× |
| `catalog-pipeline` | 542.53 | 437.18 | 1.24× |
| `parse-result` | 120.13 | 115.15 | 1.04× |
| `insertion` | 231.63 | 232.14 | 1.00× |
| `heap-sort` | 1665.11 | 442.87 | 3.76× |
| `stack` | 788.23 | 741.79 | 1.06× |
| `queue` | 890.91 | 849.94 | 1.05× |
| `min-heap` | 5564.24 | 1119.94 | 4.97× |
| `dsu` | 3852.90 | 2661.70 | 1.45× |
| `bfs` | 5578.81 | 3100.61 | 1.80× |
| `dijkstra` | 2406.56 | 314.64 | 7.65× |
| `frequency-map` | 482.07 | 351.08 | 1.37× |
| `unique-set` | 356.92 | 268.10 | 1.33× |
| `rune-frequency` | 249.38 | 195.80 | 1.27× |

Candidate SHA-256: `e1af284eed3e5385063ee87478147bc7d187452f250c34212ec9fedec3be1116`.
The validation record and timing snapshot refer to this same compiler. A
repeat build at the recorded output path is byte-identical. Changing the output
path changes SLJIT diagnostic `__FILE__` strings and the executable hash;
path-independent binary reproducibility is not claimed.

## Reproduction and open gates

The [experiment README](../../../tools/performance/sljit-prototype/README.md)
provides exact build/test commands. `build.py --unit --memory-test` rebuilds
the ordinary and focused suites; `--sanitize` requires a matching instrumented
baseline archive. A macOS x86-64 baseline uses
`-DCMAKE_OSX_ARCHITECTURES=x86_64` and the adapter uses `--arch x86_64`.
`check.py` independently preserves all 114 reference input hashes and rejects
any changed checksum, argument or compiler. It makes no performance claim.

This closes the earlier experiment's missing central hooks and local
executable-memory ownership checks. Production dependency/build integration,
complete JIT conformance, every supported Tier 1 host and Linux LSan remain
open. Calls still return to ordinary VM activations; frequent calls, scalar
register traffic, runtime string/collection costs and source preparation still
separate this version from Go/Rust parity. Installed `mmay` is not replaced.
