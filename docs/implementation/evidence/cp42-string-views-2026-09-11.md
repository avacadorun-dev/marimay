# Retained string bytes in Native, interpreter and JIT

> **Private experiment payloads.** Archived implementations and patches
> referenced in this historical report are retained privately. Commands
> and digests below document the original experiment; they are not public
> reproduction instructions. See the [artifact index](private-artifacts.md).

Status: **accepted locally**, against `d7f48b65550adebcf6e00974f14a99fe8926df37`. The active
family remains the original title-search 10k/100k pair. Normalization and
word-frequency follow it in the fixed workload order. This step does not prove
all32 parity, complete Tier1 conformance, Linux LeakSanitizer or physical x86
performance. JIT stays opt-in.

[Protocol, validation and results](cp42-string-views-2026-09-11-summary.json),
[raw evidence](private-artifacts.md#artifact-055), [candidate patch](private-artifacts.md#artifact-054),
[initial phase diagnosis](cp42-string-views-2026-09-11-diagnosis.md). Applying the patch to the parent
with `git apply --unidiff-zero` reproduces tree `4a1ec2ab45c674533789fab9f6a115a9490856af` in an isolated index.
The summary records archive hashes, exact compiler/binary/source hashes and all
rejected attempts. Raw evidence retains scripts, commands, diagnostic sources,
all observations, host snapshots, profiles and failed setup attempts.

## Work removed

Original title100k performs 7,994,208 comparisons. The accepted baseline performs
one checked string-byte lookup per comparison in Native, and three in VM: child
conversion, then both comparison operands. The baseline diagnostic measured about
36.6ms Native and 413.3ms JIT-enabled VM in repeated search, without allocation or
GC there. Source preparation alone costs about 40ms in title10k, followed by
about 3.7ms VM/JIT admission. Those are instrumented diagnostic values, not the
acceptance timings or a single additive elapsed-time observation.

Native now projects immutable bytes from an exact validated rooted array child,
or immediately after successful string construction. No safepoint intervenes;
other cell conversions and untrusted host references retain checked access.
The JIT validates the selected string before publication and retains its byte
view. Reuse checks complete reference/provenance, execution/function, type/layout
and GC epoch; cold overwrites and epoch saturation retain checked fallbacks.

D104 interpreter regions now also support checked string-array reads and byte
equality. A separate owner keeps two checked string views within one nonallocating
region. Full reference identity is compared on reuse; every view expires at a
cold exit, call, allocation or GC boundary. Parent descriptor, bounds, selected
child identity and failure transaction remain checked. Equality validates both
operands before publishing its boolean. Empty and embedded-zero strings preserve
byte semantics. The existing array-cache predicate combines all initialized
identity fields without a short-circuit branch for each field. Capacity,
replacement, cache lifetime and checked miss handling remain unchanged.

Two counter replays per original size and engine preserve every phase transition,
string creation, byte allocation and comparison. Native search checked-byte calls
fall 7,994,208→0; both ON and OFF fall 23,982,624→7,994,336: one check per selected
string plus 128 query bindings. Native setup checks fall 400,000→0. GC remains
zero. Counter-only runs may overlap validation; none of their durations is speed
evidence. No source/workload identity selects production optimizations.

## Validation and controls

Full fresh CTest passes **103/103 ON** and **88/88 OFF**. Focused macOS ASan/UBSan
and x86-64 Rosetta each pass **20/20**, including Native and the new actual-region
string test; both reproduce all32 original VM outputs. Tests cover checked/proven
reads, signed bounds, empty/NUL/Unicode bytes, cache eviction, aliases, overwrites,
rooted GC and stale/wrong-heap/wrong-kind children, exact finite/shared/SIZE_MAX
fuel, unchanged failed destinations and every reachable allocation-failure index.
Allocation and byte ledgers return to zero, including JIT code pages. macOS uses
`ASAN_OPTIONS=detect_leaks=0:halt_on_error=1`; Rosetta is functional evidence.
LinuxLSan, full Tier1 and physical x86 timing remain open.

The preceding task stopped during its x86 test selection after 19 reported
passes. Its unfinished command record and original log remain archived. This
task repeated the complete 20-test x86 selection, then completed the original
all32 x86 correctness replay and final gates. The validation-resume record
identifies the replacement command; the interrupted run is not a completed pass.

Immutable measurement compilers are separate from full validation builds: CP39
packaging may rebuild its compiler. All32 original source/argument/stdout cases,
114 source/harness hashes and the 64MiB heap limit stay fixed. V4 Native binaries
are identical to V2, and all32 differ from the accepted control. All47 Release
runtime object files and all48 archive member bodies match the control; archive
container headers differ. Fresh Native binaries must match the frozen candidate,
and Go/Rust/Bun executables must match accepted comparator hashes.

## Rejected attempts and final gates

V1 improves the Native early pair by 14.845%, missing its fixed 15% minimum;
its 54.22% VM improvement does not erase that rejection. V2 adds the independently
justified constructor-result projection. Its early gates and full Native/JIT
measurements pass, but OFF fails: sum +3.92%, with title100k and Dijkstra exceeding
the confirmed 5% per-row gate. V2 remains rejected-full. A later host audit also
finds long monitor gaps during OFF timing, retained explicitly in the archive.
No favorable rerun replaces that rejected result.

Diagnostic identical-binary A/A controls give about 2.25s for accepted OFF and
2.67s for V2 OFF on Dijkstra, with within-binary median differences below 1%.
Sampling attributes the extra time to the existing array-cache comparison loop;
its hot machine code is unchanged apart from relocation operands. This supports
a layout-sensitive cost hypothesis, not proof of a particular CPU mechanism.
V3 combines the complete cache identity predicate. Dijkstra returns to control
speed, but title100k is +5.45% in the predeclared repair run; V3 stays rejected.
V4 additionally removes redundant string validation inside interpreter regions.
Its repair run improves OFF title100k by 48.45%, while Dijkstra is -0.71%.

V4 early Native improves 16.85% and ON improves
54.53%. Both original minimum gates pass; the Native 30% target is
missed, separately from its 15% acceptance minimum. Every accepted timing stage
uses 3 warmups and 11 rotating, alternating fresh-process pairs. Known foreign
compute is monitored every 5s; V4 also rejects monitor gaps over 15s. These are
local checks, not a claim of a dedicated host. Builds/tests/profiles do not overlap
acceptance timing. Any per-row median/P90 regression over 5% receives exactly one
confirmation; a confirmed excess blocks acceptance. Each engine's summed medians
must not regress more than 1%. Smaller positive changes remain visible tradeoffs.

| Workload | Native before → after ms | Native change | VM+JIT before → after ms | VM change | OFF before → after ms |
| --- | ---: | ---: | ---: | ---: | ---: |
| title-search-10k | 9.19 → 7.84 | -14.65% | 90.91 → 57.15 | -37.13% | 124.82 → 84.79 |
| title-search-100k | 78.79 → 59.86 | -24.02% | 491.92 → 161.84 | -67.10% | 789.78 → 429.87 |
| normalize-title | 58.76 → 57.41 | -2.28% | 131.95 → 128.67 | -2.48% | 167.91 → 163.54 |
| word-frequency | 147.04 → 144.20 | -1.94% | 266.20 → 253.24 | -4.87% | 330.25 → 313.04 |
| report-builder | 15.19 → 15.08 | -0.77% | 79.88 → 79.77 | -0.14% | 110.16 → 107.86 |
| report-concat | 100.53 → 101.40 | +0.87% | 157.80 → 159.28 | +0.93% | 189.00 → 187.12 |
| map-int | 7.70 → 7.46 | -3.09% | 66.47 → 65.71 | -1.14% | 80.69 → 80.08 |
| map-string | 23.96 → 23.45 | -2.11% | 88.27 → 82.63 | -6.39% | 104.72 → 102.55 |
| map-string-book | 31.65 → 31.00 | -2.03% | 90.82 → 89.67 | -1.26% | 103.33 → 101.49 |
| set-string-churn | 47.83 → 47.91 | +0.17% | 109.46 → 107.46 | -1.83% | 117.34 → 114.36 |
| catalog-match | 13.32 → 13.06 | -1.99% | 120.13 → 119.51 | -0.52% | 206.50 → 204.59 |
| catalog-dotcall | 8.32 → 8.19 | -1.47% | 102.83 → 100.38 | -2.38% | 154.33 → 150.85 |
| catalog-interface | 12.18 → 11.91 | -2.20% | 125.63 → 124.37 | -1.00% | 183.25 → 181.71 |
| record-reference-copy | 6.89 → 6.79 | -1.35% | 82.77 → 82.70 | -0.08% | 139.91 → 137.62 |
| record-create | 22.99 → 23.67 | +2.94% | 111.98 → 112.77 | +0.70% | 170.01 → 169.08 |
| array-grow | 6.65 → 6.84 | +2.77% | 80.73 → 80.72 | -0.02% | 160.35 → 160.50 |
| array-reserved | 6.69 → 6.52 | -2.49% | 80.85 → 81.41 | +0.69% | 159.73 → 159.18 |
| bfs-int | 7.40 → 7.25 | -2.04% | 71.27 → 71.15 | -0.16% | 174.10 → 172.56 |
| bfs-record | 9.26 → 9.48 | +2.31% | 79.71 → 77.89 | -2.28% | 186.46 → 186.38 |
| catalog-pipeline | 38.66 → 37.18 | -3.81% | 222.12 → 203.21 | -8.51% | 444.54 → 414.28 |
| parse-result | 19.45 → 19.71 | +1.33% | 74.54 → 74.67 | +0.17% | 91.07 → 90.70 |
| insertion | 50.25 → 50.63 | +0.76% | 206.40 → 206.05 | -0.17% | 213.24 → 212.34 |
| heap-sort | 32.83 → 32.28 | -1.67% | 240.02 → 242.41 | +1.00% | 1438.47 → 1470.40 |
| stack | 9.97 → 9.81 | -1.59% | 260.52 → 259.26 | -0.48% | 604.19 → 606.12 |
| queue | 9.68 → 9.76 | +0.80% | 252.88 → 253.30 | +0.17% | 677.13 → 671.97 |
| min-heap | 47.83 → 47.14 | -1.44% | 514.84 → 524.06 | +1.79% | 4561.48 → 4252.00 |
| dsu | 29.74 → 30.21 | +1.56% | 769.01 → 769.08 | +0.01% | 3307.22 → 3292.15 |
| bfs | 68.04 → 66.99 | -1.55% | 655.49 → 654.77 | -0.11% | 4429.23 → 4416.95 |
| dijkstra | 43.46 → 42.68 | -1.80% | 198.93 → 201.06 | +1.07% | 2253.02 → 2234.91 |
| frequency-map | 33.77 → 33.80 | +0.10% | 196.79 → 197.73 | +0.48% | 375.13 → 379.70 |
| unique-set | 28.19 → 27.70 | -1.74% | 131.75 → 132.75 | +0.76% | 264.96 → 262.24 |
| rune-frequency | 22.43 → 22.20 | -1.00% | 107.13 → 107.33 | +0.18% | 195.45 → 193.10 |

native: sum **1048.61 → 1019.41ms**, **-2.78%**; geometric latency ratio 0.9788. Initial flags: `{"bfs-record": ["p90_ratios"]}`. Confirmed blockers: `{}`.

on: sum **6260.00 → 5862.01ms**, **-6.36%**; geometric latency ratio 0.9440. Initial flags: `{}`. Confirmed blockers: `{}`.

off: sum **22507.77 → 21704.00ms**, **-3.57%**; geometric latency ratio 0.9569. Initial flags: `{}`. Confirmed blockers: `{}`.

The [fresh five-way comparison](cp42-string-views-2026-09-11-comprehensive.md) supplies Native, Go,
Rust, original source-run VM+JIT and Bun values. Native excludes compilation;
VM includes source loading/checking/lowering/JIT, and Bun includes process
startup, TypeScript loading and fresh JIT. These are distinct product boundaries.

The initial Native fixture used unsupported string `+`. Its corrected version
exposed an existing B0001 lowering failure for a string-returning call directly
on an indexed-assignment RHS. A named temporary works on both accepted and
candidate compilers; the equivalent fixture keeps the alias/root/GC checks.
The initial V3 fixture used unsupported `int(value)` conversion syntax; it was
corrected to typed arithmetic and explicit value assertions before timing.
Those failures, the minimal B0001 repro, component probes, old F32 descriptor gap
and D055 string-budget preflight precedence defect remain recorded and open.

The original stage deadline, 15:22:50 UTC, was missed. The V3/V4 repair stage keeps
its deadline 2026-09-11T18:04:25.399753+00:00; V4 began 2026-09-11T16:17:12.839301+00:00 and evidence was
prepared at 2026-09-11T17:56:19.056522+00:00. Actual commit/push time is recorded separately.
This result closes neither title row nor the overall parity goal.
