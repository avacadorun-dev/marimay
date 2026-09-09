# Managed VM strings: report-concat lifetime regression

Local macOS arm64 evidence, 2026-09-09; parent `6cb35fa`.
[Raw reproduction, catalogue matrix, source identities and verification](cp42-vm-string-lifetime-2026-09-09.json).
This is a runtime correction under accepted D055 and execution spec §9.2;
CP42, portfolio acceptance, Tier 1 and release-gate states remain open.

The unchanged external `algorithm-benchmarks` command builds 10,000 formatted
rows and consumes all 272,015 resulting bytes. On the preceding installed
release (SHA-256 `7da536ea73bfdec7809ee6369bc4839133fb99aff4adfc167635d47287c8b0be`),
`report-concat 10000 1` exits 70 with empty stdout and generic I0002 at both
64 and 256 MiB. `report-builder 10000 1` succeeds with checksum 762303259.
The root cause is confirmed in the old `vm_value.c`: every distinct dynamic
string is strongly owned until execution teardown, and `total_string_bytes`
reaches its independent 64 MiB limit. Increasing the managed-heap option cannot
change that counter.

VM strings now use the shared managed-string runtime and precise typed register
roots. Discarded intermediate values become collectible, byte budgets apply to
the live managed graph, and cell/register transport passes validated references
without copying strings. `vm_string.c` owns creation and byte access; publication
into a target register happens only after success so an aliased source remains
rooted during GC. No runtime string pool, replacement permanent cache, increased
limit, concatenation rewrite or benchmark-source change is used. Portable
bytecode constant metadata remains separately bounded. The preview C embedding
API follows the existing managed-result/root-frame lifetime contract; its old
string-pool IDs/options are removed. The bounded MIR test oracle is unchanged.

The exact external commands now return exit 0, empty stderr and
`report-concat 762303259\n` in VM at both 64 and 256 MiB and in native at 64 MiB.
Builder remains `report-builder 762303259\n`. The checked-in
`tests/public-alpha/vm-string-lifetime` freezes the same catalogue titles,
10,000-row construction and complete-byte checksum. CTest compares VM with
native Debug/Release and checks that a 65,536-byte heap fails with classified
managed-budget I0002, empty stdout and exit 70. Two source tests compare
concat/builder at 8 MiB and retain strings through arrays, records, enums,
Map entries, nested calls, defer, return, Unicode/NUL and substring operations.

The focused C owner repeatedly rebinds concat/format registers in an 8 KiB,
16-object heap, sweeps every deterministic allocation index with automatic and
forced collection, and checks result nonpublication on failure and both zero
allocator counters after full teardown. Host checks retain a root across calls
and collection, reject a cross-heap same-slot reference, and reject a stale
reference after slot reuse. A live graph exceeding its budget still fails.
The existing CP33 text allocation sweep now forces GC before each allocation,
covering empty strings, slices, split/graphemes, joins and conversions.

Validation: full Release suite in `build/string-lifetime` passed **86/86**
(277.25 s).
Architecture, C memory policy, generated artifacts and roadmap checks pass.
System `/opt/homebrew/bin/mmay` resolves to the rebuilt `build/release/mmay`;
its exact concat and builder commands both pass at 64 MiB.
Focused macOS ASan/UBSan passed **5/5** (85.02 s): bytecode, CP33 text, the new
lifetime process fixture and both stateful applications. Linux LeakSanitizer
and other Tier 1 results remain CI/external obligations.

The independent catalogue smoke report passed **21 workloads × 4 engines ×
3 samples = 252 executions**: Marimay VM/native, Go and Rust. Smoke uses its
published reduced inputs and verifies checksums; the full-size report regression
above is separate. Raw report metadata preserves the compiler hash, source and
workload hashes, all samples and the `local-unverified` environment. These runs
establish correctness only. No 11-sample full comparative run or parity claim
is made; immutable repeated concat still copies growing prefixes.

This fix follows local ownership/allocator traces and the existing managed-memory
contract. No new idea from Vladimirov's book was used or attributed to unread
pages. The preceding RPO use remains documented at printed 145–147 / PDF 146–148
in the research journal.
