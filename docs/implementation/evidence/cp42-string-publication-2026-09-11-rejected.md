# Shared string allocation and budget preflight: rejected candidate

> **Private experiment payloads.** Archived implementations and patches
> referenced in this historical report are retained privately. Commands
> and digests below document the original experiment; they are not public
> reproduction instructions. See the [artifact index](private-artifacts.md).

Status: **rejected**, against `e74872a7a38e378e1dce3dc541fab456ee3cdb08`. The full Native gate
contains a confirmed per-row regression, despite an improved aggregate. The
candidate is archived and production sources are restored before publication.
The title-search 10k/100k family, original all32 parity, Linux LeakSanitizer and
complete Tier1 evidence remain open. JIT remains opt-in.

[Protocol and complete result summary](cp42-string-publication-2026-09-11-rejected-summary.json),
[raw evidence](private-artifacts.md#artifact-050), [candidate patch](private-artifacts.md#artifact-049).
Applying the patch to the parent with `git apply --unidiff-zero` reproduces tree
`c751c22d18c6d1a99e2b323fd1a095bc8a4c9e32`. Exact sources, binaries, commands, every observation and failed setup
logs are retained. The summary gives patch and compressed/decoded archive hashes.

## Candidate and diagnostic evidence

The [preceding phase diagnosis](cp42-title-after-string-views-2026-09-11.md)
selected shared string construction: title100k setup constructed 300016 Native
and 400016 VM strings without GC. This candidate combines the unchanged header
and exact payload in one allocation and copies concatenation inputs directly
into the final block. Native and VM use the shared constructor; VM commits its
destination only on success, preserving input roots. Exact payload bytes,
embedded zeros, empty strings, handle identity and logical charges are retained.

The candidate addresses the known D055 string budget-preflight defect. A new
collection owner traces roots and forecasts exact post-sweep charges without
allocating. A private token links dead slots while retaining their bytes, so
valid borrowed inputs can be copied before physical sweep. The publication
owner checks the budget before host allocation, finishes or cancels collection
on all paths, then publishes the handle. Ordinary collection finishes at once.
Slot order, generation retirement and view-epoch invalidation are preserved.
No source execution, recursive same-heap allocator callback or unrelated heap
mutation may intervene. Other object constructors use the publication wrapper;
their allocation-before-preflight behavior is not claimed repaired. These are
candidate changes only: D055 remains open in accepted production after rejection.

Two counter replays per original size, engine and variant preserve all logical
string constructions, payload bytes, comparisons, byte-resolution calls and
phase transitions. GC remains zero. Counter-only copies may overlap validation;
their elapsed times are not performance evidence. The unused earlier runner
with stricter host isolation is archived beside the executed counter runner.

| Workload | Engine | Setup allocations before → candidate | Setup resizes before → candidate | Setup strings |
| --- | --- | ---: | ---: | ---: |
| title-search-10k | native | 80037 → 30021 | 24 → 24 | 30016 |
| title-search-10k | vm | 100048 → 40032 | 25 → 25 | 40016 |
| title-search-100k | native | 800037 → 300021 | 31 → 31 | 300016 |
| title-search-100k | vm | 1000048 → 400032 | 31 → 31 | 400016 |

## Validation and measurement protocol

Full CTest passes **104/104 ON** and **89/89 OFF**. Focused macOS ASan/UBSan and
x86-64 Rosetta each pass **21/21**; both reproduce all32 expected VM outputs.
The new focused suite covers every allocation index for string construction
and slot growth, exact/one-byte-short budgets before allocator calls, rooted
and unrooted borrows, array-storage slices, aliased outputs, GC errors and
cancellation, slot retirement/reuse and zero live allocation/byte ledgers.
Existing VM/native lifetime, budget, text and formatter fixtures pass. There
is no new syntax. Architecture, memory policy, roadmap, links, terms and
generated-artifact checks pass. macOS ASan uses `detect_leaks=0:halt_on_error=1`;
this and Rosetta functionality do not establish Linux LSan or physical x86 speed.

The first Rosetta selection ended with five wall-clock timeouts during repeated
system Sleep/DarkWake events. Two tests printed success before timeout; none of
these timeout results counts as a pass. The original command, logs, profiles
and system events remain archived. The unchanged whole selection was repeated
under an idle-sleep assertion, followed by all32 x86 outputs and final gates.
The candidate, control and all46 x86 executable hashes were checked; no timeout
or performance threshold changed. The original protocol deadline was missed.

All32 source algorithms, arguments, expected stdout, 114 corpus/harness hashes
and the 64MiB limit remain frozen. Native controls use explicit archived binaries
from the accepted string-view stage. Full validation uses separate compilers.
Timing uses 3 warmups and 11 rotating/alternating fresh-process pairs for each
engine. Three quiet snapshots precede timing; 5s monitoring rejects sampled
known foreign compute at 40% CPU or gaps over 15s. Following the actual overnight
interruption, the same 15s guard covers prelaunch, between-pair and final intervals.
The old runner and retrospective early-series audit remain archived; the early
series has no interval over the limit. Builds, tests and profiles do not overlap
timings. These are local observations, not a dedicated-host claim.

The early Native title-pair minimum is a 10% geometric reduction with each row
at least 5% faster; ON requires 3% pair reduction with neither median/P90 row over
5% slower. Full Native summed medians must improve at least 3%; ON/OFF sums may
not regress over 1%. Each row median/P90 over 5% slower receives exactly one
confirmation; the same metric repeating above the limit rejects the candidate.

Early reductions are 18.77% Native and 7.17% ON: minima pass, separate 20%/10% aspirations are missed.

| Engine | Sum of medians before → candidate ms | Change | Confirmed regressions |
| --- | ---: | ---: | --- |
| native | 979.587 → 912.343 | -6.86% | {"record-create": ["median_ratios"]} |
| on | 5865.839 → 5788.076 | -1.33% | {} |
| off | 21445.511 → 21323.993 | -0.57% | {"dijkstra": ["p90_ratios"]} |

| Workload | Native before → candidate ms | ON before → candidate ms | OFF before → candidate ms |
| --- | ---: | ---: | ---: |
| title-search-10k | 7.309 → 6.698 | 56.355 → 55.136 | 82.170 → 80.614 |
| title-search-100k | 56.115 → 44.865 | 163.351 → 141.188 | 419.661 → 397.590 |
| normalize-title | 56.167 → 56.197 | 128.886 → 127.772 | 161.118 → 159.160 |
| word-frequency | 141.405 → 138.594 | 254.489 → 250.079 | 309.726 → 306.186 |
| report-builder | 14.131 → 11.415 | 80.455 → 77.468 | 106.425 → 103.629 |
| report-concat | 94.403 → 60.644 | 158.618 → 126.983 | 182.338 → 151.000 |
| map-int | 7.315 → 7.335 | 66.529 → 66.255 | 78.223 → 77.984 |
| map-string | 22.049 → 20.555 | 83.390 → 81.994 | 98.945 → 97.741 |
| map-string-book | 30.065 → 25.368 | 90.034 → 84.235 | 98.823 → 93.164 |
| set-string-churn | 45.259 → 33.015 | 108.416 → 96.091 | 111.398 → 100.104 |
| catalog-match | 13.045 → 12.992 | 120.153 → 121.168 | 202.351 → 202.008 |
| catalog-dotcall | 8.013 → 8.227 | 101.344 → 101.818 | 149.254 → 149.583 |
| catalog-interface | 11.614 → 11.927 | 123.853 → 124.335 | 179.025 → 179.090 |
| record-reference-copy | 6.540 → 6.537 | 83.562 → 84.129 | 136.849 → 137.364 |
| record-create | 22.715 → 24.266 | 112.131 → 112.977 | 166.286 → 167.017 |
| array-grow | 6.407 → 6.558 | 81.288 → 81.370 | 156.619 → 156.971 |
| array-reserved | 6.454 → 6.371 | 80.904 → 80.620 | 156.591 → 156.351 |
| bfs-int | 7.086 → 7.055 | 71.563 → 71.343 | 170.437 → 170.826 |
| bfs-record | 8.880 → 8.936 | 78.064 → 78.237 | 183.548 → 183.849 |
| catalog-pipeline | 35.324 → 35.573 | 203.521 → 203.701 | 409.534 → 410.691 |
| parse-result | 18.892 → 17.861 | 74.526 → 72.624 | 88.933 → 87.223 |
| insertion | 50.063 → 51.322 | 207.022 → 206.939 | 210.266 → 209.762 |
| heap-sort | 31.446 → 31.436 | 244.152 → 242.235 | 1460.256 → 1449.822 |
| stack | 9.176 → 9.170 | 261.343 → 260.950 | 599.928 → 599.212 |
| queue | 9.145 → 9.230 | 255.406 → 256.111 | 671.778 → 665.374 |
| min-heap | 44.916 → 44.739 | 519.270 → 517.364 | 4201.538 → 4186.626 |
| dsu | 29.080 → 29.361 | 769.482 → 776.414 | 3242.189 → 3233.093 |
| bfs | 64.934 → 64.760 | 652.861 → 653.758 | 4374.398 → 4369.035 |
| dijkstra | 41.236 → 41.043 | 200.305 → 200.000 | 2218.406 → 2226.225 |
| frequency-map | 32.100 → 32.026 | 196.290 → 197.095 | 369.276 → 367.567 |
| unique-set | 26.635 → 26.634 | 132.066 → 131.670 | 258.947 → 259.306 |
| rune-frequency | 21.665 → 21.633 | 106.211 → 106.014 | 190.273 → 189.826 |

Initial flags, P90 values, confirmations, dispersion and all raw observations are retained in the summary and archive. Confirmed blockers:

- native `record-create` median: initial +6.83%, confirmation +6.20%.
- off `dijkstra` p90: initial +20.28%, confirmation +20.86%.

No fresh five-way candidate comparison or further phase timing is run after
rejection. The [last accepted five-way snapshot](cp42-string-views-2026-09-11-comprehensive.md)
continues to describe the accepted implementation. The next step is a focused
diagnosis of the confirmed regressions before revising shared publication.
An extra prepare/commit path is a hypothesis, not an established cause.

Protocol began 2026-09-11T18:13:42.004485+00:00; deadline 2026-09-11T20:13:42.004485+00:00.
Evidence prepared 2026-09-12T04:56:00.968866+00:00; deadline met: **False**.
