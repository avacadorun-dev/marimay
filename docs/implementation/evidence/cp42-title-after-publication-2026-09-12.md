# Title runtime costs after accepted string publication

> **Private experiment payloads.** Archived implementations and patches
> referenced in this historical report are retained privately. Commands
> and digests below document the original experiment; they are not public
> reproduction instructions. See the [artifact index](private-artifacts.md).

Status: **diagnosed locally**, on `613b70329a18e024a1a0527ef0ae6ee480b65dc8`. The
[accepted V2 comparison](cp42-string-publication-v2-2026-09-12.md) remains the
performance baseline. This record changes no production implementation, source
workload, argument, output or 64MiB execution heap. Title-family and original
all32 Go/Rust parity, complete Tier1 and Linux LSan remain open.

[Protocol and results](cp42-title-after-publication-2026-09-12-summary.json), [raw evidence](private-artifacts.md#artifact-060),
[generated-C alias experiment](private-artifacts.md#artifact-061).

## Repeated-entry profiles and cleanup

Separate diagnostic copies repeat the original entry 64 times, retaining the
original initialization, workload and teardown on every return. They reuse the
accepted coarse phase instrumentation and accumulate markers across entries.
Two trials for each size/engine in each of the ledger and sampling variants
produce 16 successful runs and 1024 exact original stdout results. Every one of
the 512 ledger-variant returns asserts zero central-allocator live allocations
and zero live bytes. Sampling variants omit allocator counters. All expected
phase-transition counts remain exact; the frozen source/corpus/binary hashes
are rechecked. These are diagnostic repetitions, not acceptance timings.

macOS `sample` records one second at one-millisecond intervals with `-mayDie`.
Repeated process state, host allocator retention, sampler startup and relinking
limit representativeness. The 100k Native sample footprints are 441.3M/421.4M;
the zero ledger results cover runtime allocations after teardown and do not
claim that process RSS or every non-allocator resource returns to zero.
Some 10k samples include dyld startup, and one Native 10k record contains only
85 application samples. These limitations are retained without a favorable
replacement profile. JIT code has unsymbolized addresses; unknown leaves are
not assigned to a semantic owner by guesswork.

| 100k engine/trial | Main application samples | Principal leaf | Leaf samples |
| --- | ---: | --- | ---: |
| sample-native-title-search-100k-0 | 844 | `mmay_g_f_a97fab987fdc9664d94fdb37dfa88596  (in native-sample)` | 326 |
| sample-native-title-search-100k-1 | 838 | `mmay_g_f_a97fab987fdc9664d94fdb37dfa88596  (in native-sample)` | 347 |
| sample-vm-title-search-100k-0 | 842 | `mmay_bytecode_jit_string_load  (in vm-sample)` | 142 |
| sample-vm-title-search-100k-1 | 842 | `mmay_bytecode_jit_string_load  (in vm-sample)` | 151 |

The Native title body accounts for 326/844 and 347/838 leaf observations;
`memcmp` accounts for only 32 and 38. The preserved disassembly maps offset
1476 to array length, 1488–1508 to array storage → slot index → slot table →
string object → length, followed by length comparison and `memcmp`.
`generated_string.h` confirms the slot-to-object-to-bytes/length projection.
This locates dependent loads; it does not prove a particular hardware-cache
miss rate or the gain of a representation change.

The VM's checked `mmay_bytecode_jit_string_load` bridge accounts for 142/842
and 151/842 leaf observations. It resolves the child string and publishes both
the managed register and its validated byte binding. Any optimization must
retain complete child validation, failure atomicity, binding epochs and exact
fuel/side exits. Source preparation also remains visible in range-analysis and
verification owners; the accepted phase record measures about 38ms separately.

## Refuted Native alias-cache hypothesis

The generated inner loop copies `av5` into `av44` before each read. Static
inspection suggested that an empty source view might repeatedly discard a
successful destination bind. A diagnostic copy returns the validated `av44`
view to same-reference `av5` after the read, while keeping the existing
safepoint invalidation. Separate counters count entry to the original bind.
Both baseline and variant have **zero binds** on both sizes in both trials.
Thus the hypothesized repeated binding does not occur in these workloads.

One fixed series of three warmups plus eleven alternating pairs uses the
original fresh-process Native commands. It provides no acceptance evidence:
only the two title rows were measured, and no full host-isolation gate was
applied. There is no favorable retry. Results are:

| Workload | Before median ms | Extra view-copy median ms | Ratio | Bind attempts before/after |
| --- | ---: | ---: | ---: | --- |
| title-search-10k | 5.978625 | 5.978750 | 1.000021 | [0, 0] / [0, 0] |
| title-search-100k | 43.817500 | 45.641208 | 1.041621 | [0, 0] / [0, 0] |

The extra view assignment is not selected for production. The first script
stopped before compilation because its function pattern omitted the existing
`MMAY_G_NOINLINE` qualifier. The next setup failed compiling the counter helper
without `<stdlib.h>`. Both scripts/logs and the failed build record are retained;
no counter or timing observation existed before those setup corrections.

The next contract audit should examine reducing the dependent array/string
projection and checked JIT publication work, using the actual load chain and
bridge above. No new storage layout, budget exception, source shortcut or
production candidate is approved by these profiles. Complete reference
identity, GC rooting/lifetime, exact charges and side exits remain obligations.

Diagnosis began 2026-09-12T07:31:42.193927+00:00, due 2026-09-12T08:01:42.193927+00:00; prepared
2026-09-12T07:49:13.186956+00:00, deadline met: **True**.
