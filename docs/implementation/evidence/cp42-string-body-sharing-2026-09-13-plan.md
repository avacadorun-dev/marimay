# Shared immutable bodies with fresh string references: experiment protocol

Status: **rejected at the early gate**; [outcome](cp42-string-body-sharing-2026-09-13-rejected.md).
The original predeclared protocol follows; it is not a parity result. Parent
`451d98c33e049391b219986bf515154bf7b47e6b` retains production implementation
`613b70329a18e024a1a0527ef0ae6ee480b65dc8`. The checked-search candidate remains
rejected and is not included. This stage began 2026-09-13T16:30:46Z and is due
2026-09-13T20:30:46Z. The machine-readable protocol is frozen in the local stage
`build/string-body-sharing/SMART.json` before implementation or candidate timing.

The active family remains title-search-10k/title-search-100k. The
[preceding census](cp42-jit-string-search-loop-2026-09-13-rejected.md) identifies
repeated short string content during title creation and later text workloads.
It motivates eliminating repeated physical body allocation in the shared
runtime; its counts are not predicted elapsed-time improvements.

The candidate keeps a fresh logical slot/reference and the original complete
header/payload/slot-table charge for every successful construction. A fixed
256-entry weak per-heap content cache may share immutable physical bodies for
strings of at most 64 bytes, after full identity and exact content checks.
Private physical-owner counts preserve storage through deferred collection and
failed publication. Cache entries are never roots or storage owners. Source
algorithms, compiler verification, managed limits and generated string/slot
layouts remain contracts; this does not introduce a prepared-source cache.

## Fixed admission and validation

Pin the original114 corpus/harness hashes and all accepted control binaries.
Before early timing, require focused ownership/publication/string/GC tests and
all32 original Native, VM ON and VM OFF results. Every reachable allocation
failure index must finish with zero live allocations and live bytes after full
teardown. Preserve cold slot-growth allocation coverage with distinct contents,
and separately add cache-hit growth failures. Existing source differential and
formatter fixtures remain applicable; no public syntax is introduced.

Early Native and source-run VM+JIT title-pair geometric latency ratios require
**at least one engine at or below 0.97 and the other at or below 1.00**. Neither
row median may exceed 1.05. This allows a useful engine-specific improvement,
as earlier JIT candidates allowed unchanged Native executables, without changing
the final two-engine objective. Record early P90 values; the full gate below
decides tail admission. A failed early series has no favorable retry.

Promising candidates require full CTest ON/OFF, focused ASan/UBSan and actual
x86-64 Rosetta execution, all32 differential results, all memory-failure checks,
and architecture, C memory policy, roadmap, links, spec terms, generated and
diff gates before full timing. macOS tests do not close Linux LSan or complete
Tier1 requirements.

Full32 summed medians must not regress for Native or ON; OFF permits 1%.
Every row median/P90 above 1.05 receives **exactly one 51-sample confirmation
after three warmups**. Recurrence of that same metric above 1.05 rejects.
This larger, predeclared confirmation estimates P90 with more observations;
it neither changes the 5% allowance nor revisits any rejected result. No third
series, favorable resampling, benchmark edit or aggregate-only parity claim is
permitted.

Ordinary series retain three warmups and eleven fresh-process samples, rotating
rows and alternating participants. The heap is 67,108,864 bytes. Source loading,
checking, lowering and JIT preparation remain in VM timings. Complete builds,
tests and profiling before isolated timing; use three quiet host snapshots and
five-second monitoring, rejecting disallowed foreign compute or observation
gaps. Preserve all raw samples, failed setups, repairs and frozen hashes.

Accept only after a fresh unchanged five-participant all32 comparison and the
required local gates; otherwise archive the exact rejected candidate and restore
production. Commit/fast-forward/push the result while preserving main's foreign
work. The final goal remains per-workload Native and VM Go/Rust parity with
complete platform evidence.
