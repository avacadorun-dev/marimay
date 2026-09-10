# CP42: revalidate checked JIT object views across native entries

Date: 2026-09-10. Status: **accepted as an opt-in implementation after local correctness
and performance validation; default enablement, Tier 1 and parity remain open.**

Source parent: `3562de98797320c135c03816c3bf9ef4de2d7a97` plus the 21 inputs in
the [validation record](cp42-sljit-views-validation-2026-09-10.json) and
[replayable candidate patch](cp42-sljit-views-candidate-2026-09-10.patch). The compiler embeds that
parent identity; its indexed inputs and binary hash identify the candidate.
Candidate SHA-256: `699a329a6959da419b2c51a498370f4dc6ebe3d37f0737e0599ccd115b398f52`.

## Measured cause and change

The preceding nine diagnostic profiles placed 49.6% of BFS and 58.2% of DSU
samples under object binding. Every return to native code discarded the prior
binding proof and repeated structural descriptor lookup and managed-reference
resolution. Inclusive sample percentages overlap and are not additive timings.

The D111 binding owner now retains view metadata and revalidates it before
reading any object pointer. A hit requires the exact execution, function,
register layout, value type, full managed reference, current heap provenance
and unchanged runtime view epoch. A miss retains the existing full checked
descriptor/reference binding. A failed miss cannot leave a partial reusable
binding. This adds no binding-hit allocation or benchmark-specific rule.

The runtime advances the lifetime epoch at its existing view-cache invalidation
boundary before collection traces or sweeps, including a collection that later
fails. UINT64_MAX saturation disables reuse for that heap lifetime. Fresh heap
provenance distinguishes disposal/reinitialization even at the same execution
address, slot, generation and epoch. Arrays retain pointers to nonmoving live
object fields, so growth changes the observed storage, capacity and length;
no copied storage-buffer address survives growth.

The candidate also carries forward the shared logical call/return owner,
bounded direct-callee admission, persistent invocation context and frame cursor
from the previously held experiment. Native transfers preserve logical depth,
arguments, typed roots and actual callee PCs on cold exits. Finite/shared calls
retain ordinary dispatch and exact original instruction accounting. D087's
unlimited-frame/no-shared-counter rule is unchanged. VM registers remain the
authoritative values; scalar register allocation and inlining are not added.

D111 and the execution specification state the retained-metadata revalidation
proof explicitly. No source syntax, language semantics, workload, argument,
checksum, managed-heap budget or JIT default changes.

## Correctness and reproduction

| Configuration | Tests | Frozen workload correctness |
|---|---:|---:|
| ARM64 Release, JIT ON | 93/93 full CTest | full paired series |
| ARM64 Release, JIT OFF | 86/86 full CTest | full paired series |
| ARM64 ASan/UBSan, JIT ON | 9/9 focused families | 32/32 |
| x86-64 Release under Rosetta, JIT ON | 8/8 focused families | 32/32 |

The new focused view owner proves an actual cache hit by leaving an unrelated
runtime cache entry untouched, with allocation failure armed at the next index.
It checks mutation/growth, successful and failed GC, callee changes, stale slot
reuse, value-type mismatch, wrong heaps, fresh provenance and sticky saturation
for both arrays and records. Teardown asserts zero live allocations and bytes.
The call, roots, fuel and code-memory owners retain differential, forced-GC,
deep-recursion, precise trap/PC and deterministic failure-index coverage.

The validation JSON retains compiler hashes, all build/test commands, logs,
both 32-workload correctness replays, source hashes and reproduction scripts.
The candidate patch applies to the named parent and reproduces all 21 indexed
inputs byte for byte. Use the documented opt-in CMake build and pinned SLJIT
dependency. The old prototype source guards and historical evidence remain
unchanged.

All timings use fresh processes, 3 warmups and 11 samples on the local Apple M2.
Source loading, checking and lowering remain inside VM observations, with the
unchanged 67,108,864-byte managed-heap limit and all 114 source/harness hashes.
Builds, tests and profiling are not run concurrently with measurements.
Local host isolation is unverified. macOS ASan uses detect_leaks=0; Linux LSan
and full Tier 1 acceptance remain open. Rosetta provides x86 functionality,
not physical x86 performance. The installed mmay remains unchanged and JIT
remains OFF by default. Overall native/VM parity remains unachieved.

## Predeclared acceptance and early result

The stage began at 12:18 UTC with a deadline of 14:18 UTC on September 10.
Before implementation, the SMART record required BFS improvement of at least
25%, DSU 20% and min-heap 10% against published JIT `8c14357`. The early 3+11
paired series met all three: 31.3%, 47.4% and 17.8%, respectively.

Full acceptance preserves the 2% median regression rule and adds a 5% P90
rule before measuring. P90 is interpolation at `0.9 * (n - 1)` in sorted
samples (the tenth value for 11 samples). Each flagged workload receives
exactly one further paired 3+11 series, independently for JIT and OFF. A
threshold exceeded in both series rejects the candidate. No observation
is discarded and no series is rerun until passing.

## JIT compared with published 8c14357

[Raw paired matrix](cp42-sljit-views-jit-paired-2026-09-10.json): sum of medians **14571.82 → 11190.30 ms (-23.2%)**.
Geometric mean of per-workload speedups: **1.138×**.
The sum weights longer workloads more heavily and is not an equal speedup
for each workload. Each full matrix contains 896 fresh processes.

| Workload | Before, ms | After, ms | Median change | P90 change |
|---|---:|---:|---:|---:|
| `title-search-10k` | 121.58 | 116.51 | -4.2% | -4.6% |
| `title-search-100k` | 663.69 | 611.44 | -7.9% | -8.3% |
| `normalize-title` | 204.79 | 185.77 | -9.3% | -8.4% |
| `word-frequency` | 344.87 | 333.79 | -3.2% | -2.5% |
| `report-builder` | 133.61 | 114.95 | -14.0% | -14.0% |
| `report-concat` | 212.03 | 194.48 | -8.3% | -7.5% |
| `map-int` | 89.95 | 90.11 | +0.2% | +0.1% |
| `map-string` | 116.37 | 115.19 | -1.0% | -1.2% |
| `map-string-book` | 130.34 | 127.95 | -1.8% | -2.0% |
| `set-string-churn` | 148.05 | 145.84 | -1.5% | -1.6% |
| `catalog-match` | 233.62 | 214.03 | -8.4% | -8.4% |
| `catalog-dotcall` | 179.10 | 167.77 | -6.3% | -5.8% |
| `catalog-interface` | 237.69 | 219.22 | -7.8% | +45.3% |
| `record-reference-copy` | 162.93 | 147.63 | -9.4% | -8.9% |
| `record-create` | 198.66 | 183.18 | -7.8% | -8.1% |
| `array-grow` | 169.64 | 143.25 | -15.6% | -16.5% |
| `array-reserved` | 168.83 | 142.74 | -15.5% | -15.7% |
| `bfs-int` | 138.85 | 120.21 | -13.4% | -13.6% |
| `bfs-record` | 152.96 | 132.41 | -13.4% | -13.5% |
| `catalog-pipeline` | 443.57 | 388.45 | -12.4% | -12.3% |
| `parse-result` | 114.30 | 108.43 | -5.1% | -5.5% |
| `insertion` | 230.09 | 227.85 | -1.0% | -3.6% |
| `heap-sort` | 433.22 | 343.80 | -20.6% | -20.4% |
| `stack` | 742.50 | 580.50 | -21.8% | -21.3% |
| `queue` | 852.49 | 611.63 | -28.3% | -28.5% |
| `min-heap` | 1116.97 | 925.62 | -17.1% | -16.9% |
| `dsu` | 2683.60 | 1375.28 | -48.8% | -50.2% |
| `bfs` | 3079.32 | 2137.11 | -30.6% | -31.1% |
| `dijkstra` | 265.02 | 246.56 | -7.0% | -7.3% |
| `frequency-map` | 346.03 | 323.32 | -6.6% | -5.7% |
| `unique-set` | 264.05 | 240.94 | -8.8% | -6.3% |
| `rune-frequency` | 193.09 | 174.34 | -9.7% | -9.0% |

The first catalogue-interface series retains two slower candidate observations
(346.48 and 347.51 ms versus a 219.22 ms median). The confirmation retains all
11 observations (219.89–223.90 ms); the P90 regression is not reproduced.

One predeclared confirmation for each flagged workload:

| Workload | Full median | Repeat median | Full P90 | Repeat P90 | Confirmed regression |
|---|---:|---:|---:|---:|---|
| [catalog-interface](cp42-sljit-views-jit-catalog-interface-confirmation-2026-09-10.json) | -7.8% | -7.7% | +45.3% | -8.6% | none |

## Default interpreter compared with f43647e

[Raw paired matrix](cp42-sljit-views-interpreter-paired-2026-09-10.json): sum of medians **27063.20 → 27050.94 ms (-0.0%)**.
Geometric mean of per-workload speedups: **1.000×**.
The sum weights longer workloads more heavily and is not an equal speedup
for each workload. Each full matrix contains 896 fresh processes.

| Workload | Before, ms | After, ms | Median change | P90 change |
|---|---:|---:|---:|---:|
| `title-search-10k` | 144.30 | 143.99 | -0.2% | -0.6% |
| `title-search-100k` | 888.05 | 884.63 | -0.4% | -3.1% |
| `normalize-title` | 210.62 | 209.62 | -0.5% | -0.5% |
| `word-frequency` | 402.69 | 400.19 | -0.6% | -0.7% |
| `report-builder` | 137.08 | 137.62 | +0.4% | +1.3% |
| `report-concat` | 216.71 | 217.63 | +0.4% | -2.2% |
| `map-int` | 105.20 | 105.04 | -0.2% | -0.8% |
| `map-string` | 133.04 | 133.46 | +0.3% | -0.0% |
| `map-string-book` | 132.71 | 132.61 | -0.1% | -0.0% |
| `set-string-churn` | 147.84 | 146.92 | -0.6% | -0.7% |
| `catalog-match` | 268.23 | 267.83 | -0.1% | -0.6% |
| `catalog-dotcall` | 195.41 | 197.30 | +1.0% | +1.0% |
| `catalog-interface` | 255.13 | 257.37 | +0.9% | -2.4% |
| `record-reference-copy` | 178.69 | 179.55 | +0.5% | -0.5% |
| `record-create` | 216.48 | 216.25 | -0.1% | -0.1% |
| `array-grow` | 202.35 | 203.15 | +0.4% | +0.2% |
| `array-reserved` | 201.78 | 202.78 | +0.5% | +0.7% |
| `bfs-int` | 216.77 | 217.83 | +0.5% | +0.6% |
| `bfs-record` | 234.21 | 233.82 | -0.2% | -2.6% |
| `catalog-pipeline` | 539.91 | 539.95 | +0.0% | +1.1% |
| `parse-result` | 119.43 | 117.85 | -1.3% | -1.6% |
| `insertion` | 228.90 | 228.00 | -0.4% | +1.5% |
| `heap-sort` | 1647.51 | 1646.16 | -0.1% | -1.1% |
| `stack` | 781.52 | 789.56 | +1.0% | +0.3% |
| `queue` | 884.85 | 885.25 | +0.0% | +0.3% |
| `min-heap` | 5527.46 | 5466.16 | -1.1% | +1.3% |
| `dsu` | 3834.34 | 3845.09 | +0.3% | +2.3% |
| `bfs` | 5540.49 | 5565.49 | +0.5% | +0.7% |
| `dijkstra` | 2392.85 | 2402.16 | +0.4% | +1.3% |
| `frequency-map` | 478.51 | 476.80 | -0.4% | -0.9% |
| `unique-set` | 352.06 | 352.04 | -0.0% | -1.3% |
| `rune-frequency` | 248.09 | 248.84 | +0.3% | +1.2% |

No workload exceeds either full-series threshold; no confirmation was run.

## Fresh five-participant comparison

[Collector table with full dispersion](cp42-sljit-views-comprehensive-2026-09-10.md)
retains the generated report unchanged; its VM column uses this opt-in JIT.

[Raw full comparison](cp42-sljit-views-comprehensive-2026-09-10.json), generated `2026-09-10T20:18:12+07:00`.
All five columns below come from this one new series using the candidate
compiler for both Marimay modes. VM means source run with explicitly enabled
SLJIT. Native compilation is excluded; VM includes source preparation and
Bun includes fresh-process startup, TypeScript loading and JIT.

| Workload | Marimay native | Go native | Rust native | Marimay VM + JIT | Bun / TypeScript |
|---|---:|---:|---:|---:|---:|
| `title-search-10k` | 9.27 | 4.47 | 4.47 | 116.76 | 13.23 |
| `title-search-100k` | 74.38 | 15.16 | 18.21 | 606.68 | 36.66 |
| `normalize-title` | 57.90 | 9.01 | 11.11 | 184.15 | 17.68 |
| `word-frequency` | 146.07 | 19.08 | 30.49 | 333.45 | 18.97 |
| `report-builder` | 14.96 | 5.21 | 5.48 | 115.04 | 12.32 |
| `report-concat` | 97.99 | 80.50 | 47.80 | 194.21 | 12.56 |
| `map-int` | 7.63 | 5.22 | 6.01 | 90.14 | 13.76 |
| `map-string` | 23.22 | 7.49 | 9.89 | 115.56 | 15.35 |
| `map-string-book` | 31.60 | 6.66 | 10.56 | 127.30 | 15.48 |
| `set-string-churn` | 47.96 | 10.13 | 17.36 | 145.72 | 17.95 |
| `catalog-match` | 13.54 | 4.72 | 4.38 | 214.39 | 19.21 |
| `catalog-dotcall` | 8.75 | 4.88 | 3.88 | 167.59 | 18.24 |
| `catalog-interface` | 12.29 | 4.92 | 4.25 | 220.32 | 18.47 |
| `record-reference-copy` | 7.03 | 4.56 | 3.34 | 148.37 | 14.73 |
| `record-create` | 23.23 | 6.84 | 6.43 | 183.69 | 15.95 |
| `array-grow` | 7.07 | 6.73 | 4.14 | 143.21 | 14.05 |
| `array-reserved` | 6.98 | 5.39 | 4.33 | 142.99 | 12.91 |
| `bfs-int` | 7.62 | 5.38 | 4.53 | 120.84 | 17.10 |
| `bfs-record` | 9.60 | 5.98 | 4.82 | 132.54 | 17.39 |
| `catalog-pipeline` | 38.08 | 8.18 | 10.46 | 391.60 | 23.26 |
| `parse-result` | 19.89 | 6.04 | 6.10 | 108.71 | 13.50 |
| `insertion` | 49.87 | 29.28 | 28.73 | 229.13 | 81.29 |
| `heap-sort` | 32.82 | 39.29 | 22.38 | 344.28 | 93.65 |
| `stack` | 10.15 | 14.95 | 9.25 | 584.52 | 22.24 |
| `queue` | 10.16 | 14.84 | 9.39 | 611.38 | 26.42 |
| `min-heap` | 47.01 | 54.28 | 30.09 | 922.54 | 110.04 |
| `dsu` | 30.20 | 24.59 | 22.10 | 1374.13 | 48.92 |
| `bfs` | 67.15 | 63.90 | 24.22 | 2123.41 | 69.01 |
| `dijkstra` | 42.06 | 41.66 | 39.70 | 246.75 | 76.36 |
| `frequency-map` | 33.10 | 14.34 | 22.49 | 327.66 | 28.09 |
| `unique-set` | 27.65 | 16.40 | 19.74 | 240.85 | 22.27 |
| `rune-frequency` | 22.79 | 6.08 | 6.56 | 173.35 | 17.37 |

Times are medians in milliseconds; smaller is better. This comparison does
not establish parity. Any variability labels remain visible in the raw report.

## Remaining work

The measured binding overhead is reduced. Source preparation, scalar
register traffic, logical activation costs and independent native text/runtime
costs still require measured work. Full Tier 1 validation, Linux LeakSanitizer,
physical x86 performance and overall Go/Rust parity remain open.
