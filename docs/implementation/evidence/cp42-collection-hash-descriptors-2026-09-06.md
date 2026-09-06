# CP42: collection hash and descriptor boundaries

Local macOS arm64 / Apple M2 / AppleClang 21 follow-up to `e4cbdc4`.
This is another partial repair of the required Map/Set performance item, not
its completion, a Go/Rust parity claim, or a Tier 1/alpha gate.

## Profile and implementation

A fresh eight-second sampled profile of the unchanged frequency-map entry,
repeated 100 times with separate heap teardown, no longer puts generic enum
construction on the hot path. Top-of-stack counts include 1,475 map-find,
1,108 generic SipHash, 437 descriptor-validation and 421 map-lookup samples.
The run produced all 100 exact checksums with exit 0 and empty stderr. It
overlapped a diagnostic build, so these are call-attribution observations,
not latency measurements or a stable percentage of CPU time.

Two costs are removed under the existing
[D079](../../design/decisions.md#d079-ordered-map-set-and-checked-hash-key-coherence)
and [collection contract](../../spec/standard-library.md):

- The base hash owner has a fixed-word SipHash-2-4 entry point exactly
  equivalent to the generic routine over the native bytes of one `uint64_t`.
  It removes variable-length loop/tail dispatch, retaining every round and
  both seed words. String and nominal hashing are unchanged.
- Map and Set creation both validate the immutable field descriptor before
  publication. Operations rely on that existing lifetime/immutability contract
  instead of repeating shape validation on every access; raw values have no
  additional reference-validation work. Every receiver handle still passes
  heap, slot, generation and object-kind checks. Reference keys/values retain
  their validation, including error paths. GC tracing still validates shape.

The Set constructor now rejects malformed descriptors previously admitted by
its incomplete check. Unknown reference-kind tags are rejected as invalid
shapes. There is no new allocation, cached pointer, descriptor field, object
layout, source syntax, bytecode opcode, table algorithm or generated ABI.
Probing, insertion order, mutation generations, limits, storage accounting,
publication and failure-atomic cleanup are unchanged.

## Paired measurements

Both participants use the same existing external `algorithm-benchmarks`
business source, inputs, operation counts, checksums, release flags and
64 MiB heap limits. Three warmups and eleven measured rounds alternate
baseline/candidate order. Each sample is a fresh sequential process checked
for exact stdout, exit 0 and empty stderr, with a 30 s timeout. No builds or
tests ran alongside timing; ordinary desktop applications remained open.
All outliers are retained. These are exploratory whole-process measurements,
not the dedicated D056 performance runner.

| Workload | Run 1 baseline → candidate | Run 2 baseline → candidate |
|---|---:|---:|
| frequency-map | 62.865 → 57.880 ms (7.9%) | 65.946 → 59.866 ms (9.2%) |
| unique-set | 51.005 → 48.481 ms (4.9%) | 48.383 → 46.358 ms (4.2%) |
| rune-frequency | 37.233 → 36.869 ms (1.0%) | 38.604 → 35.268 ms (8.6%) |

The rune result is too variable to claim a stable percentage. Both complete
paired runs are in [raw samples](cp42-collection-hash-descriptors-2026-09-06.json).
An earlier hash-only experiment gave 58.483 → 55.784 ms for frequency-map,
33.921 → 32.473 ms for rune-frequency, and 52.788 → 54.685 ms for unique-set;
it did not establish Set improvement. Its raw samples, including a 1,217 ms
Set outlier, remain in ignored `build/map-hotpath-perf.xieI6A/word-hash.json`.

The same ignored directory contains the driver, profile, baseline runtime and
both candidate binary sets. The baseline binaries are the prior report's
`e4cbdc4` candidate binaries. Only build artifacts are copied, not compiler
checkouts or the benchmark business program.

```text
1000735be9c2f18aac54f4830a60f674ec56630244eedf5f572f8bb75627dc7a  saved e4cbdc4 compiler
51275c52d840738c97c43b878a4f47ecbbf86edfc26b456127f486d247aa44de  compare.py
3d2c6c81ca761687f0340a2af413a83e06f83a3315387977c864e0a7755d7232  candidate frequency-map
d174ebbe17c1f0eaad640d30bb62af4399dd58173e1fecdca0de1335d00ee8aa  candidate unique-set
56ec4be7ccf55f62a34804f48fde1dae67b9833e14263ee02dadaaed953eb33c  candidate rune-frequency
```

## Boundary verification

The base hash test compares 100,000 deterministic words, including bit/sign
extrema and patterns, under two seed pairs each against generic SipHash.
The focused collection owner tests malformed raw/reference descriptors,
unknown kinds and prohibited key layouts before allocation, foreign/stale/
wrong-kind handles, invalid reference keys/values, unchanged error-path length
and mutation generation, and fresh handles after collection/reuse. Invalid
constructor calls leave allocation attempts and live counts/bytes unchanged;
complete teardown asserts both counters are zero. Existing exhaustive Map/Set
allocation-failure sweeps, dual-seed order, nominal/string keys, collision
probing, growth, cyclic GC and limits remain in place.

The rebuilt Debug frontend/runtime/CP36 unit/source suites passed 4/4
(46.76 s). The same rebuilt ASan/UBSan suites passed 4/4 (61.16 s). CP36 source
acceptance runs both VM and native; its C unit owner also covers generated-C
O0/O2, forced collection and allocation failures. Correctness jobs overlapped
other builds/tests and their durations are not performance evidence.

The complete Release suite passed all 68 tests (407.16 s, parallel 2),
including the native owner and all eight CP31 allocation shards. The exact
CI-failing CP31 shard also passed locally under ASan/UBSan (10.46 s).

The subsequent documentation-link gate exposed a distribution omission:
installed Markdown reports did not include their linked JSON data. CMake now
includes JSON alongside documentation/examples; CP29 checks each source
documentation JSON exists in the relocated install with an identical SHA-256.
This restores the existing version-matched offline kit contract, without
changing source semantics or the distribution layout.
The new check failed against the old generated installer (missing JSON),
then passed after regeneration. Relocation, CP29 and the staged CP39 source/
distribution retest passed 3/3 (23.38 s).

The rebuilt independent Linux Clang 18.1.3 ASan/UBSan/LSan frontend, runtime
and CP36 C-unit suites all passed (0.96, 419.21 and 137.66 s respectively).
The source acceptance initially failed B0005 because its native test cache
targeted the read-only source mount; the underlying checks were not disabled.
The exact five-file CP36 fixture was copied, excluding tool cache state, into
the writable ignored output directory. The unmodified acceptance script then
passed format/check, VM execution, all seven tests in both engines, release
build and native execution. The first CTest invocation is therefore 3/4 with
an environment failure; the corrected source-acceptance invocation passed
separately. Linux sanitizer options included leak detection and halt-on-error.

Architecture, memory policy, roadmap, documentation links, generated files,
spec terminology, example syntax, syntax coverage and release-contract gates
passed. Source formatting and both runtime profiles in build-tree
`mmay doctor` passed. These results do not replace the unresolved original CI.

## Separate Linux CI investigation

The independent Ubuntu 24.04 amd64 Docker reproduction, Clang/compiler-rt
18.1.3 under Apple M2 emulation, passed all three CLI check/run/test suites
with ASan/UBSan/LSan at `e4cbdc4` (209.87 s parallel total). Sources were mounted
read-only, output went only to the ignored diagnostic directory, and networking
was disabled. This run predates the present runtime optimization and is not
its Linux verification. It is also not the identical runner image/host.

[Run 196 sanitizer job](http://ci.example:3000/Marimay/marimay/actions/runs/196/jobs/1667)
at `e4cbdc4` is red: CP31 allocation shard 4 reports an invalid `_Bool` value 4
at `syntax.c:102`, in the lossless-tree check. The last printed deterministic
allocation-failure index is 2796. The stack reaches the CP31 argument test
through AST projection, compiler service and CLI run. This is distinct from
run 195's misaligned-node report and remains unresolved. No sanitizer check
or CI job has been weakened, retried, cancelled or declared repaired.

The exact index 2796 passed in the independent Linux sanitizer build via the
existing `--cp31-allocation-shard 2796 4096` selector. The complete shard 4
then passed as well (32.79 s). Neither success reproduces or explains the
original CI failure.

## Remaining work

Map probing/handle overhead, rune-frequency cost, the rune Eq contract defect,
formatter recovery and the prior config/startup obligations remain open.
These partial gains do not justify proceeding to the next portfolio utility
or declaring CP42, the collection optimization, or the alpha complete.
