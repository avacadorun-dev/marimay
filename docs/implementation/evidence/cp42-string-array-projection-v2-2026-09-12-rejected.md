# String-array projection V2: rejected early

> **Private experiment payloads.** Archived implementations and patches
> referenced in this historical report are retained privately. Commands
> and digests below document the original experiment; they are not public
> reproduction instructions. See the [artifact index](private-artifacts.md).

Status: **rejected at the predeclared early gate**, against
`fd92bc8f36a063e8b18cdd871c3c5e838006eef5`. Native again meets its minimum; VM+JIT does not
meet the required 3% geometric improvement. Production and normative contracts
are restored. The [accepted string-publication V2 baseline](cp42-string-publication-v2-2026-09-12.md)
and original all32/Tier1 parity status remain unchanged. JIT remains opt-in.

[Protocol, checks and all timing results](cp42-string-array-projection-v2-2026-09-12-rejected-summary.json),
[raw scripts, sources, commands and observations](private-artifacts.md#artifact-043),
[rejected candidate patch](private-artifacts.md#artifact-042).
The patch with `git apply --unidiff-zero` on the parent reproduces tree
`b143506c3c3e9b30c95dd0351d1fc886e691a11f`; independent temporary indexes verified this identity.
All source, control/candidate binary and 114 corpus/harness hashes were checked
before and after timing. There are no replacement performance samples.

## Material change from the first rejected candidate

The [first general string-array candidate](cp42-string-array-projection-2026-09-12-rejected.md)
retained a per-element C projection call. The earlier repeated-entry profile
found 142/842 and 151/842 VM leaf samples in that bridge. V2 reuses the complete
same-stride runtime representation, normal collection and mutations, checked
shared decoder and Native projection, but adds a separate machine-code owner:
`vm_jit_string_emit.c` behind `vm_jit_string_emit.h`.

The owner emits owning-heap validity, slot bounds, string kind, generation,
non-null object and exact pointer-association checks directly. It does not
read the string object until they pass. Every failure precedes publication
and joins the caller-owned original-PC/fuel exit. Success writes complete
reference words, byte views and all existing binding execution/function,
layout/type and epoch fields. There is no runtime C call in this projection.
The coordinator passes only the compiler, verified function and target index
under a narrow documented machine-register convention. It does not absorb
this implementation or expose its mutable state.

The C projection remains an independent oracle. D111 and execution/architecture
contracts explicitly permit only a complete checked mirror, not relaxed source
semantics or unchecked pointer access. Both SLJIT-using emitter files have the
same pinned configuration. All changes are rejected and archived, not accepted
production contracts.

## Checks before timing

Focused Release selections pass **8/8 ON** and **6/6 OFF**, including the new
actual-native-entry versus C projection test. That owner compares 19 cases
across checked/proven array reads and metered/unmetered code: stale/out-of-range
identities, wrong kinds, null/foreign/unreadable object pointers, invalid heap
state, high-bit/max generations, epoch saturation, empty and embedded-NUL/UTF-8
strings, complete destination/binding contents, exact PCs and remaining fuel.
No projection allocates; both allocator and code-page ledgers are zero after
teardown. Existing string/runtime/JIT tests retain allocation-index sweeps,
GC, alias mutation and lifetime coverage.

All 32 original expected outputs pass in Native and both VM configurations.
After adding the common pinned SLJIT source configuration, a final build,
focused ON selection and all32 ON check pass before freezing. The compiler
hash remains identical, so the already checked Native snapshot remains exact.
Six architecture/memory/documentation gates and `git diff --check` pass.

No full ON/OFF CTest, ASan/UBSan, x86-64 Rosetta, full32 paired timing or fresh
five-way comparison follows early rejection. Those uncompleted gates are not
implied by focused success. Earlier candidates' failures and setup logs remain
historical evidence. A manifest records deletion of reproducible compiled files
from completed historical validation trees to free local space; logs, metadata,
source, evidence and all timing controls are retained.

## Fixed early result

One fixed series uses 3 warmups and 11 rotating/alternating fresh-process pairs
per original size. Quiet process/power/thermal snapshots precede timing and
5-second monitoring detects no known foreign compute or gap over 15 seconds.
No compilation, testing or cleanup overlaps timing. Inputs, source algorithms,
arguments, stdout and the 64MiB budget remain unchanged. These are local Apple
M2 measurements, not dedicated-host or physical secondary-platform evidence.

| Engine | Workload | Control median ms | V2 median ms | Median ratio | P90 ratio |
| --- | --- | ---: | ---: | ---: | ---: |
| native | title-search-10k | 6.347708 | 5.858916 | 0.922997 | 0.914061 |
| native | title-search-100k | 43.057792 | 34.127375 | 0.792595 | 0.758259 |
| on | title-search-10k | 55.540208 | 55.261625 | 0.994984 | 0.993294 |
| on | title-search-100k | 140.438875 | 141.641875 | 1.008566 | 1.016609 |

Native pair ratio **0.855314294**
passes the at-least-10% pair and at-least-5% each-row reductions. ON pair ratio
**1.001752044** misses the at-least-3%
reduction. Its row median/P90 no-regression guard passes, but that is not its
speedup criterion. V2 is therefore rejected without weakening either minimum.
The Native gain is not transferred into accepted all32 results.

## Changed next action

Removing the C-call boundary while retaining all projection/publication work
does not produce a useful measured gain here. This does not establish that the
call has zero cost: load scheduling, generated instruction selection and the
unchanged publication work are also present. The next hypothesis is reducing
repeated stores of invariant binding metadata within an uninterrupted native
entry, while every loaded child still passes all existing checks and publishes
its full value and bytes. Source inspection also finds an explicit slot-stride
multiplication in the new emitter; an equivalent constant shift may avoid work
when the runtime slot size is a power of two.

Before revising, prove metadata reuse against the existing entry epoch,
execution/function/layout, heap lifetime and register-write invalidation rules.
An epoch assigned by a general bind is not by itself proof that a new projection
already published the invariant fields: the focused oracle seeds old bindings
before changing heap state. A separate internal publication marker may fit the
unused tail of the existing array-sized binding union, but unchanged binding
size/charge, wrap behavior and failed-publication atomicity must be verified.
This is a proposed mechanism, not an accepted optimization or a permission to
skip child validation. A materially revised candidate needs a fresh protocol;
this failed series is never rerun to obtain favorable observations.

Original title-family closure, all32 per-workload Go/Rust parity and complete
Tier1/Linux LSan remain open. The [accepted all32 comparison](cp42-string-publication-v2-2026-09-12-comprehensive.md)
still supplies current performance numbers.
Protocol start 2026-09-12T09:05:53.513058+00:00, due 2026-09-12T12:05:53.513058+00:00; prepared
2026-09-12T09:22:51.547984+00:00, deadline met: **True**.
