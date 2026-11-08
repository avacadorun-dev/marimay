# Deduplicated canonical-program verification

> **Private experiment payloads.** Archived implementations and patches
> referenced in this historical report are retained privately. Commands
> and digests below document the original experiment; they are not public
> reproduction instructions. See the [artifact index](private-artifacts.md).

Status: **accepted-local** at `2026-09-11T02:44:39.875384+00:00`. Overall Go/Rust parity,
default JIT enablement, Linux LeakSanitizer and complete Tier 1 remain open.
The 75-minute checkpoint deadline was met.

[Validation, reproduction scripts and identity proofs](cp42-program-verification-validation-2026-09-11.json);
[replayable candidate patch](private-artifacts.md#artifact-028);
[preceding source-phase diagnosis](cp42-source-phase-profile-2026-09-11.md).

## Change and invariants

Canonical source preparation previously checked each bytecode function eight
times. The verified program encoder now uses a private encoder without repeating
the full function check. Program lowering defers function-output verification
to its existing complete-program check. The CLI uses the successful decoder's
complete verification instead of immediately repeating it. Public standalone
encoding/lowering, all MIR checks, function/program work budgets, canonical
encode/decode, complete decoded-program verification and VM admission remain.
No public mutable trust bit, cached validation result, decoder shortcut, VM fuel
change or workload-specific specialization is added. This implements D049;
source and executable semantics do not change.

An instrumented count/identity replay of all 32 unchanged workloads proves
byte-for-byte equality of the complete encoded program before and after.
Catalogue checks fall 536 to 335 (67 functions, eight to five checks each);
algorithm checks fall 704 to 440 (88 functions). All outputs match. This is
separate from the uninstrumented speed measurements. Of 153 native-emitter and
runtime objects, 152 match directly; the generated-build-path artifact owner
matches when compiled with the control's paths. This is scoped object evidence,
not a claim of complete compiler or native executable identity.

## Performance

The predeclared four-case early 3-warmup/11-sample gate records a geometric
mean time reduction of **8.7%** (target at least 7%).
Both complete paired matrices use the exact accepted baseline compiler for
their mode, immutable candidate compilers, all 32 original workloads, 114
unchanged input hashes, original arguments/outputs and the 64 MiB heap limit.
Rotating workload order alternates before/after within each round. Native
compilation time is outside this VM source-run comparison.

JIT-enabled median sum: **9096.90 to 8800.23 ms
(-3.26%)**, geometric mean speedup
**1.0552x**. Default-interpreter median sum:
**26831.08 to 26625.48 ms
(-0.77%)**, geometric mean speedup
**1.0316x**. Long execution-heavy cases have a smaller
preparation share; these results do not establish overall VM/native parity.

The predeclared regression gate is 2% median and 5% P90. P90 is sorted sample
10 of 11. Each flagged case receives exactly one confirmation, and repeating
the same exceeded metric rejects the candidate. No workload exceeds either
threshold in either complete matrix, so no confirmation series was needed
or executed. Every observation is retained.
The sum of medians does not increase in either mode. A process/power/thermal
monitor accompanies every full/confirmation series; host isolation remains
unverified. All testing/building and diagnostic replays completed before these
acceptance timings. The validation aggregator initially read a prior stage's
summary-log path; correcting it required no test rerun. All four current logs
are independently checked for their exact test counts and retained in full;
the original script/aggregation error is preserved in the validation record.

## Validation

Full JIT suite: 96/96. Full interpreter suite: 86/86. ASan/UBSan: 12 focused
suites plus all 32 unchanged workloads. x86-64 under Rosetta: 12 focused suites
plus all 32 workloads. Focused preparation owners test canonical body equivalence,
mutation rejection after successful encoding, finite function/program verification
and encoded-byte limits, sorted program assembly and every deterministic
allocation failure through successful teardown; both live-allocation and
live-byte ledgers return to zero. Existing negative, conformance and VM/native
differential suites remain required. Architecture, central-memory, roadmap,
document-link, spec-term and generated-artifact gates passed locally.

## Complete paired matrix

All values are source-run wall-clock medians in milliseconds. Raw dispersions,
P90 and observations are linked from the validation record.

| Workload | JIT before | JIT after | Interpreter before | Interpreter after |
|---|---:|---:|---:|---:|
| title-search-10k | 112.77 | 104.17 | 142.21 | 133.92 |
| title-search-100k | 579.92 | 565.93 | 871.29 | 869.44 |
| normalize-title | 177.74 | 167.78 | 207.40 | 199.18 |
| word-frequency | 328.53 | 319.48 | 396.49 | 387.59 |
| report-builder | 106.69 | 96.66 | 135.45 | 128.21 |
| report-concat | 180.87 | 171.54 | 210.42 | 203.34 |
| map-int | 89.17 | 80.30 | 103.17 | 94.38 |
| map-string | 113.09 | 103.61 | 131.46 | 122.02 |
| map-string-book | 119.06 | 108.94 | 129.61 | 121.48 |
| set-string-churn | 140.35 | 130.40 | 144.67 | 135.80 |
| catalog-match | 164.18 | 154.80 | 265.29 | 259.28 |
| catalog-dotcall | 140.07 | 130.97 | 195.19 | 187.68 |
| catalog-interface | 180.61 | 171.34 | 255.80 | 248.61 |
| record-reference-copy | 116.41 | 107.52 | 177.61 | 168.38 |
| record-create | 151.52 | 142.12 | 212.96 | 205.27 |
| array-grow | 124.36 | 115.37 | 201.63 | 192.49 |
| array-reserved | 123.86 | 114.86 | 201.31 | 192.70 |
| bfs-int | 102.97 | 93.83 | 215.16 | 206.15 |
| bfs-record | 110.66 | 101.32 | 231.74 | 222.42 |
| catalog-pipeline | 272.21 | 261.39 | 535.49 | 528.71 |
| parse-result | 102.07 | 93.37 | 116.76 | 108.41 |
| insertion | 222.05 | 217.67 | 226.22 | 220.96 |
| heap-sort | 300.27 | 295.61 | 1638.54 | 1635.96 |
| stack | 434.72 | 427.45 | 785.90 | 790.91 |
| queue | 444.21 | 427.88 | 880.84 | 885.74 |
| min-heap | 728.92 | 712.67 | 5431.78 | 5446.99 |
| dsu | 1180.69 | 1180.13 | 3799.48 | 3763.39 |
| bfs | 1375.40 | 1353.78 | 5528.71 | 5528.96 |
| dijkstra | 226.96 | 221.79 | 2387.86 | 2373.40 |
| frequency-map | 290.52 | 280.97 | 473.59 | 470.74 |
| unique-set | 200.13 | 196.10 | 350.63 | 349.30 |
| rune-frequency | 155.91 | 150.47 | 246.40 | 243.63 |

## Fresh five-participant comparison

The [fresh matrix](cp42-program-verification-comprehensive-2026-09-11.md)
([raw observations](cp42-program-verification-comprehensive-2026-09-11.json)) runs all five participants
on all 32 unchanged workloads with 3 warmups and 11 samples. It is descriptive,
not the paired acceptance gate. Native/Go and native/Rust geometric mean time
ratios are 2.04x and 2.18x;
VM/Go and VM/Rust are 18.20x and 19.42x.
VM includes all source preparation; Bun includes fresh-process startup,
TypeScript loading and JIT. The collector's dedicated-host label is unverified;
the independent host monitor and dispersion are retained. Absolute times from
different series are not a causal code-regression comparison.

All 32 Native, Go, Rust and Bun executable hashes match the preceding
five-participant series exactly. Their different absolute times are retained
as environmental variation, not attributed to a compiler change or a specific
concurrent agent. The paired comparisons above establish the VM change.
