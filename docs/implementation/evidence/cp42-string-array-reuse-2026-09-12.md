# Stable string-array read windows in accepted execution

Status: **diagnosed locally**, on `ff214f1a0f78a2ee875c82300fdfb4ac575fe2b4`. Production remains
the accepted string-publication implementation `613b703`. The
[accepted all32 comparison](cp42-string-publication-v2-2026-09-12-comprehensive.md)
remains current. Original all32 parity, complete Tier1 and Linux LeakSanitizer
remain open; JIT remains opt-in.

[Protocol and independently checked results](cp42-string-array-reuse-2026-09-12-summary.json),
[exact sources, commands, raw counters and owner review](cp42-string-array-reuse-2026-09-12-raw.jsonl.xz).
The archive is newline-delimited JSON compressed with XZ: its first record names
the schema/parent, then each record has path, UTF-8 data and SHA-256. Every decoded
file and the entire decoded stream were verified against their recorded hashes.

## Measurement and independent validation

A private ignored emitter copy passes the canonical parent register to a bounded
diagnostic wrapper. The original checked string-load bridge executes **exactly
once first**, with unchanged inputs/result, before instrumentation reads the
already-validated parent view. No VM/heap write, managed allocation or per-read
output is introduced. Fixed diagnostic BSS tables are outside the unchanged
64MiB managed heap and make these timings unsuitable for performance acceptance.
The production source, original 32 workloads and accepted controls retain their
hashes. Each process runs the original main once, including setup and teardown.

All **128 original results** pass: 32 workloads in unchanged order, two fresh
processes per plain/ledger variant. All four replicas of each workload have
identical complete counter records after only provenance ordinal normalization.
Errors, failed child projections, table overflows and unknown functions are zero.
All **64 ledger returns** have zero central-allocator live allocations and bytes;
this is not a Linux LeakSanitizer or process-RSS claim.

An independent analyzer rereads every raw file, checksum and ledger. For both
title sizes it derives hits from `(query * 7919) % size`, with every fourth query
missing. Unique decimal suffixes imply exactly hit-index-plus-one comparisons;
misses read the whole array. Closed sums reproduce read counts, index sums,
index extrema, backwards transitions, 128 search entries and checksums in all
four replicas. The names-array index sequence is independently reconstructed.

| Title size | Values-array reads | Entry epochs | Stable binding runs | Heap epoch | Mutation generation |
| --- | ---: | ---: | ---: | ---: | ---: |
| 10,000 | 804,208 | 128 | 1 | 1 | 10,000 |
| 100,000 | 7,994,208 | 128 | 1 | 1 | 100,000 |

Both use function symbol `a97fab987fdc9664d94fdb37dfa88596`, canonical register 5.
The independent names array has length 16 and 10,096/100,096 reads, each in a
new entry epoch. Neither title array changes storage during the observed reads.

## Three predeclared work-count policies

The complete observed key includes parent provenance/slot/generation, function,
canonical register/layout, heap view epoch and live mutation generation. Policy
state resets when a binding observes a different key. Immediate validation scans
on the first read; second-entry validation waits for a different native entry;
two-length validation waits for at least twice the array length in prior reads.
The table below models the third policy. Remaining work is original checked
reads minus eligible reads plus full-scan cells; it does not model elapsed time.

| Workload | Original string-child reads | Observed windows | Scans | Scan cells | Eligible reads | Remaining checked reads + scan cells |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| title-search-10k | 814,304 | 2 | 2 | 10,016 | 794,272 | 30,048 |
| title-search-100k | 8,094,304 | 2 | 2 | 100,016 | 7,894,272 | 300,048 |
| normalize-title | 152,618 | 10,004 | 2 | 8 | 19,984 | 132,642 |
| word-frequency | 500,090 | 100,011 | 1 | 4 | 99,992 | 400,102 |
| report-builder | 10,000 | 1 | 1 | 16 | 9,968 | 48 |
| report-concat | 10,000 | 21 | 21 | 336 | 9,328 | 1,008 |
| map-int | 0 | 0 | 0 | 0 | 0 | 0 |
| map-string | 139,264 | 2 | 2 | 4,112 | 131,040 | 12,336 |
| map-string-book | 50,000 | 1 | 1 | 16 | 49,968 | 48 |
| set-string-churn | 110,592 | 1 | 1 | 16 | 110,560 | 48 |
| catalog-match | 10,000 | 1 | 1 | 16 | 9,968 | 48 |
| catalog-dotcall | 10,000 | 1 | 1 | 16 | 9,968 | 48 |
| catalog-interface | 10,000 | 1 | 1 | 16 | 9,968 | 48 |
| record-reference-copy | 1,024 | 1 | 1 | 16 | 992 | 48 |
| record-create | 1,024 | 1 | 1 | 16 | 992 | 48 |
| array-grow | 0 | 0 | 0 | 0 | 0 | 0 |
| array-reserved | 0 | 0 | 0 | 0 | 0 | 0 |
| bfs-int | 0 | 0 | 0 | 0 | 0 | 0 |
| bfs-record | 0 | 0 | 0 | 0 | 0 | 0 |
| catalog-pipeline | 10,768 | 2 | 2 | 32 | 10,704 | 96 |
| parse-result | 0 | 0 | 0 | 0 | 0 | 0 |
| insertion | 0 | 0 | 0 | 0 | 0 | 0 |
| heap-sort | 0 | 0 | 0 | 0 | 0 | 0 |
| stack | 0 | 0 | 0 | 0 | 0 | 0 |
| queue | 0 | 0 | 0 | 0 | 0 | 0 |
| min-heap | 0 | 0 | 0 | 0 | 0 | 0 |
| dsu | 0 | 0 | 0 | 0 | 0 | 0 |
| bfs | 0 | 0 | 0 | 0 | 0 | 0 |
| dijkstra | 0 | 0 | 0 | 0 | 0 | 0 |
| frequency-map | 0 | 0 | 0 | 0 | 0 | 0 |
| unique-set | 0 | 0 | 0 | 0 | 0 | 0 |
| rune-frequency | 0 | 0 | 0 | 0 | 0 | 0 |

For the title pair, this model removes about **96.3% of reference validations**,
not 96.3% of runtime. Both immediate and second-entry policies would scan 10,004
normalize-title arrays and 100,011 word-frequency arrays. A new entry can occur
between elements of a one-pass array, so second-entry alone does not establish
reuse. Waiting for two lengths selects only two length-4 normalization constants
and one length-4 word-frequency constant, excluding their one-pass split arrays.
It models only 13.09%/19.99% fewer validations in those workloads.

## Scope and next experiment

These are windows between observed reads, not a trace of every intermediate
register write. A future cache that invalidates more eagerly may reuse less.
Counters, guards, scanning and cache pressure cost time. No certificate, new
runtime projection, production optimization or performance acceptance is part
of this diagnosis.

The archived owner review follows live mutation commits, private unchecked
storage primitives, reserve relocation, collection/epoch saturation, compiled
function bindings and teardown. Reserve can replace storage without changing
generation; any certificate must follow the live storage field. The currently
unused private storage_store primitive does not commit generation and cannot be
used for future live writes without the mutation protocol.

D104/D111 parent views do not grant a complete-child certificate. Existing
whole-array generated caches are allocating ARRAY/RECORD facilities restricted
to read-only native parameters. A new nonallocating STRING proof requires an
explicit D111/spec/architecture extension and focused tests. Speculative scan
failure must silently deny reuse, leaving the original selected-child failure,
target, PC, fuel, resource record and allocation behavior intact. Success must
be bound to the exact live rooted parent, execution/function/layout/type,
provenance/reference, heap epoch, mutation generation and length; no old storage
pointer or stale child address may survive those guards.

Next declare a fresh bounded candidate using the two-length hypothesis. Preserve
the original checked bridge/dispatcher as oracles, cover mutation/GC/recursion/
rebinding/malformed unvisited children and all allocation failures, then apply
fixed title-pair and all32 performance guards. Native remains an independent
parity obligation. No workload, threshold or final parity criterion changes.

Protocol began 2026-09-12T11:19:06.192432+00:00; due 2026-09-12T12:19:06.192432+00:00. Evidence prepared
2026-09-12T11:35:14.243558+00:00; deadline met: **True**. Commit/push is recorded
separately in the stage publication record.
