# Reusing checked string destination metadata: rejected early

> **Private experiment payloads.** Archived implementations and patches
> referenced in this historical report are retained privately. Commands
> and digests below document the original experiment; they are not public
> reproduction instructions. See the [artifact index](private-artifacts.md).

Status: **rejected at the fixed early gate**, against accepted production at
parent `f148f9bdda2f7b1d089a52cb130c79a0b771159a`. The title-pair geometric median latency ratio is
**0.993818388114**, a measured **0.618161% reduction** against the minimum
3% requirement. Individual median/P90 guards remain below 1.05. There is no
favorable rerun, smaller workload or threshold adjustment. Production and its
candidate normative/architecture changes are restored.

Accepted [string publication V2](cp42-string-publication-v2-2026-09-12.md) and the
[full 32-workload comparison](cp42-string-publication-v2-2026-09-12-comprehensive.md)
remain current. Original Native/VM parity, complete Tier1 and Linux LSan are open.

[Protocol, results and analyses](cp42-jit-string-metadata-2026-09-12-rejected-summary.json),
[exact source, commands, counters and assembly](private-artifacts.md#artifact-025),
[replayable candidate patch](private-artifacts.md#artifact-024).
Applying with `git apply --unidiff-zero` on the parent reproduces tree `65e68f7396de5e395315930d812731b2a93dabaf`;
private-index replay and every archived file's roundtrip are verified. Candidate,
control binaries and all 114 original source/harness hashes stay fixed.

## Implementation and proof boundary

Only the existing `vm_jit_strings.c` production owner changes. It validates every
selected string reference through the unchanged runtime owner before any write.
Every success publishes complete reference, byte pointer/length and VM value.
When a nonzero current native-entry epoch already matches the destination
binding, execution, function, type/layout, heap epoch and entry epoch stores are
omitted. Zero or mismatch still performs original full publication. No object
proof, binding-size change, SLJIT emission change, allocation or root is added.

This reuses the same within-entry proof already consumed by generated binding
lookup. Native entry after ordinary dispatch and native call/return advance the
epoch; wrap clears both shared and per-function binding tables. Reference
overwrites invalidate affected entries. No execution/function/layout/lifetime
change or collection/host boundary may occur inside that proof interval. D111,
execution spec and ownership text explicitly document the candidate allowance;
they are restored with the rejected code. Earlier rejected certificates and
inline/local-view optimizations are absent from this candidate.

## Local correctness and setup history

The initial new test incorrectly indexed the public `void *` binding table and
failed C17 compilation. A typed fixture-local pointer fixes it; the initial
source, compiler diagnostics and failed command remain archived. Production
compiled successfully and needed no repair. Rebuild then passes **18/18 focused
ON tests** and all 32 original ON outputs. **5/5 OFF focused tests** and all 32
Native/OFF outputs pass; every Native executable is byte-identical to accepted
control. Six final architecture/memory/documentation gates pass before freeze.

An earlier setup copy also requested a nonexistent optional Native timing
template. The already written SMART and ON/OFF scripts remain intact; the setup
record documents recovery. Native byte identity is the declared control and no
Native timing script is needed or run. No build/timing preceded the fixed SMART.

The focused owner compares full destination values/bindings with an independent
copy of accepted checked-C publication. It executes actual checked/proven and
metered/unmetered JIT reads across repeated empty/NUL/Unicode values, poisoned
invalid metadata, changed entry epochs, bad child provenance/generation/slot,
wrong/null object slots, exact failure destinations/resource records, mutation,
reserve, successful/failed GC, saturated heap lifetime and real global epoch wrap.
Direct C zero-epoch calls prove first publication is not skipped. Repeated source
reads undergo ordinary/program differential fuel and every deterministic
allocation-failure index; complete teardown asserts zero allocation/byte ledgers.
Existing alias/call/root/ownership owners remain in the focused suite.

Discovered full inventories are 105 ON and 89 OFF tests. Full CTest, focused
ASan/UBSan/Rosetta, full 32 paired timing and a fresh five-way comparison are not
run after the failed early minimum. Focused local success is not full Tier1
acceptance.

## Fixed early timing

Three process warmups and eleven rotating/alternating fresh-process pairs retain
original arguments, source-run preparation, checksums and 64MiB heap. Quiet
preflight and five-second monitoring show no known foreign compute or monitor
gaps; independent dedicated-host isolation is not established. P90 is the fixed
tenth ordered observation of eleven.

| Workload | Control median ms | Candidate median ms | Median ratio | P90 ratio |
| --- | ---: | ---: | ---: | ---: |
| title-search-10k | 55.362709 | 55.391416 | 1.000518526 | 0.993085383 |
| title-search-100k | 140.944750 | 139.135459 | 0.987163119 | 0.989800963 |


## Actual metadata reuse and generated C path

Post-rejection counters in a private copy of the frozen C owner distinguish
complete publication from reused metadata. All 128 original runs pass: 32
workloads, two repeats each with plain and allocator-ledger variants. All four
replicas agree per workload; every one of 64 ledger returns has zero live
allocations and bytes. Logical read counts agree with the independently retained
earlier C-proof diagnostics on all 32 workloads. There are no failed child checks
or zero-epoch native publications in these inputs. Diagnostic time is never
acceptance evidence.

| Workload | Checked reads | Complete publications | Reused metadata |
| --- | ---: | ---: | ---: |
| title-search-10k | 814,304 | 10,224 | 804,080 |
| title-search-100k | 8,094,304 | 100,224 | 7,994,080 |
| normalize-title | 152,618 | 152,618 | 0 |
| word-frequency | 500,090 | 500,090 | 0 |


Title100k really reuses metadata on 7,994,080 of 8,094,304 reads, approximately
98.76%. Rare activation of the optimization is refuted. Normalize and word
frequency always encounter a new/mismatched entry; they receive no metadata
reuse. The source algorithms and logical reads are unchanged.

Frozen Release ARM64 assembly retains original complete child validation. Its
successful metadata-reuse C path has **49 instructions, 14 loads, 3 stores and
10 conditional branches**, compared with accepted **54 instructions, 17 loads,
8 stores and 8 conditional branches**. Exact paths and hashes are archived;
counts exclude generated callers, slow metadata publication, CPU cycles and
cache effects. They cannot establish a cause for the small timing result.

## Changed next action

The separate [inline certified projection experiment](cp42-jit-string-array-proof-v2-2026-09-12-rejected.md)
proved that child-validation and C-boundary work can be removed, but retained
expensive complete publication. This experiment proves that invariant metadata
can be reused on almost every title-search read, but retains every checked child
and C call. Neither isolated reduction reaches the fixed early minimum.

The next bounded candidate may combine the already tested complete array proof
with entry-epoch metadata reuse in the successful emitted path. It must use
accepted production as the timing control, never either rejected candidate.
Current parent identity/lifetime, valid/mutation/length guards, every changing
reference/byte store and full publication on epoch miss remain mandatory.
The C proof/checked fallback must preserve original failure/fuel behavior.
Actual machine tests must cover all proof and metadata guards together, including
GC, native calls/returns, alias overwrite, saturation and allocation teardown.
Declare fresh fixed early/full gates; no combined implementation is accepted by
this rejection. No source/workload identity may select production behavior.

Stage began 2026-09-12T12:53:36.687955+00:00; due 2026-09-12T15:53:36.687955+00:00; evidence prepared 2026-09-12T13:07:29.745959+00:00; deadline met:
**True**. Commit/push time is recorded separately.
