# Keep immediate enum construction inside JIT segments

Status: **Rejected**, against `f7a12459b7bf0dff8e6c3671861ea67f297a35d8`. This is a measured
implementation step under D111, not Go/Rust parity or complete Tier 1 verification.
JIT remains opt-in. The frozen stage started at 2026-09-11T11:26:16.078186+00:00 and its
publication deadline is 2026-09-11T13:26:16.078186+00:00; publication time is recorded separately.

[Protocol and summary](cp42-jit-immediate-enum-2026-09-11-rejected-summary.json), [complete raw evidence](cp42-jit-immediate-enum-2026-09-11-rejected-raw.json.gz),
and [candidate patch](cp42-jit-immediate-enum-2026-09-11-rejected-candidate.patch). Apply with `git apply --unidiff-zero`
on the parent above. The isolated-index replay reproduces tree `7789a965b26182fe704166412ffd1c3c690b7ff5`.
The summary supplies both compressed and decoded hashes. Raw evidence includes
exact diagnostic copies, scripts, counter runs, address maps, profiler outputs,
unsuccessful setup runs, validation, host snapshots and every timing observation.

## Diagnosis and shared intervention

The accepted compiler was first replayed twice on all 32 unchanged workloads,
then sampled three times on each of six selected tasks without per-operation
counters. Counter frequencies were stable in both replays and outputs matched.
The diagnostic clock splits bytecode program preparation from execution;
**it excludes earlier source loading, checking and lowering**. Its approximately
3 ms preparation interval is not the complete source startup cost.

BFS repeatedly left generated code to create an immediate enum: almost twelve
million exits. The cold path resolves nominal descriptors, builds temporary
cells, invokes the shared runtime and disposes the cells before re-entering JIT.
Stack, queue and min-heap share this cost. DSU instead spends much of its sampled
C work preparing and returning logical frames; title-search-100k repeatedly exits
for string array loads/comparisons. These require separate interventions.

The new focused owner `vm_jit_enums.c` admits already-registered two-variant enum
constructors with an empty or one immutable scalar payload. The cache checks
execution, function, nominal layout and heap provenance. Missing descriptors or
IDs, managed payloads and other shapes remain cold. The bridge proves the shared
runtime's existing immediate representation, invokes its validation/construction
owner and publishes the result only after success. It neither allocates nor
collects. Original PC, bounded/shared fuel, depth, roots, reference representation
and heap accounting remain intact. Registration still happens through the cold
owner. Complete nominal metadata remains alive through execution teardown.

No benchmark source, algorithm, input, output, 114 source/harness hashes or
64 MiB heap limit changes. There is no benchmark-name specialization. All 94
Native runtime objects across debug/release and every one of the 32 emitted
Native executable hashes match the accepted controls. Archive containers differ;
object identity and executable identity are checked separately. OFF compiler
hashes differ, so a paired regression measurement was started. It was stopped
after ON rejection; its partial observations are retained without an OFF aggregate.

## Verify the mechanism independently of elapsed time

Both accepted and candidate diagnostic compilers reproduce each original output
twice, for 128 checksum-correct runs. Across all 32 workloads, candidate hot plus
cold enum construction counts equal the baseline cold count exactly. This checks
that the work remains semantically present while its execution path changes.

| Workload | Enum exits before | Enum exits after | JIT entries before | JIT entries after |
| --- | ---: | ---: | ---: | ---: |
| bfs | 11,991,900 | 301 | 12,040,902 | 49,303 |
| stack | 999,981 | 1 | 2,000,002 | 1,000,022 |
| queue | 1,999,981 | 1 | 3,000,002 | 1,000,022 |
| min-heap | 999,982 | 1 | 1,500,002 | 500,021 |
| dsu | 199,968 | 0 | 299,983 | 100,015 |
| title-search-100k | 0 | 1 | 8,194,439 | 8,194,440 |

DSU has fewer enum exits but little early speed change, consistent with the
independently sampled logical-call cost. Reduced transition count alone does
not establish a speed benefit. JIT return-bridge counts differ by one in three
workloads as segment boundaries change; an initial analysis assumption requiring
those bridge counts to remain equal failed and is retained. It was removed
without changing any observation or workload. The enum conservation check above
passes independently on all tasks.

Counter-free sampling after the candidate repeats three original executions for
each selected task. Exact generated-code ranges and per-PC label offsets map
only complete, unambiguous sampled addresses; unknown addresses remain unknown.
Samples partition observed stacks, not complete CPU time. Inlining can attribute
work to callers, and startup/tail may be missed. The old catch-all category named
`other_c_startup_preparation` also contains VM runtime C functions and must not be
interpreted entirely as frontend preparation. Neither instrumented elapsed time
nor sampled stack counts are acceptance measurements.

## Validation and predeclared acceptance

Full CTest passes **100/100 ON** and **87/87 OFF**. Focused macOS ASan/UBSan and
x86-64 Rosetta each pass **16/16**, and both reproduce all 32 expected outputs.
Tests cover cold descriptor/ID misses, two constructors reusing one destination,
heap teardown/reinitialization at the same execution address, 11 supported
scalar types and empty variants, exact fuel for bounded and shared counters
including SIZE_MAX, reachable deterministic allocation failures and both zero
allocation/byte ledgers. JIT code-page ledgers also return to zero. The warm
bridge is tested with the next allocation forced to fail and collection requested;
it performs no allocation attempt, collection or logical-heap increase.

The six architecture, memory policy, roadmap, links, terminology and generated
artifact gates and whitespace checks pass. macOS ASan uses `detect_leaks=0`;
Rosetta proves functional x86 behavior, not physical x86 timing. Complete Tier 1,
Linux LeakSanitizer and overall parity remain open.

Initial test setup incorrectly filled an unsigned operand instead of the separate
signed variant selector; the fixture was corrected. An old segment-entry assertion
was updated because enum construction now connects adjacent supported operations.
F32 payload admission was removed after an independent baseline probe confirmed
an existing nominal-descriptor classification gap. Both accepted and candidate
return status 2 after four allocation attempts and tear down to zero ledgers;
this candidate does not repair that gap or claim full F32 conformance. The earlier
D055 string budget-before-host-allocation gap also remains a required follow-up.
All initial failed test/probe logs are preserved.

The early gate requires BFS median improvement of at least 15%, six-task geometric
latency reduction of at least 5% and no repeated per-task median >2% or P90 >5%
regression. The full gate requires VM+JIT summed medians to fall at least 5% over
all 32 workloads, with the same per-task regression gate for ON and OFF. Exactly
one confirmation is allowed for a flagged metric. No threshold is retuned after
observation. Native uses the exact executable identity proof above.

Timing uses three warmups and eleven alternating fresh-process pairs with rotating
workloads. Three quiet preflight snapshots precede each stage; known foreign
compute >=40% CPU is checked every five seconds outside the owned benchmark tree.
Builds, tests and profiling finish before timing. All observations and sampled host
state are retained. This is local evidence, not independently isolated hardware.

| Full mode | Before median sum ms | Candidate median sum ms | Change | Confirmed regressions |
| --- | ---: | ---: | ---: | --- |
| on | 7543.38 | 6895.60 | -8.59% | {"dijkstra": ["median_ms"]} |

| Workload | Paired ON before ms | Candidate ms | Change |
| --- | ---: | ---: | ---: |
| title-search-10k | 99.52 | 99.92 | +0.41% |
| title-search-100k | 550.40 | 547.37 | -0.55% |
| normalize-title | 150.38 | 143.26 | -4.73% |
| word-frequency | 285.95 | 285.56 | -0.13% |
| report-builder | 87.54 | 86.52 | -1.17% |
| report-concat | 170.95 | 170.86 | -0.05% |
| map-int | 70.23 | 69.91 | -0.45% |
| map-string | 90.53 | 91.49 | +1.06% |
| map-string-book | 94.50 | 95.13 | +0.67% |
| set-string-churn | 115.63 | 114.71 | -0.79% |
| catalog-match | 126.42 | 125.83 | -0.47% |
| catalog-dotcall | 107.30 | 105.47 | -1.71% |
| catalog-interface | 132.58 | 131.24 | -1.01% |
| record-reference-copy | 89.49 | 89.50 | +0.01% |
| record-create | 121.68 | 118.73 | -2.42% |
| array-grow | 93.09 | 84.29 | -9.45% |
| array-reserved | 94.10 | 86.80 | -7.76% |
| bfs-int | 78.87 | 75.42 | -4.38% |
| bfs-record | 85.60 | 81.62 | -4.66% |
| catalog-pipeline | 230.87 | 232.77 | +0.82% |
| parse-result | 80.89 | 77.92 | -3.67% |
| insertion | 223.72 | 216.66 | -3.16% |
| heap-sort | 285.22 | 270.48 | -5.17% |
| stack | 349.47 | 279.64 | -19.98% |
| queue | 351.56 | 279.42 | -20.52% |
| min-heap | 628.26 | 566.97 | -9.75% |
| dsu | 923.00 | 927.17 | +0.45% |
| bfs | 1056.25 | 726.86 | -31.19% |
| dijkstra | 231.33 | 236.18 | +2.09% |
| frequency-map | 241.47 | 219.02 | -9.30% |
| unique-set | 166.91 | 141.84 | -15.02% |
| rune-frequency | 129.69 | 117.04 | -9.75% |

Dijkstra median latency rises 2.09% in the full matrix and 2.36% in its single planned confirmation. The candidate fails its declared gate despite an 8.59% ON median-sum reduction. Remaining OFF timing was stopped by resolving and terminating its exact owned process tree; partial observations and original interruption records are retained without an OFF aggregate. No candidate fiveway was run. Preserve this patch and evidence and restore production before publication.

## Other open findings and next general bottleneck

Six additional alternating original Dijkstra executions all reproduce the
checksum. Address-mapped sampling places most observed execution in generated
array index checks, branches and in-bounds loads, in both binaries. Counts show
133,026 versus 6,467 JIT entries and 130,473 cold enum constructions versus
1,522 cold plus 128,951 hot constructions. This confirms removal of boundary
work, but does not establish the cause of the 2.09%/2.36% timing regression.
A source-level follow-up should check whether the existing language maximum
array length makes the separate negative-index guard redundant with the unsigned
upper-bound guard. This requires a complete constructor/mutation invariant and
negative/extreme-index/fuel tests, not an inference from code placement.

The preceding rejected shared-string candidate remains rejected. Twelve original
min-heap checksum runs on its exact Native controls produced eleven nonempty
profiles; the empty one is preserved without replacement. A deliberately suspended
startup interval dominates many sampled stacks. These profiles do not establish
the cause of the repeated +3.78%/+3.49% regression. Function relocation is not
proof of causation, and there is no evidence attributing it to another agent.
The diagnostic packet is included in this stage's raw archive.

The next VM diagnosis should measure the complete short-call transaction in DSU,
stack and queue: which calls rebuild register templates, bytes copied for arguments
and returns, old managed references cleared, root locations validated and frame
transitions performed. It should distinguish first-use/growth from repeated work.
Choose a shared intervention from those counts and the updated profile. Logical
depth, traps, fuel, rooting and failure ordering remain acceptance requirements.
