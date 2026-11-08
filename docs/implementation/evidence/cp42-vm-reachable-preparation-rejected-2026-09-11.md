# VM reachable preparation: rejected candidate

> **Private experiment payloads.** Archived implementations and patches
> referenced in this historical report are retained privately. Commands
> and digests below document the original experiment; they are not public
> reproduction instructions. See the [artifact index](private-artifacts.md).

Status: **rejected**. Production remains the implementation from `f4c9961`.
No performance change or new language/toolchain contract is accepted by this
record. The bounded source-phase diagnosis completed, but its selected
preparation candidate failed the declared repeated P90 regression gate.
Overall Go/Rust parity, full Tier 1 coverage and Linux LeakSanitizer remain open.

[Validation summary](cp42-vm-reachable-preparation-rejected-2026-09-11-validation.json),
[complete compressed evidence](private-artifacts.md#artifact-067),
[replayable candidate patch](private-artifacts.md#artifact-066).
The summary gives compressed and decoded SHA-256. Replay the patch on
`f4c9961bc807277c1aa3f34a1d5dc1ff7497c6f2` with `git apply --unidiff-zero`.
The patch's source and proposed contract changes are historical experiment
material, not the accepted specification.

## Diagnosis and candidate

Three alternating baseline/probe trials reproduce all 32 expected results:
192 complete executions with 114 fixed source/harness hashes, original arguments
and a 64 MiB heap. Sequential phase markers identify approximately 43 ms of
pre-execution work in catalogue workloads and 28 ms in individual algorithms.
JIT compilation itself is below 1 ms; checking, MIR optimization/lowering and
bytecode codec work dominate preparation. Probe perturbation and the first-launch
outlier remain in raw data; this is diagnostic attribution, not a speed claim.

Two additional all-32 replays produce identical conservative graph counts:
67 catalogue functions include 40 reachable functions; algorithm modules have
88 functions, but insertion reaches 6, stack 8 and BFS 5. Roots include main and
its structural Error renderer; complete call/interface-method tables include
deferred calls, function values and references in locally unreachable blocks.
The diagnostic selected this intervention before its recorded 15-minute deadline.

The candidate keeps complete selected-package source checking, then selects this
closed function graph before optional MIR passes and lowering. Graph allocation
or work exhaustion and missing edges fall back to the complete function set.
Bytecode encode/decode/verification, independent VM admission and runtime fuel,
call-depth, root and heap checks remain. The correctly typed lowering pointer
array has its own allocation and cleanup. Native and test compilation retain
their whole-set behavior. [LLVM GlobalDCE](https://www.llvm.org/docs/Passes.html#globaldce-dead-global-elimination)
is a reference for root-based elimination; the candidate is independent C17.

The first variant passed its early target, then was withdrawn before full
validation to remove a deep-qualification pointer-array cast. Its exact source,
binary identity and measurements remain preserved. The corrected variant's
three-task early geometric latency reduction was 8.68%, meeting the declared 8%.
No candidate source or compiler binary changed during subsequent measurements.

## Host interference and fixed recheck

The first full pair of matrices mechanically passed after the prescribed
confirmation. Review of host logs invalidated that acceptance: foreign Node work
was at >=50% CPU in 132 of 163 OFF snapshots, and another project's linter peaked
at 605.3%. Its -5.11% OFF median-sum change is not accepted causal evidence.
The original matrices, confirmation and [candidate-only fiveway](cp42-vm-reachable-preparation-rejected-2026-09-11-superseded-comprehensive.md)
remain preserved separately. That fiveway does not describe a released revision.

One fixed recheck retained the exact binaries, all 32 workloads, 3 warmups and
11 paired fresh-process samples, the original thresholds and the original
07:49:59–09:49:59 UTC stage window. It required three quiet preflight snapshots
ten seconds apart, then sampled every five seconds. Foreign compiler, test,
profiler, Node or Python work at >=40% CPU outside the benchmark process tree
would reject the timing. No such work was sampled in the recheck. This sampled
gate does not establish independently isolated hardware.

## Rejection result

JIT median sum: **7022.28 → 6783.06 ms
(-3.41%)**; geometric speedup
**1.0436x**. JIT has no initial median/P90 flags.
OFF median sum: **22529.18 → 22203.35 ms
(-1.45%)**; geometric speedup
**1.0299x**. OFF has no initial median flags, but five P90 flags.

The protocol requires JIT median-sum reduction of at least 3%, a nonincreasing
OFF sum and no repeated same-metric increase above 2% median or 5% P90. Each
initially flagged workload receives exactly one confirmation. **Dijkstra OFF
P90 repeats above the limit: 2254.62 → 2665.39 ms (+18.22%), then
2352.29 → 2720.26 ms (+15.64%).** Its medians move only +0.86% and +1.29%.
The result rejects this candidate; it does not by itself identify why the tail
changed. No additional repeat or easier replacement gate follows this result.
A second fiveway was not run after this rejection.

| Workload | JIT before ms | JIT candidate ms | OFF before ms | OFF candidate ms |
| --- | ---: | ---: | ---: | ---: |
| title-search-10k | 91.86 | 88.58 | 122.07 | 118.44 |
| title-search-100k | 503.27 | 497.09 | 803.56 | 777.56 |
| normalize-title | 140.27 | 137.44 | 169.12 | 164.37 |
| word-frequency | 268.37 | 266.61 | 326.82 | 322.08 |
| report-builder | 82.49 | 78.91 | 110.36 | 106.88 |
| report-concat | 162.15 | 159.76 | 189.46 | 185.86 |
| map-int | 67.52 | 64.15 | 81.41 | 77.82 |
| map-string | 88.48 | 84.50 | 104.82 | 101.01 |
| map-string-book | 92.30 | 89.10 | 102.93 | 99.78 |
| set-string-churn | 111.91 | 108.74 | 118.28 | 115.58 |
| catalog-match | 122.72 | 119.13 | 207.47 | 203.56 |
| catalog-dotcall | 102.79 | 99.17 | 154.04 | 150.31 |
| catalog-interface | 126.13 | 123.72 | 184.19 | 179.95 |
| record-reference-copy | 86.56 | 83.20 | 140.84 | 136.32 |
| record-create | 115.80 | 112.43 | 170.66 | 166.89 |
| array-grow | 91.41 | 87.72 | 160.97 | 157.50 |
| array-reserved | 90.92 | 87.74 | 160.65 | 156.68 |
| bfs-int | 76.50 | 73.13 | 174.57 | 169.50 |
| bfs-record | 82.87 | 79.41 | 186.60 | 182.11 |
| catalog-pipeline | 224.17 | 220.61 | 448.75 | 443.19 |
| parse-result | 78.26 | 74.84 | 91.56 | 88.07 |
| insertion | 211.40 | 195.41 | 215.55 | 199.04 |
| heap-sort | 265.64 | 249.32 | 1438.88 | 1410.74 |
| stack | 319.64 | 305.36 | 610.41 | 591.78 |
| queue | 323.91 | 305.56 | 681.45 | 660.54 |
| min-heap | 581.07 | 564.83 | 4554.50 | 4499.95 |
| dsu | 848.40 | 830.18 | 3301.58 | 3294.01 |
| bfs | 960.29 | 950.70 | 4437.56 | 4395.08 |
| dijkstra | 211.14 | 198.33 | 2244.36 | 2263.57 |
| frequency-map | 220.31 | 205.41 | 374.65 | 357.39 |
| unique-set | 154.35 | 138.25 | 266.77 | 248.98 |
| rune-frequency | 119.38 | 103.77 | 194.36 | 178.79 |

## Correctness and retained limitations

The rejected candidate passes **102/102 ON** and **90/90 OFF** tests,
**20/20** focused macOS ASan/UBSan tests and **20/20** under x86-64 Rosetta.
Both instrumented/platform builds reproduce all 32 expected outputs. New tests
cover all 65,536 four-node graphs using an independent closure oracle; stable
ordering, recursion, interfaces, deferred calls, error-renderer reachability,
exact finite/shared/unmetered fuel and depth behavior, work fallback and
reachable deterministic allocation failures with zero allocation/byte ledgers.
Process differential tests retain imported-but-uncalled source diagnostics.
Both 47-object Native runtime profiles and all Native/Go/Rust/Bun executable
identities remain unchanged in the candidate comparison.

A process fixture also exposed a pre-existing Native B0003 failure with an
unreachable unconditional recursive function. The accepted baseline reproduces
it, while removing that function succeeds. This limitation and the reductions
remain open in raw evidence. Incorrect fixture syntax and an unimported-package
expectation were corrected before validation; baseline and candidate agree on
the actual imported source graph. macOS uses `detect_leaks=0`; Rosetta proves
functional execution, not physical x86 performance.

Production source changes were restored after verifying the complete saved
patch and archive. This commit records diagnosis and rejection only. The next
performance stage must measure remaining execution costs; static inspection
identifies excluded JIT string operations and shared string allocation/copying
as hypotheses, not selected or measured optimizations.
