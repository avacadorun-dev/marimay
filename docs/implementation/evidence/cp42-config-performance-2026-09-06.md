# CP42 config performance: unnecessary dispatch and proof searches

Scope: local implementation follow-up to `7f68843fa2` (D087), macOS arm64,
Apple M2 / AppleClang 21, Release compiler. This does not close CP42, CP41,
the external configuration application, Tier 1, or any public release gate.
The user requires performance defects to be diagnosed and repaired before
advancing to the next external utility, not merely entered into a findings log.

## Causes and repairs

Sampling the external `Marimay-Examples/marimay-config` validator on a
1,048,459-byte document showed scalar VM instructions repeatedly probing array
mutation, Map/Set, numeric, text, filesystem and context handlers before their
actual switch case. `vm_value.c` now dispatches each opcode directly to the
same owner. Numeric/format/concat cases already present in the switch replace
their redundant preliminary probes. Mutation, collection, text, filesystem and
context families have explicit cases. Unsupported opcodes still fail.

Sampling its seven-test `mmay test . --engine both` loop then placed 3,580 of
4,873 main-thread samples in the native add-one relational proof. The transfer
owner searched the entire function for each register's definition before
checking whether that register could be a live equivalent expression. It did
this even for non-arithmetic instructions. `c_emitter_relations.c` now checks
the existing arithmetic-kind and live-expression predicates first. Only eligible
candidates cause a definition search. D063's facts, joins, invalidation,
convergence limit, fallback and emitted checks are unchanged.

These are implementation-only reorderings of pure eligibility checks. No new
language syntax, API, wire opcode or source semantics is introduced. They add
no allocations, cache, verifier bypass or relaxed limit. Existing D063 and D087
contracts remain authoritative; memory/capability checks and test isolation
are unchanged. The existing opcode-family, hostile-bytecode, relational-proof,
allocation and differential suites are retained in full.

## Measurement conditions and identity

The application business sources and synthetic input bytes are unchanged.
The input contains 43,680 distinct required paths; `version` is last, so the
application intentionally scans the prefix and then performs typed decoding.
Its SHA-256 is
`e8b36fe61735866040d39a4ab5492ba6ab390879c843534fc2672411df64ceb7`.
No workload reduction, version-first-only restriction or relaxed validation
was used.

The baseline compiler was reconstructed from the same Release archive by
compiling `7f68843fa2:bootstrap/src/bytecode/vm_value.c` with the existing
`build/release/CMakeFiles/mmay_bootstrap.dir/flags.make` flags and replacing
only that object before linking. This uses ignored artifacts under
`build/dispatch-baseline`, not another checkout. The archive still contained
the original relational proof at that point. All participants use the same
pinned C compiler and matching runtime archives.

Compiler SHA-256:

```text
abdf7e31aa805d5dd2b63e538b7e0c128377bf0598e5da6add2ca9042d3b3e53  reconstructed 7f68843 baseline
2bb3807021b9cece79c71c130001029a37c5bd9eb8a3558d23483ce3c0ad1db2  dispatch-only measurement
dd569c22e561108b8c9273f71698d93798e100e1eac5e85aacd7f32813668483  both repairs, installed Release target
```

Measurements were sequential after our builds/tests had finished. Desktop
background applications remained running, with no deliberate OS-cache eviction
or idle-host claim. The first exploratory small-run dispatch measurement
overlapped the architecture checker; it is not used to claim small-run latency.
The paired VM comparison has three warmup rounds and eleven measured rounds,
alternating participant order. Every process returned the exact full JSON
success report, exit 0 and empty stderr. These are local diagnostic reports,
not a dedicated-runner, complete D056 release baseline.

## Results

| Workload | Baseline | Repaired |
|---|---:|---:|
| Large version-last VM, median of 11 | 11.390956 s | 9.516447 s |
| Same VM sample range | 10.978335–11.731474 s | 9.241362–10.588266 s |
| Same VM median absolute deviation | 0.203656 s | 0.210288 s |
| Seven both-engine tests, empty native-cache median of 3 | 14.822758 s | 10.950945 s |
| Seven both-engine tests, warm native-cache median of 6 | 5.627969 s | 1.837099 s |

The dispatch change reduced the paired median by 16.5%. One candidate sample
still exceeded 10 s and is retained. The test-loop median is about 3.06 times
faster when warm; cold compilation remains above the predeclared 10 s budget.
Cold samples were baseline `[15.044572, 14.741327, 14.822758]` and candidate
`[11.209232, 10.950945, 10.929032]`, not a warm-cache substitute for the miss.

Each cold pair uses exactly the same temporary module path. After the first
participant, only that harness-owned `.mmay` directory is moved aside before
the second participant runs. The seven generated C files match byte-for-byte
within each of three pairs. The first harness attempt incorrectly compared
different module paths and stopped on differing identities; it was corrected
to compare the same source location. A separate application `build --emit c`
comparison from the ordinary directory also matched byte-for-byte, SHA-256
`df48506c776307f99d3ff2909d97a4b2798ce1eb4c35c003b1ea902d2ef52b1e`.

Five fresh trivial C executables took 0.374–0.573 s on their first run and
0.004–0.009 s subsequently. Five fresh config native executables took
0.375–0.400 s first and 0.005–0.011 s subsequently. This control demonstrates
that the first-launch delay is not specific to Marimay's runtime; it does not
identify a particular macOS service or justify disabling security checks.
Builds never execute user programs to manufacture warm-start results.

## Reproduction and raw local reports

From the external application directory, using new output names:

```sh
python3 tools/profile.py --input build/resource-probe-20260906/near-limit-last.json --output build/new-vm-profile.txt
python3 tools/profile.py --mode test --output build/new-test-profile.txt
python3 tools/compare-vm.py --baseline /path/to/baseline/mmay --candidate /path/to/candidate/mmay --input build/resource-probe-20260906/near-limit-last.json --output build/new-vm-comparison.json
python3 tools/compare-test.py --baseline /path/to/baseline/mmay --candidate /path/to/candidate/mmay --output build/new-test-comparison.json
python3 tools/probe-native-startup.py --output build/new-startup-control.json
python3 tools/measure.py --output build/new-measurements.json
```

Raw reports retain commands, statuses, output, timings and hashes. SHA-256:

```text
5cd8ea0a3663a68da942f4d2688d25b7713f0e228ea0380e422d5a5244cad77a  build/paired-dispatch-last-20260906.json
e0b4d23870c881feed470d957001627562f17e1bc080023d9e759b494fab50cf  build/paired-test-relations-fixed-path-20260906.json
231154571be099ddb4c7586829a1753cc842b81f19c888990cc37802ae2420f5  build/native-startup-control-20260906.json
341129c8e06ba683176c4ebb11477fb04569be01e3cefb931a5511274fdc6632  build/measurements-dispatch-relations-20260906.json
```

## Verification and remaining stop gate

The final Release suite passed all 68 CTests (309.46 s). Focused Debug passed
all four selected tests (282.29 s): bytecode, native C, CLI test execution and
the long local-execution differential corpus. The corresponding ASan/UBSan
selection passed all four (361.08 s); this is not a Linux LeakSanitizer claim.
These overlapping correctness durations are not performance comparisons.
The external application's 32 exact process cases passed in all three engines,
including atomic-write, denied-I/O and broken-pipe checks (89.56 s under other
test load). The architecture, C-memory, generated, spec, documentation, example,
syntax, roadmap and release-contract gates pass. Ordinary installed `mmay`
resolves to the single checkout's Release binary above, and `mmay doctor` passes.

Do not advance to the manifest utility yet. Empty-native-cache tests remain
above 10 s; the extra full measurement also observed a small-run median of
0.305 s versus 0.3 s, and first-new-native execution remains above 0.1 s.
At that revision the test runner still lowered/encoded/decoded the full program
for each test and compiled separate native executables. The subsequent
[test preparation repair](cp42-test-preparation-2026-09-06.md) reuses decoded
code; cold native compilation remains open. Preparation reuse must preserve full
verification, fresh execution state, output/panic comparison and failure cleanup.
The remaining VM/native call-stack resource mismatch, broader ergonomics,
OS-permission/symlink acceptance and platform evidence also stay open.

Separately, Gitea [run 190](http://ci.example:3000/Marimay/marimay/actions/runs/190)
finished successfully for `f53b2896582e60806f4bb1295c7227054fbb528b`: all 13
jobs, including Linux Clang/GCC debug/release, ASan+UBSan+LSan, TSan and the
four fuzz directions. That validates the prior JSON-metadata revision, not
D087 or these performance changes. Run 191 for D087 was still running at the
last inspection; no newer-revision CI success is implied.
