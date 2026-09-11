# Prepared VM activations and native view revalidation

The accepted baseline is `719aa54`. Fresh sampling of its preserved JIT binary
places logical call transitions in 44.9% of DSU, 49.7% of stack and 52.1% of
queue samples. Retained-view binding is another substantial cost. These are
diagnostic stack samples, not timing measurements; startup and tails may be
missed and inclusive percentages overlap.

## Bounded experiment

The original SMART record precedes implementation at 16:51 UTC on 2026-09-10,
with a two-hour deadline. Early acceptance requires at least 20% improvement in
stack and queue and 15% in DSU against the accepted executable. Full acceptance
requires an 8% gain in the sum of JIT medians and no confirmed regression beyond
2% median or 5% P90 on any workload, independently in JIT and OFF. Exactly one
additional 3+11 series confirms each flagged workload; every observation is
retained. P90 is the tenth sorted observation of eleven samples.

The first candidate prepares direct-call metadata, initial register values and
managed-root indexes once per selected function. It passes focused correctness
and all32 replay, but misses every early target: stack 566.72 → 482.87 ms
(-14.8%), queue 592.10 → 481.48 ms (-18.7%), DSU 1439.32 → 1358.95 ms
(-5.6%). Its binary, replayable patch, original samples and post-run profile are
preserved. It is not accepted by this checkpoint.

Before the next edit, a second SMART record extends preparation with optional
function-specific view tables, bounded to 2 MiB, and native retained-view
revalidation. It keeps the original controls, numeric thresholds and deadline.
Its early matrix passes: stack 583.96 → 461.14 ms (-21.0%), queue
595.84 → 461.86 ms (-22.5%), DSU 1439.46 → 1206.49 ms (-16.2%).
Control timings also differ, so the two early series alone do not isolate the
causal contribution of each implementation change. The full paired matrices
and planned confirmations determine acceptance.

## Ownership and preserved behavior

The frame-layout owner prepares immutable types, invalid references and root
indexes. It stores no dynamic value or linked root. The frame owner applies
those templates only to inactive storage; layout changes invalidate old root
locations before storage can move. The shared activation owner retains depth,
argument and return checks, checked root registration, source PCs and cleanup.
Prepared call sites refer to the original verified instructions, whose program
outlives their generated code. Finite/shared fuel retains ordinary call handling.

Separate function view tables reduce cross-function cache collisions. Excess
functions use the existing shared table. Both contain borrowed views, not GC
roots or extra activations. Native revalidation preserves execution, function,
layout, type, full reference/provenance and heap-epoch checks before accessing
retained object pointers. Every miss calls the existing checked binding owner.
Entry-epoch wrap clears all tables, including future callees.

Focused tests compare ordinary/prepared frame layouts under growth, shrinkage,
collection and allocation failure; actual JIT calls preserve deep recursion,
precise cold exits and logical limits. Eighteen native proof states are compared
with the C binding owner, with poisoned retained pointers exposing a missing
guard. Bounded table allocation/fallback and all table clearing are tested.
Allocator and executable-page ledgers are checked after teardown.

## Reproduction

All timing uses the unchanged 32 workloads, 114 source/harness hashes, expected
checksums and 64 MiB managed heap, with three warmups and eleven fresh-process
samples. Builds, tests and profiling finish before timing. VM source loading,
checking and lowering remain included. The published source patch replays all
22 frozen production, test and contract inputs against the accepted parent.

The JIT control is the exact accepted binary, SHA-256
`e600be2b0834f0e9cb9291458c92c68d6de80fdb049a8968586235e3c36ca5b7`.
The OFF control is the previously accepted, source-equivalent binary, SHA-256
`90901682e6fdcc366cf87d65b49d15d6892f5347d2ff7622a22ad13cf25a0537`.
Because this candidate changes common VM owners, it receives a new OFF build,
complete tests and a new paired matrix. The validation record includes exact
commands, compiler hashes, logs, profiles and source provenance.

Status: **rejected-performance**. Decision recorded at `2026-09-10T18:21:00.798725+00:00`. The checkpoint deadline was met.

[Complete validation and reproduction record](cp42-sljit-prepared-activations-validation-2026-09-10.json), [candidate patch](cp42-sljit-prepared-activations-candidate-2026-09-10.patch), [early matrix](cp42-sljit-prepared-activations-early-2026-09-10.json).

The first candidate is preserved as a [replayable patch](cp42-sljit-prepared-activations-first-candidate-2026-09-10.patch) and [early matrix](cp42-sljit-prepared-activations-first-early-2026-09-10.json); it missed the original early targets.

## jit versus accepted control

[Raw 32-workload matrix](cp42-sljit-prepared-activations-jit-paired-2026-09-10.json): sum of medians **11195.83 → 10213.08 ms (-8.8%)**; geometric mean speedup **1.058×**. The sum weights longer workloads more heavily.

| Workload | Before, ms | After, ms | Median change | P90 change |
|---|---:|---:|---:|---:|
| `title-search-10k` | 128.28 | 126.34 | -1.5% | -0.3% |
| `title-search-100k` | 669.53 | 648.75 | -3.1% | -1.8% |
| `normalize-title` | 203.26 | 197.39 | -2.9% | -2.0% |
| `word-frequency` | 358.13 | 363.98 | +1.6% | +0.8% |
| `report-builder` | 124.45 | 118.12 | -5.1% | -4.6% |
| `report-concat` | 206.74 | 204.22 | -1.2% | -2.5% |
| `map-int` | 98.01 | 96.46 | -1.6% | +0.5% |
| `map-string` | 124.35 | 125.72 | +1.1% | +0.4% |
| `map-string-book` | 136.53 | 130.08 | -4.7% | -1.4% |
| `set-string-churn` | 156.49 | 153.22 | -2.1% | -1.2% |
| `catalog-match` | 211.72 | 180.60 | -14.7% | -14.6% |
| `catalog-dotcall` | 170.50 | 155.78 | -8.6% | -9.7% |
| `catalog-interface` | 223.38 | 199.26 | -10.8% | -10.5% |
| `record-reference-copy` | 135.26 | 129.28 | -4.4% | -3.5% |
| `record-create` | 173.33 | 167.76 | -3.2% | -3.3% |
| `array-grow` | 140.24 | 137.53 | -1.9% | +0.7% |
| `array-reserved` | 140.93 | 138.85 | -1.5% | -0.8% |
| `bfs-int` | 114.58 | 113.46 | -1.0% | -0.9% |
| `bfs-record` | 124.87 | 122.91 | -1.6% | -1.1% |
| `catalog-pipeline` | 314.10 | 297.18 | -5.4% | -5.5% |
| `parse-result` | 117.17 | 114.08 | -2.6% | -5.8% |
| `insertion` | 240.51 | 246.30 | +2.4% | +0.3% |
| `heap-sort` | 359.69 | 336.20 | -6.5% | -8.3% |
| `stack` | 612.91 | 486.99 | -20.5% | -22.4% |
| `queue` | 635.10 | 498.69 | -21.5% | -20.8% |
| `min-heap` | 1019.04 | 815.51 | -20.0% | -17.6% |
| `dsu` | 1598.27 | 1359.00 | -15.0% | -12.8% |
| `bfs` | 1657.02 | 1568.51 | -5.3% | -4.9% |
| `dijkstra` | 258.83 | 258.36 | -0.2% | -0.3% |
| `frequency-map` | 333.83 | 320.85 | -3.9% | -3.0% |
| `unique-set` | 233.42 | 226.15 | -3.1% | -10.3% |
| `rune-frequency` | 175.37 | 175.54 | +0.1% | -1.8% |

Exactly one planned confirmation per flagged workload:

| Workload | Full median | Repeat median | Full P90 | Repeat P90 | Confirmed regression |
|---|---:|---:|---:|---:|---|
| [insertion](cp42-sljit-prepared-activations-jit-insertion-confirmation-2026-09-10.json) | +2.4% | +1.1% | +0.3% | +7.7% | none |

## interpreter versus accepted control

[Raw 32-workload matrix](cp42-sljit-prepared-activations-interpreter-paired-2026-09-10.json): sum of medians **30008.75 → 30210.65 ms (+0.7%)**; geometric mean speedup **0.991×**. The sum weights longer workloads more heavily.

| Workload | Before, ms | After, ms | Median change | P90 change |
|---|---:|---:|---:|---:|
| `title-search-10k` | 159.18 | 161.23 | +1.3% | +0.3% |
| `title-search-100k` | 990.77 | 1010.06 | +1.9% | +0.8% |
| `normalize-title` | 231.28 | 233.00 | +0.7% | +7.7% |
| `word-frequency` | 433.13 | 439.53 | +1.5% | +1.2% |
| `report-builder` | 151.58 | 153.14 | +1.0% | +2.9% |
| `report-concat` | 233.98 | 236.54 | +1.1% | -19.3% |
| `map-int` | 116.68 | 119.13 | +2.1% | +0.1% |
| `map-string` | 156.69 | 151.84 | -3.1% | -2.5% |
| `map-string-book` | 147.65 | 152.79 | +3.5% | +1.9% |
| `set-string-churn` | 177.57 | 178.06 | +0.3% | -3.8% |
| `catalog-match` | 315.28 | 306.43 | -2.8% | -25.7% |
| `catalog-dotcall` | 215.58 | 221.06 | +2.5% | -1.2% |
| `catalog-interface` | 281.42 | 286.13 | +1.7% | +5.1% |
| `record-reference-copy` | 197.55 | 198.42 | +0.4% | -1.7% |
| `record-create` | 236.59 | 238.79 | +0.9% | +0.9% |
| `array-grow` | 222.74 | 225.42 | +1.2% | +6.2% |
| `array-reserved` | 222.58 | 222.17 | -0.2% | +3.6% |
| `bfs-int` | 240.25 | 238.98 | -0.5% | -5.3% |
| `bfs-record` | 257.10 | 257.77 | +0.3% | +1.0% |
| `catalog-pipeline` | 591.32 | 593.90 | +0.4% | +5.2% |
| `parse-result` | 129.36 | 127.85 | -1.2% | -0.7% |
| `insertion` | 243.40 | 243.76 | +0.1% | -2.0% |
| `heap-sort` | 1786.64 | 1810.75 | +1.3% | -8.3% |
| `stack` | 872.76 | 902.29 | +3.4% | +12.6% |
| `queue` | 972.01 | 1018.35 | +4.8% | -0.9% |
| `min-heap` | 6264.15 | 6170.54 | -1.5% | -5.2% |
| `dsu` | 4218.45 | 4375.04 | +3.7% | +9.0% |
| `bfs` | 6170.00 | 6147.98 | -0.4% | +9.9% |
| `dijkstra` | 2583.24 | 2587.01 | +0.1% | +0.3% |
| `frequency-map` | 526.57 | 528.83 | +0.4% | -14.3% |
| `unique-set` | 388.95 | 397.76 | +2.3% | +25.8% |
| `rune-frequency` | 274.29 | 276.12 | +0.7% | -20.7% |

Exactly one planned confirmation per flagged workload:

| Workload | Full median | Repeat median | Full P90 | Repeat P90 | Confirmed regression |
|---|---:|---:|---:|---:|---|
| [normalize-title](cp42-sljit-prepared-activations-interpreter-normalize-title-confirmation-2026-09-10.json) | +0.7% | +2.4% | +7.7% | +8.3% | p90 |
| [map-int](cp42-sljit-prepared-activations-interpreter-map-int-confirmation-2026-09-10.json) | +2.1% | -0.2% | +0.1% | -0.3% | none |
| [map-string-book](cp42-sljit-prepared-activations-interpreter-map-string-book-confirmation-2026-09-10.json) | +3.5% | +0.4% | +1.9% | +0.2% | none |
| [catalog-dotcall](cp42-sljit-prepared-activations-interpreter-catalog-dotcall-confirmation-2026-09-10.json) | +2.5% | +2.4% | -1.2% | +1.7% | median |
| [catalog-interface](cp42-sljit-prepared-activations-interpreter-catalog-interface-confirmation-2026-09-10.json) | +1.7% | +0.8% | +5.1% | +0.7% | none |
| [array-grow](cp42-sljit-prepared-activations-interpreter-array-grow-confirmation-2026-09-10.json) | +1.2% | +1.0% | +6.2% | +1.2% | none |
| [catalog-pipeline](cp42-sljit-prepared-activations-interpreter-catalog-pipeline-confirmation-2026-09-10.json) | +0.4% | -0.1% | +5.2% | -1.2% | none |
| [stack](cp42-sljit-prepared-activations-interpreter-stack-confirmation-2026-09-10.json) | +3.4% | +1.7% | +12.6% | +10.7% | p90 |
| [queue](cp42-sljit-prepared-activations-interpreter-queue-confirmation-2026-09-10.json) | +4.8% | +4.3% | -0.9% | +2.5% | median |
| [dsu](cp42-sljit-prepared-activations-interpreter-dsu-confirmation-2026-09-10.json) | +3.7% | +1.7% | +9.0% | +1.8% | none |
| [bfs](cp42-sljit-prepared-activations-interpreter-bfs-confirmation-2026-09-10.json) | -0.4% | +0.5% | +9.9% | -4.2% | none |
| [unique-set](cp42-sljit-prepared-activations-interpreter-unique-set-confirmation-2026-09-10.json) | +2.3% | +2.0% | +25.8% | +3.5% | median |

## Limits of this evidence

- Optional JIT remains OFF by default; this is local implementation evidence, not a release or parity claim.
- Apple M2 timings; host isolation is not independently verified. VM includes source preparation.
- macOS ASan/UBSan uses detect_leaks=0. Linux LeakSanitizer and the complete Tier 1 matrix remain open.
- Rosetta checks x86-64 correctness, not physical x86 performance.
- Sampling can miss startup and tails; inclusive counts overlap. Anonymous frames are not automatically identified as generated code.
