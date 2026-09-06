# CP42: native collection results and verifier worklists

Local macOS arm64 / Apple M2 / AppleClang 21 follow-up to `4def3af`.
This is a partial repair of the required Map/Set performance item, not its
completion and not a Tier 1 or alpha gate.

## Measured cause and repair

The unchanged frequency-map workload repeatedly entered generic enum
construction after managed map get/put. A sampled repeated-entry profile put
about 22% of main-body samples in that constructor path. These scalar enums
already used immediate storage: the cost was repeated descriptor validation
and lookup, not one heap allocation per result.

The native collection emitter now uses the existing generated-enum ABI with
the canonical descriptor ID registered at program entry for None, scalar Some,
and successful bool Result payloads. Reference-valued Some and growth-error
paths retain their rooted checked construction. Runtime map validation,
SipHash, probing, insertion order, mutation generations, allocation behavior
and public signatures are unchanged. This implements existing
[D079](../../design/decisions.md#d079-ordered-map-set-and-checked-hash-key-coherence)
and [collection execution](../../spec/execution.md#422-result-bearing-collection-operations);
it introduces no source semantics, syntax, opcode or runtime ABI.

## Paired measurements

Three warmups and eleven measured rounds alternate baseline/candidate order.
Every sample is a fresh sequential process with exact stdout checksum, exit 0,
empty stderr and a 30 s timeout. Both participants use identical existing
`algorithm-benchmarks` business source, inputs, operation counts, release
flags and 64 MiB heap limits. No builds or tests ran alongside these samples;
ordinary desktop apps remained open. Outliers are retained. This is exploratory
whole-process latency, not a dedicated D056 performance runner or a new Go/Rust
comparison.

| Workload | Baseline median | Candidate median | Time reduction |
|---|---:|---:|---:|
| frequency-map | 84.049 ms | 60.995 ms | 27.4% |
| unique-set | 58.189 ms | 46.836 ms | 19.5% |
| rune-frequency | 47.394 ms | 35.876 ms | 24.3% |

[Raw samples](cp42-collection-results-2026-09-06.json) are retained with this report.
The paired driver and both sets of native executables remain in ignored
`build/collection-result-perf.Pc2ktN/`. The baseline compiler is saved
as `build/dispatch-baseline/mmay-4def3af`, not another compiler checkout.
The measured candidate binaries include the native result repair before the
subsequent bytecode-only verifier repair.

```text
42136431c5d6f4844dccfb07aaf2933343a670df4beaf7e60eda3dd060a0d13e  baseline compiler
55ee801193de3a1ad6820051b45cdb18166e318cc895c20fc2cc968535a5e396  first-paired.json
096fddd8b76a9817ebc8accf6af2d6fc03bd4ab99aa29762189f2b11ce3fb98f  compare.py
3410d7fb73fdb86f60066d8c474bea205a23067d9f4bd4f874f0f381b443cd35  baseline frequency-map
165f1e05e8293021ab1485f7da379abdd6bf91e0cf641bbb1f7c36a8facb26cd  candidate frequency-map
0b5cca0c3a4f7f79b931e0a389da273130c9c21421336aa6b3b3bc1f433c9947  baseline unique-set
e2ed495b3002d042b3c84e234e2f7e48ba0d24c789a04b656576a91ead0c949f  candidate unique-set
8d06af284279bfded7bc4bc090ec37b08dea686aa511c6564b3f3c8b1e5a8927  baseline rune-frequency
823fc394a53eedd1ba78149308746e8fb82e939114c955a054162bb69ea7e46c  candidate rune-frequency
```

## Verifier defect exposed by the boundary tests

The new scalar/reference result tests passed all seven CP36 cases with the
baseline native compiler, but its VM preparation failed with I0002. LLDB
identified `MMAY_BYTECODE_VERIFY_LIMIT_REACHED`: a generated function with
188 blocks and 452 registers exhausted the unchanged 16,000,000 work budget.
Both definite-assignment and proof-provenance fixed points repeatedly scanned
every block, including blocks whose inputs had not changed.

Separate focused owners now use successor worklists with deduplication.
They preserve the entry boundary, meet every incoming edge and transfer the
same facts. The assignment queue reuses finished predecessor cursor storage;
each provenance lattice owns and cleans up a temporary queue. The default
limits and existing assignment work units remain unchanged. The new source
test now passes in both engines without splitting or shrinking its function.
LLDB measured 12,810,416 total verification work units for that function after
the repair, with `MMAY_BYTECODE_VERIFY_OK` and the same 16,000,000 limit.

A separate negative test also confirmed a pre-existing inconsistency: block
transfer invalidated array proofs on void calls/stores, but the final
instruction walk skipped those non-register-writing opcodes. Both walks now
use the same invalidation predicate. A new explicit bounds check restores the
proof; retaining the old carrier does not. This restores existing
[D058](../../design/decisions.md#d058-bounds-proofs-are-verified-mir-capabilities)
and [hostile-bytecode verification](../../spec/execution.md#43-verification).

Focused tests cover a 65-block reverse-ordered CFG under a 10,000-work budget,
conflicting/missing proofs, existing joins/backedges, tiny-budget rejection,
void-call/store invalidation and restoration. Loop and reverse-CFG allocation
failure sweeps assert both live allocation count and live bytes return to zero.
The CP36 source boundary tests cover signed/unsigned extrema, narrow integers,
bool, float and rune payloads, distinct generic descriptors, nested options,
absence, saved values, removed reference payloads and insertion order.

## Local verification

The complete Release suite passed all 68 tests (360.62 s, parallel 2), including
the native owner, JSON, distribution and all existing bytecode negative tests.
ASan/UBSan bytecode, CP36 unit/source and local-execution suites passed 4/4
(117.40 s). The CP36 unit owner includes generated-C O0/O2, forced collection
and deterministic allocation failures. Rebuilding the final test owner and
rerunning bytecode tests passed in Release (0.66 s) and ASan/UBSan (3.58 s),
including both loop and reverse-CFG allocation sweeps. Debug bytecode, CP36
unit and CP36 source suites passed 3/3 (18.09 s). The staged source-distribution
retest passed (14.75 s); source formatting and both runtime profiles in
`mmay doctor` passed as well.

The rebuilt bytecode fuzzer completed 13,979,434 executions in 181 s with
ASan/UBSan and exit 0. It used a disposable copy of the three committed
bytecode seeds, seed 20260906, max input 4096 bytes, timeout 30 s and RSS limit
512 MiB. There was no reported failure artifact or sanitizer error. This is
a bounded local campaign, not sustained Linux CI or Linux LeakSanitizer.

Architecture, memory policy, roadmap, generated artifacts, documentation links,
spec terminology, example syntax, syntax coverage and release-contract gates
passed without exceptions or relaxed rules. Correctness runs overlapped other
builds/tests and must not be interpreted as paired latency measurements.

The previous revision's Linux [run 195 sanitizer job](http://ci.example:3000/Marimay/marimay/actions/runs/195/jobs/1654)
is still red: a misaligned GreenNode report in CLI-run allocation testing plus
CLI-check/native-test expectation failures. The corresponding three local
ASan/UBSan CLI suites passed (68.26 s). This does not explain or resolve the
Linux report; it remains a separate investigation and no CI gate is closed.

## Remaining work

The residual hashing/probing and managed-handle/descriptor costs still need
profiling and repair before this optimization obligation is closed. The
reported gains do not claim parity with the user's Go/Rust figures. Fresh
native startup and the remaining config acceptance gates also stay open.

Two source ergonomics defects discovered while authoring the tests remain
explicit follow-ups: direct rune equality is rejected despite the D080
primitive Eq contract, and unsupported tuple binding in a `for` header can
produce a defensive formatter-limit diagnostic instead of a useful syntax
error. The result test currently checks exact rune identity through Set and
uses the supported entry binding plus tuple match; neither workaround closes
those language issues. Do not advance to the next portfolio utility first.
