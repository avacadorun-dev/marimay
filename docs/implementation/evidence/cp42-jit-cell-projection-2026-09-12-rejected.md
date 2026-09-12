# Fully checked original-Cell string projection: rejected early

Status: **rejected at the predeclared early gate**, against
`33b9e8cf1b20dfd5a89c0b7ff72992b7d5a465c4`. The unchanged VM title pair improves by
**2.353783%**, below the fixed **3%** minimum.
Its exact geometric latency ratio is **0.976462165073**.
There is no favorable rerun, resampling or threshold adjustment. Production and
normative contracts are restored; accepted
[string publication V2](cp42-string-publication-v2-2026-09-12.md) remains current.
Original all32 Native/VM parity and complete Tier1 remain open. JIT stays opt-in.

[Protocol and results](cp42-jit-cell-projection-2026-09-12-rejected-summary.json),
[raw sources, commands and observations](cp42-jit-cell-projection-2026-09-12-rejected-raw.json.gz),
[exact candidate patch](cp42-jit-cell-projection-2026-09-12-rejected-candidate.patch).
Private candidate/replay indexes prove applying the patch with
`git apply --unidiff-zero` on the parent reproduces tree `15991b0bd84a64eff97a37d7a72785ecd414955a`.
All candidate, accepted controls and 114 source/harness hashes were verified.

## Candidate and scope

This candidate combines archived local-view/boolean/alias-cache V2 with a new
independently compiled `vm_jit_string_emit.c` owner. The previous V2 remains
rejected and is not a performance control. No rejected direct-STRING runtime
representation is included. Array storage remains the original Cell containing
the complete managed reference; Native code generation is unchanged.

The new emitter mirrors the unchanged C string-load bridge and runtime
string_bytes/resolve_const predicate. Ten failure edges check execution presence,
heap initialization and nonzero/nonmax provenance, the Cell's own heap provenance,
slot sentinel/range, exact string kind, generation and nonnull authoritative
object. Slot indexing follows the range checks, and the object is dereferenced
only after its kind and pointer pass. All destination and binding stores follow
all successful guards. Success publishes both reference words and every original
byte-view, execution/function, heap epoch, layout/type and entry epoch field.

The narrow owner API documents saved/scratch machine registers and returns a
success jump to the coordinator's original-PC/fuel guard. It consumes the
existing compiler layout contract in generated_slots.h, without exposing a
runtime-private umbrella or creating heap state. C `vm_jit_strings.c` remains
unchanged as the independent oracle. Parent binding, checked/proven indexing,
GC roots, mutation, limits, exact fuel and failure publication remain intact.

## Checks

Initial build and **21/21 ON** focused tests pass without repair; **5/5 OFF**
tests also pass. Original stdout for all **32 workloads** passes in Native and
both VM modes. Every Native workload executable is **byte-identical to the
accepted control**. Six architecture/memory/documentation gates and
`git diff --check` pass. No Native speedup is claimed.

The new projection owner compares **27 cases in four configurations**, checked
or proven access and metered or unmetered code, with the unchanged C oracle.
Real machine-code execution covers nonempty embedded-zero Unicode, empty strings,
foreign/zero/MAX provenance, invalid/MAX/out-of-range slots, wrong/zero/high-bit/
MAX generations, all nonstring object kinds and an unknown tag, null objects,
uninitialized/invalid heaps, collecting state and saturated view epochs. The
oracle does not reject collecting or saturation, and the emitter preserves
that behavior. A separately checked parent determines exact proven-prefix
consumption. Every target binding and VM-value byte matches the oracle, including
unchanged old destination on failure; original PC and fuel match as well.
Projection performs no allocation. Synthetic heap/slot states are restored
before teardown; allocator and code mapping/byte ledgers reach zero.

Existing focused string, local-view, alias, call/root, epoch, bounds, ownership
and fuel owners retain collection, overwrite, exact exits and deterministic
allocation/platform failure-index coverage for the changed emission. Full
ON/OFF CTest, ASan/UBSan, Rosetta, full32 paired timing and a fresh five-way
comparison are **not run after early rejection**. Local focused success is not
complete Tier1 or Linux LSan evidence.

## Fixed early series

Three warmups and eleven rotating/alternating fresh-process pairs preserve the
original workload sources, arguments, stdout, source-run preparation and 64MiB
limit. Quiet host preflight and five-second monitoring report no known foreign
compute and no observation gap over 15 seconds. Builds/tests/profiles do not
overlap measurement. These are local Apple M2 observations, without independently
verified host isolation.

| Workload | Control median ms | Candidate median ms | Median ratio | P90 ratio |
| --- | ---: | ---: | ---: | ---: |
| title-search-10k | 54.842000 | 54.052583 | 0.985605612 | 0.983753265 |
| title-search-100k | 140.345000 | 135.770250 | 0.967403541 | 0.968531212 |

Both row no-regression guards pass; the pair fails its 3% minimum and the
separate 10% aspiration. Different candidates' earlier unpaired durations cannot
isolate this emitter's cost, and are not used to assert that inlining itself
regresses or improves performance.

## Changed next action

The complete predicate and publication still perform work on every string read.
Inlining their C call together with prior local optimizations has not met the
fixed acceptance threshold. The next investigation should measure how often
the same original string array is searched while its complete identity, live
generation and heap lifetime epoch remain unchanged.

A possible material optimization is reuse of a complete child-validation proof
for a stable array. Before implementing it, count stable reuse windows in the
accepted execution, identify every mutation/GC/provenance boundary, and establish
the full proof and fallback contract. Bulk validation must be nonallocating and
speculative: a malformed unvisited element cannot cause an earlier observable
failure. Inability to certify must retain the original per-element checked path.
Array writes/growth, collection, new heaps and changed identities must invalidate
the proof before use. Whether reuse across independent native entries is valid
requires explicit lifetime/host contract review and negative tests; a cached
pointer alone is insufficient. No such certificate or semantic allowance is
implemented or accepted by this rejected candidate.

Declare a fresh protocol for that investigation and any later candidate. The
[accepted all32 comparison](cp42-string-publication-v2-2026-09-12-comprehensive.md)
continues to supply Go/Rust figures. Stage began 2026-09-12T11:00:30.199335+00:00, due
2026-09-12T14:00:30.199335+00:00; evidence prepared 2026-09-12T11:10:09.847764+00:00, deadline met:
**True**. Commit/push time is recorded separately.
