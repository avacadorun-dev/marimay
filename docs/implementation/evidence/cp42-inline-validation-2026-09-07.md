# CP42: portable inline reference validation — 2026-09-07

Local macOS 26.6.2 arm64 / Apple M2 / AppleClang 21 follow-up to `344388b`.
This is partial collection optimization, not a Tier 1 budget, cross-language
parity claim, or closure of CP42 and the ten-utility portfolio.

## Diagnosis and selected change

Eight-second, one-millisecond sampling of unchanged generated benchmark C,
linked against the saved baseline runtime, separates entry work from startup.
The wrapper repeats frequency-map 250 times and rune-frequency 400 times;
each invocation creates and disposes its own heap. All repetitions completed
with exact checksums, exit zero and empty stderr. Main-body top-of-stack counts:

| Site | frequency-map (6,262 samples) | rune-frequency (6,445 samples) |
|---|---:|---:|
| bounded bucket probe | 1,901 | 1,595 |
| fixed-word SipHash | 911 | 598 |
| resolved map lookup | 626 | 515 |
| Map put / get | 473 / 459 | 349 / 342 |
| reference resolve | 408 | 334 |
| heap validity | 282 | 265 |
| occupied slot | 211 | 166 |

The rune profile also contains 534 array-view-get, 348 array-view-push and
250 array-value-validation samples. Preparation costs are real but do not
explain the dominant collection stacks. Sampling is not an exact CPU-time
share or hardware-cache-miss measurement; a lightweight architecture check
may have overlapped initial sampling, but not the paired latency runs below.

`managed_reference_internal.h` now contains the heap owner's small complete
heap-validity, occupied-slot and reference-resolution predicates as private
C17 inline helpers. Separately compiled runtime owners can optimize those
calls without archive/program LTO. Baseline binary symbols contain the four
out-of-line helpers; accepted Release binaries no longer contain them.

All initialization, provenance, slot bounds, known occupied kind/non-null
payload and exact generation checks are preserved. The mutable entry point
shares the const resolver only when given a mutable heap. No reference or
storage pointer is retained across growth/collection. There is no new
allocation, descriptor/layout/capacity change, unchecked public API, hash
change, cache, mutation change, or failure-index change. This is helper
placement, not an architectural decomposition of a semantic owner.

Existing [D059](../../design/decisions.md#d059-native-exact-array-access-uses-a-versioned-runtime-abi-not-lto),
[D079](../../design/decisions.md#d079-ordered-map-set-and-checked-hash-key-coherence)
and the [collection contract](../../spec/standard-library.md) remain unchanged.
Source syntax, public runtime API, typed MIR, bytecode and formatter behavior
are unchanged; runtime-private helper symbols become local inline definitions.

Two exploratory alternatives are retained in the raw report, not shipped:

- A raw-only specialization of the bounded probe showed only 0.34% frequency,
  1.21% Set and -0.26% rune differences in one paired run. It was fully reverted.
- ThinLTO of the unchanged generated C and runtime gave one-run improvements
  of about 10%, 4% and 26% respectively. That compiler-coupled archive/program
  boundary is not the D059 distribution contract, and the single rune series
  was noisy. No production LTO flag or build contract was changed. It served
  only as a diagnostic for optimization across call boundaries.

## Paired measurements

The external `algorithm-benchmarks` business sources, inputs, operation counts,
reusable APIs and checksums are unchanged. Accepted binaries use the ordinary
`mmay build --release` flags and a 64 MiB heap. Each series has three warmups
and eleven measured rounds with alternating baseline/candidate order. Each
sample is a fresh sequential process with a 30-second timeout; exact stdout,
exit zero and empty stderr are mandatory. No builds/tests overlapped timing;
ordinary desktop applications remained open. All outliers are retained.

| Workload | Run 1 baseline → candidate | Run 2 baseline → candidate | Run 3 baseline → candidate |
|---|---:|---:|---:|
| frequency-map | 41.778 → 38.665 ms | 40.698 → 38.424 ms | 41.194 → 38.553 ms |
| unique-set | 36.579 → 35.867 ms | 36.642 → 35.794 ms | 37.592 → 36.750 ms |
| rune-frequency | 26.612 → 25.595 ms | 26.867 → 25.810 ms | 26.976 → 26.431 ms |

Frequency-map improves 5.6–7.5%; Set improves 1.9–2.3% and rune-frequency
2.0–3.9%. The smaller differences remain noise-sensitive. The third selected
frequency-map candidate series includes 105.819584 ms and 63.273375 ms samples;
they are not trimmed. These exploratory desktop observations are not a D056
performance-gate result. No new Go/Rust or complete Tier 1 comparison was run.

[Raw samples and artifact hashes](cp42-inline-validation-2026-09-07.json)
include all three accepted series and both exploratory candidates. Baseline
benchmark hashes equal the preceding Set-insertion report's final artifacts;
the rune-equality repair did not change these benchmark programs.

Ignored `build/map-access-perf.som1MN/` retains the baseline compiler/archive,
benchmark binaries, profiles, comparison driver and exploratory outputs. The
ThinLTO directory is an ignored runtime-only CMake build, not another source
checkout. Existing external benchmark edits were not changed or committed.

Local reproduction of the preserved artifact comparison:

```sh
python3 build/map-access-perf.som1MN/compare.py --candidate inline-validation
python3 build/map-access-perf.som1MN/profile.py frequency-map
python3 build/map-access-perf.som1MN/profile.py rune-frequency
```

The profile driver compiles the original cached generated C with `-O2 -g`,
renames only its entry to call it repeatedly, and links the saved baseline
runtime. Accepted timing binaries are ordinary tool-built single-entry binaries,
not these repeated profiling executables.

## Verification

`runtime/test_managed_reference_validation.c` covers all six occupied object
kinds and their null-payload forms, null heap/slot, initialization, zero/max/
foreign provenance, slot count and sentinel bounds, stale generations, invalid
and unknown kinds, and exact `UINT32_MAX` generation matching. Both mutable
and const entry points are checked. This focused test uses only synthetic
caller-owned stack metadata and does not allocate.

Existing runtime allocation-failure, collection, alias/lifetime, raw/reference
Map/Set and native O0/O2 tests remain the allocation/teardown evidence. Their
allocator-backed cases require both live allocation and live byte counts to
return to zero. The complete Release CTest suite passed 68/68 (280.95 s),
including all eight CP31 allocation shards. Focused runtime, CP36 unit and
CP36 source acceptance passed 3/3 in Release (41.51 s), Debug (49.77 s) and
macOS ASan+UBSan (74.43 s). Test durations are correctness-job observations,
not benchmark comparisons.

Independent Ubuntu 24.04 amd64 / Clang 18.1.3 validation under M2 emulation
passed the runtime and CP36 unit suites 2/2 (389.66 s and 101.58 s). This uses
the existing diagnostic image with ASan, UBSan and
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:symbolize=1`, plus
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`. Source is mounted read-only,
output is writable and networking is disabled. The previously created writable
CP36 fixture was compared recursively against current sources, excluding only
tool-owned `.mmay` caches, before running the unmodified CMake acceptance
driver. Format/check, VM execution, all 12 tests in both engines, Release
native build and native execution passed under the same sanitizer environment.
This is not the failing Gitea host/image or a complete Tier 1 matrix; no local
GCC or Windows verification is claimed.

All nine architecture, C-memory, roadmap, document-link, generated-artifact,
specification-term, example-syntax, syntax-coverage and release-contract gates
passed. Release `mmay doctor` and `git diff --check` passed. With the new private
header and focused test staged, CP39 distribution, installation relocation
and CP29 release-contract tests passed 3/3 (21.01 s).

```sh
cmake --build build/release --parallel 2
ctest --test-dir build/release --output-on-failure --parallel 2
ctest --test-dir build/debug -R '^(mmay_c_unit_runtime|mmay_cp36_unit|mmay_cp36_map_set)$' --output-on-failure
ctest --test-dir build/asan-ubsan -R '^(mmay_c_unit_runtime|mmay_cp36_unit|mmay_cp36_map_set)$' --output-on-failure
```

## Remaining scope

Residual collection probing/hash/handle and memory costs remain open, as do
the parser-recovery issue, config/startup acceptance and remaining CP42
ergonomics/portfolio work. The last verified remote main is `4154c6d`; parent
commits `267d81a` and `344388b` were local-only when this investigation began.
A fresh bounded remote-ref check again timed out connecting to
`ci.example:2222` before authentication; it did not change remote refs.
No fresh Gitea CI result or resolution of the prior Linux runner failures is
claimed by this local validation.
