# Retaining unchanged array identity after a native push

> **Private experiment payloads.** Archived implementations and patches
> referenced in this historical report are retained privately. Commands
> and digests below document the original experiment; they are not public
> reproduction instructions. See the [artifact index](private-artifacts.md).

The preceding [array-segment candidate](cp42-sljit-array-segments-2026-09-10.md)
passed local correctness and improved the JIT sum of medians 10.3%, but was
rejected: DSU median regressions of 2.14% and 2.05% exceeded the 2% threshold
in the full matrix and its one planned confirmation. Its original checkpoint
also overran. Both failures and all observations remain published.

## Follow-up and predeclared gates

Before this follow-up edit, a new one-hour SMART record retained the accepted
`bbb9b2b` controls and all full-matrix acceptance thresholds. Early BFS must
improve at least 25%; early DSU must stay within 2% median and 5% P90. Full
acceptance requires a 5% gain in the JIT sum of medians, without any confirmed
per-workload median/P90 regression. Each flagged workload receives exactly one
additional 3+11 paired series. The rejected candidate is not rerun until passing.

The follow-up removes one redundant operation from machine-code lowering:
a successful scalar push changes the array object but cannot change the
reference stored in its VM register. It therefore needs no destination-reference
identity comparison or view invalidation. Nonallocating admission, spare-capacity
proof, runtime mutation and failure checks, exact instruction accounting and
cold-exit PC are unchanged. Other managed assignments still invalidate their
views when the complete identity changes.

The early series measured BFS 2176.32 → 1471.62 ms (-32.4%), DSU
1407.51 → 1424.32 ms (+1.2%; P90 +1.4%), and catalogue-pipeline
392.71 → 296.64 ms (-24.5%). These are new paired observations of a changed
candidate. They do not establish that the earlier DSU slowdown was caused by
the removed comparison: control timings also differ. The complete matrix and
predeclared confirmation rule determine acceptance.

## Reproducibility and validation

The implementation includes the non-string reference-array reads and scalar
spare-capacity push bridge described in the preceding report. The replayable
patch reconstructs all 13 frozen production, test and contract inputs against
`bbb9b2b`. Only `vm_jit_sljit.c` differs from the preceding frozen candidate;
it is excluded from the OFF configuration.

The exact previous OFF compiler, 86-test result, full paired matrix and three
successful P90 confirmations are reused with explicit hashes and provenance.
They are not presented as newly executed measurements. Enabled builds receive
new full tests, focused ASan/UBSan and x86-64/Rosetta tests, and complete
32-workload correctness replays before the new JIT timing matrix.

The control JIT binary is the preserved `bbb9b2b` executable, SHA-256
`699a329a6959da419b2c51a498370f4dc6ebe3d37f0737e0599ccd115b398f52`.
Every timing series uses 3 warmups and 11 samples, fresh processes, all 32
unchanged workloads, the same 114 source/harness hashes and 64 MiB heap budget.
Source preparation remains inside VM measurements. Builds, tests, gates and
profiling do not overlap timing. P90 is the tenth sorted value for 11 samples.
No observation is discarded.

Status: **accepted-local-opt-in**. Decision recorded at `2026-09-10T16:28:36.292516+00:00`. The follow-up checkpoint deadline was met.

[Complete validation and reproduction record](cp42-sljit-array-identity-validation-2026-09-10.json), [replayable candidate patch](private-artifacts.md#artifact-031), [early paired matrix](cp42-sljit-array-identity-early-2026-09-10.json).

## jit versus bbb9b2b

[Raw 32-workload matrix](cp42-sljit-array-identity-jit-paired-2026-09-10.json): median sum **11341.15 → 10217.07 ms (-9.9%)**; geometric mean speedup **1.078×**. The sum gives more weight to longer workloads.

| Workload | Before, ms | After, ms | Median change | P90 change |
|---|---:|---:|---:|---:|
| `title-search-10k` | 117.28 | 117.30 | +0.0% | -0.6% |
| `title-search-100k` | 607.80 | 609.27 | +0.2% | +0.4% |
| `normalize-title` | 185.08 | 185.67 | +0.3% | +1.1% |
| `word-frequency` | 338.76 | 335.85 | -0.9% | -0.7% |
| `report-builder` | 114.66 | 114.72 | +0.1% | -0.4% |
| `report-concat` | 197.16 | 195.71 | -0.7% | -2.3% |
| `map-int` | 90.64 | 91.01 | +0.4% | -0.7% |
| `map-string` | 117.03 | 117.53 | +0.4% | -0.7% |
| `map-string-book` | 129.60 | 131.01 | +1.1% | +2.6% |
| `set-string-churn` | 148.08 | 148.34 | +0.2% | -0.7% |
| `catalog-match` | 216.25 | 194.80 | -9.9% | -9.8% |
| `catalog-dotcall` | 171.76 | 158.60 | -7.7% | -11.5% |
| `catalog-interface` | 225.42 | 208.37 | -7.6% | -7.0% |
| `record-reference-copy` | 151.27 | 123.47 | -18.4% | -16.8% |
| `record-create` | 190.86 | 159.66 | -16.3% | -13.9% |
| `array-grow` | 145.52 | 129.56 | -11.0% | -16.9% |
| `array-reserved` | 145.32 | 128.26 | -11.7% | -10.0% |
| `bfs-int` | 122.23 | 106.09 | -13.2% | -13.2% |
| `bfs-record` | 135.65 | 114.34 | -15.7% | -23.6% |
| `catalog-pipeline` | 395.74 | 292.11 | -26.2% | -23.2% |
| `parse-result` | 110.52 | 110.33 | -0.2% | +2.5% |
| `insertion` | 229.08 | 225.94 | -1.4% | -2.9% |
| `heap-sort` | 350.85 | 331.82 | -5.4% | -1.2% |
| `stack` | 583.84 | 559.82 | -4.1% | -4.9% |
| `queue` | 618.07 | 592.50 | -4.1% | -3.2% |
| `min-heap` | 943.37 | 939.31 | -0.4% | -4.6% |
| `dsu` | 1395.44 | 1420.07 | +1.8% | -1.0% |
| `bfs` | 2154.86 | 1455.53 | -32.5% | -32.7% |
| `dijkstra` | 249.12 | 231.02 | -7.3% | -6.9% |
| `frequency-map` | 337.92 | 307.47 | -9.0% | -7.5% |
| `unique-set` | 246.15 | 218.94 | -11.1% | -8.8% |
| `rune-frequency` | 175.82 | 162.65 | -7.5% | -6.0% |

Neither threshold was exceeded; no confirmation was needed.

The interpreter measurements below are reused from the preceding stage, with the identical executable; they are not a new run.

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

## Fresh five-participant comparison

The same candidate executable (SHA-256 `e600be2b0834f0e9cb9291458c92c68d6de80fdb049a8968586235e3c36ca5b7`) completed a new [32-workload five-participant series](cp42-sljit-array-identity-comprehensive-2026-09-10.md) ([raw observations and executable/input hashes](cp42-sljit-array-identity-comprehensive-2026-09-10.json)). All 160 participant/workload correctness outcomes passed, using 3 warmups and 11 samples, unchanged 114 input hashes and the 64 MiB managed heap limit. All five columns come from this one series.

Descriptive geometric means of time ratios across the 32 workloads are native/Go **2.04×**, native/Rust **2.19×**, VM/Go **19.92×**, and VM/Rust **21.38×**. Native is no slower than Go on 4/32 median observations and Rust on 0/32. These descriptive ratios do not replace the paired acceptance matrix or establish overall parity. Native generation/runtime are unchanged by this JIT stage.

VM includes loading, checking and lowering source; Bun includes process startup, TypeScript loading and JIT. Native compilation is excluded. Host isolation is unverified, and the harness flags Rust frequency-map as noisy. Dispersion and every raw observation are retained without a replacement run.

## Limits of this evidence

- Optional JIT remains OFF by default; installed mmay unchanged.
- Local Apple M2 timing, host isolation unverified.
- macOS ASan/UBSan uses detect_leaks=0; Linux LeakSanitizer and complete Tier 1 validation remain open.
- Rosetta demonstrates x86-64 correctness, not physical x86 performance.
- Go/Rust parity is not achieved.
