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

Windows builds preflight the CP39 native toolchain and a relocated native build
before the full CTest suite. A failed preflight keeps the job red and exposes
bounded compiler diagnostics and path-with-spaces failures promptly; a pass
still requires the complete suite.

Additional jobs run Unix ASan/UBSan, Linux LeakSanitizer, Linux TSan,
Windows ASan, four sustained fuzz targets, and the explicitly pinned optional
SLJIT tier with sanitizers both enabled and disabled. Scheduled/manual memory
checks run Valgrind. A separate workflow checks Git history for secrets.

Generated native MSVC sanitizer builds emit object debug metadata with `/Z7`,
as required by the compiler's warnings-as-errors ASan profile. This preserves
the selected `/Od` or `/O2` optimization and avoids a shared compiler PDB;
it does not add final-executable PDB publication to the native artifact contract.
The MSVC object path is explicit and local to each generated source cache entry,
so native builds from distinct cache entries in a shared working directory do
not overwrite one another's intermediates. Completed compiler attempts remove
the transient object.
Removal is best-effort under allocation or filesystem failure; such failures can
leave an intermediate in its cache entry. Allocation-failure tests require zero
live allocator counts and validate the executable whenever optional cleanup
failure leaves the build successful.

The compiler IPO contract test runs seven separate CMake configurations, each
bounded by 30 seconds, within a CTest budget of 240 seconds. It flushes progress
and elapsed time for each case to make timeout failures diagnosable.

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

## Investigate a failing run

Read the failing job's revision and first unsuccessful step before reproducing
it. Configuration, compilation, CTest, and artifact transport are separate
failure points. Match the job's compiler, build configuration, sanitizer
options, and exact test command; rerunning a different local profile does not
establish a fix on the failing platform.

On GitHub-hosted Ubuntu, the optional JIT job invokes the bounded
`tools/ci-apt-install` helper through `sudo`, because installing missing system
packages requires root privileges. The helper still checks installed packages
first and retains its bounded network retries.

C17 Unix, Unix sanitizer, TSan, and LeakSanitizer jobs collect
[diagnostic packets](../tools/ci-diagnostics/README.md) only when their CTest
step actually ran and failed. Setup/build failures leave their diagnostics in
the job log and do not start a collector that expects a built compiler and
CTest logs. Upload is attempted even when collection reports an error, so a
partial packet can survive. Neither collection nor upload replaces the failed
test result. Windows diagnostics remain in the job log; the packet collector
currently recognizes POSIX native-compiler paths.

## Permissions and outputs

Actions are pinned to full commit IDs; Dependabot proposes updates. Workflows
use `contents: read`, `persist-credentials: false`, bounded job timeouts, and no
privileged pull-request trigger. Public forks run without secrets. No job
publishes a release, deploys a site, writes a package registry, or contacts
private development infrastructure.

The pinned Gitleaks archive uses GitHub's public release-asset API with bounded
download retries and timeouts. Its SHA-256 is verified before extraction.

The source-security workflow checks out the exact submitted PR head SHA, or
the event SHA for pushes and manual runs. It fetches complete branch/tag
history and the submitted head's ancestry, then keeps both the Gitleaks
`--all` scan and the publication guard's `--history` check. GitHub's temporary
PR merge commit can carry service-generated author metadata; it is not part
of the submitted history. Build/conformance jobs still test GitHub's PR merge
result. This checkout choice adds no scanner or metadata allowlist exception.

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
failures in the original public-import revision:

- GCC rejected mixed signed/unsigned conditional expressions in the VM fast
  paths under `-Werror=sign-conversion`.
- MSVC could not compile that revision's `max_align_t` use in the arena owner.
  Debug/ASan configuration also carried conflicting `/RTC1` and optimization
  flags for the release runtime archive.

Assess those historical failures against that revision. Subsequent C17 fixes
are validated by their focused owner tests and exact-revision CI. The original
public import excluded the original checkout's unfinished compiler changes.

The migrated optional JIT job initially invoked the container-oriented apt
helper without privilege on a hosted runner. Its package-install step now uses
`sudo`; this is a CI migration correction, not a compiler semantic change.
The [public history security scan](https://github.com/marimay-lang/marimay/actions/runs/42408219806)
passed. Other jobs provide their own results and must not be inferred to pass.
