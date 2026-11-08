# Shared string storage, publication and recent array views

> **Private experiment payloads.** Archived implementations and patches
> referenced in this historical report are retained privately. Commands
> and digests below document the original experiment; they are not public
> reproduction instructions. See the [artifact index](private-artifacts.md).

Status: **accepted locally**, against
`574c620e766019606572cc02ed31c162012d5054`. The title-search 10k/100k family remains active;
this is not original all-32-workload Go/Rust parity or complete Tier1 verification.
JIT remains opt-in. The [first publication candidate](cp42-string-publication-2026-09-11-rejected.md)
retains its rejected status and missed deadline.

[Protocol, validation and results](cp42-string-publication-v2-2026-09-12-summary.json),
[raw evidence](private-artifacts.md#artifact-053), [candidate patch](private-artifacts.md#artifact-052).
Applying the patch to the parent with `git apply --unidiff-zero` reproduces tree
`3ba342b10c4826a1e5c2aa9453a4989501af5962`. Sources, binary identities, commands, observations, counter copies and
unsuccessful setup logs are retained; the summary supplies archive/patch hashes.

## Mechanisms and contracts

The string owner combines the unchanged header and exact payload in one
allocation and copies concatenation operands directly into the final block.
Native and VM share this constructor; VM commits its target only on success.
The collection owner traces precise roots and forecasts exact post-sweep budget
charges before the host allocation, retaining unreachable bytes until borrowed
inputs have been copied. It then finishes sweep on success or cancellation.
This preserves unrooted slices, output aliases, embedded zeros, empty strings,
logical charges, handles, slot order, generation retirement and view invalidation.
It addresses the known D055 string budget-before-host-allocation defect; the
preallocation behavior of other object constructors is not claimed repaired.

The [regression diagnosis](cp42-string-publication-regressions-2026-09-12.md)
found 132096 record registrations and zero GC in record-create. The publication
owner now shares private preparation and slot installation between its two
entry paths. Public deferred transactions retain validation and checked commit;
already allocated objects prepare and install directly without an intervening
observer or repeated external transaction validation. Budget/slot logic is not
copied from the diagnostic legacy wrapper. No recursive same-heap allocator
callback, source call or unrelated heap mutation may intervene.

The D104 nonallocating region now checks its last successful array-cache entry
first, using the same complete reference/type/layout predicate. On a miss it
checks each other initialized entry once. Capacity, round-robin replacement,
rooted-view lifetime and exact fuel remain unchanged; the index resets at every
region entry. Descriptor misses and errors still stop before committing an
instruction. The focused source fixture revisits an entry, rebinds the same
typed register and mutates through the new reference; its differential result
is 183. Existing eviction, layout, alias, cold-call/GC and fuel cases remain.

Two original-workload counter replays per engine/variant retain every logical
string operation, payload byte, comparison, byte resolution and phase transition.
Title GC remains zero. Counter copies may overlap validation; their elapsed
times are not performance evidence.

| Workload | Engine | Setup allocations before → candidate | Setup resizes before → candidate |
| --- | --- | ---: | ---: |
| title-search-10k | native | 80037 → 30021 | 24 → 24 |
| title-search-10k | vm | 100048 → 40032 | 25 → 25 |
| title-search-100k | native | 800037 → 300021 | 31 → 31 |
| title-search-100k | vm | 1000048 → 400032 | 31 → 31 |

Separate OFF counters retain identical hot entries, lookup outcomes, descriptor
and binding failures and complete attempted/successful opcode histograms. The
actual comparison counts equal the previous-entry-first prediction recorded
before V2 implementation. These are counts, not claimed time reductions.

| Workload | Array lookups | Comparisons before → candidate | Completed hot instructions |
| --- | ---: | ---: | ---: |
| dijkstra | 151722696 | 346654087 → 227999334 | 501347499 |
| title-search-100k | 16388711 | 16188485 → 16188485 | 73651001 |
| heap-sort | 123880297 | 122130292 → 122130292 | 324938424 |

## Validation and predeclared acceptance

Full CTest passes **104/104 ON** and **89/89 OFF**; focused macOS ASan/UBSan
and x86-64 Rosetta each pass **21/21**, with all 32 original VM outputs on both.
Publication tests cover all reachable deterministic allocation indices in
creation/concatenation and slot growth, exact/short budgets before allocator
calls, rooted/unrooted slices, array-storage borrows, aliases, GC errors and
cancellation, slot reuse/retirement and both zero teardown ledgers. Existing
VM/native budget, lifetime, text and formatter fixtures pass. Architecture,
C memory policy, roadmap, links, spec terms and generated-file gates pass.
No new syntax is involved. macOS ASan uses `detect_leaks=0:halt_on_error=1`;
Linux LSan, physical x86 performance and complete Tier1 remain open.

Initial ON configuration omitted the required local SLJIT source path and failed
before building. The same pinned local source as the controls was then supplied;
the failed command, script and log remain recorded. No dependency was downloaded.

All 32 algorithms, original arguments, stdout, 114 corpus/harness hashes and the
64MiB limit remain frozen. Full validation uses separate compilers. Native
controls come from the accepted string-view archive. Three quiet snapshots
precede timing; 5-second monitoring rejects known foreign compute at 40% CPU and
gaps over 15s, including prelaunch, inter-pair and final intervals. No builds,
tests or profiles overlap acceptance timings. These are local measurements.

Early timing uses 3 warmups and 11 rotating/alternating fresh-process pairs.
The original title minima remain Native pair reduction of at least 10%, each row at least 5%, and
ON pair reduction of at least 3%, with neither row median/P90 over 5% slower. New required guards
are Native record-create median/P90≤1.03 and OFF Dijkstra median≤0.98/P90≤1.00.
Failed early minima reject without a favorable retry. Separate 20%/10% title
aspirations are not acceptance minima.

Early title reductions are 17.60% Native and 7.41% ON; the separate aspirations remain missed.

Early record-create: median ratio 1.01202, P90 ratio 1.01044; guard passed: True.

Early dijkstra: median ratio 0.97256, P90 ratio 0.97119; guard passed: True.

Full timing covers all 32 Native/ON/OFF workloads with 3 warmups and 11 paired
samples. Native summed medians must improve at least 3%; ON/OFF sums may not
regress over 1%. Each row median/P90 over 5% slower receives exactly one confirmation; a repeated same-metric excess
rejects the candidate. All initial flags and smaller tradeoffs remain visible.

| Workload | Native before → candidate ms | ON before → candidate ms | OFF before → candidate ms |
| --- | ---: | ---: | ---: |
| title-search-10k | 7.157 → 6.396 | 56.057 → 55.223 | 83.224 → 81.726 |
| title-search-100k | 57.913 → 42.582 | 163.659 → 142.591 | 430.221 → 407.621 |
| normalize-title | 55.695 → 55.047 | 129.415 → 127.519 | 161.757 → 160.997 |
| word-frequency | 140.371 → 138.579 | 255.336 → 251.057 | 311.016 → 309.932 |
| report-builder | 14.111 → 11.234 | 80.638 → 77.882 | 108.404 → 105.117 |
| report-concat | 91.615 → 58.873 | 159.484 → 127.426 | 182.520 → 153.300 |
| map-int | 7.161 → 7.183 | 66.603 → 66.361 | 78.440 → 79.497 |
| map-string | 21.902 → 20.569 | 83.392 → 82.266 | 99.238 → 98.015 |
| map-string-book | 29.784 → 24.965 | 89.998 → 83.801 | 98.629 → 93.436 |
| set-string-churn | 44.268 → 33.116 | 107.257 → 95.490 | 113.220 → 100.267 |
| catalog-match | 12.970 → 12.807 | 119.559 → 120.378 | 202.459 → 202.648 |
| catalog-dotcall | 8.000 → 8.050 | 101.878 → 102.993 | 149.989 → 149.027 |
| catalog-interface | 11.549 → 11.750 | 126.320 → 123.559 | 179.288 → 179.780 |
| record-reference-copy | 6.581 → 6.491 | 82.833 → 83.042 | 137.284 → 138.223 |
| record-create | 22.434 → 22.816 | 111.927 → 112.524 | 165.906 → 167.244 |
| array-grow | 6.402 → 6.406 | 80.942 → 81.125 | 156.465 → 157.038 |
| array-reserved | 6.365 → 6.330 | 80.772 → 80.783 | 157.095 → 156.956 |
| bfs-int | 7.009 → 7.042 | 71.310 → 71.425 | 171.273 → 171.379 |
| bfs-record | 8.927 → 8.880 | 78.023 → 78.189 | 183.997 → 184.402 |
| catalog-pipeline | 35.245 → 35.258 | 203.371 → 204.342 | 410.526 → 412.924 |
| parse-result | 18.496 → 17.468 | 74.320 → 72.465 | 89.350 → 87.306 |
| insertion | 51.847 → 51.894 | 207.570 → 207.597 | 212.907 → 210.560 |
| heap-sort | 31.308 → 31.325 | 242.418 → 241.826 | 1455.319 → 1485.015 |
| stack | 9.405 → 9.106 | 260.872 → 261.208 | 599.606 → 607.010 |
| queue | 9.178 → 9.019 | 260.371 → 254.823 | 674.847 → 678.585 |
| min-heap | 44.828 → 44.858 | 518.427 → 516.008 | 4205.413 → 4225.061 |
| dsu | 28.986 → 29.310 | 766.629 → 774.153 | 3254.946 → 3264.511 |
| bfs | 63.851 → 64.095 | 652.520 → 662.125 | 4419.337 → 4371.186 |
| dijkstra | 41.070 → 41.116 | 200.380 → 200.265 | 2238.316 → 2185.514 |
| frequency-map | 31.794 → 32.014 | 197.407 → 200.206 | 370.388 → 371.365 |
| unique-set | 26.375 → 26.255 | 132.344 → 131.493 | 260.360 → 259.549 |
| rune-frequency | 21.507 → 21.600 | 107.277 → 107.339 | 191.762 → 193.029 |

native: summed medians 974.102 → 902.432ms (-7.36%). Initial flags: `{}`; confirmed: `{}`.

on: summed medians 5869.312 → 5797.485ms (-1.22%). Initial flags: `{"map-string": ["p90_ratios"]}`; confirmed: `{}`.

off: summed medians 21553.503 → 21448.223ms (-0.49%). Initial flags: `{}`; confirmed: `{}`.

## Current title phase diagnosis

Five observations per size/engine use coarse markers without allocator or per-comparison counters, after acceptance timing. Phase medians guide the next step; they are not acceptance observations or additive process reconstructions.

| Workload | Engine | Process median ms | Source preparation phases1–7 ms | Data setup ms | Repeated search ms | Cleanup phases13–16 ms |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| title-search-10k | native | 6.524 | 0.000 | 1.722 | 1.524 | 0.468 |
| title-search-10k | vm | 54.763 | 38.549 | 3.664 | 4.945 | 0.968 |
| title-search-100k | native | 45.926 | 0.000 | 17.603 | 20.436 | 4.565 |
| title-search-100k | vm | 141.460 | 38.411 | 35.225 | 53.474 | 7.305 |

The [fresh five-way comparison](cp42-string-publication-v2-2026-09-12-comprehensive.md) retains every row. Native excludes compilation; source-run VM includes loading/checking/lowering/JIT preparation. Neither aggregate improvement nor a faster subset establishes parity.

## Storage recovery and remaining gates

The filesystem filled during preceding diagnosis. One 35,332,396-byte local JSON
cache was removed only after matching its exact contents to the committed raw
regression archive. Logs, CMake metadata and compiler hashes were archived before
removing thirteen completed historical sanitizer build caches. Current test copies
were converted to byte-identical APFS copy-on-write clones, preserving independent
future writes. All 284 frozen files and 46 Rosetta binaries from the preceding
stage were then rehashed unchanged. User files and current timing controls were
outside cleanup scope; the detailed local audit is included in the raw evidence.

The title family and original all-32-workload parity remain open, as do complete Tier1,
Linux LSan and the known F32/B0001 gaps. V1's deadline remains missed. V2 began
2026-09-12T05:43:24.994510+00:00, due 2026-09-12T07:43:24.994510+00:00; this evidence was prepared
2026-09-12T07:25:51.675530+00:00, deadline met: **True**.
