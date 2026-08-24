# The `mmay` toolchain contract — design baseline 0.1

Status: normative design baseline.

The identity mapping is normative: the language/product name is **Marimay**, the toolchain executable is `mmay`, source files use `.my`, bytecode bundles use `.myb`, module/workspace/lock files are `mmay.mod`, `mmay.work`, and `mmay.lock`, and local tool state uses `.mmay/`. The former `ma`/`.ma`/`ma.*`/`.mab`/`.ma/` names are historical and are not compatibility aliases. D001 records this as a semantics-neutral pre-release rename. N0 still blocks public distribution until legal review, domains, public accounts, package/registry namespaces, signing identity, and command/PATH coexistence have direct evidence; this specification does not claim that any such external identity is available or controlled.

Installing Marimay installs one user-facing executable named `mmay`. The formatter, test runner, compiler, VM, package resolver, documentation generator, vet checks, fuzz driver, and bootstrap metadata are versioned together.

This distribution boundary does not make every command part of Marimay source
semantics. The compiler reports semantic facts; first-party tools consume those
facts; manifests and runtime grants configure execution; CI decides which
checks and provenance are required for a release. Source code carries no
author, agent, trust, coverage, or attestation marker under D034.

The core promise is literal:

```sh
mmay fmt
mmay test
```

These commands require no project plugin, task runner, formatter configuration, test configuration, or copied script.

## 1. Root discovery and targets

Starting from the working directory, `mmay` searches parent directories for `mmay.mod`. The nearest file is the module root. An optional `mmay.work` above it can join multiple local modules without changing their published identity.

With no explicit target:

- `mmay fmt` formats the entire current module;
- `mmay check` checks the entire current module;
- `mmay test` tests the entire current module;
- `mmay build` builds the root executable package, or reports the available executable packages;
- `mmay run` runs the root executable package.

This behavior does not silently change when invoked from a subdirectory. A path or package selector narrows it explicitly:

```sh
mmay test ./services/orders
mmay fmt ./src
mmay run ./cmd/admin -- --repair account-42
```

A standalone `.my` file without `mmay.mod` can be checked, formatted, tested, or run in script mode. Script mode has no third-party dependencies and receives no filesystem/network/process capability unless explicitly granted. D046 capability `"io.stdout"` is granted by default for `mmay run` of a local module or script and for a native executable from `mmay build`; it is not a filesystem, network, or process grant.

### 1.1 Batch check discovery

```sh
mmay check
mmay check ./path/to/package
mmay check file.my
mmay check --all-targets
```

With no target, discovery starts at the physical current directory, finds the
nearest ancestor `mmay.mod`, and checks its whole module even when invoked from
a descendant package. An explicit directory equal to that root selects the
whole module; another directory within it selects the corresponding package.
An explicit regular `.my` file is a standalone one-file check. Other file
kinds, unreadable targets, a missing module root for a directory target, or an
empty selected graph report `K0004`.

A module check requires the D048 minimal manifest identity: exactly one
nonempty `module` value and exactly one nonempty `language` value. The root
package uses the decoded module path, and source-bearing descendant directories
append their relative `/`-separated path. All discovered packages, including
disconnected packages, are checked in canonical import-path order; files are
ordered by normalized path. The default source set excludes `_test.my` files
and the root `tests/` directory. `--all-targets` includes them. Traversal does
not follow symbolic links, enter `.git` or `.mmay`, or cross a nested module,
and it has fixed defensive limits.

Every invocation creates a fresh compiler service from immutable disk
snapshots. Editor overlays and results from earlier invocations cannot affect a
batch result. A successful check emits no stdout or stderr. Source diagnostics
use exit 1, invalid invocation uses exit 2, cancellation uses exit 130, and an
accepted but checker-incomplete construct reports `I0001` and exits 70. Human,
short, and versioned newline-delimited JSON diagnostics are written only to
stderr in deterministic order.

## 2. Global behavior

Every command supports:

```text
--help             concise task-oriented help and examples
--version          compiler and runtime build identity
--color auto|always|never
--diagnostic human|short|json
--quiet
--verbose
```

Unknown commands and flags report a spelling suggestion. They use stable diagnostics `U0001` and `U0002` respectively; JSON output follows the nullable-primary rule in the diagnostics specification. `mmay help` lists only top-level user tasks; `mmay help <command>` gives examples before exhaustive flags. The CLI does not automatically execute `mmay-<name>` binaries from `PATH`, preventing typos from invoking untrusted plugins.

Under D066, `mmay` converts the complete host invocation to owned UTF-8 before
dispatch. POSIX bytes must already be valid UTF-8; Windows uses the wide CRT
entry and strict UTF-16-to-UTF-8 conversion. Including the executable name,
this outer boundary permits at most 8192 values, 262144 bytes per value, and
8388608 bytes total. Encoding or limit failure is `U0007`/status `2` before
command execution; allocation failure is `I0002`/status `70`. A run/test
program suffix is independently revalidated against D066's tighter program
limits after the first `--`.

Exit statuses are stable:

| Status | Meaning |
|---:|---|
| `0` | requested operation succeeded |
| `1` | source, test, formatting, vet, or expected build failure |
| `2` | invalid command-line use |
| `3` | dependency or lock resolution failure |
| `4` | capability or sandbox denial |
| `5` | VM/native semantic divergence |
| `70` | internal compiler/runtime failure |
| `130` | interrupted by user |

Human diagnostics go to stderr. Program stdout remains unpolluted. JSON diagnostics are newline-delimited objects with stable schema version, code, severity, message, spans, notes, and fix edits.

## 3. Project manifest

`mmay.mod` uses the deliberately small, bounded [manifest grammar](manifest.md) so Stage 0 needs no general configuration language:

```text
module "example.com/acme/orders"
language "0.1"

require (
  "marimay.example/postgres" "^1.4.0"
  "marimay.example/telemetry" "~2.3.1"
)

capability (
  "fs.read" "./config/**"
  "net.listen" "0.0.0.0:8080"
  "net.connect" "db.internal:5432"
  "env.read" "DATABASE_URL"
)
```

Directives and grouping parentheses are fixed by the manifest grammar. Strings use JSON-compatible escapes. Unknown directives are errors unless namespaced as optional metadata. `mmay fmt` also canonicalizes `mmay.mod`.

Libraries may document capabilities they can use, but only the root executable grants them. A dependency cannot broaden the root capability set.

`mmay.lock` is generated, committed, and contains exact versions, transitive dependency edges, source/archive hashes, registry checkpoint, signatures when present, license identifiers, and feature-free build identity. Normal build/test refuses a stale lock in CI mode.

`mmay.work` contains canonical `use "relative/path"` directives. Workspace replacement never changes published module metadata and is excluded from dependency artifacts.

## 4. Formatting

### 4.1 Commands

```sh
mmay fmt                 # format the current module in place
mmay fmt ./src           # format a selected tree
mmay fmt file.my        # format one file
mmay fmt --check         # do not write; fail if any file differs
mmay fmt --diff          # print the canonical diff; do not write
mmay fmt --stdin         # stdin to stdout, filename supplied optionally
```

`--check` emits `F0004` for each non-canonical input and exits `1` without writing. `--diff` writes a deterministic unified diff and exits `0` when every selected input is syntactically valid and successfully formatted, whether or not the diff is empty; callers that need a cleanliness gate use `--check`. Formatter I/O, limit, lexical, or parser failures still make `--diff` exit nonzero.

`mmay fmt` formats `.my`, `mmay.mod`, and `mmay.work` files; it never rewrites `mmay.lock` except through dependency commands.

Target selection follows D028. With no explicit target, the nearest regular `mmay.mod` on the physical working-directory ancestor chain owns the selected module; absence of such a file is a formatter input error. The nearest regular `mmay.work` from that module root upward, when present, is also selected. An explicit regular file must end in `.my` or be named exactly `mmay.mod` or `mmay.work`. An explicit directory selects a bounded tree rooted there.

Tree traversal considers regular recognized files in unsigned UTF-8 normalized-path order. It does not follow symbolic links, does not enter `.git` or `.mmay` tool-state directories, and does not cross into a nested directory containing its own regular `mmay.mod`. An explicitly selected symbolic link or unrecognized file is an input error. Traversal has fixed defensive ceilings for directory depth, directory entries, selected files, and path bytes; exhaustion reports `F0003` before further work.

If a source file cannot be parsed with bounded recovery, the formatter reports diagnostics and leaves that file untouched. Formatting a tree is transactional per file: no partially written file is possible. Writes use a sibling temporary file, flush, permission preservation, and atomic replacement where supported.

### 4.2 Canonical style

Marimay 1.x has no formatter configuration. The style is:

- two-space indentation, never tabs;
- LF line endings and one final newline;
- mandatory source semicolons preserved/inserted only where grammar requires them;
- opening braces on the declaration/control line;
- canonical declarations use `fn`, nominal records use `type Name { ... }`, and aliases alone use `=`;
- record construction and named enum payloads use `Name { ... }`; control-flow scrutinees are not parenthesized;
- interpolated strings use `$"...{expression}..."`, and visibility uses only `public`;
- interpolation expression groups are never split by a physical newline;
  when the complete token sequence exceeds the target width it remains one
  deterministic unbreakable overflow rather than changing string syntax;
- one space around binary operators and after commas;
- no padding inside ordinary parentheses;
- multiline lists use one item per line and a trailing comma;
- under D031, import name lists, record construction/pattern fields, and named enum payload fields remain braced formatter groups: compact groups use one inner space, while groups that do not fit break one item per line; named payload declaration fields keep semicolons rather than commas;
- construction field order is preserved exactly under D040 because it controls
  initializer evaluation; formatting constructions is syntax-only and never
  performs name resolution or declaration-order sorting;
- D032 canonical string spelling: decoded non-control Unicode is literal in a
  double-quoted ordinary string, required controls use their shortest closed
  escape, and expression literals select raw syntax only when the value is
  raw-representable and contains at least two backslashes that ordinary syntax
  would double; grammar-required ordinary string tokens remain ordinary;
- imports grouped standard/current-module/third-party and sorted by path, without merging comments incorrectly;
- default target width 100 formatter columns under D026, with deterministic overflow rules for unbreakable tokens. The pinned Unicode 17.0.0 metric assigns zero columns to `Grapheme_Extend` and `Default_Ignorable_Code_Point`, two to remaining East Asian Width `W`/`F` scalars, and one to every other valid scalar including `A`; it never calls host `wcwidth` or locale APIs.

Comments are syntax-tree-attached. The formatter must preserve comment text, relative order, documentation ownership, intentionally blank documentation paragraphs, and directive placement.

### 4.3 Formatter invariants

For every syntactically valid file `x`:

```text
parse(fmt(x)) succeeds
AST(parse(fmt(x))) is semantically equivalent to AST(parse(x))
fmt(fmt(x)) == fmt(x)
```

The parser and formatter share the same token definitions. The formatter has golden fixtures for every grammar production, comment position, error-recovery boundary, and Unicode class. Formatter idempotence is fuzzed.

## 5. Testing

### 5.1 Discovery and syntax

Package-internal tests live in files ending `_test.my` and can access package-private declarations. Black-box integration tests live under the module-root `tests/` tree and import only public APIs. Documentation code blocks tagged `mmay test` are compiled and run by default.

```marimay
@test
fn parseAcceptsValidPort(): void {
  const result = parsePort("8080");

  expect.equal(result, Ok(8080));
}

@test
fn parseRejectsOverflow(): void {
  expect.equal(parsePort("999999999999999999999"), Err(OutOfRange));
}
```

`@test` marks an ordinary package-private `fn(): void` in `_test.my`.
Test discovery uses its stable fully qualified function symbol; there is no
second string-named declaration namespace. Test files are compiled only by test
commands (or an explicit all-targets check). Source order determines reporting
order, not execution order.

The built-in `std/test` expectation surface includes at least:

```text
expect.equal(actual, expected)
expect.notEqual(actual, expected)
expect.true(value)
expect.false(value)
expect.some(value)
expect.none(value)
expect.ok(value)
expect.err(value)
expect.matches(value, pattern)
expect.panics(code, closure)
expect.output(expected, closure)
```

Expectations report source expressions and a bounded structural diff. Tests use ordinary functions and lexical `defer` for fixtures; there is no hidden lifecycle annotation system in 1.0.

### 5.2 Commands

```sh
mmay test                         # all tests in current module, VM
mmay test parse                   # substring filter
mmay test --exact parseRejectsOverflow # exact function test name
mmay test ./store                 # selected package
mmay test --engine native
mmay test --engine both           # required semantic comparison
mmay test --watch                 # rerun affected tests
mmay test --list
mmay test --shuffle 42
mmay test --repeat 100
mmay test --no-cache
mmay test --diagnostic json
mmay test --max-heap-bytes 67108864
```

No test-framework dependency or configuration file is required. Test arguments after `--` are not supported by the built-in harness; tests receive explicit fixtures rather than ambient CLI state.

### 5.3 Isolation, scheduling, and output

Each test runs in a fresh root task with a deterministic scheduler, clock, random seed, temporary filesystem, captured output, task limit, heap limit, and timeout. Package tests may share compiled code but never mutable runtime state.

D055 makes that heap limit configurable with one decimal
`--max-heap-bytes`. Each VM execution and native process receives the selected
ceiling with a fresh live-byte counter and failure record. `--engine both`
applies the identical value to both observations. A resource failure is an
`I0002` command failure/exit `70`, not an ordinary failed assertion, and no
later test observes the failed execution's heap state.

Tests execute concurrently across runtime isolates by default. Results are printed in deterministic package/source order. On failure, captured stdout/stderr for that test is printed without interleaving.

`expect.output(expected, closure)` compares the exact D046 stdout bytes
published by that closure, including the trailing LF from each `println`.
Tests grant a captured `"io.stdout"` sink by default; they do not inherit the
host process stdout unless an opt-in host-stdio capability is requested.

A test that exits while child tasks or resources remain is failed even if assertions passed. Children owned by a structured `scope` are cancelled/joined normally; leaked runtime resources are separately reported with allocation/open spans.

Real network, environment, process, and host filesystem access is denied unless the test explicitly requests an opt-in capability flag. Tests using real nondeterminism are never cached.

### 5.4 Test cache

Successful deterministic test results may be cached by the hash of compiler/runtime version, engine, target, package/dependency sources, relevant manifest/lock entries, declared test capabilities, and deterministic inputs. A changed transitive implementation invalidates dependents whose compiled interface or behavior hash changed.

Failures are not reused across source changes. `--no-cache` bypasses reads and writes. The output always states when a result came from cache; JSON output includes the full cache key factors.

### 5.5 Engine equivalence

`mmay test --engine both` uses the execution specification's record/replay harness. A mismatch exits `5` and reports result/output/panic/task/capability trace divergence. Passing the same assertion independently is not sufficient if deterministic observable traces disagree.

Under D051 as extended by D066, the preview deterministic core discovers
`@test fn(): void` and `@test fn(arguments: []string): void` only in
`*_test.my`, includes module-root `tests/` as public-surface integration
packages, and executes each discovered symbol in a fresh VM execution or
native process. VM is the default; `--engine native` and `--engine both` are
accepted. Reports are sorted by package, canonical source path, declaration
span, and symbol name. A test's stdout and stderr are retained in separate
bounded byte buffers and are emitted only with that test's failure, so output
from different tests cannot interleave.

Arguments after the first `--` are injected into every argument-taking test;
zero-parameter tests remain unchanged. The executable name is excluded. The VM
record and native replay use the same D066 validated values and limits.

For `both`, the VM run produces one observation record containing termination,
exact D046 stdout bytes, and exact panic/stderr bytes. Native execution replays
the same canonical typed-MIR test symbol under the same fixed default
capability configuration and is compared to that record. Any difference is
`B0007` and exit `5`; an ordinary matching panic or `G4004` expectation failure
is a test failure and exit `1`. Success exits `0`, command misuse exits `2`,
and an internal compiler/runtime test-pipeline failure exits `70`.

Checkpoint 11 does not implement filters, watch mode, or a test-result cache.
`--filter`, `--exact`, `--watch`, and `--no-cache` are rejected explicitly with
`U0005`/exit `2`; they are never accepted and ignored. The broader command
surface above remains the I8 completion contract.

### 5.6 Fuzz and property tests

Fuzz targets are ordinary typed declarations in `_test.my`:

```marimay
@fuzz
fn decoderNeverPanics(input: Bytes): void {
  const _ = decode(input);
}
```

`@fuzz` marks an ordinary package-private function in `_test.my` with exactly
one typed input parameter and a `void` result. `mmay fuzz decoderNeverPanics`
performs coverage-guided mutation in isolated workers, persists minimized
corpus entries, and writes a reproducible regression fixture on failure. The
parser, formatter, bytecode verifier, package parser, and runtime protocols
must dogfood this facility.

Property generators and shrinking live in first-party `std/test/property`; they do not require a second runner.

## 6. Check, run, build, and execute

### 6.1 `mmay check`

```sh
mmay check
mmay check ./package
mmay check --all-targets
```

`check` runs source loading, parsing, resolution, typing, interface/exhaustiveness checks, effect inference, typed MIR construction, and MIR verification without backend code generation. It is the fastest authoritative semantic check; editor diagnostics use the same compiler service.

The reusable compiler service reaches a minimum editor gate with I2 rather than waiting for the complete I8 developer loop. It accepts versioned in-memory overlays and exposes parse/type diagnostics, go-to-definition, symbol search, completion, and canonical checked-symbol type spelling for hover/signature consumers. Managed arrays in that spelling are always `[]T`, including nested arrays and arrays inside generic arguments. Queries reject stale result generations rather than mixing type identities across overlays. Dot completion for a nominal value enumerates every visible eligible first-parameter function, labels its owning package and equivalent free-call spelling, and uses the exact same lookup result as type checking. I8 productionizes the full LSP transport, workspace indexing, cancellation, compatibility, and editor integration; it does not introduce a second completion engine.

### 6.2 `mmay run`

```sh
mmay run .
mmay run file.my
mmay run --engine vm .
mmay run --engine native .
mmay run . --max-heap-bytes 67108864
mmay run --allow-read ./data file.my
mmay run . -- program arguments
```

VM is default. Native run uses the ordinary content-addressed build cache and then executes the artifact. Program arguments begin only after `--`. Source errors never leave or execute a stale binary.

Under D049 and D066, local VM execution first completes the authoritative checker and
selects the exact root-package `main`. It accepts only `fn main(): void` or
the cross product of an optional exact `[]string` parameter with `void`, `int`,
`Result<void, E>`, and `Result<int, E>`, with the root-visible structural Error contract.
It then lowers canonical verified typed MIR, encodes the complete bytecode
program, decodes it into a new program, verifies that program, and only then
executes it. The Stage 0 checkpoint-9 command accepts `vm` only; `native` is a
`U0005` invalid option until the checkpoint-10 build/cache path exists. This
staging restriction does not change the final native-run contract above.

Successful `void` and `Ok()` returns exit `0`; direct `int` and `Ok(int)`
return the selected `0..255` status after cleanup. Root `Err` is `G4003` on stderr
and exit `1`; explicit, arithmetic, and bounds panics are respectively
`G4000`, `G4001`, and `G4002` on stderr and exit `1`. Invalid entry is `T0017`
and exit `1`. Bytecode pipeline failure `B0001` and runtime integrity/resource
failure `I0002` exit `70`. Program stdout contains only D046 bytes and never an
internal engine-result envelope. `io.stdout` is the sole default local grant.
Arguments after the first `--` are validated under D066 and passed in order;
the executable name is not present. Invalid argument bytes/limits use `G4006`,
and a returned status outside `0..255` uses `G4007` rather than host truncation.
`--max-heap-bytes N` and `--max-heap-bytes=N` apply D055's decimal byte budget
to the fresh VM execution. The option occurs at most once and is parsed before
program arguments; the default is `67108864`.

### 6.3 `mmay build`

```sh
mmay build
mmay build --release
mmay build --engine vm
mmay build --engine native
mmay build --output ./dist/orders
mmay build --target x86_64-linux-gnu
mmay build --emit c
mmay build --emit mir
mmay build --max-heap-bytes 67108864
```

Native is default. Development builds retain checked semantics and source diagnostics; release builds optimize but do not remove overflow/bounds checks unless proven redundant. `--engine vm` emits a `.myb` bundle. Cross-compilation requires a supported target runtime and target pack; target support is explicit, never guessed.

For a supported host target, D030 makes native-toolchain provisioning part of the official Marimay distribution experience. A fresh user does not select `CC`, linker, SDK paths, or runtime objects per project. Installation either supplies the reviewed version-matched native tools or verifies the platform toolchain and gives one precise platform-owned installation action. `mmay doctor` reports the selected identities and compatibility in human and stable machine-readable form. Once installation succeeds, `mmay build` performs no network access. Missing tools are diagnosed before code generation and never surface as an unexplained raw linker failure.

Build cache keys include source/content hashes, public and behavior interfaces, compiler/runtime version, language version, target, profile, backend options, C compiler identity, and relevant environment explicitly admitted by the tool. Absolute workspace paths and undeclared environment values do not affect semantic outputs.

Under D050, checkpoint 10 exposes the host-native subset of this contract.
The accepted options are `--engine native`, `--release`, `--output PATH`, and
`--emit c`; native is the default. `--engine vm` and `--emit mir` are `U0005`
until their portable serialization owners land, and cross-target options are
not inferred. A successful build writes the artifact path and LF to tool
stdout unless `--quiet` is selected.

Checkpoint 19 additionally accepts one D055 `--max-heap-bytes` option for
`run`, `test`, and `build`. Native build always emits an explicit decimal
`max_live_bytes` assignment, including `67108864` when the option is absent.
That emitted value participates in the generated-source cache key and is not
overridden by the environment at build or execution time. `--emit c` contains
the same assignment, so compiling the published C preserves the selected
budget.

Without `--output`, the executable is
`.mmay/build/native/{debug|release}/{sha256}/app` (`app.exe` on Windows) below
the discovered script or module root. The key hashes the portable generated C,
compiler/runtime/language versions, host target, host compiler identity, build
profile, and versioned backend flag contract. A regular file at the exact key
is reused. `--output` atomically replaces the exact requested executable only
after a successful host link. `--emit c` atomically writes the same
public-entry translation unit and performs no compiler invocation. These paths
never access the network or read a project-selected `CC` value.

The distribution-selected compiler is preflighted before C generation.
Unavailable tooling is `B0002`; compiler-owned C emission failure is `B0003`;
host rejection of generated C is `B0004`; cache/output publication failure is
`B0005`; and an unsupported semantic host is `B0006`. Raw compiler/linker
output is bounded and retained for controlled debugging, not substituted for
the stable primary diagnostic.

Under D052, `--release` selects the version-matched release runtime archive
even when the invoking `mmay` executable was built in a debug configuration;
the default profile selects the debug archive even for a release-built
compiler. Both profile artifacts are part of the toolchain distribution and
are validated before C generation by an exact manifest plus embedded
profile/version markers. Installed lookup is relative to the invoked `mmay`
path and remains valid when the complete installation prefix is relocated.

The cache key includes the SHA-256 of the selected runtime archive bytes, not
its absolute location. Missing, partial, stale, or wrong-profile distribution
artifacts produce `B0008`/exit `1` with a reinstall action and never expose raw
linker output. `--emit c` does not link or require a runtime artifact. No
profile lookup reads project configuration, `CC`, or the network.

### 6.4 `mmay exec`

```sh
mmay exec app.myb
mmay exec --allow-read ./data app.myb
mmay exec --max-memory 256MiB --max-tasks 10000 app.myb
```

`exec` verifies the entire bundle, resolves declared capabilities, and starts it in the VM. Untrusted bundles receive no ambient capabilities, including `"io.stdout"`. A grant is required before D046 `println` can publish to the invoking process. Verification happens before package initialization or FFI loading.

## 7. Project and dependency commands

The dependency workflow stays shallow:

```sh
mmay init [path]
mmay add marimay.example/postgres@1.4
mmay remove marimay.example/postgres
mmay update [module]
mmay list
mmay why marimay.example/crypto
mmay outdated
mmay audit
mmay vendor
```

There is intentionally no required `mmay mod <subcommand>` hierarchy for common work.

### 7.1 Resolution

Manifest constraints use semantic-version ranges. The resolver chooses one version per module, preferring the newest version satisfying all constraints, and records the exact graph in `mmay.lock`. Locked builds do not re-resolve. Conflicts report the shortest dependency paths and constraints that made the graph impossible.

Published `(module, version)` pairs are immutable. Registry artifacts are content addressed and include source hash, manifest, license metadata, compiler-language range, and provenance signature when available. A changed artifact under the same identity is rejected even if a registry serves it.

### 7.2 Hermeticity

Dependencies cannot execute install hooks, build scripts, procedural macros, compiler plugins, or arbitrary code during resolution/build. Build and test do not access the network after the explicit fetch phase. `mmay build --offline` fails with a list of missing immutable artifacts rather than attempting access.

External generation is an explicit `mmay generate` command driven by reviewed root-module declarations. It is not run by `build` or dependency installation, and generated `.my` files are ordinary formatter/checker inputs.

### 7.3 Audit

`mmay audit` reports known advisories, yanked versions, source/provenance mismatch, licenses, unsafe packages, FFI libraries, capabilities reachable from the root, duplicate versions if a future resolver permits them, and stale/unmaintained metadata where the registry can support that claim.

Audit output distinguishes cryptographically verified facts from registry/community assertions.

## 8. Documentation, vetting, benchmarks, and fixes

```sh
mmay doc                     # build/open local package documentation
mmay doc --check             # fail on broken links/examples/public-doc gaps
mmay vet                     # correctness and maintainability checks
mmay vet --strict
mmay fix                     # apply compiler-authored safe migrations
mmay bench                   # first-party benchmark harness
mmay bench --compare base.json
mmay explain T2403
mmay explain suspension loadDashboard
```

`mmay vet` is first-party and versioned with the compiler. It may warn about ignored `Result`, wildcard matches hiding known enum cases, mutable closure capture, accidental interface conformance, blocking-risk FFI, capability reachability, and suspension in latency-critical call paths. It must not become a configurable style formatter.

`mmay fix` applies only machine-checkable transformations with conflict detection and prints a diff. It never performs a language-version migration implicitly during build.

`mmay explain` reads the installed version-matched diagnostic reference or compiler effect metadata. It works offline and never evaluates project source as code.

Benchmark results include environment, engine, compiler/runtime version, sample distribution, allocations, GC/task metrics, and source revision when available. Benchmarks are not ordinary correctness tests and are not run by `mmay test`.

## 9. Release and CI profiles

The following commands are the minimum project release gate:

```sh
mmay fmt --check
mmay check --all-targets
mmay test --engine both
mmay vet --strict
mmay doc --check
mmay audit
```

The Marimay compiler repository additionally runs C unit tests, generated-code sanitizers, bytecode fuzzing, bootstrap reproducibility, platform conformance, and performance budgets described in the implementation test strategy.

`CI=true` makes lock staleness, dirty generated files, and interactive prompts hard errors. It does not silently enable different language semantics.

## 10. Toolchain versioning and compatibility

`mmay version` prints compiler, runtime, language, bytecode format, standard
library and its scope, release channel and bounded claim, bootstrap stage,
host, supported targets, and exact available/staged command inventories.
`mmay version --json` emits the versioned `marimay.version/2` schema. Its
documented fields, diagnostic classifications, and exit status are stable for
one candidate.

### 10.1 Pre-beta release contract

Under D064, compiler, runtime, and standard library share the
`0.1.0-alpha.N` line. `N` is a positive decimal sequence without leading
zeroes. Version is not publication state: the required `release` object also
contains the channel, bounded claim, and `publicationReady` boolean. The
closed pre-1.0 channels are `private-preview`, `cli-alpha`,
`concurrency-pilot`, `ecosystem-alpha`, `beta`, and `rc`.

The Stage 0 CP29 candidate is channel `private-preview`, claim
`local-checker-backed-slice`, and `publicationReady: false`. Its available
commands are exactly `build`, `check`, `fmt`, `help`, `run`, `test`, and
`version`. Every other recognized command is staged and fails with `U0003` if
invoked. Staged commands are not everyday workflow promises. In particular,
`explain` remains staged under I8 until its installed offline reference owner
exists.

The manifest directive `language "0.1"` selects the current normative design
baseline. Until that baseline's product gate changes its classification, the
version metadata reports it exactly as `0.1-design`; it does not call the
source surface stable.

The version JSON contains these required top-level members:

```text
schema compiler runtime language release bytecode standardLibrary
bootstrapStage host targets commands
```

`release` contains `channel`, `claim`, and `publicationReady`. `bytecode`
contains `version` and `compatibility`; both are respectively `unavailable`
and `none` until a public bundle decision lands. `standardLibrary` contains
`version`, `scope`, and `compatibility`; CP29 reports the common compiler
version, `bounded-preview`, and `alpha-source`. `commands` contains exact
`available` and `staged` arrays. Arrays are deterministic and sorted by command
name within each classification.

Before beta, an accepted source/API surface may break only in a new alpha
sequence with an accepted decision, simultaneous specification and tests, and
an explicit migration note. A required mechanical migration blocks release
until its `mmay fix --language` owner exists. Stable diagnostic codes,
severities, exit classifications, and documented JSON fields do not change
within a candidate; a breaking machine-schema change increments both schema
and alpha sequence. Human wording may improve without reclassifying an
outcome.

D054's pre-contract `Array<T>` spelling has no alias or compatibility window;
only `[]T` belongs to the alpha surface. Portable `.myb` bytes have no public
compatibility promise while bytecode is unavailable. The bounded standard
library is versioned, but its scope and exclusions remain explicit until its
closure matrix is complete.

Installed metadata, offline known limitations, help, README claims, roadmap,
and human/JSON version output must agree. A version number, prepared archive,
or `publicationReady` candidate never authorizes tagging, signing, upload,
namespace reservation, or announcement.

The `language` directive chooses the accepted source language. A newer toolchain must continue compiling stable source for earlier 1.x language versions. Experimental features require an explicit manifest opt-in and cannot appear in published stable packages.

Formatter changes that alter output are tied to a language/toolchain release and applied explicitly with `mmay fix --language <version>` when migration is needed. Building never rewrites source.

## 11. Performance budgets are user-facing contracts

Before 1.0, the project publishes and continuously measures:

- cold `mmay run` latency for a one-file program;
- no-change `mmay check` and `mmay test` latency;
- clean and one-file incremental builds for the reference service;
- formatter throughput and peak memory;
- VM startup and bytecode verification throughput;
- native binary size and startup;
- parked-task memory and scheduler throughput;
- GC latency/throughput under reference workloads;
- package resolution and offline cache behavior.

Exact thresholds are set only after Stage 0 prototypes establish honest baselines. Regressions beyond an accepted budget block release or require a recorded, measured trade-off.
