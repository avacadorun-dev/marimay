# Screen direct string projection from Native title arrays

Status: **diagnostic screening passed locally**, on `6f8550eab35ae07f47d5eb564ffac441039ca414`.
Production remains the [accepted string-publication V2 baseline](cp42-string-publication-v2-2026-09-12.md).
This experiment motivates a general runtime implementation; it accepts no
optimization and changes no all32, VM, Go/Rust parity or Tier1 status.

[Protocol and complete results](cp42-title-projection-probe-2026-09-12-summary.json),
[raw scripts, generated C, disassembly and logs](cp42-title-projection-probe-2026-09-12-raw.json.gz),
[generated-C change](cp42-title-projection-probe-2026-09-12-generated.patch).

## Mechanism and limits

The [prior profile](cp42-title-after-publication-2026-09-12.md) locates a dependent
array-storage → slot index → heap slot → string object → length/bytes chain.
An isolated generated-C copy converts the already completed title array into
16-byte cells containing the nonmoving string pointer, slot and generation.
The validated owning heap supplies provenance when a loaded reference is
reconstructed. It restores original complete references at every function
cleanup, including error exits. Array length, capacity, stride and heap charges
stay unchanged, with no side allocation. The generated title algorithm, its
bounds checks, comparisons, query construction and original arguments remain
intact. Pack and restore time are included in process elapsed time and separately
marked as phases 14/15. Other coarse phase markers are unchanged.

This is deliberately **not a production representation**. While the array is
packed, a diagnostic guard aborts before the collector traces any root. It does
not bypass collection or raise the original 64MiB budget. Two forced-collection
negative runs reach that guard and terminate with its exact marker. All normal
runs complete without triggering it. They do not prove that general programs
can keep this representation across collection, mutations or host boundaries.
The full runtime implementation must support those operations normally.

Separate ledger variants validate every complete child identity at conversion,
check the exact pointer/slot/generation association at restoration, and compare
a hash of the original/restored storage bytes. Each of eight successful ledger
returns asserts both zero live allocations and zero live bytes. The counts are
identical in both variants and both independent trials:

| Workload | String comparisons | Allocate / resize / deallocate calls | Packed variant conversions |
| --- | ---: | --- | --- |
| title-search-10k | 804208 | 30328 / 29 / 30328 | one pack, one restore |
| title-search-100k | 7994208 | 300328 / 36 / 300328 | one pack, one restore |

Timing binaries omit per-comparison and allocator counters. All original stdout
results match. The original corpus, frozen control binaries, diagnostic inputs
and current source identities pass before/after checks. The complete disassembly
of both title bodies is retained; measured timings, not an assumed cache-miss
rate, determine this screening result.

## Fixed screening result

One fixed series uses three warmups plus eleven alternating fresh-process pairs
for each original size, rotating workload order. Three quiet host snapshots
10 seconds apart precede the series, with process/power/thermal snapshots after
every pair. There is no detected foreign compute or gap over 15 seconds and no
replacement observation. These are two-row diagnostic timings, not the full32
Native/ON/OFF production gate. Relinking, instrumentation and temporary storage
conversion limit extrapolation to a complete compiler/runtime implementation.

| Workload | Before total ms | Projected total ms | Total ratio | Before search ms | Projected search ms | Pack / restore ms |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| title-search-10k | 6.464041 | 5.951917 | 0.920773 | 1.431000 | 0.875000 | 0.018000 / 0.017000 |
| title-search-100k | 45.078917 | 34.008834 | 0.754429 | 19.951000 | 8.395000 | 0.232000 / 0.196000 |

The pair geometric total-latency ratio is 0.833461448.
Both rows clear the predeclared total ratio ≤0.95 and search ratio ≤0.90, and the
pair clears ≤0.90. This supports spending implementation effort on eliminating
the dependent slot lookup. It does not predict VM gains or establish parity.

## Next implementation obligations

A candidate may add a descriptor-selected string storage form whose actual
stride preserves the existing 16-byte budget on supported platforms. External
`MarimayManagedReference` and `MarimayManagedCell` values must remain complete.
Construction and mutation must reject foreign, stale and wrong-kind children
before publication. Generic reads, copies, array edits and collector tracing
must reconstruct complete identity from the validated parent heap and retained
slot/generation; cached pointers must never authorize unchecked host input.
Checked VM reads must validate identity and pointer association before changing
the destination, byte binding or fuel. Native child projection must preserve
D063's exact-parent, rooting and safepoint contract. Collection, slot growth,
reuse and retirement must work without this diagnostic guard or conversion.

Before such a change, amend the relevant D060/D063 storage/projection contracts
explicitly. Keep ordinary CELL host descriptors supported. Cover storage and
mutation success/failure paths, GC and pointer identity, zero allocator ledgers,
every reachable allocation-failure index, negative and differential behavior,
Native and ON/OFF, sanitizers and local secondary architecture. Apply a fresh
SMART and the unchanged full32 acceptance gate; complete Tier1/Linux LSan and
all32 per-workload Go/Rust parity remain open.

Protocol start 2026-09-12T08:17:30.724653+00:00, due 2026-09-12T08:57:30.724653+00:00; evidence prepared
2026-09-12T08:23:10.134522+00:00, deadline met: **True**.
