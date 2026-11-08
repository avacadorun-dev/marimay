# Prepared JIT activations with an unchanged OFF implementation

> **Private experiment payloads.** Archived implementations and patches
> referenced in this historical report are retained privately. Commands
> and digests below document the original experiment; they are not public
> reproduction instructions. See the [artifact index](private-artifacts.md).

Status: **accepted-local-opt-in** at `2026-09-11T01:22:55.913901+00:00`. The original
18:51 UTC September 10 checkpoint deadline was missed. Overall Go/Rust parity,
default JIT enablement, Linux LeakSanitizer and complete Tier 1 validation remain open.

[Validation and reproduction record](cp42-sljit-prepared-isolation-validation-2026-09-11.json);
[replayable candidate patch](private-artifacts.md#artifact-038);
[previous rejected candidate](cp42-sljit-prepared-activations-2026-09-10.md).

## Result and scope

This change prepares immutable call targets, register initialization templates
and managed-root indexes once per selected JIT function. Bounded per-function
view tables and the native retained-view predicate reduce repeated checks.
The shared activation and frame owners retain argument validation, logical
depth, fuel, root lifetime, precise exits and allocation-failure teardown.
The AOT native emitter and runtime algorithms are unchanged.

The JIT-enabled compiler is byte-identical to the measured second candidate:
`5a1ed60d7f05f3ca38cf6bb6003925d8b1285196131100b7fbd38c93eb6f4db7`. Its existing paired 32-workload matrix records a median sum of
**11195.83 → 10213.08 ms (-8.8%)**. Stack improves 20.5%, queue 21.5%, DSU
15.0%, min-heap 20.0% and BFS 5.3%. The geometric mean speedup is 1.058×;
the sum weights longer workloads more heavily. This reuses measurements of
the exact same executable, not a newly measured series. The original early
stack/queue/DSU admission targets and full JIT gate passed.

## OFF isolation and measurement uncertainty

The second candidate remains rejected under its original OFF performance gate.
After the user reported concurrent agent work, one predeclared A/A diagnostic
compared identical OFF binaries on five affected workloads. All median changes
were within 1.2%, but queue P90 differed by 14.7% despite the observed host
monitor passing. The A/B follow-up was therefore not run. Every observation,
the protocol and process/power monitor are retained in the validation record.
This establishes insufficient P90 repeatability for that diagnostic; it does
not identify the other agent as the cause or erase the old rejection.

V3 compiles prepared-only frame/call additions out when SLJIT is disabled.
407 existing compiler objects match the accepted OFF build directly. The only
two changed objects contain generated build-location constants. Compiling those
current sources with the control's generated paths produces identical objects.
Relinking a copy then reproduces the complete control executable apart from
its linker UUID and dependent ad-hoc signature. The normalized linked image
SHA-256 is `69681a49236e9e0ddd1bcf16c174351d59fbc442fd76513a9d9897915dce577a`.
The added template object is not linked into OFF. The actual candidate keeps
its own build paths. The reproduction script records every command and compared
object, and excludes only the explicitly identified nonexecution metadata.

This is an explicit canonical-build identity proof of an unchanged OFF
implementation, not another OFF timing result or a relaxed numeric threshold.
There is no instruction to repeat failed performance measurements until passing.

## Validation

The current enabled full run passed 95/96 tests; the array-search test timed
out across a recorded 978-second maintenance sleep. Its focused recheck passed
in 4.20 seconds, with the initial failure and power events preserved. The current
OFF full suite passed 86/86. ASan/UBSan passed 12 focused suites and all 32
unchanged workloads; x86-64 under Rosetta passed 11 suites and all 32 workloads.
Prepared/ordinary frame tests exhaust deterministic allocation failures and
assert zero allocation/byte ledgers after teardown. Native view-proof tests
exercise every retained-view guard, bounded table fallback and epoch wrapping.
Architecture, C memory policy and documentation gates passed.

## Fresh five-participant comparison

The same candidate executable (SHA-256 `5a1ed60d7f05f3ca38cf6bb6003925d8b1285196131100b7fbd38c93eb6f4db7`) completed a new [32-workload five-participant series](cp42-sljit-prepared-isolation-comprehensive-2026-09-11.md) ([raw observations and executable/input hashes](cp42-sljit-prepared-isolation-comprehensive-2026-09-11.json)). All 160 participant/workload correctness outcomes passed, using 3 warmups and 11 samples, unchanged 114 input hashes and the 64 MiB managed heap limit. All five columns come from this one series.

Descriptive geometric means of time ratios across the 32 workloads are native/Go **2.12×**, native/Rust **2.22×**, VM/Go **19.98×**, and VM/Rust **20.99×**. Native is no slower than Go on 4/32 median observations and Rust on 0/32. These descriptive ratios do not replace the paired acceptance matrix or establish overall parity. Native generation/runtime are unchanged by this JIT stage.

VM includes loading, checking and lowering source; Bun includes process startup, TypeScript loading and JIT. Native compilation is excluded. Host isolation is unverified. The full report retains per-participant dispersion, noise flags and every raw observation without a replacement run.

Compared with the preceding five-participant series, geometric mean absolute times rise 33.9% for Go, 36.8% for Rust, 33.8% for Bun and 38.8% for native Marimay. For all 32 workloads, the native Marimay, Go, Rust and Bun executable hashes match the preceding series exactly. This common drift is retained as an environment diagnostic; the native Marimay executables themselves did not change. The fresh table is descriptive and does not replace the paired JIT acceptance result.

## Limits of this evidence

- Optional JIT remains OFF by default. This is local implementation acceptance, not overall parity or a release claim.
- The previous candidate remains rejected under its original observed OFF gate. The A/A failure weakens code-causality attribution; it does not erase any measurement.
- V3 adds an explicit canonical-build identity proof for OFF rather than another noisy OFF timing series. Actual build paths, linker UUID and signature differ; current production objects and normalized linked image are compared explicitly.
- Apple M2 timings are local-unverified; VM includes loading, checking and lowering source. No new physical x86 timing is claimed.
- macOS ASan/UBSan uses detect_leaks=0. Linux LeakSanitizer and complete Tier 1 validation remain open.
