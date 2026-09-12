# Alias-aware JIT destination cache invalidation: V2 rejected early

Status: **rejected at the predeclared early gate**, against
`e6ad8eae137118e7d7bc2b9561be274f2eeee093`. The original VM title pair improves by
**2.995623%**, below the fixed **3%** minimum.
The exact ratio is **0.970043769715**, greater than 0.97.
Rounding the displayed improvement does not satisfy the gate. No resampling,
confirmation, relaxed threshold or favorable rerun follows this failed minimum.
Production and normative contracts are restored. The accepted
[string publication V2](cp42-string-publication-v2-2026-09-12.md) remains current;
original all32 Native/VM parity and complete Tier1 remain open. JIT stays opt-in.

[Protocol and results](cp42-jit-local-views-v2-2026-09-12-rejected-summary.json),
[raw sources, commands and observations](cp42-jit-local-views-v2-2026-09-12-rejected-raw.json.gz),
[exact rejected patch](cp42-jit-local-views-v2-2026-09-12-rejected-candidate.patch).
The private candidate/replay index proves patch application with
`git apply --unidiff-zero` reproduces tree `b110956c73e437a7d194c2e38ca085b91f491130` on the parent.
All candidate, accepted-control and 114 source/harness hashes were verified.

## Material V2 change

V2 retains the rejected V1 emission-local successful binding facts and adjacent
normalized boolean forwarding, then changes destination cache invalidation for
exact array/record aliases. V1 is not accepted or used as a performance control.
The accepted [mapped title profile](cp42-title-jit-map-2026-09-12.md) shows full
reference COPY followed by loads comparing the destination's old cached identity,
although subsequent local accesses canonicalize that destination to another
register. V2 replaces that identity comparison with a direct entry-epoch zero
store only when the existing alias owner establishes a different canonical
register. Strings and self-canonical destinations retain identity comparisons.

The emitter separates write-proof invalidation from binding-cache emission and
preserves the existing alias-write-before-alias-copy order, including when the
source aliases the destination. All original full VM-reference stores, local
facts, saved-record values and dependent-alias invalidation remain. The existing
exact type/layout eligibility is not duplicated. Independent entries retain the
complete retained-view predicate and checked binding miss; no lifetime, bounds,
heap representation, allocation, source syntax or Native codegen change is made.

## Checks and repairs

Focused Release tests pass **20/20 ON** and **5/5 OFF**. Original stdout passes
for all **32 workloads** in Native and both VM modes. Every Native workload
executable is **byte-identical to the accepted control**. Six architecture,
memory and documentation gates plus `git diff --check` pass. This VM-only
candidate makes no Native speedup claim.

The new alias-cache owner seeds valid same-reference bindings at entry epoch 17
and executes real machine code. It proves the noncanonical destination epoch
cleared, canonical source retained and complete reference values materialized.
Array, nominal record and string fixtures cover self-copy, reverse alias and
canonical overwrite. Real cold ARRAY_NEW with and without forced GC is followed
by independent-entry access at epoch 18, proving revalidation before use. Existing
alias analysis rejects incompatible nominal/layout/type pairs. Finite, shared
and mixed fuel boundaries and original trap spans compare against ordinary
execution. Every reachable tested allocation-failure index for metered and
unmetered runs reaches zero live allocations and live bytes after teardown;
direct machine-code teardown also reaches zero code mappings and bytes.
Existing local-view, boolean, bounds, root/call, epoch and ownership checks pass.

A negative mechanism control links the new seeded-cache assertion against the
archived V1 implementation. It fails as expected because the old identity
comparison retains the same-reference destination epoch. Its source snapshot,
command, library/executable hashes and exact diagnostic are retained. This
deliberate failure is neither speed evidence nor a cleanup-success claim.

Two test-construction defects were corrected before timing: initial compilation
used a nonexistent managed-cell field; the corrected positive integer cell uses
the existing f64_bits payload. The second focused run passed 19/20 but the string
fixture returned zero because CONST_I64 populated only unsigned_value in an
operand struct with a separate signed_value. The fixture now initializes its
signed constant. Production and expected results did not change for either
repair. Initial/second command logs, diagnostic run and source snapshot remain
archived. Full ON/OFF CTest, ASan/UBSan, Rosetta, full32 paired timing and fresh
five-way comparison are **not run after early rejection**. Focused macOS success
does not prove complete Tier1 or Linux LSan.

## Fixed early series

Each original title workload uses three warmups and eleven rotating/alternating
fresh-process pairs. The original arguments, stdout, source-run preparation and
64MiB limit are unchanged. Quiet preflight and five-second monitoring record no
known foreign compute or observation gap over 15 seconds. No build, test or
profile overlaps timing. These are local Apple M2 observations; host isolation
is not independently verified.

| Workload | Control median ms | Candidate median ms | Median ratio | P90 ratio |
| --- | ---: | ---: | ---: | ---: |
| title-search-10k | 56.367208 | 55.319209 | 0.981407648 | 0.989161351 |
| title-search-100k | 143.527042 | 137.615375 | 0.958811476 | 0.963684499 |

Both row no-regression guards pass. The pair misses its minimum by
**0.004377 percentage points**; this remains a
failed gate. The separate 10% aspiration is also unmet.

## Changed next action

The next material hypothesis is complete checked projection of the **original
Cell string reference** directly in the JIT. The mapped accepted profile still
attributes 138/152 of 843/842 main-thread leaves in the two 100k samples to the
checked C string-load bridge. Removing only repeated bindings, boolean reloads
and copied-alias identity comparisons has not met the fixed pair minimum.

A new candidate must preserve every existing original-cell kind, provenance,
slot, generation, object and string check before transactional full-reference
and immutable-byte-view publication, including cold original-PC/fuel recovery.
It must use narrow runtime projection contracts and a focused independently
compiled emitter owner, with actual-machine cases compared to the unchanged C
oracle and full allocation/GC/epoch proofs. Rejected direct-STRING runtime
representations and their prior Native/VM timings cannot serve as proof for
this different original-Cell mechanism. Declare a fresh fixed protocol before
that changed candidate; this V2 series remains rejected.

The [accepted all32 comparison](cp42-string-publication-v2-2026-09-12-comprehensive.md)
continues to supply Go/Rust figures. Stage began 2026-09-12T10:42:54.219529+00:00, due
2026-09-12T13:42:54.219529+00:00; evidence prepared 2026-09-12T10:56:35.542687+00:00, deadline met:
**True**. Actual commit/push time is recorded separately.
