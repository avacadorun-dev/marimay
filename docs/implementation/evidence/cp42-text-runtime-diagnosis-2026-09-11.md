# Text runtime diagnosis and accepted-version snapshot

Status: **diagnosis complete; no production change**. Implementation remains
`f4c9961bc807277c1aa3f34a1d5dc1ff7497c6f2`; its documentation parent is
`a6dd1366b4fc05c89db8eff7e1beab3138ac460c`. The rejected reachable-preparation
candidate is excluded. Overall parity, complete Tier 1 results and Linux
LeakSanitizer remain open. JIT remains opt-in.

[Fresh five-participant all32 table](cp42-text-runtime-diagnosis-2026-09-11-comprehensive.md),
[all samples and pinned inputs](cp42-text-runtime-diagnosis-2026-09-11-comprehensive.json),
[diagnostic summary](cp42-text-runtime-diagnosis-2026-09-11-summary.json),
[complete compressed diagnostic record](cp42-text-runtime-diagnosis-2026-09-11-raw.json.gz).
The summary supplies compressed and decoded SHA-256. Decode the gzip as JSON;
its `files` map preserves raw profiles, scripts, generated diagnostic C, counter
build commands/results, host snapshots, exact stdout and instrumented binary
identities. Diagnostic scripts retain their original absolute workspace paths.

## Accepted-version timing boundary

The fresh snapshot uses 3 process warmups and 11 fresh-process samples for every
participant and all 32 unchanged workloads, with the original 64 MiB managed
heap and all 114 source/harness hashes verified. Its accepted compiler SHA-256
is `53c2c1396bb4b206465a2a48bda03385d7bea6f6ed72d36ae230478187f288a5`. Native, Go, Rust and Bun executable identities
match the earlier accepted reference: **True**.
This is a current local snapshot, not an optimization before/after experiment.

Native release timings exclude compilation and include process startup, input
generation, output and teardown. VM+JIT source runs include loading, checking,
lowering and JIT preparation. Bun includes TypeScript loading and JIT in each
fresh process. Three quiet preflight snapshots precede timing; every-five-second
monitoring found no foreign compiler/test/profiler/Node/Python process at >=40%
CPU outside the benchmark tree. This sampled gate is not independently verified
hardware isolation. Raw dispersion flags remain in the full table.

## Bounded diagnosis

The 15-minute diagnosis starts at 09:38:01 UTC and selects its intervention at
09:49:51 UTC, before its 09:53:01 deadline. Three sampled original executions
for each of four text workloads and two engines give 24 correct outputs.
Some Native processes are too short for useful attached samples. Consequently,
three additional Native processes per workload repeat 16 complete generated
entries, retaining heap initialization/teardown and each original checksum:
192 correct complete entries. These repeated entries are diagnostic only, and
are not added to the acceptance corpus or reported as performance improvements.

Two independent fresh counter-only replays per workload/engine (16 runs) agree
exactly. Instrumentation changes only diagnostic copies of the string owners;
production and accepted binaries are unchanged. The VM diagnostic declaration
uses Clang's `__SIZE_TYPE__`; these local probes do not establish portable C17
or Tier 1 validation. The prospective production change must use portable APIs.

| Workload | Native string creations | VM string creations | Concatenations per engine | Temporary concat bytes per engine |
| --- | ---: | ---: | ---: | ---: |
| word-frequency | 400,100 | 500,122 | 2 | 37 |
| title-search-100k | 300,310 | 400,556 | 200,194 | 4,755,754 |
| normalize-title | 152,632 | 162,656 | 2 | 38 |
| report-concat | 80,022 | 110,045 | 60,002 | 1,356,336,482 |

`report-concat` writes 1,356,336,482 bytes into temporary concatenation buffers
and then copies those bytes again into managed strings, in both engines.
Repeated Native profiles attribute 2060/2560 exclusive samples to `memmove`;
fresh VM profiles attribute 162/251. These counts support a copy-volume target,
not an exact end-to-end CPU percentage or a promised latency reduction.
Nonempty string construction also allocates its header and payload separately.

Native title search repeatedly resolves checked string bytes (1057/2529
exclusive repeated-entry samples). Word frequency also spends samples in
comparison/delimiter scanning; normalization shows allocation and Unicode work.
Anonymous JIT frames remain unattributed, and one repeated word-frequency label
resolves to `_dyld_start`. The raw profiles retain these attribution limitations.

## Selected next intervention

Test one-allocation string storage and direct concatenation in the shared
runtime owner; route VM concatenation through that same checked owner. Preserve
string/reference representation, empty-string behavior, logical managed-byte
accounting and the numeric heap cap. Keep validation/error ordering, root and
provenance checks, transactional result publication and aliased inputs. Cover
all reachable deterministic allocation failures and both zero teardown ledgers.
Before implementation record numerical targets, the unchanged all32 regression
gates, host-load gate, correctness/platform checks and a publication deadline.
This record selects the experiment; it does not implement or accept it.

## Diagnostic suite to shorten future iterations

Keep the fixed32 comparison as the outcome gate. Reuse existing array access,
growth/reserve and proven/unproven-bounds probes. Add small independent probes
only where profiles lack attribution, with checksummed outputs and parameters
that prevent constant folding. Proposed additions are not implemented here:

| Focus | Cases | Decisive metrics |
| --- | --- | --- |
| Source preparation | tiny entry; many unused functions; repeated source runs | load/check/MIR/lower/codec/admit/JIT/execute phase times; cold/warm boundary |
| Scalar JIT and calls | arithmetic loop; direct and interface calls; isolated cold operation | compiled operations, exits by opcode/reason, transitions per loop, spills |
| Strings | create; equal/unequal; concatenate; split; ASCII/Unicode | allocations, allocated/copied bytes, checked byte resolutions, time per operation |
| Collections | scalar/string keys; hit/miss; growth/churn | hash/probe counts, comparisons, resize volume, checked-view misses |
| Managed memory | short-lived strings/records; retained references | GC count/time, bytes traced, root/validation work, peak managed bytes |

Use separate builds/runs for profiling/counters and acceptance timing. Select
one hypothesis from a profile, run the smallest discriminating probe and an
original target workload first, then run the full correctness and all32 gates
only for a promising candidate. Pin revisions, compiler/runtime hashes, source
hashes, commands, stdout, raw order/samples and host state automatically. Keep
failed experiments and set the target and stop condition before each candidate.
A single repeatable diagnostic command assembling these existing tools and
missing counters is preferable to multiplying broad end-to-end workloads.
