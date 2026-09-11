# Shared string storage and direct concatenation

Status: **Rejected** against `7ee78591052071f1d6c0a3371cadc986669f6792`. This stage implements
the intervention selected by the [text runtime diagnosis](cp42-text-runtime-diagnosis-2026-09-11.md).
Overall Go/Rust parity, complete Tier 1 verification and Linux LeakSanitizer
remain open. JIT remains opt-in. The stage starts at 2026-09-11T10:03:46.647042+00:00
and its publication deadline is 2026-09-11T12:03:46.647042+00:00.

[Protocol, validation and measurement summary](cp42-shared-string-storage-2026-09-11-rejected-summary.json),
[complete compressed evidence](cp42-shared-string-storage-2026-09-11-rejected-raw.json.gz),
[replayable candidate patch](cp42-shared-string-storage-2026-09-11-rejected-candidate.patch).
The summary supplies compressed and decoded hashes. The decoded JSON retains
exact candidate sources, scripts, commands, logs, diagnostic probes and all
observations, including unsuccessful setup/preflight launches. Replay the patch
on the parent revision above with `git apply --unidiff-zero`. A temporary index
replay reconstructs all seven candidate files at their frozen hashes. Diagnostic
code is not production runtime code.

## Change and invariants

The shared string owner allocates one block for the unchanged header and payload
and copies concatenation operands directly into that payload. Disposal frees
that block once. Native already calls the shared concat API; VM now calls it
with a local result, committing the target register only on success. The target
may alias either input. Borrowed input reads finish before publication may run
GC. Empty strings retain a null byte view and zero length; embedded NUL and
Unicode bytes are preserved exactly. Published references, object layout and
logical header/payload/slot-table byte accounting remain unchanged.

Only the string construction/disposal objects differ among the 47 Native runtime
objects, in both debug and release profiles. No benchmark source, input,
algorithm, output or heap limit changed. All 114 source/harness hashes remain
fixed and the managed heap remains 64 MiB. The change uses the existing D055
managed-string contract; it does not introduce source syntax or semantics.

## Correctness and diagnostic evidence

Full CTest passes **99/99 with JIT** and **87/87 without it**. Focused macOS
ASan/UBSan and x86-64 Rosetta each pass **15/15**, including the runtime/bytecode
owners, JIT root/fuel tests and VM/native string lifetime differential test.
Both platform/instrumented binaries reproduce all 32 expected workload outputs.
All six architecture, memory policy, roadmap, link, terminology and generated
artifact gates pass, as does whitespace validation. macOS ASan uses
`detect_leaks=0`; Rosetta establishes functionality, not physical x86 performance.

Tests cover empty/one-sided/two-sided joins, embedded NUL/Unicode, borrowed slices
and self-concatenation through an aliased output, left/right aliased VM targets,
GC and reference invalidation, exact logical byte limits, both zero teardown
ledgers, and every observed deterministic allocation-failure index. A new source
fixture initially used unsupported string `+`; it was corrected to interpolation.
Its failure and the first harness summary-format assertion are preserved; neither
produced performance observations.

Sixteen counter-only executions (two per four workloads and two engines) agree
with the baseline's string-creation counts. `report-concat` input-copy volume
falls from 2,712,722,191 to 1,356,385,709 bytes in Native and from 2,712,752,477 to
1,356,415,995 in VM. Title-search-100k falls from 10,001,205 to 5,245,451 bytes in
Native and 10,203,859 to 5,448,105 in VM. These are summed byte-copy counts, not
peak heap sizes or acceptance timings. Small-string workloads chiefly target
the removal of the separate payload allocation.

The initial timing preflight found foreign Node work at up to 203.6% CPU and
stopped before any observation. Necessary functional validation proceeded while
timing was unavailable. Subsequent timing requires three quiet preflight samples
ten seconds apart and every-five-second rejection of known foreign compute at
>=40% CPU outside the benchmark tree. All sampled host state is retained; this
gate does not establish independent hardware isolation.

## Predeclared performance gate

Early paired measurement uses the unchanged report-concat, title-search-100k,
word-frequency and normalize-title workloads. Report-concat must improve at
least 15% Native and 8% VM+JIT; four-task geometric latency must improve at least
5% and 3% respectively, without an early 2% median or 5% P90 regression.
Full measurement uses all 32 workloads independently for Native, VM+JIT and OFF.
Native median sum must fall at least 3%; neither VM sum may increase. Each
workload exceeding 2% median or 5% P90 receives exactly one confirmation;
repeating the same metric rejects the candidate. All samples use 3 warmups and
11 fresh-process observations with alternating participants and rotating workloads.

| Early workload | Native before ms | Native candidate ms | JIT before ms | JIT candidate ms |
| --- | ---: | ---: | ---: | ---: |
| report-concat | 104.54 | 65.66 | 165.26 | 129.98 |
| title-search-100k | 79.13 | 59.66 | 502.30 | 478.16 |
| word-frequency | 145.89 | 138.83 | 267.74 | 260.00 |
| normalize-title | 57.97 | 55.54 | 140.08 | 137.71 |

| Full mode | Before median sum ms | Candidate median sum ms | Change | Confirmed regressions |
| --- | ---: | ---: | ---: | --- |
| native | 1039.27 | 941.98 | -9.36% | {"min-heap": ["median_ms"]} |
| on | 7098.95 | 7015.98 | -1.17% | {} |

The declared gate rejects this candidate. The replayable patch and all evidence
are preserved; production sources are restored before publication. No candidate
speed is substituted for current accepted-version performance.

## Rejection and post-measurement inspection

Native min-heap exceeds the 2% median gate in both its full and single
confirmation series: +3.78% and +3.49%. Native array-grow and BFS-record flags
do not repeat. Native median sum falls 9.36%, but the repeated regression rejects
the candidate. JIT median sum falls 1.17%; its queue flag does not repeat. The
remaining OFF timing was interrupted after resolving and stopping the exact
owned process tree. Its partial journal is retained without an aggregate score;
no fresh five-participant candidate comparison was run.

Two initial process lookups failed before sending any signal because macOS
reports its executable as `Python` and the orchestration used a relative script
path. The terminal record confirms every observed task process exited. The
original running-state files, stop reason and exact partial observations remain
preserved; the final rejected status is based on the completed Native gate.

Post-measurement disassembly finds identical addresses/instructions in the hot
array access/get/set/index functions. Generated workload functions retain their
entry addresses and instruction counts, with relocated callees/static data.
This does not prove the slowdown's cause. The selected follow-up is a diagnostic
packet for the exact Native controls, including array access, immediate enums
and allocation layout, before selecting a new general runtime intervention.

## Confirmed pre-existing limitation

A separate minimal probe verifies a D055 precedence gap in both the accepted
baseline and this candidate: string construction with a 1-byte heap budget and
failure of the first allocator call reports host allocation failure, although
D055 requires budget preflight before the rejected host allocation. Both tear
down to zero ledgers. String storage is staged before the existing publication
preflight; this stage retains that ordering and does not claim full D055
conformance. Correcting the owner transaction/preflight must also preserve
borrowed bytes across collection and is a separate required follow-up. The
probe source, compile commands and both exact outputs are in the raw record.
