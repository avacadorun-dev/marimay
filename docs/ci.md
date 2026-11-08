# CI and local checks

GitHub Actions uses public hosted runners and requires no private build server,
private container registry, SSH key, PAT, or repository secret. The retained
Gitea workflow is historical/optional infrastructure; GitHub does not execute it.

## Build and conformance

The `C17 bootstrap` workflow runs documentation/generated/spec/example checks,
architecture and roadmap gates, the C memory policy, release contract checks,
publication privacy, and Python tool tests. The C17 Debug/Release matrix covers
Linux x86_64 GCC/Clang, Linux ARM64 GCC, macOS ARM64/x86_64, and Windows MSVC.
CTest contains focused C owners, deterministic allocation failures, negative
cases, VM/native differentials, and distribution lifecycle checks.

Additional jobs run Unix ASan/UBSan, Linux LeakSanitizer, Linux TSan,
Windows ASan, four sustained fuzz targets, and the explicitly pinned optional
SLJIT tier with sanitizers both enabled and disabled. Scheduled/manual memory
checks run Valgrind. A separate workflow checks Git history for secrets.

```sh
cmake --preset debug
cmake --build --preset debug --parallel
ctest --preset debug
cmake --preset asan-ubsan
cmake --build --preset asan-ubsan --parallel
ctest --preset asan-ubsan
```

Set `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1` for Linux leak checks. A
macOS sanitizer result does not establish Linux LeakSanitizer coverage.
Follow [CONTRIBUTING.md](../CONTRIBUTING.md) for the documentation gates.

## Permissions and outputs

Actions are pinned to full commit IDs; Dependabot proposes updates. Workflows
use `contents: read`, `persist-credentials: false`, bounded job timeouts, and no
privileged pull-request trigger. Public forks run without secrets. No job
publishes a release, deploys a site, writes a package registry, or contacts
private development infrastructure.

Release-profile CI archives are test artifacts, not published releases. Uploads
are limited to distribution archives/checksums, fuzz executables/reproducers,
and selected test/JIT diagnostics. Failure collection can include generated
source; use it only on the clean public checkout. Artifact retention is bounded.

The first public run provides fresh platform evidence. Migrating a job definition
or passing a local check does not close any release gate. See
[SECURITY.md](../SECURITY.md) for trust boundaries and private reporting.

## First public run

The [initial run](https://github.com/marimay-lang/marimay/actions/runs/42408219743)
passed documentation gates and the fuzz build, but exposed build portability
failures in the unchanged committed compiler:

- GCC rejects mixed signed/unsigned conditional expressions in the VM fast
  paths under `-Werror=sign-conversion`.
- MSVC does not compile the current `max_align_t` use in the arena owner.
  Debug/ASan configuration also carries conflicting `/RTC1` and optimization
  flags for the release runtime archive.

These remain visible failing checks, not verified platforms. Fixing the C17
owners and their platform tests is separate implementation work. The publication
copy does not include the original checkout's unfinished compiler changes.

The migrated optional JIT job initially invoked the container-oriented apt
helper without privilege on a hosted runner. Its package-install step now uses
`sudo`; this is a CI migration correction, not a compiler semantic change.
The [public history security scan](https://github.com/marimay-lang/marimay/actions/runs/42408219806)
passed. Other jobs provide their own results and must not be inferred to pass.
