# CP42: checked Set insertion and probe-field locality

Local macOS 26.6.2 arm64 / Apple M2 / AppleClang 21 follow-up to `4154c6d`.
This is further partial collection optimization, not a Tier 1 performance
gate, Go/Rust parity claim, or completion of CP42 and the ten-utility portfolio.

## Diagnosis and implementation

An eight-second sample of the unchanged `unique-set` entry, repeated 250 times
with fresh heap teardown, recorded 6,682 main-thread samples. Top-of-stack
counts include 3,215 bounded-probe, 846 fixed-word SipHash, 533 receiver-resolve,
412 lookup, 340 put and 285 Set-add samples. The call tree separately shows
Set-add resolving the receiver before Map-put resolves it again. Input-array
preparation is present but does not explain the dominant collection stacks.
The profile completed all 250 exact checksums with exit 0 and empty stderr.
Sampling identifies call sites; these counts are not exact CPU-time shares.

Under the existing
[D079](../../design/decisions.md#d079-ordered-map-set-and-checked-hash-key-coherence)
and [collection specification](../../spec/standard-library.md):

- Set-add delegates to a fully checked operation in the shared map owner via
  `managed_map_set_internal.h`. It resolves heap/slot/generation/kind once,
  checks the Set discriminator, then uses the same insertion core as Map-put.
  No resolved object or storage pointer crosses the owner interface.
- Set's receiver-before-output error order is retained, including unchanged
  output on a rejected receiver. Key validation, nominal/string equality,
  full SipHash-2-4, both seeds, probe integrity and bounds checks remain.
  Growth still re-resolves the handle and preserves temporary roots, budgets,
  failure-atomic publication and deterministic allocator behavior.
- The shared insertion core has a guarded Clang/GCC inlining hint, with an
  ordinary C17 inline fallback. An initial out-of-line version improved Set
  but repeatedly slowed frequency-map; it is not the accepted candidate.
  Symbol inspection confirms the final arm64 binary no longer has that extra
  out-of-line insertion core. Source logic remains owned in one place.
- The private entry groups `active`, hash and key before the value. Its size
  stays 48 bytes and the Map/Set object stays 128 bytes on this host. No new
  allocation or storage array is introduced. Field offsets change, so this
  is not an internal-layout binary-compatibility claim. This grouping reduces
  the span of fields inspected by probing; no hardware cache-miss reduction
  or independently established layout-only speedup is claimed.

Source syntax, public function signatures, MIR/bytecode, insertion order,
mutation generations and failure classifications are unchanged. A duplicate
Set-add remains valid at length/generation limits without allocating or
invalidating iteration; real mutations retain all existing checks.

## Paired measurements

The external `algorithm-benchmarks` business sources, values, operation counts,
checksums, release flags and 64 MiB heap budgets are unchanged. Each series has
three warmups and eleven measured rounds, alternating baseline/candidate
order. Every sample is a fresh sequential process, timeout 30 s, exact stdout,
exit zero and empty stderr. No builds or tests ran alongside timing; ordinary
desktop applications remained open. All outliers are retained.

| Workload | Run 1 baseline → candidate | Run 2 baseline → candidate | Run 3 baseline → candidate |
|---|---:|---:|---:|
| frequency-map | 48.771 → 44.741 ms | 49.042 → 44.445 ms | 44.389 → 44.387 ms |
| unique-set | 43.240 → 38.144 ms | 49.797 → 41.924 ms | 42.448 → 37.911 ms |
| rune-frequency | 27.805 → 26.944 ms | 31.752 → 29.753 ms | 27.595 → 26.870 ms |

Set improves 10.7–15.8%. Rune differences are smaller (2.6–6.3%). Frequency-map
ranges from effectively unchanged to 9.4% faster, so a stable Map percentage
is not established; the earlier regression does not recur in these series.
These are exploratory desktop whole-process measurements, not D056-runner
evidence. [Raw samples](cp42-set-insertion-2026-09-07.json) also include all
three intermediate runs with their frequency-map regressions, not only the
selected candidate. No new cross-language comparison was performed.

Ignored `build/set-probe-perf.LCm6IQ/` retains the profile, driver, baseline
compiler/runtime, intermediate and final binaries. No compiler checkout was
copied. The baseline benchmark binaries are the preceding report's candidate.

```text
a0df8e2209c61da49aba481fb9ceb3e7d397817b8594786d556e76ba11192c54  saved 4154c6d compiler
51275c52d840738c97c43b878a4f47ecbbf86edfc26b456127f486d247aa44de  compare.py
08d372363bfc52d07ab628d1ea1fe86346c887bdbf7058b9c644402d39baef0b  candidate frequency-map
0144b789b9ba37406eba359dbf5b7b20445f1c32a2e906f8032ba5b340066452  candidate unique-set
3e43e91cee35f83b34130200b6fee85babf4e74f00ca5801eae85954a6276968  candidate rune-frequency
```

## Verification

`runtime/test_managed_set_insertion.c` covers receiver/output error precedence,
wrong-kind, stale and foreign handles, reference-key validation, equal strings
with distinct identities, length and generation exhaustion, duplicate inserts
with the next allocation configured to fail, rooted collection and teardown.
Both allocator live counters are zero after teardown. Existing exhaustive
Map/Set allocation-failure sweeps still exercise the shared insertion path,
including forced collection, growth and compaction. Existing collision/probe
and enabled/disabled Map-memo differential tests remain unchanged.

A new source regression adds duplicates through an alias during an active
Set iterator and checks order and length. All ten source tests pass against
the saved baseline VM, demonstrating unchanged behavior. The candidate Debug
runtime/CP36 unit/source suites passed 3/3 (57.13 s); macOS ASan/UBSan passed
3/3 (175.68 s). These include source VM/native acceptance and C O0/O2 cases.
Correctness jobs overlapped other builds/tests; durations are not performance
comparisons. The complete Release suite passed 68/68 (494.49 s), including
all eight deterministic CP31 allocation shards. Staged-source CP39 distribution,
relocation and CP29 release checks passed 3/3 (32.60 s). All nine architecture,
memory, documentation/generated and release gates, source formatting and
build-tree `mmay doctor` passed. Independently rebuilt Linux Clang 18.1.3
ASan/UBSan/LSan runtime and CP36 unit suites passed 2/2 (416.56 and 155.20 s).
The unmodified acceptance script passed format/check, VM execution, all ten
tests in both engines, release build and native execution. Only the exact
source fixture was copied without tool caches into writable output. The diagnostic
environment is Ubuntu 24.04 amd64 under M2 emulation, with read-only source,
writable ignored output, disabled networking and leak/halt-on-error checks.
It is not the original runner host/image. GCC was unavailable there, so no
local GCC verification is claimed.

## Remaining scope and external state

Residual table/handle and collection-memory costs, the rune Eq and formatter
recovery defects, config/startup acceptance, later CP42 ergonomics and all ten
external applications remain obligations. This slice does not close them.

The previously pushed revision is `4154c6d`. A fresh CI status could not be
retrieved: browser inspection timed out, HTTP returned no headers within eight
seconds, and SSH reset the connection during key exchange before authentication.
A subsequent read of the remote main ref stalled for over 90 seconds and only
that owned read process was terminated; no write was attempted in that check.
No runner cause is established and no new CI outcome is claimed. No remote
jobs or server settings were changed, retried or cancelled.
