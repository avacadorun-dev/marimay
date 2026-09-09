# Full catalogue measurement and native child-access regression

Local Apple M2 / macOS arm64 investigation, 2026-09-09; parent `fc754a8`.
[Raw matrices, exact commands/hashes and sampling profiles](cp42-catalog-performance-2026-09-09.json).
This is CP42 performance evidence under D055/D063 and execution spec §9.2.
It does not close CP42, CLI0, other Tier 1 or existing portfolio budgets.

## Measurement boundary

The unchanged external `algorithm-benchmarks` sources were frozen for each run.
All 32 standard-size workloads were measured on Marimay native, Go and Rust:
11 samples after 3 warmups, 1,344 process observations including warmups.
The installed baseline release SHA-256 was
`aef6305d9adaa331873381db7fbb8fc82c678cc2993abfa7f4671574209ec130`.
All checksums, source and artifact integrity checks passed. A separate VM
full-size diagnostic run passed all 32 workloads, with 3 samples each and no
warmups (96 observations). It establishes functionality and locates large
costs; it is not an 11-sample VM performance acceptance result.

Native timings include input construction, workload, checksum, process startup
and teardown; compilation is excluded. VM also includes source checking and
lowering. VM numbers are not used for Go/Rust ratios. Runs are local-only with
unverified host isolation; browser activity was present. Small differences and
noisy cells cannot establish a general competitive-performance claim.

The previous user run lost its results after a final source-hash rejection:
console-output edits changed a hashed harness file during measurement. The
external harness now flushes every observation to a JSONL journal and checks
integrity after each round, prints changed paths and diagnostic partial results,
and supports read-only journal replay. Invalid runs still fail validation;
there is no waiver or publication of mixed-input samples. Its 89 Python tests
passed, including changed sources/binaries, interruption, failed cells and a
truncated journal tail. Normal runs print the final table to the console as
well as writing their report. The earlier terminated process had no journal;
its samples could not be recovered. The harness changes remain in the external
repository's pre-existing dirty worktree; they are not compiler changes.

## Confirmed native defect

Baseline `bfs-record 4096 16` takes 553.85 ms versus Go 5.67 ms and Rust 4.54 ms;
Marimay's adjacent integer-queue representation takes 7.27 ms. All versions
construct a fixed-degree graph and traverse the same edges with equal checksums.
A three-second CPU sample of the same record workload with only its rounds
increased finds 2,511 of 2,539 samples (98.9%) beneath
`mmay_managed_array_generated_cache_children`.

Generated native child access refreshes the whole parent cache whenever its
mutation generation changes. A growing record queue therefore rescans and may
reallocate for its complete prefix after a push. Linear element reads acquire
quadratic extra work. This is a general array-of-records/arrays defect, not a
problem in the BFS algorithm or the benchmark's source model.

The correction confines bulk caches to existing proven read-only parameters at
activation entry. A mutable/local parent reads its selected, already validated
cell and obtains the exact child through the current managed slot in constant
time. It ignores any stale cache retained from a preceding read-only callee.
D063 identity/layout invariants, nonmoving objects, precise roots, checked
bounds and mutation barriers remain unchanged. No source API, syntax, MIR or
wire semantics change and no runtime allocation API is added.

The managed-view fixture preserves parameter prewarming and structurally rejects
per-access refreshes and stale-cache fallback. It executes replaced and grown
record/nested-array children in generated C at O0 and O2. The root fixture
prewarms through callees, replaces through an alias, retains the previous
children, grows both arrays, and reads them after allocation pressure. Its
shared engine harness covers MIR, encoded/decoded VM, generated C, forced GC,
all reachable deterministic allocation failures and both zero allocator
counters after full teardown.

## After the correction

The full 32-workload native/Go/Rust matrix passed again with the same inputs,
3 warmups and 11 samples per cell (another 1,344 observations). BFS with records
fell from **553.85 to 9.58 ms**. In the final matrix Go was 6.80 ms and Rust
4.79 ms: Marimay is now 1.41× Go and 2.00× Rust in this case, instead of
97.70× and 122.05×. This closes the measured quadratic-cache defect locally;
it is not general language parity.

Other runtimes also slowed between the two matrices, so small before/after
changes cannot be attributed to this patch. A separate paired check alternated
the preserved baseline binary and the new binary for all 21 catalogue cases,
with 3 warmups and 11 samples (588 checksum-validated observations). Binary
hashes were checked before and after. BFS medians were **575.91 / 9.73 ms**,
confirming a **59.2×** improvement under alternating execution. Reference-copy
medians were 8.67 / 7.08 ms. Other cases ranged from 6.3% faster to 4.5% slower;
this local diagnostic does not establish a significant regression or speedup
for those small changes. All raw paired observations are retained in the JSON.

All **86 Release CTest cases passed** across the full run and corrected reruns:
84 passed initially; the native and CP41 owners passed after fixing two new
test-source Result discards (230.99 s and 6.28 s respectively). Those initial
test-authoring failures remain in the raw evidence. The focused root/view
driver also passed under ASan/UBSan; it links the CMake test objects and invokes
only the two changed owners. Generated C inherits address/undefined sanitizers,
forced collection, and both O0/O2 configurations. Every applicable allocation
failure sweep checks zero live allocations and bytes after teardown. Linux
LeakSanitizer and other Tier 1 evidence remain CI obligations.

Architecture, C memory policy, generated artifacts and roadmap gates passed.
The system `mmay` symlink resolves to rebuilt `build/release/mmay`, SHA-256
`43759662b55b3fc5c9518757fc301997241272bb9b046de8726ed5c4c3e7301e`.
Its original VM report-concat reproducer still returns checksum 762303259.

## Remaining shared costs

These are measured open work, not acceptance waivers or predictions of speedup:

The timings below are from the final full native matrix; the sampling evidence
was collected on the baseline before the array-access correction.

| Area | Native ms | Go ms | Rust ms | Evidence and next shared boundary |
|---|---:|---:|---:|---|
| Word frequency | 143.12 | 20.11 | 31.50 | Split result construction is 1,393/2,537 sampled stacks (54.9%); owned strings/arrays, repeated scans, allocation and GC are visible. |
| Title normalization | 60.63 | 9.83 | 11.68 | Option construction is 723/2,548 (28.4%); owned grapheme result construction is 670/2,548 (26.3%). |
| String map | 25.06 | 8.15 | 11.10 | Above the external 2× Go target; requires separating lookup from result wrapping and key construction. |
| String set churn | 52.18 | 10.96 | 19.35 | Creation, deletion/reinsertion and retained key storage require a focused profile; do not assume the hash table alone is responsible. |
| Records | 24.38 | 7.49 | 6.65 | Escaping nominal records, a shared allocation/representation case. |
| Catalogue pipeline | 38.28 | 8.42 | 10.47 | End-to-end guard for improvements in text, maps and the record/enum model. |

Sampling percentages are inclusive within their call trees and must not be
summed across overlapping owners. Larger round counts extend profiling windows
only and are excluded from the comparative measurements. All profiling command
checksums were independently verified. No allocation-count or GC-pause
benchmark, or memory-leak conclusion, follows from these CPU profiles.

The route is to improve owned-result construction and transient Option/Result
transport across the shared collection/text paths, keeping escaping payloads,
allocation-failure behavior and precise GC roots correct. Measure lookup and
construction separately before changing Map/Set storage again. Keep the full
catalogue pipeline as an acceptance guard, not just individual helper timings.
The exact representation and its safety proof still require design and tests;
this report does not approve an unchecked/unrooted fast path.

VM insertion takes 21,202.58 ms per complete fixed anchor (50 × 2,500 integers).
BFS, DSU and Dijkstra also take seconds. Sampling after startup is inside the VM
execution loop: descriptor lookup, value/type validation, array access, vector
access and per-instruction accounting recur. Source preparation cannot explain
this steady-state cost. The common follow-up is a verified execution packet
with prepared layout/type metadata and a bounded dispatch/access path, retaining
hostile-bytecode validation, exact resource limits and panic timing. CLI startup
and portfolio preparation budgets remain a separate existing obligation.

Several original native integer workloads already meet their external Go
budgets: heap-sort, stack, queue, min-heap and Dijkstra. This evidence argues
against indiscriminate micro-optimization or rewriting every collection. The
borderline integer-map ratio (1.54× versus a 1.5× target) and noisy frequency-map
Rust cell need isolated repetition before a firm threshold conclusion.

## Book attribution

This investigation uses measured stacks, local ownership code and accepted
D055/D063 contracts. No new book idea was used. The earlier RPO application
remains separately documented at Vladimirov printed 145–147 / PDF 146–148.
