# String-array projection: rejected early

> **Private experiment payloads.** Archived implementations and patches
> referenced in this historical report are retained privately. Commands
> and digests below document the original experiment; they are not public
> reproduction instructions. See the [artifact index](private-artifacts.md).

Status: **rejected at the predeclared early gate**, against
`a5edc2cd12b2145ddeaf6650f1ca77b18f7d74c2`. Native passes its minimum, but VM+JIT misses
the required 3% geometric reduction. Production code and normative contracts
are restored. The accepted baseline remains
[string-publication V2](cp42-string-publication-v2-2026-09-12.md).
No all32 or complete Tier1 parity status changes; JIT remains opt-in.

[Protocol, complete results and identities](cp42-string-array-projection-2026-09-12-rejected-summary.json),
[raw scripts, sources, commands and observations](private-artifacts.md#artifact-041),
[rejected candidate patch](private-artifacts.md#artifact-040).
Applying the patch with `git apply --unidiff-zero` to the parent reproduces tree `4674c785625788ca8e20189e8fe5f9e41bf61ac8`;
this was checked with independent temporary Git indexes. All candidate source
and binary hashes and all 114 corpus/harness hashes were checked before and
after timing. Rejection is preserved without replacement performance samples.

## Candidate

The [positive Native screen](cp42-title-projection-probe-2026-09-12.md)
motivated a general D060 string-array storage form containing an immutable
object pointer plus slot and generation. Its union retains Cell stride and
alignment without enlarging heap charges, array headers or borrowed views.
Complete logical references remain the public value/root representation.
Construction and mutation validate full input identity before encoding.
Generic reads, copies, pops and collector tracing use a shared checked decoder
that validates slot, generation, kind and object association before publishing
the reconstructed reference or bytes. Collection works normally; there is no
temporary conversion or diagnostic collection guard in this candidate.

The internal borrowed get/pop ABI receives the owning heap for reconstruction.
Native uses the existing D063 exact rooted parent, bounds and safepoint proofs
for direct immutable-byte projection. VM hot reads and the JIT bridge use the
checked decoder before publishing destination registers and byte bindings.
The program-argument constructor accepts both legacy CELL and new STRING forms.
The candidate explicitly amends D060/D063 and the architecture contract; those
amendments are archived with the patch and are not accepted normative changes.

## Checks completed before timing

The final focused Release selections pass **7/7 ON** and **6/6 OFF**, including
the new string-array owner, runtime, bytecode, string publication/lifetimes,
hot strings and JIT strings where enabled. All 32 original expected outputs
pass in Native and both VM configurations. Six architecture, memory-policy and
documentation gates pass. The new tests cover every reachable allocation
failure index in their success/forced-collection scenarios, both zero allocator
ledgers, original capacity/charge rules, empty/NUL/Unicode strings, mutations,
copy, collection, slot growth/reuse/retirement and malformed child identities.

Two setup defects and their original failing logs remain visible. An initial
test expected capacity 20 instead of the existing doubling planner's 32; only
the expectation was corrected. The first all32 run failed before the entry
body because the argv constructor still accepted only CELL descriptors. The
constructor and its success/allocation-failure tests were extended to both
layouts, then focused tests and all32 correctness passed. These corrections
preceded the source/binary freeze and all performance observations.

Full ON/OFF CTest, sanitizer/Rosetta validation, full32 paired timing and a fresh
five-way comparison were not run after early rejection. The focused results
must not be presented as those uncompleted gates. Linux LSan and complete
Tier1 evidence remain open.

## Fixed early result

One series used 3 warmups and 11 rotating/alternating fresh-process pairs per
original title size. Quiet process/power/thermal snapshots preceded the series;
5-second monitoring found no known foreign compute or gap over 15 seconds.
No builds or tests overlapped timing. This is local Apple M2 evidence, not a
dedicated-host or cross-platform performance claim. Original arguments, stdout,
algorithms, source-run boundary and 64MiB budget were unchanged.

| Engine | Workload | Control median ms | Candidate median ms | Median ratio | P90 ratio |
| --- | --- | ---: | ---: | ---: | ---: |
| native | title-search-10k | 6.533125 | 6.014667 | 0.920642 | 0.883871 |
| native | title-search-100k | 44.986000 | 35.070959 | 0.779597 | 0.740665 |
| on | title-search-10k | 56.763583 | 56.517834 | 0.995671 | 1.000862 |
| on | title-search-100k | 146.726708 | 146.404666 | 0.997805 | 1.004745 |

Native geometric latency ratio is
**0.847189260**: both row minima
(at least 5% faster) and the pair minimum (at least 10% faster) pass.
VM+JIT ratio is **0.996737336**:
the no-regression guard passes, but the at-least-3% reduction does not.
The combined candidate therefore fails. Its Native gain is not promoted into
accepted all32 results, and the VM threshold is not lowered after observation.

## Next bounded work

The data supports the Native storage mechanism and shows that this checked VM
integration produces little elapsed improvement. Source inspection shows that
the hot JIT path still calls a C bridge and the checked slot decoder for every
loaded string. Whether removing that boundary while preserving all checks
provides sufficient benefit is the next hypothesis, not a measured conclusion.
Inspect the generated sequence and profile/counter evidence before a revised
candidate. Any revision needs a fresh protocol and materially changed JIT work,
with the same failure-atomic publication, complete identity, exact fuel,
rooting/GC and malformed-input checks. Preserve this failed series and the
positive Native result; do not rerun it to obtain favorable samples.

The original title family remains active. Last accepted all32 medians and
Go/Rust ratios remain in the [V2 comparison](cp42-string-publication-v2-2026-09-12-comprehensive.md).
Protocol start 2026-09-12T08:32:30.791263+00:00, due 2026-09-12T11:32:30.791263+00:00; evidence prepared
2026-09-12T09:00:15.614578+00:00, deadline met: **True**.
