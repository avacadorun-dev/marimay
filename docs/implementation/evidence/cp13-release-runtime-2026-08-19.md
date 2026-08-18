# Profile-coherent native runtime — post-preview checkpoint 13 — 2026-08-19

Status: locally implemented and validated; Tier 1 CI evidence is required
before I4 can become `verified`.

## Contract

- Accepted D052 makes the requested `ruda build` output profile authoritative
  for generated C and all linked first-party runtime objects.
- Every compiler configuration builds separate debug (`-O0`/`/Od`) and release
  (`-O2`/`/O2`) runtime archives with exact manifests and embedded
  version/profile markers.
- `runtime_artifact.c` owns installed/build-tree discovery, validation, archive
  hashing, and owned path cleanup. `build.c` remains the emission/cache/link
  coordinator and includes the selected archive SHA-256 in cache contract v2.
- A complete installed tree is resolved relative to the actual `ruda`
  executable. Missing, partial, stale, or wrong-profile distribution artifacts
  fail with `B0008`/exit `1` before C emission or host linking. `--emit c`
  requires no unused runtime archive.

Normative coverage: D052, execution §5.6, toolchain §6.3, diagnostics §13, and
TOOL-021.

## Focused coverage

`tests/c-unit/test_cli_build_profiles.c` is the dedicated checkpoint owner. It
checks both configured profiles, archive-owned profile markers, distinct
runtime hashes, a copied runtime path containing spaces, runtime-content hash
invalidation, missing archives, stale manifests, a valid release manifest with
a wrong-profile debug archive, and every deterministic artifact-selection
allocation failure. Every fail index ends with `live_allocations == 0` and
`live_bytes == 0`.

`ruda_install_relocation` installs then renames the complete prefix before
building debug/release executables, including invocation as bare `ruda` through
the relocated prefix's `PATH`. It also covers installed `--emit c`, a
wrong-profile manifest, a missing release archive, exact `B0008`, no raw linker
diagnostic, no failed output publication, paths with spaces, and execution of
both installed artifacts. Existing build and final-preview suites retain exact
stdout/stderr/exit and panic equivalence coverage for VM and native programs.

## Local artifact evidence

Host: Darwin arm64, AppleClang 17.0.0.17000604, CMake 4.4.

In the debug toolchain tree, CMake recorded:

```text
ruda_native_runtime_debug:   -std=c17 ... -O0 ... -Werror
ruda_native_runtime_release: -std=c17 ... -O2 ... -Werror
```

The initial focused build produced different archive identities:

```text
debug   ac6967a7f912272f2c41d42b0ac96e38050ac9bbf3fbc59c9babbc766c20332f
release ef99ac0ad1ef1ff227c78c154e6727b2150d3df32f29a443dc3a23237197bac0
```

These hashes are evidence for this local build, not stable distribution IDs.
Both archives export `ruda_runtime_build_profile`,
`ruda_runtime_build_version`, and their marker data. The manifest records the
same runtime version, host, processor, compiler identity, and exact profile.

## Validation record

The final command table is populated only with commands actually run in this
checkout:

| Command | Exit/result |
|---|---|
| `./tools/check-architecture` | 0 |
| `./tools/check-c-memory-policy` | 0 |
| `./tools/check-generated` | 0 |
| `./tools/check-doc-links` | 0 |
| `./tools/check-spec-terms` | 0 |
| `cmake --build --preset debug --target ruda_c_unit --parallel` | 0 |
| `ctest --preset debug --output-on-failure` | 0; 2/2, 223.29 s |
| `cmake --build --preset release --target ruda_c_unit --parallel` | 0 |
| `ctest --preset release --output-on-failure` | 0; 2/2, 203.25 s |
| `cmake --build --preset asan-ubsan --parallel` | 0 |
| `ctest --preset asan-ubsan --output-on-failure` | 0; 2/2, 303.47 s |
| `ctest --preset debug -R '^ruda_install_relocation$' --output-on-failure` | 0 |
| `build/debug/ruda build tests/run/cp9/acceptance.ruda --release --output build/debug/cp13-smoke` | 0; selected release runtime |
| `build/debug/cp13-smoke` | 0; `cp9 acceptance ok\n` |
| `build/release/ruda build tests/run/cp9/acceptance.ruda --output build/release/cp13-debug` plus execution | 0; debug profile, exact acceptance stdout |
| `build/release/ruda build tests/run/cp9/acceptance.ruda --release --output build/release/cp13-release` plus execution | 0; release profile, exact acceptance stdout |
| `build/debug/ruda test tests/preview/checkpoint12 --engine both` | 0; 5 passed, VM/native records equal |
| debug-VM and release-native `examples/matrix.ruda` | both 0; exact `415.0\n` |
| debug-VM and release-native `tests/run/cp9/panic-bounds.ruda` | both 1; exact `panic[G4002]: array index 2 is outside length 1\n` |

Docker was installed locally but no daemon was available, so no Linux result is
inferred from this Darwin run. No Windows/MSVC or Linux/GCC result is inferred
either; the existing Tier 1 CI matrix is the required cross-host evidence.

## Gate status

Checkpoint 13 closes the local profile-coherence defect and introduces no
source-language or runtime-observable semantic change. I4 remains
`in-progress`: the roadmap still requires all Tier 1 installation and engine
equivalence gates before `verified` is permitted.
