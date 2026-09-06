# CP42: prepare test bytecode once per invocation

Scope: local macOS arm64 / Apple M2 / AppleClang 21 follow-up to
`1b6a89fa18729b8c1327cfd8954bd174f7bd2c5e`. CP42 and the external config
application remain incomplete; this is not a public-alpha or Tier 1 claim.

## Cause and change

After the previous dispatch/proof repairs, sampling the config application's
seven-test both-engine loop attributed 538 of 1,381 main-thread samples to
repeated whole-program lowering, encoding and decoding. The CLI prepared the
same complete function set independently for every test.

`cli_test_bytecode.c` now owns one invocation-scoped decoded program. The test
coordinator prepares it lazily at the first VM test and disposes it on every
command-exit path. Native-only and empty suites do not prepare VM code. Each
test selects its actual symbol and verifies the complete program. Entry
selection failure restores the prior index. `mmay_bytecode_run_program` still
verifies at its normal public boundary; no unverified execution path was added.
Heaps, arguments, capabilities, streams, captures and panic state are created
and destroyed separately for each test, as before. No runtime values or test
results are cached. Native preparation and fresh child processes are unchanged.

This implements the existing D051/D066 isolation contract and the toolchain
specification's permission to share compiled code, not mutable runtime state.
It changes no source syntax, language semantics, bytecode format or public CLI
option. The new narrow owner/header avoid adding preparation state to the VM
or using global mutable caches.

## Correctness coverage

`test_cli_test_bytecode.c` checks repeated entry switching, stable body storage,
exact encoded-byte identity against independent preparation, unknown entries,
duplicate/uninitialized MIR, rejection of a damaged prepared body, replacement
cleanup, and every deterministic allocation-failure index through preparation
and entry verification. All allocator-backed cases assert both zero live
allocations and zero live bytes after teardown.

Final review extended the failure sweep to construct borrowed MIR using the
same fault allocator: MIR verification uses the function's allocator, not the
output allocator. Injection starts only after the input is built; teardown
checks include both input and output allocations. This closes a gap in the
first test draft rather than assuming the bytecode-only sweep covered it.
The expanded final CLI-test owner passed again in Release (4.67 s), Debug
(11.86 s), and ASan/UBSan (15.05 s), after the full suites described below.
Production code and the measured compiler hash did not change during this
test-only coverage extension.

The adjacent `checkpoint11-prepared` fixture mutates injected Unicode
arguments, enters/exits output capture, panics with deferred output, and then
runs both zero-argument and argument-taking tests. Exact output, status and
empty stderr are checked in VM, native and both modes. It intentionally has
one matching failure followed by three passing tests, and was also run on the
unmodified `1b6a89f` binary to establish the pre-existing contract. Initial
fixture authoring was corrected to include `language "0.1"` and use supported
element-wise assertions; array-valued `expect.equal` is not currently accepted.

Focused Debug passed six CTests (59.71 s): bytecode, CLI test, CLI heap, CP31
arguments, typed JSON and local execution. ASan/UBSan passed the same six
(97.60 s). This is not Linux LeakSanitizer evidence. The unchanged external
application passed 32 exact process cases in VM/debug native/release native
plus atomic-write, denied-I/O and broken-pipe checks (102.94 s). These were
overlapping correctness runs, not latency measurements.

Full Release passed all 68 CTests (241.63 s), including the installed source
distribution and all eight CP31 allocation shards. The complete Debug C-unit
selection passed all 26 tests (203.25 s). The new fixture's direct baseline
run initially left four generated C files under its source-side `.mmay`;
the architecture checker correctly rejected these as source-tree artifacts.
That harness-owned cache was moved into ignored `build/` (not deleted or
exempted), and the unchanged architecture checker passed. All subsequent unit
runs use their existing build-tree fixture copies.

The staged source-distribution recheck passed (16.59 s). Running the architecture
scanner concurrently with that distribution test hit a disappearing temporary
`CMakeLists.txt` during its cleanup; the architecture gate was rerun separately
after distribution teardown and passed. No rule or exception was relaxed.

Commands:

```sh
cmake --build build/release -j4
cmake --build build/debug -j4
cmake --build build/asan-ubsan -j4
ctest --test-dir build/release --output-on-failure -j4
ctest --test-dir build/debug --output-on-failure -j4 -L c-unit
ctest --test-dir build/debug --output-on-failure -j3 -R 'mmay_c_unit_(bytecode|cli-test|cli-heap|cp31)$|mmay_cli_local_execution$|mmay_typed_json$'
ctest --test-dir build/asan-ubsan --output-on-failure -j3 -R 'mmay_c_unit_(bytecode|cli-test|cli-heap|cp31)$|mmay_cli_local_execution$|mmay_typed_json$'
python3 tools/acceptance.py # from the unchanged external config application
```

## Paired performance evidence

`tools/compare-test.py` uses three disposable fixture modules. In each pair
both compilers see the exact same source path; only that harness's generated
native cache is moved aside between participants. Each participant starts with
no `.mmay` directory, then runs twice more with its own cache. Participant order
alternates. No source edits, test omissions, budget changes or OS-cache eviction
are involved. Our builds/tests had completed before this sequential measurement;
ordinary desktop background applications remained running.

| Seven tests, VM + native | `1b6a89f` | Prepared bytecode |
|---|---:|---:|
| Empty native-cache median of 3 | 10.867077 s | 10.179814 s |
| Empty native-cache range | 10.728508–11.161645 s | 10.073093–10.553562 s |
| Repeat median of 6 | 1.812848 s | 1.242902 s |

Every command produced the same seven-pass report, exit 0 and empty stderr.
All seven generated C files matched byte-for-byte within each pair. Warm time
fell 31.4%; cold time fell 6.3%. All three cold candidate samples still exceed
the predeclared 10 s limit, so this does not close performance acceptance.

Compiler SHA-256:

```text
dd569c22e561108b8c9273f71698d93798e100e1eac5e85aacd7f32813668483  baseline 1b6a89f Release
0a108ed25f4287e24200c9b2e4ae9fb1eccbba3e423215f0078ba6fa8f673645  prepared-bytecode Release
```

The baseline is a saved binary under ignored `build/dispatch-baseline`, not a
second checkout. Business-source hashes are unchanged from the
[prior report](cp42-config-performance-2026-09-06.md). Raw local reports in the
external `Marimay-Examples/marimay-config` repository:

```text
bb80ac28dc332ee56896ca4208c33df6fea472566ec4213c922879ef5bacda9c  build/paired-test-preparation-20260906.json
fd8f2c3cbde1c3a8cd55bdd71a1a6586ec57b26c7e35c55ed7d9a7ca81aac7a5  build/profile-test-after-relations-20260906.txt
```

Reproduce with new output names:

```sh
python3 tools/compare-test.py --baseline /path/to/1b6a89f/mmay --candidate /path/to/new/mmay --output build/new-preparation-comparison.json
python3 tools/profile-cold-test.py --output build/new-cold-test-profile.txt
```

Next: profile the remaining cold native build/launch pipeline and fix its
redundant preparation without merging mutable test state or weakening checks.
Do not advance to the manifest utility. The small-run and first-new-native
startup misses, VM/native stack-policy mismatch and broader portfolio/resource/
platform acceptance also remain open.

## Remaining cold-path attribution

After all our correctness runs finished, `tools/profile-cold-test.py` ran the
same seven tests in a disposable module with no native cache. It passed with
exact output and empty stderr. The profile contains 7,901 main-thread samples:
4,633 (58.6%) are blocked reads waiting for `mmay_native_build_compile`'s C
compiler child; 2,347 (29.7%) are in native test-process capture, almost entirely
`poll`. These are sample proportions, not CPU utilization or a new benchmark
baseline. The elapsed value including the sampler is 10.228 s.

The run generated seven separate C files, each 4,045,463 bytes, and seven
separate executables. The coordinator passes the same complete MIR function set
to `mmay_native_build` each time, changing the entry symbol. This identifies
the next repair: invocation-scoped native code preparation with a fresh child
execution for every test. It must preserve user arguments, heap/capability
limits, deterministic order and exact panic/output observations. No such
native reuse is implemented by this bytecode-only change.

Raw reports (the script cleans only its temporary module; reports remain):

```text
1c7478f7e27af76c314eff02a1cda2a7bb5fcbdb18a08690b43731361cee0b96  build/profile-cold-test-prepared-20260906.txt
144fc56a802a9fc52f741468fd6413c343897dedefb290e643ac3723ecf99440  build/profile-cold-test-prepared-20260906.json
```

The architecture, C-memory, generated, spec-term, documentation-link, example,
syntax, roadmap and release-contract checks pass. CI for this revision has
not been verified: the browser is unavailable while the Mac is locked, and
unauthenticated HTTP inspection returned 404. Local checks do not establish
Linux sanitizer, fuzz or complete Tier 1 success.
