# The `ruda` toolchain contract — design baseline 0.1

Status: normative design baseline.

The identity mapping is normative: the language/product name is **Ruda**, the toolchain executable is `ruda`, source files use `.ruda`, bytecode bundles use `.rudab`, module/workspace/lock files are `ruda.mod`, `ruda.work`, and `ruda.lock`, and local tool state uses `.ruda/`. The former `ma`/`.ma`/`ma.*`/`.mab`/`.ma/` names are historical and are not compatibility aliases. D001 records this as a semantics-neutral pre-release rename. N0 still blocks public distribution until legal review, domains, public accounts, package/registry namespaces, signing identity, and command/PATH coexistence have direct evidence; this specification does not claim that any such external identity is available or controlled.

Installing Ruda installs one user-facing executable named `ruda`. The formatter, test runner, compiler, VM, package resolver, documentation generator, vet checks, fuzz driver, and bootstrap metadata are versioned together.

This distribution boundary does not make every command part of Ruda source
semantics. The compiler reports semantic facts; first-party tools consume those
facts; manifests and runtime grants configure execution; CI decides which
checks and provenance are required for a release. Source code carries no
author, agent, trust, coverage, or attestation marker under D034.

The core promise is literal:

```sh
ruda fmt
ruda test
```

These commands require no project plugin, task runner, formatter configuration, test configuration, or copied script.

## 1. Root discovery and targets

Starting from the working directory, `ruda` searches parent directories for `ruda.mod`. The nearest file is the module root. An optional `ruda.work` above it can join multiple local modules without changing their published identity.

With no explicit target:

- `ruda fmt` formats the entire current module;
- `ruda check` checks the entire current module;
- `ruda test` tests the entire current module;
- `ruda build` builds the root executable package, or reports the available executable packages;
- `ruda run` runs the root executable package.

This behavior does not silently change when invoked from a subdirectory. A path or package selector narrows it explicitly:

```sh
ruda test ./services/orders
ruda fmt ./src
ruda run ./cmd/admin -- --repair account-42
```

A standalone `.ruda` file without `ruda.mod` can be checked, formatted, tested, or run in script mode. Script mode has no third-party dependencies and receives no filesystem/network/process capability unless explicitly granted. D046 capability `"io.stdout"` is granted by default for `ruda run` of a local module or script and for a native executable from `ruda build`; it is not a filesystem, network, or process grant.

### 1.1 Batch check discovery

```sh
ruda check
ruda check ./path/to/package
ruda check file.ruda
ruda check --all-targets
```

With no target, discovery starts at the physical current directory, finds the
nearest ancestor `ruda.mod`, and checks its whole module even when invoked from
a descendant package. An explicit directory equal to that root selects the
whole module; another directory within it selects the corresponding package.
An explicit regular `.ruda` file is a standalone one-file check. Other file
kinds, unreadable targets, a missing module root for a directory target, or an
empty selected graph report `K0004`.

A module check requires the D048 minimal manifest identity: exactly one
nonempty `module` value and exactly one nonempty `language` value. The root
package uses the decoded module path, and source-bearing descendant directories
append their relative `/`-separated path. All discovered packages, including
disconnected packages, are checked in canonical import-path order; files are
ordered by normalized path. The default source set excludes `_test.ruda` files
and the root `tests/` directory. `--all-targets` includes them. Traversal does
not follow symbolic links, enter `.git` or `.ruda`, or cross a nested module,
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

Unknown commands and flags report a spelling suggestion. They use stable diagnostics `U0001` and `U0002` respectively; JSON output follows the nullable-primary rule in the diagnostics specification. `ruda help` lists only top-level user tasks; `ruda help <command>` gives examples before exhaustive flags. The CLI does not automatically execute `ruda-<name>` binaries from `PATH`, preventing typos from invoking untrusted plugins.

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

`ruda.mod` uses the deliberately small, bounded [manifest grammar](manifest.md) so Stage 0 needs no general configuration language:

```text
module "example.com/acme/orders"
language "0.1"

require (
  "ruda-lang.example/postgres" "^1.4.0"
  "ruda-lang.example/telemetry" "~2.3.1"
)

capability (
  "fs.read" "./config/**"
  "net.listen" "0.0.0.0:8080"
  "net.connect" "db.internal:5432"
  "env.read" "DATABASE_URL"
)
```

Directives and grouping parentheses are fixed by the manifest grammar. Strings use JSON-compatible escapes. Unknown directives are errors unless namespaced as optional metadata. `ruda fmt` also canonicalizes `ruda.mod`.

Libraries may document capabilities they can use, but only the root executable grants them. A dependency cannot broaden the root capability set.

`ruda.lock` is generated, committed, and contains exact versions, transitive dependency edges, source/archive hashes, registry checkpoint, signatures when present, license identifiers, and feature-free build identity. Normal build/test refuses a stale lock in CI mode.

`ruda.work` contains canonical `use "relative/path"` directives. Workspace replacement never changes published module metadata and is excluded from dependency artifacts.

## 4. Formatting

### 4.1 Commands

```sh
ruda fmt                 # format the current module in place
ruda fmt ./src           # format a selected tree
ruda fmt file.ruda        # format one file
ruda fmt --check         # do not write; fail if any file differs
ruda fmt --diff          # print the canonical diff; do not write
ruda fmt --stdin         # stdin to stdout, filename supplied optionally
```

`--check` emits `F0004` for each non-canonical input and exits `1` without writing. `--diff` writes a deterministic unified diff and exits `0` when every selected input is syntactically valid and successfully formatted, whether or not the diff is empty; callers that need a cleanliness gate use `--check`. Formatter I/O, limit, lexical, or parser failures still make `--diff` exit nonzero.

`ruda fmt` formats `.ruda`, `ruda.mod`, and `ruda.work` files; it never rewrites `ruda.lock` except through dependency commands.

Target selection follows D028. With no explicit target, the nearest regular `ruda.mod` on the physical working-directory ancestor chain owns the selected module; absence of such a file is a formatter input error. The nearest regular `ruda.work` from that module root upward, when present, is also selected. An explicit regular file must end in `.ruda` or be named exactly `ruda.mod` or `ruda.work`. An explicit directory selects a bounded tree rooted there.

Tree traversal considers regular recognized files in unsigned UTF-8 normalized-path order. It does not follow symbolic links, does not enter `.git` or `.ruda` tool-state directories, and does not cross into a nested directory containing its own regular `ruda.mod`. An explicitly selected symbolic link or unrecognized file is an input error. Traversal has fixed defensive ceilings for directory depth, directory entries, selected files, and path bytes; exhaustion reports `F0003` before further work.

If a source file cannot be parsed with bounded recovery, the formatter reports diagnostics and leaves that file untouched. Formatting a tree is transactional per file: no partially written file is possible. Writes use a sibling temporary file, flush, permission preservation, and atomic replacement where supported.

### 4.2 Canonical style

Ruda 1.x has no formatter configuration. The style is:

- two-space indentation, never tabs;
- LF line endings and one final newline;
- mandatory source semicolons preserved/inserted only where grammar requires them;
- opening braces on the declaration/control line;
- canonical declarations use `fn`, nominal records use `type Name { ... }`, and aliases alone use `=`;
- record construction and named enum payloads use `Name { ... }`; control-flow scrutinees are not parenthesized;
- interpolated strings use `$"...{expression}..."`, and visibility uses only `public`;
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

Package-internal tests live in files ending `_test.ruda` and can access package-private declarations. Black-box integration tests live under the module-root `tests/` tree and import only public APIs. Documentation code blocks tagged `ruda test` are compiled and run by default.

```ruda
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

`@test` marks an ordinary package-private `fn(): void` in `_test.ruda`.
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
ruda test                         # all tests in current module, VM
ruda test parse                   # substring filter
ruda test --exact parseRejectsOverflow # exact function test name
ruda test ./store                 # selected package
ruda test --engine native
ruda test --engine both           # required semantic comparison
ruda test --watch                 # rerun affected tests
ruda test --list
ruda test --shuffle 42
ruda test --repeat 100
ruda test --no-cache
ruda test --diagnostic json
```

No test-framework dependency or configuration file is required. Test arguments after `--` are not supported by the built-in harness; tests receive explicit fixtures rather than ambient CLI state.

### 5.3 Isolation, scheduling, and output

Each test runs in a fresh root task with a deterministic scheduler, clock, random seed, temporary filesystem, captured output, task limit, heap limit, and timeout. Package tests may share compiled code but never mutable runtime state.

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

`ruda test --engine both` uses the execution specification's record/replay harness. A mismatch exits `5` and reports result/output/panic/task/capability trace divergence. Passing the same assertion independently is not sufficient if deterministic observable traces disagree.

Under D051, preview checkpoint 11 exposes the deterministic core of this
contract. `ruda test [target]` discovers `@test fn(): void` only in
`*_test.ruda`, includes module-root `tests/` as public-surface integration
packages, and executes each discovered symbol in a fresh VM execution or
native process. VM is the default; `--engine native` and `--engine both` are
accepted. Reports are sorted by package, canonical source path, declaration
span, and symbol name. A test's stdout and stderr are retained in separate
bounded byte buffers and are emitted only with that test's failure, so output
from different tests cannot interleave.

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

Fuzz targets are ordinary typed declarations in `_test.ruda`:

```ruda
@fuzz
fn decoderNeverPanics(input: Bytes): void {
  const _ = decode(input);
}
```

`@fuzz` marks an ordinary package-private function in `_test.ruda` with exactly
one typed input parameter and a `void` result. `ruda fuzz decoderNeverPanics`
performs coverage-guided mutation in isolated workers, persists minimized
corpus entries, and writes a reproducible regression fixture on failure. The
parser, formatter, bytecode verifier, package parser, and runtime protocols
must dogfood this facility.

Property generators and shrinking live in first-party `std/test/property`; they do not require a second runner.

## 6. Check, run, build, and execute

### 6.1 `ruda check`

```sh
ruda check
ruda check ./package
ruda check --all-targets
```

`check` runs source loading, parsing, resolution, typing, interface/exhaustiveness checks, effect inference, typed MIR construction, and MIR verification without backend code generation. It is the fastest authoritative semantic check; editor diagnostics use the same compiler service.

The reusable compiler service reaches a minimum editor gate with I2 rather than waiting for the complete I8 developer loop. It accepts versioned in-memory overlays and exposes parse/type diagnostics, go-to-definition, symbol search, and completion. Dot completion for a nominal value enumerates every visible eligible first-parameter function, labels its owning package and equivalent free-call spelling, and uses the exact same lookup result as type checking. I8 productionizes the full LSP transport, workspace indexing, cancellation, compatibility, and editor integration; it does not introduce a second completion engine.

### 6.2 `ruda run`

```sh
ruda run .
ruda run file.ruda
ruda run --engine vm .
ruda run --engine native .
ruda run --allow-read ./data file.ruda
ruda run . -- program arguments
```

VM is default. Native run uses the ordinary content-addressed build cache and then executes the artifact. Program arguments begin only after `--`. Source errors never leave or execute a stale binary.

Under D049, local VM execution first completes the authoritative checker and
selects the exact root-package `main`. It accepts only `fn main(): void` or
`fn main(): Result<void, E>` with the root-visible structural Error contract.
It then lowers canonical verified typed MIR, encodes the complete bytecode
program, decodes it into a new program, verifies that program, and only then
executes it. The Stage 0 checkpoint-9 command accepts `vm` only; `native` is a
`U0005` invalid option until the checkpoint-10 build/cache path exists. This
staging restriction does not change the final native-run contract above.

Successful `void` and `Ok()` returns exit `0`. Root `Err` is `G4003` on stderr
and exit `1`; explicit, arithmetic, and bounds panics are respectively
`G4000`, `G4001`, and `G4002` on stderr and exit `1`. Invalid entry is `T0017`
and exit `1`. Bytecode pipeline failure `B0001` and runtime integrity/resource
failure `I0002` exit `70`. Program stdout contains only D046 bytes and never an
internal engine-result envelope. `io.stdout` is the sole default local grant.
Arguments after the first `--` are retained as program arguments; until a
source argument API is accepted they have no observable language value.

### 6.3 `ruda build`

```sh
ruda build
ruda build --release
ruda build --engine vm
ruda build --engine native
ruda build --output ./dist/orders
ruda build --target x86_64-linux-gnu
ruda build --emit c
ruda build --emit mir
```

Native is default. Development builds retain checked semantics and source diagnostics; release builds optimize but do not remove overflow/bounds checks unless proven redundant. `--engine vm` emits a `.rudab` bundle. Cross-compilation requires a supported target runtime and target pack; target support is explicit, never guessed.

For a supported host target, D030 makes native-toolchain provisioning part of the official Ruda distribution experience. A fresh user does not select `CC`, linker, SDK paths, or runtime objects per project. Installation either supplies the reviewed version-matched native tools or verifies the platform toolchain and gives one precise platform-owned installation action. `ruda doctor` reports the selected identities and compatibility in human and stable machine-readable form. Once installation succeeds, `ruda build` performs no network access. Missing tools are diagnosed before code generation and never surface as an unexplained raw linker failure.

Build cache keys include source/content hashes, public and behavior interfaces, compiler/runtime version, language version, target, profile, backend options, C compiler identity, and relevant environment explicitly admitted by the tool. Absolute workspace paths and undeclared environment values do not affect semantic outputs.

Under D050, checkpoint 10 exposes the host-native subset of this contract.
The accepted options are `--engine native`, `--release`, `--output PATH`, and
`--emit c`; native is the default. `--engine vm` and `--emit mir` are `U0005`
until their portable serialization owners land, and cross-target options are
not inferred. A successful build writes the artifact path and LF to tool
stdout unless `--quiet` is selected.

Without `--output`, the executable is
`.ruda/build/native/{debug|release}/{sha256}/app` (`app.exe` on Windows) below
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

### 6.4 `ruda exec`

```sh
ruda exec app.rudab
ruda exec --allow-read ./data app.rudab
ruda exec --max-memory 256MiB --max-tasks 10000 app.rudab
```

`exec` verifies the entire bundle, resolves declared capabilities, and starts it in the VM. Untrusted bundles receive no ambient capabilities, including `"io.stdout"`. A grant is required before D046 `println` can publish to the invoking process. Verification happens before package initialization or FFI loading.

## 7. Project and dependency commands

The dependency workflow stays shallow:

```sh
ruda init [path]
ruda add ruda-lang.example/postgres@1.4
ruda remove ruda-lang.example/postgres
ruda update [module]
ruda list
ruda why ruda-lang.example/crypto
ruda outdated
ruda audit
ruda vendor
```

There is intentionally no required `ruda mod <subcommand>` hierarchy for common work.

### 7.1 Resolution

Manifest constraints use semantic-version ranges. The resolver chooses one version per module, preferring the newest version satisfying all constraints, and records the exact graph in `ruda.lock`. Locked builds do not re-resolve. Conflicts report the shortest dependency paths and constraints that made the graph impossible.

Published `(module, version)` pairs are immutable. Registry artifacts are content addressed and include source hash, manifest, license metadata, compiler-language range, and provenance signature when available. A changed artifact under the same identity is rejected even if a registry serves it.

### 7.2 Hermeticity

Dependencies cannot execute install hooks, build scripts, procedural macros, compiler plugins, or arbitrary code during resolution/build. Build and test do not access the network after the explicit fetch phase. `ruda build --offline` fails with a list of missing immutable artifacts rather than attempting access.

External generation is an explicit `ruda generate` command driven by reviewed root-module declarations. It is not run by `build` or dependency installation, and generated `.ruda` files are ordinary formatter/checker inputs.

### 7.3 Audit

`ruda audit` reports known advisories, yanked versions, source/provenance mismatch, licenses, unsafe packages, FFI libraries, capabilities reachable from the root, duplicate versions if a future resolver permits them, and stale/unmaintained metadata where the registry can support that claim.

Audit output distinguishes cryptographically verified facts from registry/community assertions.

## 8. Documentation, vetting, benchmarks, and fixes

```sh
ruda doc                     # build/open local package documentation
ruda doc --check             # fail on broken links/examples/public-doc gaps
ruda vet                     # correctness and maintainability checks
ruda vet --strict
ruda fix                     # apply compiler-authored safe migrations
ruda bench                   # first-party benchmark harness
ruda bench --compare base.json
ruda explain T2403
ruda explain suspension loadDashboard
```

`ruda vet` is first-party and versioned with the compiler. It may warn about ignored `Result`, wildcard matches hiding known enum cases, mutable closure capture, accidental interface conformance, blocking-risk FFI, capability reachability, and suspension in latency-critical call paths. It must not become a configurable style formatter.

`ruda fix` applies only machine-checkable transformations with conflict detection and prints a diff. It never performs a language-version migration implicitly during build.

`ruda explain` reads the installed version-matched diagnostic reference or compiler effect metadata. It works offline and never evaluates project source as code.

Benchmark results include environment, engine, compiler/runtime version, sample distribution, allocations, GC/task metrics, and source revision when available. Benchmarks are not ordinary correctness tests and are not run by `ruda test`.

## 9. Release and CI profiles

The following commands are the minimum project release gate:

```sh
ruda fmt --check
ruda check --all-targets
ruda test --engine both
ruda vet --strict
ruda doc --check
ruda audit
```

The Ruda compiler repository additionally runs C unit tests, generated-code sanitizers, bytecode fuzzing, bootstrap reproducibility, platform conformance, and performance budgets described in the implementation test strategy.

`CI=true` makes lock staleness, dirty generated files, and interactive prompts hard errors. It does not silently enable different language semantics.

## 10. Toolchain versioning and compatibility

`ruda version` prints compiler, runtime, bytecode format, standard library, bootstrap stage, host, and supported target versions. `ruda version --json` is stable machine-readable data.

The `language` directive chooses the accepted source language. A newer toolchain must continue compiling stable source for earlier 1.x language versions. Experimental features require an explicit manifest opt-in and cannot appear in published stable packages.

Formatter changes that alter output are tied to a language/toolchain release and applied explicitly with `ruda fix --language <version>` when migration is needed. Building never rewrites source.

## 11. Performance budgets are user-facing contracts

Before 1.0, the project publishes and continuously measures:

- cold `ruda run` latency for a one-file program;
- no-change `ruda check` and `ruda test` latency;
- clean and one-file incremental builds for the reference service;
- formatter throughput and peak memory;
- VM startup and bytecode verification throughput;
- native binary size and startup;
- parked-task memory and scheduler throughput;
- GC latency/throughput under reference workloads;
- package resolution and offline cache behavior.

Exact thresholds are set only after Stage 0 prototypes establish honest baselines. Regressions beyond an accepted budget block release or require a recorded, measured trade-off.
