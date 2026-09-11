# JIT reference aliases and record-field value reuse

Status: **accepted-local**. The JIT remains opt-in. Overall Go/Rust parity,
complete Tier 1 validation and Linux LeakSanitizer remain open.

[Validation summary](cp42-jit-reference-values-2026-09-11-validation.json), [complete compressed validation](cp42-jit-reference-values-2026-09-11-validation.json.gz),
[exact source patch](cp42-jit-reference-values-2026-09-11-candidate.patch) and [fresh five-participant comparison](cp42-jit-reference-values-2026-09-11-comprehensive.md).
Decompress the raw JSON with `gzip -dc`; encoded and decoded SHA-256 values are
in the summary. Replay the patch on `ff8f96a021d4d497c9e4b2e6cc7f8378d99cb122` with
`git apply --unidiff-zero`.

## Change and proof boundaries

Two emission-only owners track exact-type/layout array/record aliases and
previously read record fields. A copied reference can use its canonical
register's existing checked binding. A repeated field read from the same unchanged
receiver can copy the already materialized value. Both analyses use bounded
compiler-stack arrays and add no runtime allocations or retained managed objects.

Every VM destination still receives its complete value. Overwrites invalidate
relations, and independently enterable segments and native call/return boundaries
reset the analyses. Record mutation remains an unsupported/cold boundary.
Array mutation changes array contents rather than the record's field reference;
length, storage and generation continue to be read from the live checked view.
Wide field operands are validated before reuse. Bounds, provenance, epochs,
finite/shared fuel, logical frames, roots and failure locations retain D111.
The interpreter and native backend are unchanged.

## Predeclared selection

The stage began at 06:43:12 UTC on 2026-09-11, with the unchanged deadline
`2026-09-11T08:13:12.212011+00:00`. V1 added reference aliases only. Its 10.015% early
geometric latency reduction passed the 8% target, but min-heap P90 increased
7.295%, exceeding the 5% threshold. V1 was rejected without an early rerun;
its exact source, binary identity, samples and initial compile errors are retained.
This observation does not by itself prove a code-induced tail regression.

Before measuring V2, the implementation was extended with same-segment
record-field value reuse. The original deadline and thresholds were retained.
V2's DSU/BFS/min-heap early latency geometric mean fell
**12.04%** and every early median/P90 gate passed.

## Full unchanged-corpus result

All32 paired observations use three warmups, eleven fresh-process samples,
114 pinned source/harness hashes, original arguments and stdout, and 64 MiB heap.
VM timing includes source loading, checking and lowering. No build, test or
profiler from this task ran during acceptance measurements; process/power/thermal
samples are retained, while host isolation remains local-unverified.

The JIT sum of medians changed **7450.91 → 7041.97 ms
(-5.49%)**. Equal-workload geometric
speedup is **1.0224x**. The full gate requires at least a 3%
sum reduction and no confirmed regression. Median increases over 2% or P90
increases over 5% receive exactly one confirmation; repeating the same metric
rejects the candidate. Initial flags: `{}`.
Confirmed flags: `{}`.

| Workload | Baseline median ms | Candidate median ms | Change |
| --- | ---: | ---: | ---: |
| title-search-10k | 91.78 | 92.09 | +0.34% |
| title-search-100k | 503.28 | 504.25 | +0.19% |
| normalize-title | 139.92 | 139.78 | -0.10% |
| word-frequency | 266.66 | 267.99 | +0.50% |
| report-builder | 83.06 | 82.17 | -1.07% |
| report-concat | 160.52 | 160.90 | +0.24% |
| map-int | 67.43 | 67.37 | -0.09% |
| map-string | 87.92 | 88.71 | +0.90% |
| map-string-book | 91.59 | 92.21 | +0.68% |
| set-string-churn | 112.22 | 111.53 | -0.62% |
| catalog-match | 124.09 | 123.26 | -0.67% |
| catalog-dotcall | 103.98 | 102.89 | -1.05% |
| catalog-interface | 127.28 | 127.65 | +0.29% |
| record-reference-copy | 88.07 | 87.23 | -0.95% |
| record-create | 117.27 | 116.14 | -0.97% |
| array-grow | 91.82 | 91.46 | -0.40% |
| array-reserved | 91.95 | 91.36 | -0.63% |
| bfs-int | 79.09 | 77.06 | -2.56% |
| bfs-record | 85.91 | 83.25 | -3.10% |
| catalog-pipeline | 230.31 | 225.96 | -1.89% |
| parse-result | 78.08 | 78.46 | +0.48% |
| insertion | 213.18 | 212.72 | -0.21% |
| heap-sort | 281.31 | 267.20 | -5.02% |
| stack | 321.62 | 320.90 | -0.22% |
| queue | 323.67 | 325.09 | +0.44% |
| min-heap | 636.43 | 584.89 | -8.10% |
| dsu | 972.04 | 849.21 | -12.64% |
| bfs | 1144.13 | 961.12 | -16.00% |
| dijkstra | 217.30 | 211.83 | -2.52% |
| frequency-map | 227.70 | 222.00 | -2.50% |
| unique-set | 167.67 | 155.80 | -7.08% |
| rune-frequency | 123.61 | 119.47 | -3.35% |

## Validation and independent interpreter

All **99/99** enabled tests pass. The **14/14** focused tests pass with macOS
ASan/UBSan and with x86-64 under Rosetta; both configurations also reproduce all
32 expected outputs. New focused tests cover alias-state sequences, overwrites,
branch joins, actual native reuse, record mutation across cold boundaries,
finite/shared/unmetered fuel and exhaustive reachable allocation failures.
After teardown both allocation and byte ledgers are zero. Existing tests retain
call/root, GC, provenance, generation, wide-operand and code-publication coverage.
macOS uses `detect_leaks=0`; this does not replace Linux LeakSanitizer.

The accepted interpreter executable is reused byte for byte:
`ca08b8f301fe7895f6e5b16a58ed965b2f42f3cf4378ed57d441b03da43c4a89`. Its 841 tracked compiler/test dependencies match
current parent blobs; 659 dependency files were inspected. Root CMake changes
only add JIT-conditional sources, and the new tests are also JIT-conditional.
Its previous 87/87 tests and all32 paired acceptance remain authoritative.
The full dependency and input proof is in compressed validation; this is neither
a fresh interpreter timing nor an additional interpreter speedup claim.

All 47 native runtime objects in each profile match the accepted baseline.
All32 Native/Go/Rust/Bun executable identities also match the preceding snapshot.
The fresh comparison's equal-workload geometric time ratios are Native/Go
**2.026x**, Native/Rust **2.193x**,
VM/Go **14.533x** and VM/Rust **15.730x**.
These descriptive comparisons do not close the per-workload parity objective.
Evidence preparation met the stage deadline: **True**;
actual main/push time is recorded separately in the local publication record.
