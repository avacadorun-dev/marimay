# Immediate enums and one checked-index guard

> **Private experiment payloads.** Archived implementations and patches
> referenced in this historical report are retained privately. Commands
> and digests below document the original experiment; they are not public
> reproduction instructions. See the [artifact index](private-artifacts.md).

Status: **Accepted locally**, against `4231bda824fd800307e8be9ec023b616df2876f4`. This D111 step retains
portable interpreter semantics, safety and limits. Overall Go/Rust parity,
complete Tier 1 verification, Linux LeakSanitizer and physical x86 performance
remain open. JIT remains opt-in. The stage started at 2026-09-11T12:21:13.437257+00:00 and
its publication deadline is 2026-09-11T14:21:13.437257+00:00; commit/push time is recorded separately.

[Protocol and results](cp42-jit-enum-bounds-2026-09-11-summary.json), [complete raw evidence](private-artifacts.md#artifact-009),
[replayable patch](private-artifacts.md#artifact-008). Apply with `git apply --unidiff-zero`
on the parent above; isolated-index replay reproduces `66391b591acbe07d61c133b6f67fbd520e04f472`. The summary
supplies both compressed and decoded hashes. The archive retains exact sources,
scripts, build and validation commands, all observations, profiler/address maps,
counter probes and unsuccessful setup attempts.

## Shared work removed

The [enum-only experiment](cp42-jit-immediate-enum-2026-09-11-rejected.md) removes
almost twelve million BFS exits to ordinary execution. It improves several
workloads but fails its original 2% gate on Dijkstra. This stage keeps the same
nonallocating enum bridge and addresses a separately proven redundant bounds
predicate in generated loops. The earlier regression's precise cause remains
unproven; this stage does not attribute it to code relocation or another agent.

Already-registered two-variant enums with empty or one immutable scalar payload
may stay inside generated segments. Cache identity includes execution, function,
nominal layout and heap provenance. Missing descriptor/ID and other payload
shapes stay cold. The existing runtime still owns validation and representation;
the bridge proves it cannot allocate or collect, then commits the destination
only on success. Original PC/fuel, roots, first-registration failures and heap
accounting remain authoritative.

Every published array length is bounded by `min(SIZE_MAX, INT64_MAX)`. Construction,
ordinary/view pushes and extension preserve that bound; truncation/pop/clear only
reduce length. With an exact i64 index, unsigned `index < length` rejects every
negative index as well as the upper-bound failure. The emitter therefore removes
the separate signed-negative predicate and its side-exit body. It keeps current
length, reference/view validation, original failed PC and unspent fuel, and no
element access precedes success. The change is shared by checked array accesses
and `array_require_index`, without recognizing benchmark names or inputs.

A real generated guard-only fixture shrinks from 552 to 512 bytes unmetered and
636 to 588 bytes metered. Both accepted and candidate binaries pass the same
empty/ordinary/synthetic-maximum-length and signed-extreme checks. Synthetic
metadata isolates guards only: no element access, collection or ordinary
execution occurs before restoring actual length. This code-size measurement is
a mechanism check, not an elapsed-time benchmark.

All 32 original workloads, arguments, expected outputs, 114 source/harness hashes
and 64 MiB heap limit stay fixed. All 94 Native runtime objects and every one of
the 32 emitted Native executables are byte-identical to their accepted controls.
Consequently this step claims no Native implementation speedup.

## Correctness and reproducibility

Full CTest passes **101/101 ON** and **87/87 OFF**. Focused macOS ASan/UBSan and
x86-64 Rosetta each pass **17/17**, and both reproduce all 32 original outputs.
Tests retain descriptor/ID cold misses, execution-address reuse with new heap
provenance, scalar bits and empty variants, rooted collection and stale views,
exact frame/shared fuel including SIZE_MAX, negative/extreme indices, source
bounds/panic spans, transactional destinations and every reachable deterministic
allocation-failure index. Allocation and byte ledgers, including JIT code pages,
return to zero after teardown. macOS ASan uses `detect_leaks=0`; Rosetta is
functional evidence, not physical x86 timing.

The accepted OFF compiler stays immutable. Only two of its tracked source/header
dependencies changed, within JIT conditionals. Recompiling all seven dependent
objects with its original flags and generated headers produces identical hashes;
all remaining source dependencies match. Prior OFF timing is therefore reused
without a new speed claim. The complete OFF test suite runs on a fresh isolated
validation build: CP39 packaging invokes a build internally and must not modify
an immutable control. The six architecture, memory, roadmap, links, terminology
and generated-artifact gates and whitespace checks pass before publication.

The first new fixture used nonexistent concrete i64-array symbols; it was
corrected to the existing scalar F64-array owner before timing. The frame probe
initially had an ambiguous instrumentation replacement and an unguarded header;
both setup failures and fixes are retained. These probes never modify production
sources. The previously established F32 nominal-descriptor and D055 string budget
preflight gaps remain separate required follow-ups; this stage does not claim
complete conformance for those existing defects.

## Acceptance policy and measurements

Before this candidate had any timing observations, the user questioned rejecting
a substantial aggregate improvement over a small 2.1%/2.4% loss. The recorded
policy update requires at least **5% reduction in summed ON medians**, with a
**10% target**, and blocks a repeated per-task **median >5% or P90 >5%** regression.
Exactly one confirmation is allowed for a flagged metric. Smaller positive
regressions stay visible as tradeoffs rather than being relabeled as noise.
The old enum-only result remains rejected under its original historical gate.
An exploratory paired bootstrap of its existing Dijkstra confirmation estimates
a 4.50–5.52 ms interval around a 5.01 ms difference; eleven pairs and host
correlation limit this inference, but it does not support calling the loss zero.

Early admission requires BFS improvement >=20% and seven-task geometric latency
reduction >=5%, under the same regression policy. The stronger early targets are
BFS >=25%, Dijkstra >=5% and geometric reduction >=7%. Observed early Dijkstra
improvement is 4.65%, short of that target; it is not a regression. Other target
outcomes and all seven exact observations are retained in the summary.

Three warmups and eleven alternating fresh-process pairs use rotating workloads.
Builds, tests and diagnostic sampling finish before acceptance timing. Three
quiet preflight snapshots precede each stage; known foreign compute >=40% CPU
outside the benchmark process tree is checked every five seconds. All observed
host state remains in the archive. This is local evidence, not independently
isolated hardware. VM includes source loading/checking/lowering/JIT preparation;
the smaller diagnostic program-preparation clock excludes the earlier frontend.

The initial full-run preflight stopped before any timing observation: it detected
the pipeline coordinator's residual CPU use after diagnostic JSON processing.
The process snapshot establishes that coordinator as the measurement process's
parent. Both exited; the failed preflight and pipeline records remain archived.
Measurement resumed under the unchanged quiet-host gate without repeating
validation, diagnostics or any completed timing observation.

| Workload | Paired ON before ms | Candidate ms | Change |
| --- | ---: | ---: | ---: |
| title-search-10k | 92.06 | 92.58 | +0.56% |
| title-search-100k | 509.93 | 504.59 | -1.05% |
| normalize-title | 142.25 | 135.28 | -4.90% |
| word-frequency | 269.02 | 270.59 | +0.58% |
| report-builder | 83.27 | 82.51 | -0.92% |
| report-concat | 170.37 | 167.85 | -1.48% |
| map-int | 67.94 | 68.03 | +0.14% |
| map-string | 89.14 | 88.70 | -0.49% |
| map-string-book | 92.83 | 93.02 | +0.20% |
| set-string-churn | 113.11 | 114.15 | +0.92% |
| catalog-match | 123.79 | 123.75 | -0.04% |
| catalog-dotcall | 103.89 | 103.21 | -0.66% |
| catalog-interface | 127.82 | 127.78 | -0.03% |
| record-reference-copy | 87.35 | 85.51 | -2.10% |
| record-create | 117.76 | 115.85 | -1.62% |
| array-grow | 91.86 | 83.81 | -8.77% |
| array-reserved | 91.41 | 83.09 | -9.11% |
| bfs-int | 77.28 | 73.20 | -5.28% |
| bfs-record | 84.15 | 80.53 | -4.30% |
| catalog-pipeline | 226.82 | 225.61 | -0.54% |
| parse-result | 78.51 | 75.87 | -3.36% |
| insertion | 212.64 | 210.16 | -1.16% |
| heap-sort | 267.87 | 245.82 | -8.23% |
| stack | 322.94 | 263.92 | -18.27% |
| queue | 326.14 | 261.15 | -19.93% |
| min-heap | 589.45 | 526.09 | -10.75% |
| dsu | 856.55 | 787.85 | -8.02% |
| bfs | 976.62 | 671.97 | -31.19% |
| dijkstra | 213.88 | 203.24 | -4.98% |
| frequency-map | 222.05 | 201.94 | -9.06% |
| unique-set | 154.79 | 133.88 | -13.51% |
| rune-frequency | 120.04 | 108.80 | -9.36% |

Sum of medians: **7103.53 → 6410.33 ms**, **-9.76%**. Confirmed blocking regressions: `{}`.

The 10% improvement target was not met; acceptance separately requires at least 5%. The exact initial flags and single confirmation observations remain in the summary and raw evidence.

The [fresh five-participant comparison](cp42-jit-enum-bounds-2026-09-11-comprehensive.md) and
[raw comparison](cp42-jit-enum-bounds-2026-09-11-comprehensive.json) contain current Native, Go, Rust,
source-run VM+JIT and Bun measurements. Every Native/Go/Rust/Bun comparator
executable matches the accepted control hash. Native excludes compilation; Bun
includes process startup, TypeScript loading and fresh JIT.

## Mechanism attribution and next common cost

Two original counter replays of every workload preserve all outputs. Across all
32 tasks, hot plus cold enum construction totals equal the accepted cold count;
per-task equality with the enum-only operation counts is recorded independently.
Three counter-free address-mapped profiles of each of seven tasks locate the
remaining generated and C work. Complete unambiguous address lists map to exact
per-invocation code ranges and labels; unknown frames stay unknown. Stack samples
are not full CPU-time percentages, and inlined work may be attributed to callers.

A separate frame probe executes DSU, stack, queue, BFS, min-heap and Dijkstra
twice each. Every checksum passes and both counter replays agree exactly. It
counts frame reuse/rebuild, template bytes, argument/return values, old references
cleared, roots relinked and root borrowing. These are logical C operations and
nominal assignment bytes, not measured DRAM traffic or uninstrumented duration.

| Task | Frame initializations | Layout rebuilds | Template bytes | References cleared | Argument-copy bytes | Return-copy bytes |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| dsu | 15,002,066 | 2,068 | 894,048 | 64,999,987 | 840,099,024 | 720,099,144 |
| stack | 4,000,004 | 3,000,005 | 600,001,536 | 6,999,993 | 144,000,024 | 144,000,168 |
| queue | 4,000,004 | 3,000,005 | 768,001,560 | 6,999,993 | 144,000,024 | 144,000,168 |
| bfs | 902 | 602 | 714,624 | 0 | 43,200 | 43,272 |
| min-heap | 2,500,003 | 1,500,006 | 660,003,624 | 9,999,980 | 95,999,976 | 72,000,168 |
| dijkstra | 1,834 | 163 | 255,648 | 1,591 | 87,936 | 88,008 |

DSU mostly reuses layouts yet clears and relinks nearly 65 million references.
Stack/queue/min-heap also rebuild layouts millions of times. The next intervention
should address the complete repeated short-call transaction using these counts
and profiles. Any stronger noncollecting-call proof must cover transitive calls,
recursion, cold fallbacks, allocation failure, roots, depth, traps and exact fuel;
it cannot silently bypass observable semantics or configured limits.
