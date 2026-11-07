<div align="center">

<img src="docs/assets/marimay-banner.png" alt="Marimay — from source to something solid" width="960" />

<p><strong>A little programming language I’m building to learn and have fun.</strong><br />
A place to try ideas, write algorithms, and see how languages work.</p>

<p>
  <a href="#build-and-try-it">Get started</a> ·
  <a href="docs/spec/language.md">Language</a> ·
  <a href="ROADMAP.md">Roadmap</a> ·
  <a href="CONTRIBUTING.md">Contribute</a> ·
  <a href="SECURITY.md">Security</a>
</p>

<p><strong>C17 bootstrap</strong> &nbsp; / &nbsp; <strong>VM + native</strong> &nbsp; / &nbsp; <a href="LICENSE"><strong>MIT</strong></a></p>

</div>

Marimay is my personal learning project: a programming language, a compiler,
and a runtime that I can tinker with. I’m exploring how these pieces fit
together, and having fun writing small programs and algorithms in the language
itself.

The command is **`mmay`**, and source files use **`.my`**. Under the hood, a
C17 bootstrap produces typed intermediate code for a bytecode VM and native
compilation through C17. Building both paths is part of the learning exercise.

**Development status:** this is an experimental, publicly available source
repository. The compiler identifies itself as `0.1.0-alpha.1` on the
`private-preview` channel. That historical channel name describes its bounded
implementation contract; this repository is not a published alpha release.
Language compatibility, performance parity, and all-platform release acceptance
remain open. See [known limitations](docs/known-limitations.md), the
[release contract](docs/releases/0.1.0-alpha.1.md), and the
[source publication policy](docs/security/publication.md).

## A small taste

```marimay
import { Error as IoError, println } from "std/io";

enum PreviewState {
  Ready(int),
  Waiting,
}

fn previewScore(state: PreviewState): int {
  return match state {
    Ready(value) => value,
    Waiting => 0,
  };
}

fn main(): Result<void, IoError> {
  println("Marimay CP29 quick start")?;
  return Ok();
}
```

This uses the same syntax as the executable
[quick-start fixture](tests/public-alpha/cp29-quick-start/main.my).
`Result` and postfix `?` make expected failures explicit; closed enums make
pattern matching exhaustive.

## Things I’m exploring

- **Data and failures.** Nominal records and enums, `Option`, `Result`,
  exhaustive `match`, and no `null` or `any` escape hatch.
- **Functions and interfaces.** Ordinary top-level functions, restricted
  first-parameter dot calls, and structural interfaces. No `impl` blocks or
  separate method declarations.
- **A VM and a native path.** A shared typed MIR, verified bytecode, a VM,
  and native C17 emission with differential tests.
- **Everyday tooling.** Built-in formatting, checking, testing, local project
  initialization, and offline installation diagnostics.
- **Bootstrapping.** A portable C17 Stage 0 bootstrap; later
  self-hosting remains part of the design.

Direct-style structured concurrency, networking, portable `.myb` execution,
and desktop support are **target design**, not completed features. The
[specification](docs/spec/language.md) describes the intended language;
[limitations](docs/known-limitations.md) describe what runs today.

## Build and try it

You need **CMake 3.25+**, a **C17 compiler** (GCC, Clang, AppleClang, or MSVC),
and **Python 3.10+** for repository checks. The default bootstrap has no fetched
third-party dependency. Run these commands from the repository root:

<!-- cp29-quick-start:begin -->
```sh
cmake --preset debug
cmake --build --preset debug --parallel
build/debug/mmay version --json
build/debug/mmay fmt tests/public-alpha/cp29-quick-start --check
build/debug/mmay check tests/public-alpha/cp29-quick-start
build/debug/mmay run tests/public-alpha/cp29-quick-start
build/debug/mmay test tests/public-alpha/cp29-quick-start --engine both
build/debug/mmay build tests/public-alpha/cp29-quick-start --output build/debug/cp29-quick-start
build/debug/cp29-quick-start
```
<!-- cp29-quick-start:end -->

The VM and native executable both print `Marimay CP29 quick start`.
On Windows with a multi-configuration generator, add `--config Debug` to the
build command and use `build/debug/Debug/mmay.exe`. Native executable paths
also end in `.exe`.

For an optimized compiler, use `cmake --preset release` and
`cmake --build --preset release --parallel`. Supported optimized builds enable
interprocedural optimization through `MMAY_COMPILER_IPO=AUTO`. The optional
[SLJIT tier](docs/implementation/vm-jit.md) is an explicit experiment, disabled
by default, and requires a separately obtained, hash-verified local dependency.

## The everyday workflow

After building, use `build/debug/mmay` wherever `mmay` appears below:

```sh
mmay init hello
mmay fmt --check hello
mmay check hello
mmay run hello
mmay run --engine native hello
mmay test --engine both hello
mmay build hello --output hello-app
mmay doctor --json
```

`run` and `test` default to the VM; `build` defaults to a native executable.
`mmay help` is authoritative for implemented commands. Future commands report
that they are staged. Filesystem and environment access require explicit
grants; see [security boundaries](SECURITY.md).

Run the repository tests with:

```sh
ctest --preset debug
./tools/check-architecture
./tools/check-roadmap
```

GitHub Actions covers the platform build matrix, sanitizers, Linux leak checks,
fuzzing, optional JIT checks, and documentation gates. A configured CI job is
not evidence that a platform or release gate has passed. See [CI documentation](docs/ci.md).

## Explore the repository

| Start here | What you will find |
| --- | --- |
| [Language specification](docs/spec/language.md) | Types, functions, patterns, and source semantics |
| [Toolchain contract](docs/spec/toolchain.md) | CLI, projects, capabilities, and distribution |
| [Execution model](docs/spec/execution.md) | Typed MIR, VM/native equivalence, runtime boundaries |
| [Standard library](docs/spec/standard-library.md) | First-party API contracts |
| [Architecture](docs/implementation/compiler-architecture.md) | Compiler owners and dependency boundaries |
| [Roadmap](ROADMAP.md) | Current work, acceptance gates, and evidence |
| [Examples](examples/README.md) | Readability examples and their implementation status |
| [Implementation brief](IMPLEMENTATION-BRIEF.md) | A starting point for implementation work |

## Contributing with people and agents

If you feel like looking around or trying something, you’re welcome.
Read [CONTRIBUTING.md](CONTRIBUTING.md) and [AGENTS.md](AGENTS.md) before changing
code. Human and agent contributions follow the same architecture, evidence,
memory-ownership, and review rules. [Agent workflow](docs/agent-workflow.md)
explains context loading, handoffs, and reporting checks accurately.

Use [GitHub issues](https://github.com/avacadorun-dev/marimay/issues) for ordinary
bugs and design discussion. Report vulnerabilities privately using
[SECURITY.md](SECURITY.md). Keep reports and logs free of credentials and personal paths.

## License

Original Marimay code and documentation are available under the [MIT License](LICENSE).
Third-party data and optional dependencies retain their own terms; see
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). The banner lives in this
repository and loads no remote image, font, badge, or tracking pixel.
