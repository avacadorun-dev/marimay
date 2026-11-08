# Inline certified string-array projection: rejected early

> **Private experiment payloads.** Archived implementations and patches
> referenced in this historical report are retained privately. Commands
> and digests below document the original experiment; they are not public
> reproduction instructions. See the [artifact index](private-artifacts.md).

Status: **rejected at the fixed early gate**, parent `8f5dd3926fcd15dc7d164b8e8fe437d581b280a3`.
The original VM title pair takes **1.447650% longer**,
geometric median latency ratio **1.014476502028**. This misses
the minimum 3% improvement. Both individual median/P90 guards remain below 1.05;
that does not rescue the failed improvement minimum. There is no favorable rerun
or threshold adjustment. Production, normative text and architecture changes are
restored. Accepted [string publication V2](cp42-string-publication-v2-2026-09-12.md)
and its [all 32 comparison](cp42-string-publication-v2-2026-09-12-comprehensive.md)
remain current. Original Native/VM parity, complete Tier1 and Linux LSan remain open.

[Protocol, results and analyses](cp42-jit-string-array-proof-v2-2026-09-12-rejected-summary.json),
[exact sources, commands, counters and emitted bytes](private-artifacts.md#artifact-021),
[replayable candidate patch](private-artifacts.md#artifact-020).
The patch reproduces tree `55b3d0b4880b130d52507cabcf01ad22ac3b5b68` on the parent with `git apply --unidiff-zero`,
verified using private indexes. Every archived text and binary was roundtrip
verified. The original 114 source/harness hashes and frozen compilers match.

## Candidate and local checks

The rejected [C proof V1](cp42-jit-string-array-proof-2026-09-12-rejected.md) is
replayed only as candidate source, never as a performance control. Its adaptive
nonallocating whole-array proof still follows twice the length in successful
checked reads; malformed unvisited children deny reuse without an early failure.
Full parent identity/lifetime and live mutation/length remain authoritative.

The new separately compiled `vm_jit_string_array_emit.c` owner receives a narrow
saved-register description and an already checked parent/current selected Cell.
It emits valid/mutation/length guards. A miss preserves the Cell and calls the
same C proof/checked owner. A certified success follows current heap slots,
publishes the complete string VM value and binding, and jumps past the C call.
No allocation, collection, root, safepoint or source-fuel work is added to that
success. Proof construction excludes saturated epochs/generation; parent
revalidation and mutation mismatch retain their fallback. The binding stride
remains the rejected proof's 136 bytes versus the accepted 104 bytes.

The original checked C string bridge, dispatcher and runtime mutation/validation
owners are unchanged. No earlier rejected local-view, alias-publication or
checked-Cell inline implementation is included. D111/spec/architecture changes
describe the optional proof and emission ownership; all are restored on rejection.

Initial build, **18/18 focused ON tests** and all 32 ON outputs pass. Before
freezing, compile-command inspection finds that configuration preceded addition
of the new emitter's `SLJIT_HAVE_CONFIG_PRE=1` source property. The explicit
configure/build refresh applies the same allocator configuration as the existing
emitter; **18/18** and all 32 ON outputs pass again. Both compile-command states
and the refresh are preserved. **5/5 OFF focused tests**, all 32 Native/OFF outputs
and Native byte identity against accepted controls pass. All six gates pass again
on the final candidate before timing. Full 105/89 CTest, focused sanitizer/Rosetta,
full 32 timing and a fresh five-way comparison are not run after the early failure.

The actual-machine fixture compares complete destination values/bindings with
the unchanged C oracle. Besides all V1 admission/denial, child/parent identity,
mutation/growth/alias, relocation, successful/failed GC, saturation, exact fuel
and allocation-index zero-ledger cases, V2 poisons a selected slot while each
emitted proof guard is independently false. Every case must reach checked failure
without trusted dereference. Repeated certified empty/NUL/Unicode reads preserve
the complete publication. Existing call/root tests retain recursion/entry-wrap
coverage. These are focused local checks, not full platform acceptance.

## Fixed early timing

Three process warmups and eleven rotating/alternating fresh-process pairs use the
original workloads, arguments, checksums, source preparation and 64MiB heap. Quiet
preflight and five-second monitoring record no known foreign compute or monitor
gaps. This does not independently establish dedicated-host isolation. P90 is the
predeclared tenth ordered observation of eleven.

| Workload | Control median ms | Candidate median ms | Median ratio | P90 ratio |
| --- | ---: | ---: | ---: | ---: |
| title-search-10k | 55.211958 | 55.438416 | 1.004101611 | 1.004548710 |
| title-search-100k | 141.232250 | 144.757208 | 1.024958591 | 1.005738598 |

## Direct C-call and emitted-success counters

A separately declared post-rejection diagnostic uses ignored copies of the
proof/binding owners and the new emitter. A fixed BSS counter increments after
all three emitted guards, using scratch registers overwritten by the original
projection. This directly counts inline successes, rather than inferring them
from missing C calls. The code is not used for acceptance timing.

All 128 original runs pass: 32 workloads, two trials each for plain and allocator
ledger variants. Four replicas per workload have identical counters. All 64
ledger returns have zero live allocations and bytes. C calls plus emitted reads
equal V1 logical reads, and C certified plus emitted reads equal V1 certified
reads on every workload. All ten other counters match exactly, including scans,
checked reads, key resets and discarded reuse. Disabled/failed reads are zero.

| Workload | Logical reads | C calls | Emitted successes | Scans | Scanned cells | Checked reads |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| title-search-10k | 814,304 | 20,034 | 794,270 | 2 | 10,016 | 20,032 |
| title-search-100k | 8,094,304 | 200,034 | 7,894,270 | 2 | 100,016 | 200,032 |
| normalize-title | 152,618 | 132,636 | 19,982 | 2 | 8 | 132,634 |
| word-frequency | 500,090 | 400,099 | 99,991 | 1 | 4 | 400,098 |

The title100k C boundary is really reduced from 8,094,304 calls to 200,034;
7,894,270 certified reads run inline. Failure to remove the C call is therefore
refuted. This count reduction still does not meet the elapsed-time gate.

## Emitted code and changed next action

A second private diagnostic captures the original title100k run's uninstrumented
generated code at publication. All 47 code lifetimes close and the original
output passes. Function 48 has the same symbol, typed registers and operation
sequence as the accepted mapped diagnostic; code grows from 3,372 to 3,536 bytes.
The exact emitted bytes are copied into an assembler wrapper, whose text section
is verified identical before disassembly. Both generated code captures and
operation offsets are retained in the archive.

For the already-bound parent's successful ARRAY_GET_IN_BOUNDS path, accepted
emitted caller plus checked C helper executes **67 static instructions, 21 loads,
8 stores and 10 conditional branches**. The candidate's certified emitted path
has **49 instructions, 23 loads, 12 stores and 4 conditional branches**. Full
publication emits separate stores for both reference words, bytes/length,
execution, function, heap epoch, layout, type, entry epoch and both VM value words.
The following STRING_EQUAL generated span stays 588 bytes. Exact counted paths
are in the analysis; cold paths, earlier index/alias operations, later equality,
CPU cycles, data dependencies and cache effects are excluded. These instruction
counts do not establish why elapsed time failed.

The next bounded hypothesis starts from the accepted production baseline:
avoid repeatedly writing invariant destination metadata when the existing
native-entry epoch already proves that the destination binding belongs to this
execution/function/layout/lifetime. Every full reference and bytes/length update
must remain, and absence of the entry proof must keep complete original
publication. First inspect all invalidation/entry/call/GC invariants and add
actual native/C-oracle tests for both paths, exact exits/fuel and all-index
cleanup. Use fresh predeclared early/full gates; neither rejected certificate
becomes an accepted optimization through this plan. No source, workload or
symbol identity may select production behavior.

Stage began 2026-09-12T12:18:23.059861+00:00; due 2026-09-12T15:18:23.059861+00:00; evidence prepared
2026-09-12T12:48:22.098481+00:00; deadline met: **True**. Commit/push time is recorded separately.
