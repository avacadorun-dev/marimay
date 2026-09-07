# CP42: independent ordered collection cursors — 2026-09-07

Local macOS arm64 / Apple M2 follow-up to `82e3513`. This closes the
reproduced quadratic traversal defect. It does not close overall collection
performance, CP42, CLI0, or the pushed Tier 1 acceptance. The subsequent
investigation must examine the collection implementation and generated calling
code as a whole before selecting further optimizations.

## Defect and implementation

Legacy active-ordinal reads restart at storage position zero. A source Map
loop does this separately for its key and value; visiting all entries is
quadratic. [D089](../../design/decisions.md#d089-independent-cursors-for-linear-ordered-collection-traversal)
adds a scalar cursor owned by each traversal: zero starts NEXT, a positive
result identifies an active storage position plus one, and zero ends the loop.
NEXT scans only forward; cursor reads validate the receiver, range and active
entry before copying a complete value. The checked runtime boundary allocates
nothing and adds no collection fields or retained bytes.

The checker retains the generation barrier before every advance, including
exhaustion. Continue goes directly to that condition; break exits immediately.
This also removes an unreachable step predecessor that made an existing
always-terminating loop body fail MIR initialization verification with CLI exit
70. An empty collection whose body unconditionally panics reproduced that
failure against the saved baseline. The source regression now covers empty,
removed, reinserted and unconditionally terminating bodies in both engines.

The vertical implementation includes an independent MIR oracle, MIR and hostile
bytecode verification, encoding/decoding, VM, C17 emission and rooting rules.
Wire IDs 158–160 are appended; 151/152 keep active-ordinal semantics. Invalid
dynamic cursors report VM invalid-reference/native integrity failure, with no
fabricated source panic payload. Negative, zero, removed and oversized inputs
are checked. No fuzzer outcome whitelist was broadened. Generation, full
managed handles, source spans, hashing, insertion order and storage accounting
retain D079/D088 semantics; syntax and formatter grammar do not change.

## Measurements

[Raw samples, manifests and verification](cp42-collection-cursors-2026-09-07.json)
retain three warmups and eleven alternating paired rounds. Each invocation
checks exact output, empty stderr and exit zero. The selected series ran after
builds, correctness tests and fuzzing finished; no other benchmark process was
observed at its start. Ordinary desktop applications remained open. This is
local evidence with all outliers retained, not a cross-host budget claim.

The C17 runtime probe creates the table before its monotonic timer and performs
four complete traversals. Both versions retrieve a Map key/value together.
Allocator live bytes are unchanged between participants and both counters
return to zero after teardown.

| Entries | Map before → cursor, ms | Set before → cursor, ms |
|---:|---:|---:|
| 512 | 0.407 → 0.020 | 0.317 → 0.016 |
| 2,048 | 4.859 → 0.061 | 5.034 → 0.063 |
| 8,192 | 79.122 → 0.244 | 80.083 → 0.250 |
| 16,384 | 320.530 → 0.501 | 320.453 → 0.501 |

Doubling the largest input multiplies the old traversal time by approximately
four and the new traversal time by approximately two. The approximately 640×
ratio at 16,384 applies only to this isolated traversal, not to complete apps.

The source fixture includes process startup, table creation, twelve traversals
and its checksum. Sparse variants remove 75% of entries before traversal.
Source Map loops still materialize tuple items; the C probe does not. Their
remaining costs must not be inferred from the isolated runtime ratio.

| Source workload | 2,000 entries, ms | 4,000 entries, ms | 8,000 entries, ms |
|---|---:|---:|---:|
| Map | 33.471 → 4.819 | 122.406 → 7.444 | 475.817 → 12.706 |
| Set | 17.097 → 2.865 | 60.841 → 3.452 | 233.921 → 4.482 |
| Sparse Map | 8.652 → 3.172 | 25.813 → 3.972 | 92.812 → 5.740 |
| Sparse Set | 5.415 → 2.582 | 13.635 → 2.976 | 46.952 → 3.564 |

The three unchanged external counting workloads contain no full Map/Set
traversal. Their complete-process medians confirm that this fix does not solve
the user's reported insertion/lookup gap:

| Workload | Before, ms | Cursor build, ms |
|---|---:|---:|
| frequency-map | 38.506 | 38.143 |
| unique-set | 36.402 | 36.456 |
| rune-frequency | 26.216 | 26.156 |

The small differences do not establish a stable counting speedup. The external
business sources and user's latest reports were not rewritten. Both compilers,
the old runtime archive, exact binaries, generated C/assembly and measurement
drivers are retained under ignored `build/cp42-iteration/`; no compiler checkout
was copied. Artifact hashes are in the raw report.

## Correctness and resource acceptance

Focused runtime tests cover all six raw/reference Map/Set shapes, sparse
removals, interleaved and nested cursors, complete handles, wrong kinds, missing
outputs, removed/stale/oversized positions, collection-before-allocation and
an exact heap budget. Traversal neither allocates nor changes generation.
Every deterministic setup allocation-failure index is exercised through
successful teardown with zero live allocations and zero live bytes.

The CP36 public corpus now has 16 tests, including three new focused iteration
tests. Existing mutation/panic-span, aliases, reference-rooting and allocation
sweeps remain active. Forty-two well-typed forged cases cover two collection
kinds, three protocols (cursor read, legacy ordinal read, NEXT), and seven signed
values. Independent initialized bool operands/destinations and Set VALUE reads
exercise type rejection. Valid/invalid cases cross oracle, codec, VM and native
O0/O2, retaining legacy results. Thirty-two valid/hostile bytecode seeds are added.

Initial native coverage compiled 84 programs. The final test shares one build
per collection/protocol at each optimization level: twelve compilations retain
all 84 executions in fresh child processes. Each child checks exact
output/status and both allocator live counters after full teardown. This
test-harness change avoids repeated runtime compilation; it changes no production
behavior or coverage count.

| macOS acceptance | Result | Elapsed |
|---|---|---:|
| Full Release | 68/68 | 727.73 s |
| Full Debug | 68/68 | 1,152.84 s |
| Full ASan + UBSan | 68/68 | 1,364.40 s |
| Final batched CP36, Release | passed | 21.50 s |
| Final batched CP36, Debug | passed | 24.37 s |
| Final batched CP36, ASan + UBSan | passed | 40.71 s |

Full suites include every CP31 allocation shard. The final batching was a
test-only change and was rerun in all builds. Correctness jobs sometimes
overlapped other correctness jobs; these durations are not paired performance
measurements. An accidentally overlapping earlier focused invocation was
discarded and replaced by the recorded clean focused/full runs.

Ubuntu 24.04 amd64 / Clang 18.1.3 ran under Apple M2 emulation in the existing
diagnostic image, with source mounted read-only, networking disabled and
ASan/UBSan/LSan (`detect_leaks=1:halt_on_error=1`). Runtime passed in 902.88 s.
The old unbatched CP36 process was stopped as superseded (container exit 137);
it is not counted as a pass. Final batched CP36 passed in 244.18 s. The unchanged
public acceptance driver also passed against a byte-identical writable source
fixture, with only an explicit module manifest added: fmt/check, VM/native
16 tests, Release native build and execution.

The Linux bytecode fuzzer completed 10,529,447 executions in 601 seconds
(`-max_total_time=600 -timeout=10`), with no crash artifact or sanitizer failure,
215 MiB peak RSS and 6,282 new corpus units. Those are executions, not a count
of distinct valid programs. This does not establish absence of every leak or
repair the earlier Gitea runner failures.

```sh
ctest --test-dir build/release --output-on-failure --parallel 2
ctest --test-dir build/debug --output-on-failure --parallel 2
ASAN_OPTIONS=halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  ctest --test-dir build/asan-ubsan --output-on-failure --parallel 2
ctest --test-dir build/release -R '^mmay_cp36_unit$' --output-on-failure
python3 build/cp42-iteration/measure.py
```

All nine architecture, generated registry, C memory policy, roadmap,
documentation, specification, examples, syntax and release-contract gates
passed, as did `git diff --check` and ordinary-PATH `mmay doctor`. Final
source-distribution/release-contract checks passed 2/2 (14.85 s), and install
relocation passed separately (3.03 s). No checkpoint is promoted to verified.
A bounded remote-ref read again
timed out connecting to `ci.example:2222` before authentication and changed no
remote state. Physical x86, GCC/Windows and current pushed CI remain unverified.

## Book attribution

[The reading/application journal](../../research/vladimirov-optimizing-compilers-2026-09-07.md)
records the idea of preserving progress between iterations: §5.2.1, printed
158–160 / PDF 159–161, and §5.2.4, printed 168–169 / PDF 169–170. This adapts
the induction/strength-reduction principle to a sparse collection cursor; it
is not a general LSR pass. The handoff identified the defect before the book
was read. LICM restrictions on printed 173–174 / PDF 174–175 support retaining
the generation barrier when aliases or called functions can mutate the table.
