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

Windows builds run the CP39 native-toolchain preflight before the full CTest
suite. A failed preflight keeps the job red and exposes bounded raw compiler
query diagnostics promptly; a pass still requires the complete suite.

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
