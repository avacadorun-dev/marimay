# Nonallocating array segments in the optional SLJIT tier

This stage extends the accepted `bbb9b2b` implementation with complete
non-string reference-array reads and scalar pushes into existing spare capacity.
The source workloads and language semantics are unchanged. JIT remains opt-in.

## Hypothesis, controls and acceptance

The stage began at 13:58 UTC on September 10, 2026, with a 15:58 UTC checkpoint.
Before editing, the record required at least 25% early BFS improvement against
`bbb9b2b` and at least 5% improvement in the full JIT sum of medians. Every one
of the 32 workloads must avoid confirmed regressions above 2% median or 5% P90,
independently for JIT and the default interpreter. Each flagged workload gets
exactly one further paired series; a statistic exceeding its threshold in both
series rejects the candidate. P90 is the tenth sorted observation for 11 samples.
No observation is discarded and no series is repeated until passing.

The first read-only candidate missed the early gate: BFS 2135.28 → 1915.02 ms
(10.3%), BFS-record 133.00 → 120.34 ms (9.5%), catalogue-pipeline
386.29 → 289.29 ms (25.1%). Its executable, patch, observations and sampling
report were preserved before the next edit. It is a rejected intermediate
experiment, not the accepted implementation.

Fresh sampling of that candidate still placed 43.7% of sampled BFS stacks under
cold value dispatch, including pushes and repeated array descriptor/type work.
Sampling is diagnostic: inclusive stacks overlap, process startup and the tail
can be missed, and anonymous frames are not automatically claimed as JIT code.
The second scope was recorded before editing: scalar pushes may remain native
only when a checked live view proves spare capacity. The baseline, deadline,
workloads and numerical acceptance thresholds were unchanged.

The second early 3+11 paired series measured BFS 2149.39 → 1439.04 ms (33.0%),
BFS-record 134.28 → 115.51 ms (14.0%), and catalogue-pipeline
389.90 → 290.70 ms (25.4%). Its executable hash was checked against the final
candidate before the full validation. Full matrices, rather than this early
subset, determine performance acceptance.

Both controls are preserved binaries from `bbb9b2b`: enabled JIT SHA-256
`699a329a6959da419b2c51a498370f4dc6ebe3d37f0737e0599ccd115b398f52`,
and interpreter SHA-256
`552202637a49a0866fc4e99d2131b91c4fb0d30987f95bf499938e293f7e991a`.
Every matrix uses 3 warmups and 11 samples per participant, fresh processes,
unchanged checksums, 114 source/harness hashes and the same 64 MiB heap budget.
Source loading, checking and lowering remain inside source-run observations.
Builds, tests, sampling and gates finish before timing begins.

## Implementation and proof boundaries

The existing D111 parent-view binding validates exact descriptor, heap provenance
and reference identity. Reference reads keep the original checked or verifier-
proved index rules, load both 64-bit identity words from a 16-byte cell, publish
the destination and invalidate its previous view if the identity changes.
The existing runtime conversion also copies non-string references without an
additional child lookup. Strings require byte-view validation and remain cold;
reference stores retain their runtime child-type checks.

`vm_jit_arrays.c` bridges scalar pushes to the existing runtime view-mutation
owner. A live length/capacity guard makes allocation and GC unreachable in the
helper. Runtime code retains value conversion, language/configured length
limits, generation saturation, resource-diagnostic reset and mutation commit
order. A full array or failed check leaves the original push uncommitted and
resumes ordinary execution at that exact source PC. Growth and allocation
failures remain with the runtime. This adds no persistent cache, code patching,
new allocation API or relaxed memory limit.

The ordinary VM register/root stack remains authoritative. Segment accounting
charges exact original instructions and refunds the unexecuted suffix. The
new focused tests require actual native admission and execution, compare cold
and JIT state/fuel/source exits, cover all scalar storage widths, force growth
and GC, change reference identity, reject foreign/stale handles, and sweep all
reachable deterministic run-allocation failure indices. Teardown requires both
allocation ledgers and both executable-page ledgers to be zero where applicable.

The validation record contains full ON/OFF test logs, focused ASan/UBSan and
Rosetta results, 32-workload correctness replays, input hashes, build options and
reproduction scripts. A temporary-index patch replay reconstructed every frozen
production, test and contract input without changing the real Git index.

Status: **rejected-performance**. Decision recorded at `2026-09-10T16:06:16.601633+00:00`. The original 15:58 UTC checkpoint deadline was missed; this later validation does not erase the overrun.

[Complete validation and reproduction record](cp42-sljit-array-segments-validation-2026-09-10.json), [replayable candidate patch](cp42-sljit-array-segments-candidate-2026-09-10.patch), [early paired matrix](cp42-sljit-array-segments-early-2026-09-10.json).

## jit versus bbb9b2b

[Raw 32-workload matrix](cp42-sljit-array-segments-jit-paired-2026-09-10.json): median sum **11332.20 → 10167.07 ms (-10.3%)**; geometric mean speedup **1.076×**. The sum gives more weight to longer workloads.

| Workload | Before, ms | After, ms | Median change | P90 change |
|---|---:|---:|---:|---:|
| `title-search-10k` | 117.92 | 117.82 | -0.1% | -0.9% |
| `title-search-100k` | 614.14 | 609.45 | -0.8% | +1.8% |
| `normalize-title` | 186.80 | 186.48 | -0.2% | -1.3% |
| `word-frequency` | 337.75 | 336.06 | -0.5% | -0.5% |
| `report-builder` | 115.76 | 115.88 | +0.1% | +0.5% |
| `report-concat` | 199.09 | 200.60 | +0.8% | -1.2% |
| `map-int` | 91.05 | 91.65 | +0.7% | +0.7% |
| `map-string` | 117.29 | 117.31 | +0.0% | -2.3% |
| `map-string-book` | 129.25 | 129.54 | +0.2% | +2.3% |
| `set-string-churn` | 147.87 | 148.30 | +0.3% | +0.4% |
| `catalog-match` | 215.78 | 196.52 | -8.9% | -8.7% |
| `catalog-dotcall` | 169.60 | 158.06 | -6.8% | -6.8% |
| `catalog-interface` | 221.57 | 207.09 | -6.5% | -6.1% |
| `record-reference-copy` | 149.61 | 123.72 | -17.3% | -17.1% |
| `record-create` | 185.45 | 158.68 | -14.4% | -12.3% |
| `array-grow` | 144.67 | 128.83 | -11.0% | -6.2% |
| `array-reserved` | 144.23 | 128.27 | -11.1% | -10.7% |
| `bfs-int` | 121.43 | 106.96 | -11.9% | -11.8% |
| `bfs-record` | 133.38 | 114.79 | -13.9% | -13.7% |
| `catalog-pipeline` | 390.54 | 291.33 | -25.4% | -24.7% |
| `parse-result` | 110.83 | 110.78 | -0.0% | -0.7% |
| `insertion` | 230.54 | 227.01 | -1.5% | +0.0% |
| `heap-sort` | 347.47 | 331.63 | -4.6% | -5.8% |
| `stack` | 586.87 | 551.58 | -6.0% | -3.7% |
| `queue` | 616.15 | 585.78 | -4.9% | -5.0% |
| `min-heap` | 936.37 | 914.14 | -2.4% | -1.4% |
| `dsu` | 1395.43 | 1425.28 | +2.1% | +10.3% |
| `bfs` | 2168.33 | 1441.07 | -33.5% | -33.6% |
| `dijkstra` | 248.57 | 231.87 | -6.7% | -7.1% |
| `frequency-map` | 336.89 | 307.73 | -8.7% | -8.7% |
| `unique-set` | 245.70 | 210.96 | -14.1% | -13.1% |
| `rune-frequency` | 175.90 | 161.88 | -8.0% | -8.3% |

Exactly one planned confirmation per flagged workload:

| Workload | Full median | Repeat median | Full P90 | Repeat P90 | Confirmed regression |
|---|---:|---:|---:|---:|---|
| [dsu](cp42-sljit-array-segments-jit-dsu-confirmation-2026-09-10.json) | +2.1% | +2.0% | +10.3% | +1.4% | median |

## interpreter versus bbb9b2b

[Raw 32-workload matrix](cp42-sljit-array-segments-interpreter-paired-2026-09-10.json): median sum **27359.15 → 27497.19 ms (+0.5%)**; geometric mean speedup **1.000×**. The sum gives more weight to longer workloads.

| Workload | Before, ms | After, ms | Median change | P90 change |
|---|---:|---:|---:|---:|
| `title-search-10k` | 146.90 | 146.53 | -0.2% | -0.6% |
| `title-search-100k` | 892.61 | 892.85 | +0.0% | -0.0% |
| `normalize-title` | 212.91 | 212.20 | -0.3% | +1.6% |
| `word-frequency` | 403.98 | 404.06 | +0.0% | -0.7% |
| `report-builder` | 138.95 | 139.35 | +0.3% | +5.3% |
| `report-concat` | 223.41 | 221.47 | -0.9% | +32.5% |
| `map-int` | 106.87 | 105.81 | -1.0% | -1.1% |
| `map-string` | 135.54 | 135.20 | -0.2% | -1.9% |
| `map-string-book` | 134.53 | 134.44 | -0.1% | -3.3% |
| `set-string-churn` | 149.36 | 149.06 | -0.2% | +3.1% |
| `catalog-match` | 271.06 | 270.20 | -0.3% | -0.6% |
| `catalog-dotcall` | 199.10 | 199.11 | +0.0% | -0.8% |
| `catalog-interface` | 259.22 | 259.59 | +0.1% | +0.1% |
| `record-reference-copy` | 181.53 | 180.55 | -0.5% | -2.7% |
| `record-create` | 217.66 | 217.64 | -0.0% | -0.2% |
| `array-grow` | 205.76 | 205.02 | -0.4% | -1.6% |
| `array-reserved` | 205.10 | 204.67 | -0.2% | +4.0% |
| `bfs-int` | 218.88 | 219.42 | +0.2% | +0.6% |
| `bfs-record` | 235.81 | 235.96 | +0.1% | -0.1% |
| `catalog-pipeline` | 545.29 | 544.70 | -0.1% | -0.0% |
| `parse-result` | 120.80 | 120.34 | -0.4% | -1.3% |
| `insertion` | 230.57 | 229.55 | -0.4% | +8.4% |
| `heap-sort` | 1657.89 | 1671.01 | +0.8% | +1.8% |
| `stack` | 796.51 | 797.02 | +0.1% | -13.4% |
| `queue` | 888.56 | 886.90 | -0.2% | +0.1% |
| `min-heap` | 5554.33 | 5598.07 | +0.8% | -1.8% |
| `dsu` | 3878.08 | 3880.68 | +0.1% | -0.1% |
| `bfs` | 5628.40 | 5693.60 | +1.2% | +1.6% |
| `dijkstra` | 2427.60 | 2447.85 | +0.8% | +0.7% |
| `frequency-map` | 484.55 | 483.75 | -0.2% | +0.4% |
| `unique-set` | 356.17 | 358.45 | +0.6% | +4.4% |
| `rune-frequency` | 251.23 | 252.13 | +0.4% | +0.6% |

Exactly one planned confirmation per flagged workload:

| Workload | Full median | Repeat median | Full P90 | Repeat P90 | Confirmed regression |
|---|---:|---:|---:|---:|---|
| [report-builder](cp42-sljit-array-segments-interpreter-report-builder-confirmation-2026-09-10.json) | +0.3% | +0.9% | +5.3% | +3.0% | none |
| [report-concat](cp42-sljit-array-segments-interpreter-report-concat-confirmation-2026-09-10.json) | -0.9% | +0.0% | +32.5% | -1.0% | none |
| [insertion](cp42-sljit-array-segments-interpreter-insertion-confirmation-2026-09-10.json) | -0.4% | -0.2% | +8.4% | +0.1% | none |

## Limits of this evidence

- Optional JIT remains OFF by default; installed mmay unchanged.
- Local Apple M2 timing, host isolation unverified.
- macOS ASan/UBSan uses detect_leaks=0; Linux LeakSanitizer and complete Tier 1 validation remain open.
- Rosetta demonstrates x86-64 correctness, not physical x86 performance.
- Go/Rust parity is not achieved.
