# Post-preview checkpoint 20: reusable algorithm corpus — 2026-08-21

Gate: cross-gate reference-workload evidence. Checkpoint status: locally
complete. I2, I3, I4, and I5 remain `in-progress`; I6 and P0 remain
`not-started`. This checkpoint changes no Ruda source semantics and introduces
no performance budget or speed claim.

Baseline Ruda commit: `bf8edd8` (`Complete checkpoint 19 resource limits`).
Sibling implementation commit: `033f17e9225d5760d40181ce13903bcda4decba3`.
Sibling smoke-evidence commit: `7183fdf387bee71575e87231a137f81044f3ccf2`.

The sibling repository arrived on an unborn `main`: every source file was
untracked, there was no parent commit, and no remote was configured. Its first
commit therefore records the complete reviewed corpus instead of fabricating a
pre-checkpoint history. The two local sibling commits cannot be pushed until a
remote is supplied. The Ruda repository retains its existing independent
history.

Authority: accepted import, D003/D043 call-resolution, D045/D053 managed-array,
Option/Result, toolchain execution-equivalence, and implementation test-strategy
contracts. The checkpoint-20 follow-up plan is workload-fidelity guidance, not
normative language authority. Review found no requirement for new semantics
beyond checkpoints 13–19.

## Delivered workload

- Timed Ruda entrypoints call the reusable `sorting`, `structures`, and
  `graphs` packages for insertion sort, heap sort, stack, queue, min-heap,
  DSU, BFS, and O(V²) Dijkstra. The benchmark coordinator only creates inputs,
  calls those owners, computes selected checks, and returns a checksum.
- Stack and min-heap use their managed-array logical length directly. The old
  `IntOption`, `SearchResult`, shadow-size, parallel target/weight, renamed
  collection-operation, and package-bypass forms are absent and are explicit
  forbidden fragments in the validator.
- Ruda, Go, and Rust share the exact LCG constants, sizes/repetition counts,
  operation families, selected checks, checksum modulus, and one-line output
  contract. DSU uses path compression plus union by rank in all three owners.
- BFS uses nested integer adjacency arrays. Dijkstra uses nested edge-record
  arrays with target and signed weight fields. Ruda/Rust return `Option`; Go's
  value/bool pair is the recorded equivalent boundary.
- All three correctness suites reject out-of-range starts/endpoints and
  negative Dijkstra weights. Rust graph indices use `usize`, so a negative
  index is unrepresentable rather than accepted; the manifest records that
  representation boundary. Invalid probes are excluded from timing.
- The Ruda correctness root and tests use public aliases, imported reusable
  packages, free/dot calls, nested arrays, record arrays, `Option`, root
  `Result`, and postfix `?`. They are correctness orchestrators, not untimed
  adapters around a different timed implementation.

## Machine-readable equivalence owner

Sibling `benchmarks/equivalence.json` has schema
`ruda.algorithm-equivalence/1` and SHA-256
`51702f2ad1bae31fb11b9df8550f58b2034e1e7044f3d6e7448e5001399d3535`.
For each workload it records input generation, operations, representation,
selected checks, expected checksum, language entrypoint markers, and forbidden
inline remnants.

`benchmarks/equivalence.py` validates the schema and all three source owners.
`run.py` consumes the validated workload order, descriptions, and checksums;
it no longer owns a second checksum/workload table. Markdown and raw JSON
reports carry the Ruda compiler revision, sibling corpus revision, manifest
schema/hash, toolchain versions, raw samples, and summaries.

Nine Python tests cover the committed manifest/source success path plus
malformed JSON, schema/generator/checksum drift, missing language coverage,
missing source, missing workload constants, and reintroduced forbidden inline
fragments. `verify.sh` runs them before language builds and compares exact Ruda
VM/debug-native/release-native stdout bytes.

## Main-repository regression owner

`tests/post-preview/cp20-algorithm-corpus` is an independent compact module
mirroring the eight package owners without importing or depending on the
sibling checkout. It exercises all eight algorithms and negative graph inputs
through public Ruda commands.

`examples/README.md` routes users to the full sibling module and to the
independent main-repository executable mirror, while this evidence pins both
sibling revisions and the exact validation commands.

`tests/c-unit/test_cli_cp20_algorithm_corpus.c` is the 121-line focused CLI
owner. It runs canonical format checking, authoritative checking, VM execution,
VM/native record-replay tests, Debug native build/run, and Release native
build/run. The artifact helper captures stdout and stderr separately and
asserts exact stdout `cp20 algorithm corpus ok\n`, empty stderr, exit 0, and no
signal for both profiles. No allocation-capable C API or production heap path
was added; existing fail-every-allocation and zero-counter suites remain the
applicable memory coverage and run in every aggregate preset.

## Exact observations

| Surface | Observation |
|---|---|
| main-repo VM run | stdout `cp20 algorithm corpus ok\n`; empty stderr; exit 0 |
| main-repo `test --engine both` | exact two tests; `2 passed; 0 failed`; matching VM/native records |
| main-repo Debug/Release artifacts | byte-identical success stdout; empty stderr; exit 0; no signal |
| sibling Ruda correctness | exact `Ruda algorithms: ok\n`; four tests pass through `--engine both` |
| sibling Go correctness | `go vet ./...`, `go test ./...`, exact root stdout pass |
| sibling Rust correctness | format, strict Clippy, all targets/tests, exact root stdout pass |
| fidelity smoke | all 8 workloads × all 3 languages print their manifest checksum |
| smoke metadata | Ruda `bf8edd83e85cc2ed9c84791a2ea8cf0fe07146ae`; corpus `033f17e9225d5760d40181ce13903bcda4decba3`; one sample, zero warmups |

The sibling smoke artifacts are
`benchmarks/results/cp20-fidelity-smoke.md` and `.json`. They preserve raw local
observations but are deliberately not a baseline: one sample without warmups
cannot define dispersion, noise envelopes, regression budgets, or speed
claims. Those remain checkpoint-21 work.

## Local validation

Environment: macOS Darwin 25.3.0 arm64; Apple Clang 17.0.0; CMake 4.4.2;
Go 1.26.4; Rust 1.96.0.

| Command | Exit | Result |
|---|---:|---|
| `RUDA="$(cd ../Ma && pwd)/build/debug/ruda" ./verify.sh` in sibling | 0 | manifest + 9 negatives, exact Ruda observations, Go vet/tests, Rust fmt/Clippy/tests |
| `python3 benchmarks/validate_equivalence.py` in sibling | 0 | 8 workloads, 3 source owners, exact manifest hash |
| `RUDA="$(cd ../Ma && pwd)/build/release/ruda" ./benchmarks/run.py --languages ruda,go,rust --samples 1 --warmups 0 --output benchmarks/results/cp20-fidelity-smoke.md` | 0 | all 24 checksum observations matched; raw Markdown/JSON recorded |
| `build/debug/ruda fmt tests/post-preview/cp20-algorithm-corpus --check` | 0 | complete main-repo fixture canonical |
| `build/debug/ruda check tests/post-preview/cp20-algorithm-corpus` | 0 | all reusable packages accepted |
| `build/debug/ruda run tests/post-preview/cp20-algorithm-corpus` | 0 | exact success stdout |
| `build/debug/ruda test tests/post-preview/cp20-algorithm-corpus --engine both` | 0 | 2/2 exact VM/native records |
| Debug and Release focused `ruda build` plus artifact execution | 0 | byte-identical stdout/stderr/exit observations |
| `cmake --build --preset debug --parallel && ctest --preset debug --output-on-failure` | 0 | final byte-exact owner, 2/2 in 211.28 seconds; C-unit 209.31 seconds |
| `cmake --build --preset release --parallel && ctest --preset release --output-on-failure` | 0 | final byte-exact owner, 2/2 in 208.30 seconds; C-unit 206.38 seconds |
| `cmake --build --preset asan-ubsan --parallel && ctest --preset asan-ubsan --output-on-failure` | 0 | final byte-exact owner, 2/2 in 308.41 seconds; C-unit 305.44 seconds; no sanitizer findings |
| `./tools/check-architecture` | 0 | focused owner registration, dependencies, and line budgets clean |
| `./tools/check-c-memory-policy` | 0 | central allocator policy clean |
| `./tools/check-generated` | 0 | bytecode, diagnostic, Unicode, and syntax artifacts current |
| `./tools/check-doc-links` | 0 | roadmap, evidence, and user-facing example links valid |
| `./tools/check-spec-terms` | 0 | normative terminology clean |
| `./tools/check-syntax-coverage` | 0 | 75 syntax kinds and 116 grammar productions covered |
| `./tools/check-example-syntax` | 0 | user-facing Ruda examples accepted |
| `git diff --check` in both repositories | 0 | no whitespace errors |

Checkpoint 20 closes reusable workload fidelity locally. It does not close the
Tier 1 performance protocol, introduce hard timing budgets, optimize native
arrays, verify any roadmap gate, or start P0. The only incomplete delivery
mechanic is the sibling push, which has no configured destination.
