# CP42: compact ordered collection storage — 2026-09-07

Local macOS 26.6.2 arm64 / Apple M2 / AppleClang 21 follow-up to `024365d`.
This slice reduces retained collection memory. It is not closure of the
remaining hashing/probing costs, CP42, or the public-alpha Tier 1 gate.

## Representation and ownership

The previous entry always reserved an active/hash prefix and two complete
16-byte managed cells, even for a raw key/value pair or a Set with no value.
[D088](../../design/decisions.md#d088-ordered-collections-use-descriptor-selected-compact-storage)
selects compact payload widths from the constructor-validated immutable
descriptor, retaining an aligned active/hash prefix. On this host:

| Entry shape | Before | Selected |
|---|---:|---:|
| raw Set | 48 B | 24 B |
| reference Set | 48 B | 32 B |
| raw → raw Map | 48 B | 32 B |
| raw → reference / reference → raw Map | 48 B | 40 B |
| reference → reference Map | 48 B | 48 B |

The object grows from 128 to 136 bytes for immutable layout metadata. Empty
collections therefore cost eight additional bytes; reference/reference Maps
do not gain entry-space savings. This tradeoff is counted, not hidden. Raw
bits use a full word and reference cells retain complete provenance/slot/
generation. Fixed-size C17 representation copies avoid unaligned or
incompatible typed payload access. Set never reads, writes or traces a value.

The new independently compiled `managed_map_storage.c` owns checked size and
budget planning, temporary roots, allocation, compaction, publication,
tracing and disposal. `managed_map.c` retains hash/equality and operations;
the bounded probe remains separate. Its private storage header provides exact
layout, checked handle resolution and small allocation-free transfers. The
operation owner drops from 1,288 to 970 lines; this is a lifecycle-owner
extraction, not an included implementation shard.

Entry/bucket growth policy, full SipHash-2-4, both seeds, collision checks,
insertion order, mutation generations and error precedence are unchanged.
Replacement buffers publish only after allocation/rebuild succeeds. The memo
is invalidated on rebuild; removed reference payloads cease to trace. D055
accounts for actual reserved compact bytes and the larger object, so physical
resource boundaries move. There is no internal-layout binary-compatibility
claim. Public source APIs, syntax, MIR, bytecode and formatter rules do not change.

## Memory measurement

A small C17 probe uses only the public runtime API, linked once to the saved
baseline archive and once to the selected archive. It constructs isolated raw
collections with the same distinct counts as the three workloads. Managed and
allocator live-byte counters agree; each result has four live allocations and
both live counters return to zero after complete teardown.

| Collection | Baseline retained bytes | Selected retained bytes | Reduction |
|---|---:|---:|---:|
| Map, 4,096 keys | 262,528 | 197,000 | 25.0% |
| Set, 20,001 keys | 1,835,392 | 1,048,968 | 42.8% |
| Map, 12 keys | 1,280 | 1,032 | 19.4% |

These totals include the object, slot table and reserved entries/buckets.
They exclude benchmark input arrays, compiler memory and process RSS. An
empty probe heap with its collection uses 384 → 392 bytes. The source and
archive hashes and exact readings are retained in the
[raw report](cp42-compact-collections-2026-09-07.json).

## Performance investigation

The first compact candidate reduced memory but regressed frequency-map and
rune-frequency by roughly 3–4% in two paired series. Symbol inspection showed
out-of-line small stores; value access also repeatedly decoded the descriptor.
The next candidate cached byte widths and inlined those stores, but Clang
merged conditional 8/16-byte `memcpy` calls into a variable-length external
`memcpy`. Two series showed frequency-map around 54 ms instead of 38 ms and
rune-frequency around 31–32 ms instead of 25 ms. Neither candidate is selected.

The selected transfer writes a fixed common word and, only for references,
a fixed reference tail. Object-representation copies are valid C17; no aliasing,
alignment or platform-specific intrinsic shortcut is introduced. Guarded
Clang/GCC inline hints have an ordinary C17 fallback. Inspection of the final
Map-put assembly shows word stores with no external copy call. All four
rejected distributions are retained beside the three selected ones.

The external business sources, inputs, counts, APIs, checksums, Release flags
and 64 MiB heap remain unchanged. Every series has three warmups and eleven
measured rounds with alternating participant order, fresh sequential processes,
30-second timeouts, exact stdout, exit zero and empty stderr. No builds/tests
overlap selected timing; ordinary desktop apps remain open. All outliers remain.

| Workload | Run 1 baseline → selected | Run 2 baseline → selected | Run 3 baseline → selected |
|---|---:|---:|---:|
| frequency-map | 41.285 → 41.016 ms | 40.370 → 38.861 ms | 38.821 → 37.903 ms |
| unique-set | 36.741 → 36.289 ms | 36.258 → 36.307 ms | 35.927 → 35.769 ms |
| rune-frequency | 25.890 → 26.052 ms | 25.766 → 25.712 ms | 25.891 → 25.935 ms |

Frequency-map is 0.7–3.7% faster here; Set/rune differences are small and
noise-sensitive. No stable Set/rune speedup is established. This is a memory
improvement with local hot-path validation, not D056 budget evidence or a
Go/Rust comparison. Baseline benchmark hashes equal the preceding report's
selected artifacts. Ignored `build/map-storage-perf.OdNLOI/` retains the saved
compiler/archive, binaries, memory probe and comparison driver; no language
checkout was copied and no external benchmark source was changed.

## Verification

The focused C storage owner covers all six raw/reference/Set layout shapes:

- growth, removal and same-capacity compaction preserve complete values/order;
- every deterministic allocation-failure index through first complete success,
  including reference construction, collector paths and replacement buffers,
  preserves logical state and ends with zero live allocations and bytes;
- exact first-reservation budget succeeds while one byte less fails atomically
  with the exact D055 live/requested/limit fields; reserved unused bytes are zero
  even when inactive bytes of an input raw-cell union are deliberately poisoned;
- removed references stop tracing, retained references survive compaction/GC,
  and clear releases all child references after collection.

Synthetic probe tests retain their independent collision/tombstone oracle,
using an independently declared raw-entry layout. Existing cache-enabled/
disabled differentials, hostile handles, seed changes, limit boundaries and
native O0/O2 tests remain active. One source regression exercises all six
layouts through growth, removal, compaction, lookup, ordered iteration and
clear; the corpus now has 13 tests. All 13 pass against the saved baseline VM.
Initial fixture attempts used unsupported tuple equality (T0003) and a block
match arm without its required explicit tail (T0016); the fixture now uses
ordinary tuple matching and void tails, without changing D080 or match semantics.

The selected Release focused suites passed 3/3 (42.51 s). After adding exact
budget assertions, the full Release suite passed 68/68 (293.14 s), including
all eight CP31 allocation shards. Debug focused suites passed 3/3 (71.41 s),
and macOS ASan+UBSan passed 3/3 (97.72 s). These test durations are not speed
comparisons; correctness jobs overlapped builds/tests, unlike accepted timings.

Independent Ubuntu 24.04 amd64 / Clang 18.1.3 validation runs under M2 emulation,
using the existing diagnostic image, read-only source, writable ignored output,
disabled networking, ASan/UBSan and `detect_leaks=1:halt_on_error=1`. Runtime
passed (370.01 s). The first CP36 run failed 1,424 instances of one new test
assertion: `memcmp` over the entire raw-cell union, including inactive bytes.
All active-word/reference-value and allocator assertions passed; the retained
log contains no other expectation or sanitizer failure. Neither the RAW64
contract nor C union value transport promises a canonical inactive tail.

The test now compares the complete active raw word (or complete reference),
poisons the unused input tail, and still checks zeroed reserved bytes, exact
allocation bounds and full failure sweeps. No production code changed for
this test correction. The saved initial log is
`build/map-storage-perf.OdNLOI/linux-initial.log`, SHA-256
`44a3b0a70f08fe29d6a614badce746cdefe3d487672548fc7b36a550c7b76de5`.
Corrected CP36 reruns passed in Release (9.64 s), Debug (9.73 s), macOS
ASan/UBSan (15.63 s), and Linux ASan/UBSan/LSan (101.56 s).

The writable Linux source fixture was recursively compared against current
CP36 source, excluding only `.mmay` caches. The unmodified acceptance driver
then passed format/check, VM execution, all 13 tests in both engines, Release
native build and native execution under the same sanitizer environment.
This is not the failing Gitea host/image or a full Tier 1 matrix; no local
GCC/Windows validation or CI closure is claimed.

All nine architecture, memory, roadmap, document-link, generated, specification,
example-syntax, syntax-coverage and release-contract gates passed. The first
specification gate caught the missing D088 decision-index entry; adding that
exact entry restored the gate without changing its checks. Release `mmay doctor`
and `git diff --check` passed. After synchronizing the final index and corrected
focused test, staged-source distribution, install relocation and release-contract
checks passed 3/3 (20.32 s).

```sh
ctest --test-dir build/release --output-on-failure --parallel 2
ctest --test-dir build/debug -R '^(mmay_c_unit_runtime|mmay_cp36_unit|mmay_cp36_map_set)$' --output-on-failure
ctest --test-dir build/asan-ubsan -R '^(mmay_c_unit_runtime|mmay_cp36_unit|mmay_cp36_map_set)$' --output-on-failure
python3 build/map-storage-perf.OdNLOI/compare.py --candidate compact-fixed
```

Residual collection execution costs, the ordinal-iteration complexity,
parser recovery, remaining ergonomics and the ten-utility portfolio stay open.
The last verified remote main was `4154c6d`; local-only parents include `267d81a`,
`344388b` and `024365d`. This report does not establish a fresh Gitea CI outcome
or the cause of the earlier runner failures.
A bounded remote-ref check again timed out connecting to `ci.example:2222`
before authentication; it changed no remote ref.
