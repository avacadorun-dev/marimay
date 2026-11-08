# String-array projection V3: rejected early

> **Private experiment payloads.** Archived implementations and patches
> referenced in this historical report are retained privately. Commands
> and digests below document the original experiment; they are not public
> reproduction instructions. See the [artifact index](private-artifacts.md).

Status: **rejected at the predeclared early gate**, against
`8f4c00e45f947212c1430756b7b25b3d7bcdbb42`. VM+JIT misses the required 3% geometric
improvement. Native passes its pair minimum but misses the individual 10k-row
minimum. Production code and normative contracts are restored; the
[accepted string-publication V2 baseline](cp42-string-publication-v2-2026-09-12.md)
remains current. No original all32 or complete Tier1 parity status changes.
JIT remains opt-in.

[Protocol, complete checks and results](cp42-string-array-projection-v3-2026-09-12-rejected-summary.json),
[raw scripts, sources, commands, counters and observations](private-artifacts.md#artifact-045),
[rejected candidate patch](private-artifacts.md#artifact-044).
The patch with `git apply --unidiff-zero` on the parent reproduces tree
`56108db554d4585f57a5a53ea148300b8444a153`. Independent temporary indexes verified this identity; source,
candidate/control binary and all 114 corpus/harness hashes were checked.
No failed series is replaced or reinterpreted as accepted.

## Material change from V2

The [direct checked projection in V2](cp42-string-array-projection-v2-2026-09-12-rejected.md)
still wrote invariant binding metadata on every string load. V3 retains the
same general STRING runtime, normal GC/mutations, Native projection and all
per-element checked JIT predicates, then changes only invariant publication
and slot address formation.

A projection-only publication epoch occupies the previously unused tail of
the existing array-sized binding union. The measured layout remains binding
104 bytes, view 64, execution-field offset 72 and alignment 8. A compile-time
assertion ensures the union stays array-sized on each configured target; the
local observation is not a substitute for complete Tier1 checks. Generic string
binding misses clear the marker. Reuse requires marker and binding epoch to
match the current entry and heap lifetime epoch to match the binding. Entry,
call/return and wrap owners retain their existing invalidation rules. A COPY
invalidation clears the binding epoch and therefore forces header refresh.

Every loaded child still passes heap validity, slot bounds, string kind,
generation, non-null object and exact pointer-association checks before any
publication. Reference words, byte pointer/length and the VM register are always
updated. Only previously established invariant header values may be retained.
No failed check changes the marker, destination or byte binding. The independent
C projection continues full publication and supplies the exact-content oracle.
An exact shift replaces slot-size multiplication only for a compile-time
power-of-two size; multiplication remains the fallback. No source arithmetic,
array stride, object charge, allocation or heap-limit rule changes.

## Checks and setup failures

Final focused Release passes **8/8 ON** and **6/6 OFF**. All 32 original
expected stdout results pass in Native and both VM configurations. Six
architecture/memory/documentation gates and `git diff --check` pass. The native/C
oracle retains 19 prior cases and adds 9 repetitions across checked/proven and
metered/unmetered execution: actual header reuse, malformed pointers after a
successful reuse, generic binding misses, overwritten targets, all marker/entry/
heap-epoch misses, saturation, exact PCs/fuel and full destination/binding bytes.
Shared and optional unmetered function-table markers clear through the existing
wrap owner. Allocator and page ledgers reach zero after teardown; earlier string,
GC and allocation-index tests remain in the focused selection.

The first build fails because the new test indexes an opaque void pointer
without the required typed conversion. The next focused run reaches a mistaken
test assertion that metered execution allocates a per-function table. The test
now explicitly verifies the real metered/shared and unmetered/function layouts
and exercises both allocated domains. Original failures remain in separate
logs; no child-check or oracle case was removed. The complete corrected
preparation passes before counters and timing.

No full ON/OFF CTest, sanitizers, Rosetta, full32 paired timing or fresh five-way
comparison follows early rejection. Those checks are not implied by focused
success, and complete Tier1/Linux LSan remain open.

## Counter-only mechanism evidence

Separate diagnostic copies link current support/runtime code with either the
V2 always-published emitter or V3 retained-header emitter. Two independent
processes per original size and variant produce exact stdout. Instrumentation
counts successful compiled string-array reads and invariant-header publication,
not every possible cold string operation. These are not performance controls;
no instrumented elapsed time is used as acceptance evidence.

| Workload | JIT reads, each variant/trial | Header publications before | Header publications after |
| --- | ---: | ---: | ---: |
| title-search-10k | 814304 | 814304 | 10224 |
| title-search-100k | 8094304 | 8094304 | 100224 |

Both trials agree exactly. More than 98% of complete header publications are
removed while successful compiled reads and original results remain unchanged.
Counter sources, compile/archive/link commands and binary identities are retained
in the raw archive. This proves removal of the selected work, not an elapsed
speedup or absence of all memory errors.

## Fixed early measurement

One fixed series uses 3 warmups and 11 rotating/alternating fresh-process pairs
per original title size. Quiet process/power/thermal snapshots precede timing;
5-second monitoring detects no known foreign compute or gap over 15 seconds.
No compilation, tests, counters or profiling overlap timing. Original arguments,
source algorithms, stdout, source-run boundary and 64MiB budget are unchanged.
These are local Apple M2 observations.

| Engine | Workload | Control median ms | V3 median ms | Median ratio | P90 ratio |
| --- | --- | ---: | ---: | ---: | ---: |
| native | title-search-10k | 6.112625 | 5.867250 | 0.959858 | 0.920281 |
| native | title-search-100k | 42.278500 | 33.620958 | 0.795226 | 0.796616 |
| on | title-search-10k | 55.025584 | 55.173417 | 1.002687 | 0.992616 |
| on | title-search-100k | 140.668458 | 140.669167 | 1.000005 | 0.989092 |

Native pair ratio **0.873672522**
passes the 10% pair minimum, but 10k ratio
**0.959857672** misses
the required 5% row reduction. All twelve Native executable hashes are identical
to V2; the difference between those series is not attributed to a new Native
implementation. ON pair ratio
**1.001344934** misses its 3% speedup
minimum. The row median/P90 no-regression guard does not replace those minima.
No favorable retry or threshold adjustment follows.

## Changed next action

Removing almost all invariant-header publication does not produce a useful
measured VM gain in this implementation. The marker and extra guards therefore
remain rejected rather than becoming permanent complexity. Do not make another
binding-layout tweak solely from store counts. The next bounded diagnosis maps
anonymous JIT profile frames to emitted code ranges and original image
instructions on the accepted baseline. Capture code/lifetime identity before
assigning unknown samples; ambiguous ranges stay unattributed.

Only then select a measured instruction family. A possible future mechanism is
avoiding immediate scalar reloads from VM registers after storing the same value
in a still-live machine register, while preserving all existing VM register
stores and original-PC/fuel states. That needs explicit scratch-register
clobber, branch/entry and call invalidation proofs; it is not implemented or
selected by these counters. Source preparation remains an independent measured
cost, and any prepared artifact must retain the plan's cold/warm, dependency,
compiler/options and corruption checks. The full original objective remains.

The [accepted all32 comparison](cp42-string-publication-v2-2026-09-12-comprehensive.md)
continues to supply current Go/Rust ratios. Protocol start
2026-09-12T09:28:12.422788+00:00, due 2026-09-12T12:28:12.422788+00:00; evidence prepared
2026-09-12T09:46:39.799454+00:00, deadline met: **True**.
