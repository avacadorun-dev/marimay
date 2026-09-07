# CP42: reuse scalar lookups and isolate the bounded probe

Local macOS arm64 / Apple M2 / AppleClang 21 follow-up to `655f185`.
This is partial collection optimization, not closure of CP42, Set performance,
Go/Rust parity, original-runner failures, or any Tier 1/alpha gate.

## Diagnosis and change

The unchanged rune workload decodes its 15-rune pattern once, then prepares
450,000 array elements and counts them with `get` followed by `put`. It is
not repeatedly decoding UTF-8 in its count loop. An eight-second sampled
profile of 100 complete baseline entries, each with separate heap teardown,
attributes 1,831 of 2,303 main-body samples to counting; the remainder is
mostly input preparation. Top-of-stack observations include 666 map-find,
368 fixed-word SipHash and 193 map-lookup samples. Every repetition produced
the exact checksum, exit 0 and empty stderr. Sampling is call attribution,
not a precise CPU-time decomposition or a latency result.

The common scalar count path hashes and probes the same key twice. The map
object now remembers the last successful scalar-key `get`, including absence,
so a following access can reuse its hash and bucket/entry indices. The existing
[D079](../../design/decisions.md#d079-ordered-map-set-and-checked-hash-key-coherence)
and [collection contract](../../spec/standard-library.md) are unchanged:

- Every use resolves the receiver first, retaining heap, slot, generation and
  kind checks; exact key bits, mutation generation and both seed words must
  match. Mutation generations do not wrap. Storage publication explicitly
  invalidates the memo, including compaction before logical mutation commit.
- The memo retains no value, managed reference or storage pointer. Reference
  values are still checked on `put` and read from the current entry on `get`.
  Reference keys and Set never populate it. There are no new heap allocations.
- Logical reads do not increment mutation generations or invalidate iterators.
  The heap-owned object is mutable; [execution §7.9/§8.1](../../spec/execution.md)
  permits only one executing task per isolate and forbids shared mutable heap
  references across isolates. No process-global state is introduced.
- The internal generated Map/Set layout grows from 64 to 128 bytes on this
  arm64 host: 64 bytes of memo even for Set/reference-key maps that do not use
  it. This fixed per-object memory tradeoff is charged by existing `sizeof`
  allocation/live-byte accounting. Entry size remains 48 bytes. Source/MIR/
  bytecode contracts and generated function signatures do not change; this is
  not a claim that the internal object layout stays binary-compatible.

`managed_map_probe.c` owns the allocation-free bounded bucket walk behind a
narrow private header. Scalar equality finishes there; reference keys yield
hash candidates to the existing recursive equality owner and resume the same
walk. Full-hash collisions, first-tombstone choice, wraparound, bounds and
entry integrity checks remain. Required local output pointers and initialized
progress are owner-private preconditions, not untrusted input. Public API
output/receiver checks remain. Hashing rounds/seeds, insertion order, storage
growth, failure atomicity and resource limits are not relaxed.

Baseline arm64 map-find saved twelve registers in a 128-byte stack frame,
even for scalar hits. The leaf probe has no stack frame or calls. A first
primary-slot prefix failed to remove that prologue and was discarded. Shared
probing alone did not establish a stable material gain; the repeated-lookup
repair is the meaningful follow-up, not proof that stack setup dominated.

## Paired measurements

The same external `algorithm-benchmarks` business sources, inputs, operation
counts, release flags, checksums and 64 MiB heap limits are retained. Three
warmups precede eleven measured rounds with alternating participant order.
Each sample is a fresh sequential process, timeout 30 s, exact stdout, exit 0
and empty stderr. All outliers are retained. Run 1 overlapped a brief formatter
check; runs 2/3 had no builds or tests alongside timing. Ordinary desktop apps
remained open. These whole-process results are not dedicated D056-runner data.

| Workload | Run 1 baseline → candidate | Run 2 baseline → candidate | Run 3 baseline → candidate |
|---|---:|---:|---:|
| frequency-map | 54.067 → 45.156 ms | 53.047 → 45.892 ms | 58.195 → 46.779 ms |
| unique-set | 42.981 → 42.654 ms | 44.865 → 44.201 ms | 47.674 → 45.856 ms |
| rune-frequency | 31.040 → 27.817 ms | 31.907 → 28.768 ms | 33.330 → 30.091 ms |

Frequency-map improves 13.5–19.6%; rune-frequency 9.7–10.4%. Set differences
of 0.8–3.8% are small/noisy and do not establish a material improvement.
[Complete raw samples](cp42-collection-lookup-reuse-2026-09-07.json) include
all three runs and the discarded/intermediate experiments, not only favorable
results. No new Go/Rust comparison or cross-run compounded speedup is claimed.

Ignored `build/collection-probe-perf.CMFea8/` retains the driver, profile,
intermediate artifacts and candidate binaries. No compiler checkout was copied.
The saved baseline compiler is `build/dispatch-baseline/mmay-655f185`.

```text
f6f49e503f197372d3345495afb723f90be916172a5a1aeb24ddf83487b37043  baseline compiler
51275c52d840738c97c43b878a4f47ecbbf86edfc26b456127f486d247aa44de  compare.py
595659544c668aa14956fbb54bc8b1152653217453b2da665e6f5333077c442d  candidate frequency-map
53ba0a06de0207b8a49d1321e1ed128bb618929705315247568f4548bfe9831c  candidate unique-set
fd48e3cfc01d6d2e68fd9a1b5fa4a6b63393fa60145aaa40051f8db8647602de  candidate rune-frequency
```

## Correctness and safety evidence

The leaf owner test exercises 4,096 deterministic bucket layouts, eight
starting positions and nine raw queries each, including exact-hash collisions,
tombstones, full tables, wraparound and invalid storage. Reference-candidate
enumeration is checked independently against the expected complete walk.

A focused map test compares 8,192 operation sequences with memoization enabled
and forcibly disabled, including alias updates, absent/present and different
keys, removal, clear, limits, order and forced collection. Boundary tests cover
seed mismatch, generation exhaustion, null public outputs, stale/foreign
receivers, reference-value validation and GC lifetime. Cached updates consume
no allocation attempt. Existing exhaustive allocation-failure sweeps now place
an absent `get` before each insertion, exercising memoized growth failures and
collect-before-allocation. Full teardown asserts zero live allocations/bytes.

Two new source regressions exercise aliasing, growth, read-only iteration,
clear/removal/reinsertion and reference-valued arrays. All nine CP36 source
tests pass against the saved baseline VM as well as the candidate VM/native;
these tests protect unchanged semantics, not new source behavior.

The complete candidate Release CTest suite passed 68/68 (400.88 s, parallel 2).
Rebuilt Debug runtime/CP36 unit/source suites passed 3/3 (48.44 s); rebuilt
macOS ASan/UBSan counterparts passed 3/3 (65.28 s). The source acceptance covers
both engines; CP36 C-unit checks include generated C O0/O2 and allocation/GC
failures. Correctness-job durations overlap builds/tests and are not benchmark
evidence. The staged-source CP39 distribution, relocation and CP29 release
contract retest passed 3/3 (25.64 s), including the new probe owner and fixture.
All nine architecture/memory/documentation/generated/release gates, source
formatting and build-tree `mmay doctor` passed.

The independently rebuilt Linux Clang 18.1.3 ASan/UBSan/LSan runtime and CP36
unit suites passed 2/2 (502.96 and 174.76 s). The unmodified source-acceptance
script then passed format/check, VM execution, all nine tests in both engines,
release build and native execution. Only the exact CP36 source fixture was
copied, without tool caches, to writable ignored diagnostic output; the full
repository mount stayed read-only and container networking was disabled.
The environment is Ubuntu 24.04 amd64 under Apple M2 emulation, not the
original runner host/image. Leak detection and halt-on-error were enabled;
no sanitizer suppressions or reduced test expectations were introduced.

## Original CI, separately scoped

[Run 197](http://ci.example:3000/Marimay/marimay/actions/runs/197) for the
baseline completed with 12 of 13 jobs successful. ASan/UBSan/LSan, TSan, all
four fuzz jobs, Clang Release and both GCC configurations passed. Only
[Clang Debug](http://ci.example:3000/Marimay/marimay/actions/runs/197/jobs/1676)
failed: CP31 allocation shard 7 segfaulted after 17.35 s; its other 67 tests
passed. The displayed failing test contains no stack or allocation index.
The same shard passes with the current candidate on local macOS Debug
(7.44 s) and ASan/UBSan (10.35 s). This neither reproduces nor explains the
original failure, and a passing sanitizer job at the baseline does not close
the distinct reports from earlier runs. No runner cause is established.

## Remaining obligations

Set hot-path cost, residual Map/rune work, per-object memory overhead, the
previous rune-Eq and formatter-recovery defects, config/startup acceptance and
the full ten-utility portfolio remain open. Runs 196 and 197 are failed.
Independent local sanitizer success does not explain or close those
original-runner findings. No CI checks were weakened, retried or cancelled
for this change.
