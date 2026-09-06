# CP42: bounded JSON string runs

Local macOS arm64 / Apple M2 / AppleClang 21 follow-up to `52e5852`.
This repairs the measured large-config parsing cost. It does not close CP42,
the config application's complete resource acceptance, or a Tier 1 gate.

## Cause and repair

A fresh profile of the unchanged 1,048,459-byte version-last config placed
8,121 of 8,298 main-thread samples in VM execution. The call tree repeatedly
entered ordinary value/nominal instructions, function and enum descriptor
lookup, and frame/root setup. Inspection of `std/json/decode.my` identified
per-byte builder calls, `Result` construction, narrowing, and decoder-field
access in the string parser, including strings with no escapes.

The source package now scans a bounded plain prefix using local offsets.
A closing quote publishes its owned UTF-8 string without a per-byte builder.
An escape, control byte, or exhausted string budget resumes the existing
decoder logic after copying the validated prefix once. The quote/control/escape
error precedence and post-error cursor remain unchanged. Complete-input UTF-8
validation and the decoder's owned input copy still happen before parsing.

This implements existing [D082](../../design/decisions.md#d082-explicit-bounded-typed-json-codecs)
and the [JSON contract](../../spec/standard-library.md#112-decoder), with no
source-semantic or public-API change. The implementation remains ordinary
Marimay, shared by the VM and native paths. There is no host JSON parser,
special JSON bytecode operation, skipped verification, or application rewrite.

## Correctness

The new focused `string_runs_test.my` owner adds five source tests to the eight
existing CP38 tests. It covers plain/escaped Cyrillic and supplementary scalars,
all JSON escapes, empty/exact/over-budget strings, every raw control byte at
the budget boundary, malformed Unicode and truncation, UTF-8 split by a byte
limit, decoded duplicate-key identity, skip behavior, and input-copy ownership.
Error checks compare exact variants, resource/limit values, byte spans, empty
paths, and the cursor exposed by `missingField` after failure. The long test
retains the full 256 KiB default string boundary.

All 13 tests also passed on the unchanged `52e5852` compiler in VM mode before
using them as regression evidence. An initial test-authoring attempt used
unsupported Marimay string escapes `\b`/`\f`; those expected literals were
corrected to the already supported `\u{8}`/`\u{c}`. JSON escape acceptance
and the language's closed literal-escape set were not changed.

Release CP38/typed-JSON tests passed together (24.51 s). Debug CP38, typed JSON,
and local-execution tests passed (52.98 s). ASan/UBSan CP38, typed JSON, local
execution, CLI heap, and the CP38 embedding/allocation owner passed (149.95 s).
The unchanged external config app passed 32 exact-parity process scenarios
across VM/native debug/native release plus atomic-write, denied-I/O, and
broken-pipe checks (60.07 s). Concurrent correctness runs are not latency data.

The complete Release CTest run passed all 68 tests (273.28 s), including
the native owner (255.83 s) and source distribution. Architecture, C-memory
policy, generated artifacts, documentation links, roadmap, spec terminology,
example syntax, syntax coverage and release-contract gates passed without
exceptions or relaxed rules. `mmay doctor` found both matching native runtime
profiles in the sole language checkout. This remains local execution evidence.

The rebuilt real-package JSON fuzzer completed 6,183 executions in 181 s with
ASan/UBSan and no failure artifact, timeout, or sanitizer report. It used a
disposable copy of the ten committed JSON seeds, seed `20260906`, a 4,096-byte
maximum, 30 s per-input timeout, and 512 MiB RSS limit. This bounded local run
does not replace the sustained Linux CI campaigns or Linux LeakSanitizer.

## Paired latency evidence

`tools/compare-vm.py` in the external config repo ran three warmup and eleven
measured rounds, alternating participant order, with sequential fresh processes.
Our builds and tests were not run concurrently with these latency measurements;
ordinary desktop applications remained open. Every invocation checked exact
JSON stdout, exit 0 and empty stderr. No workload, budget, or business source
changed. This is local exploratory evidence, not a dedicated D056 runner.

| Version-last config | `52e5852` | String-run repair |
|---|---:|---:|
| Median | 9.449318 s | 5.243337 s |
| Median absolute deviation | 0.106902 s | 0.101703 s |
| Range | 9.311893–11.061828 s | 5.138928–5.684333 s |

The median fell 44.5% (1.80× faster). All eleven candidate samples meet the
unchanged 10 s budget; the baseline again exceeded it. This addresses the
previous report's large-input headroom concern on this host.

A subsequent sequential `tools/measure.py` run retained all workloads:

- small VM first launch: 0.288047 s; all seven repeats 0.285405–0.294818 s,
  median 0.287568 s, within the 0.3 s repeat budget in this run;
- large VM, version first/last: 3.884714/5.303503 s;
- large native, version first/last: 0.274659/0.343826 s;
- seven both-engine tests: 2.157090 s first invocation after the library change,
  then 0.591233/0.583411 s;
- large-input RSS: 165,249,024 bytes VM and 96,436,224 bytes native, below
  256 MiB. RSS was higher than the previous single run, so this is a bound check,
  not evidence of reduced memory use or a paired memory comparison.

The first invocation of a newly built native file still took 0.384509 s,
missing the 0.1 s budget. Later calls took 0.003946–0.007511 s. It remains open;
neither their median nor the passing JSON timings hides that first launch.

## Native-startup attribution boundary

The external `tools/startup-marker.c` and `tools/trace-native-startup.py` also
tested fresh, synthetic C17 executables containing no Marimay runtime. Two
unobserved files took 0.574165/0.382153 s from immediately before `exec` to the
first timestamp in `main`; repeated launches took 0.003195–0.007418 s. Work
after that timestamp took less than 0.7 ms. A third observed file showed the
same pre-main delay, but its sampler returned an empty call graph, so it cannot
attribute that interval to a stack frame or service.

Scoped host logs contained AMFI signature diagnostics for those exact probe
paths. They are correlation, not proof that AMFI caused the measured delay;
all controls exited successfully. No host security, quarantine, signing policy,
or cache was changed, and no user code was pre-executed during a build. The
remaining host-startup cause is not declared repaired by the string change.

## Reproduction and identity

Run from `Marimay-Examples/marimay-config`, choosing unused report filenames:

```sh
python3 tools/compare-vm.py \
  --baseline ../../marimay/build/dispatch-baseline/mmay-52e5852 \
  --candidate ../../marimay/build/release/mmay \
  --input build/resource-probe-20260906/near-limit-last.json \
  --output build/paired-string-runs-new.json
python3 tools/measure.py --output build/measurements-string-runs-new.json
python3 tools/acceptance.py
```

SHA-256 (raw reports live in that external repository):

```text
b4b5a6c95da1956ef5e696aaacd8d2b9c47a58710669b613efaae16c8cf9e3c2  baseline Release compiler
42136431c5d6f4844dccfb07aaf2933343a670df4beaf7e60eda3dd060a0d13e  repaired Release compiler
e8b36fe61735866040d39a4ab5492ba6ab390879c843534fc2672411df64ceb7  near-limit-last.json
589261b563b4b436f6d15ad8c3cb677889721ba262b1e6b44e9e5b7304d2afa8  paired-string-runs-last-20260906.json
7f9645bdb95e54eda2e68899f7a006f59a0e4d9f78655b431c7ed6f9fa02fc46  measurements-string-runs-20260906.json
04c97b863b226da169dd3b76f863d9f2affa7abe384241c6bf27ac716f010a03  profile-native-batch-last-20260906.txt
d17c2fd4a0e94b1cef56b7ad4cce97a27d38c9a6d3dbd5a77247fbf54de43757  startup-trace-20260906.json
```

Business-source identities remain those in the
[config performance report](cp42-config-performance-2026-09-06.md). The baseline
is a saved binary in ignored `build/`, not a second compiler checkout. The new
Map/Set optimization obligation, first-native-launch issue, remaining config
resource/permission/symlink acceptance, and full Tier 1 work remain open before
advancing or declaring the alpha ready.
