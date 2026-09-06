# Marimay

Latin spelling: **Marimay**. Cyrillic spelling: **Ру́да**, pronounced «ру́да». The toolchain command is `mmay`, and source files use `.my`.

> “Marimay is a statically typed, garbage-collected programming language for reliable native applications—from CLIs and network services to desktop software. It combines algebraic data types, structural interfaces, direct-style structured concurrency, and a unified VM/native toolchain, without null, async coloring, or exceptions for expected failures.”

> «Marimay — статически типизированный язык программирования с управляемой памятью для создания надёжных нативных приложений: от CLI и сетевых сервисов до десктопных программ. Он сочетает алгебраические типы данных, структурные интерфейсы, прямой стиль структурированной конкурентности и единый VM/native toolchain — без null, async-окраски и исключений для ожидаемых ошибок».

The name means ore: source material transformed by a clear language, strict types, and a cohesive toolchain into reliable native software.

This is the canonical positioning of the intended product, not a claim that every subsystem is implemented today. The current portable C17 Stage 0 builds the `mmay` executable and provides help/version output, a lossless frontend, canonical formatting, authoritative batch `mmay check`, verified local VM/native execution, host-native `mmay build`, deterministic VM/native `mmay test`, local `mmay init`, offline `mmay doctor`, and inspectable installation artifacts for the checker-backed local/private preview surface. Portable bundles, networking, and desktop support remain gated roadmap work. [D001](docs/design/decisions.md) accepts the repository identity; [N0](docs/design/naming-audit.md) still blocks public distribution until legal review and external domains, accounts, namespaces, signing identity, and command/PATH coexistence have evidence. None of those external identities is claimed or reserved here.

Marimay is not a dialect or a collage of borrowed keywords. It uses a compact C-family surface, but has its own semantic center:

- closed algebraic enums and exhaustive `match`;
- `Option<T>` instead of nullable references;
- `Result<T, E>` and `?` instead of exceptions or repeated error branches;
- ordinary functions with restricted first-parameter dot calls, without a second method declaration system or `impl` blocks;
- structural interfaces over nominal records and enums;
- one compiler frontend and typed MIR, with both a portable bytecode VM and C17-based native AOT;
- direct-style lightweight tasks and structured concurrency, without `async`-colored APIs;
- a single `mmay` tool containing the formatter, test runner, build system, package workflow, docs, fuzzing, and diagnostics.

The first implementation is written in portable C17. After the language and bootstrap contract stabilize, the compiler is rewritten in Marimay and verified against the C17 bootstrap.

## Run the implemented private preview

The current candidate is `0.1.0-alpha.1` on channel `private-preview`. The
version identifies the contract; it is not a published alpha. From a checkout,
this checked-in quick start exercises the original CP29 workflow baseline:

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

CP39 adds native execution, local project initialization, installed toolchain
diagnostics, and deterministic distribution artifacts:

```sh
build/debug/mmay doctor --json
build/debug/mmay run --engine native tests/public-alpha/cp29-quick-start
build/debug/mmay init alpha-smoke
build/debug/mmay fmt --check alpha-smoke
build/debug/mmay check alpha-smoke
build/debug/mmay test --engine both alpha-smoke
cmake --build build/debug --target mmay_package --parallel
```

`build/debug/distribution` then contains one host archive, one portable source
archive, and their SHA-256 siblings. Extracted host trees include
`share/mmay/V/MarimayDistributionLifecycle.cmake`; its `install`, `upgrade`,
and `uninstall` actions require an explicit prefix, validate the installed
checksum manifest, preserve unrelated prefix files, and never edit `PATH`.
For an extracted host tree in `./marimay-tree`, a clean explicit-prefix install
uses:

```sh
cmake -DMMAY_DISTRIBUTION_ACTION=install \
  -DMMAY_DISTRIBUTION_SOURCE=./marimay-tree \
  -DMMAY_DISTRIBUTION_PREFIX=./marimay-prefix \
  -P ./marimay-tree/share/mmay/0.1.0-alpha.1/MarimayDistributionLifecycle.cmake
```

The current source surface also includes CP30 dynamic primitive interpolation,
boolean/all-width integer formatting, and `parseInt`/`parseIntRadix`; CP31
bounded UTF-8 program arguments and cleanup-safe process statuses; and CP32
bounded typed stdin, independent stdout/stderr, and explicit flush with exact
VM/native behavior. CP33-CP36 add bounded owned text/bytes, capability-confined
paths/files and process context, and insertion-ordered `Map`/`Set`. CP37 adds
the ordinary first-party `std/cli` parser with typed definition/usage errors
and deterministic bounded help. CP38 adds ordinary `std/json` bounded cursors,
explicit package-owned typed codecs, strict UTF-8/number/container handling,
and deterministic compact encoding. For an argument-taking package, for example:

```sh
build/debug/mmay run tests/public-alpha/cp31-args -- alpha "two words"
build/debug/mmay test tests/public-alpha/cp31-args --engine both
build/debug/mmay build tests/public-alpha/cp31-args --output build/debug/cp31-args
build/debug/cp31-args alpha "two words"
```

The executable name is excluded from source `[]string`; empty strings, Unicode,
leading dashes, and a literal `--` after the separator are preserved exactly.

The CP32 stream fixture can be exercised through either engine:

```sh
printf 'hello\r\n' | build/debug/mmay run tests/public-alpha/cp32-stdio
build/debug/mmay test tests/public-alpha/cp32-stdio --engine both
build/debug/mmay build tests/public-alpha/cp32-stdio --output build/debug/cp32-stdio
printf 'hello\r\n' | build/debug/cp32-stdio
```

Both run paths keep program stdout, program stderr, and tool diagnostics
separate. `readLine` is bounded to at most 1048576 returned UTF-8 bytes;
printing never flushes implicitly, so durable/interactive boundaries use
`flushStdout` or `flushStderr` explicitly.

The CP37 parser example exposes long/short flags, required and optional
values, repeatable options, positionals, `--`, exact usage errors, and help:

```sh
build/debug/mmay run tests/public-alpha/cp37-cli-parser -- --help
build/debug/mmay run tests/public-alpha/cp37-cli-parser -- --count 3 input.txt
build/debug/mmay test tests/public-alpha/cp37-cli-parser --engine both
```

The CP38 transformer reads capability-granted JSON bytes, decodes records,
enums, arrays, and insertion-ordered maps through an explicit codec, updates
the typed model, and writes deterministic JSON:

```sh
build/debug/mmay run tests/public-alpha/cp38-json \
  --allow-read tests/public-alpha/cp38-json \
  -- tests/public-alpha/cp38-json/input.json
build/debug/mmay test tests/public-alpha/cp38-json --engine both
```

The quick-start VM and native paths print `Marimay CP29 quick start`. The
[known limitations](docs/known-limitations.md) distinguish this executable
slice from CLI0 and the 1.0 target. `mmay help` lists the nine implemented
commands, including local `init`, installed-layout `doctor`, and VM/native
`run`; recognized future commands are visibly staged and return `U0003`.

## Target design: read the language in one minute

The following example describes the accepted target language. It is parsed and
formatter-checked as documentation; its primitive interpolation is executable
under CP30, while the complete program remains separate from the deliberately
smaller CP29 quick start.

```marimay
import { Error as IoError, println } from "std/io";

enum Lookup<T> {
  Found(T),
  Missing,
  Denied { reason: string, },
}

type User {
  id: int,
  name: string,
}

interface Stringer {
  string(): string;
}

fn string(user: User): string {
  return $"{user.name} (#{user.id})";
}

fn describe(result: Lookup<User>): string {
  return match result {
    Found(user) => user.string(),
    Missing => "not found",
    Denied { reason } => $"denied: {reason}",
  };
}

fn main(): Result<void, IoError> {
  println(describe(Lookup.Found(User { id: 7, name: "Mara" })))?;
  return Ok();
}
```

The dot call and ordinary function call are the same symbol:

```marimay
const a = user.string();
const b = string(user);
```

There is no hidden method body and no second behavior system. Because `User` is owned by the package and is the exact first-parameter type of `string`, the compiler adds `string(): string` to its dot-call set. That same set satisfies `Stringer`; no conformance declaration is needed.

## Target design: two engines, one language

```sh
mmay run .                         # fast typed bytecode VM
mmay run --engine native .         # compile and execute native
mmay build --release               # standalone native executable
mmay build --engine vm             # portable .myb bytecode bundle
mmay test                          # fast VM test loop
mmay test --engine both            # VM/native semantic conformance
mmay run . --max-heap-bytes 67108864 # managed live-byte budget
mmay fmt                           # canonical formatting
mmay check                         # authoritative batch type checking
```

These commands are the normative target product workflow, not the CP29 command
inventory. In the current bootstrap, `mmay fmt`, `mmay check`, VM/native
`mmay run`, host-native `mmay build`, deterministic `mmay test` with
VM/native comparison, local `mmay init`, and offline `mmay doctor` (plus
help/version inspection) are available for the local/private preview surface;
`mmay help` is authoritative for the exact inventory. The roadmap gates the
remaining options and commands. `mmay check`
refuses success when accepted source reaches a checker path that is not
implemented yet. The VM is not a looser or dynamically typed version of the
language: both implemented execution paths consume code that passed the same
parser, resolver, type checker, exhaustiveness checker, and typed MIR verifier.

`mmay run`, `mmay test`, and `mmay build` accept one decimal
`--max-heap-bytes`; the unchanged default is 64 MiB. Native builds embed the
selected value, and configured-budget versus host-allocation failures have
distinct `I0002` diagnostics with exit `70`.

Local `run` and `test` do not impose an instruction-count or elapsed-time
cutoff. Interrupt a nonterminating program explicitly, and set process timeouts
in CI. The embedding VM's finite fuel remains available for sandboxed hosts;
verification, memory and capability checks still apply to local commands.

## Repository map

- [`docs/research/evidence.md`](docs/research/evidence.md) — primary evidence and the pain-point matrix;
- [`docs/design/principles.md`](docs/design/principles.md) — the design constitution;
- [`docs/design/decisions.md`](docs/design/decisions.md) — accepted and deferred architectural decisions;
- [`docs/spec/language.md`](docs/spec/language.md) — normative language baseline;
- [`docs/spec/execution.md`](docs/spec/execution.md) — VM, AOT, runtime, tasks, and equivalence;
- [`docs/spec/toolchain.md`](docs/spec/toolchain.md) — the `mmay` command contract;
- [`docs/spec/standard-library.md`](docs/spec/standard-library.md) — required first-party APIs and production behavior;
- [`docs/spec/diagnostics.md`](docs/spec/diagnostics.md) — stable human and machine diagnostics;
- [`docs/requirements.md`](docs/requirements.md) — requirement-to-gate traceability;
- [`IMPLEMENTATION-BRIEF.md`](IMPLEMENTATION-BRIEF.md) — complete handoff to an implementation agent;
- [`docs/implementation/roadmap.md`](docs/implementation/roadmap.md) — gates from C17 bootstrap to production and self-hosting;
- [`docs/implementation/checkpoints.md`](docs/implementation/checkpoints.md) — the single CP0–CP40 execution ledger and next ready checkpoint;
- [`docs/implementation/evidence/README.md`](docs/implementation/evidence/README.md) — evidence grouped by product gate and historical checkpoint;
- [`docs/implementation/bootstrap.md`](docs/implementation/bootstrap.md) — build and verify the current C17 bootstrap slice;
- [`docs/implementation/compiler-architecture.md`](docs/implementation/compiler-architecture.md) — Stage 0 phase ownership, validation boundaries, and refactoring checkpoints;
- [`docs/research/compiler-architecture.md`](docs/research/compiler-architecture.md) — primary-source research behind the physical compiler architecture;
- [`docs/implementation/test-strategy.md`](docs/implementation/test-strategy.md) — required verification matrix;
- [`examples/`](examples) — readability and conformance examples.

The product roadmap owns gate state, the checkpoint ledger owns execution
order, and the evidence index owns direct reports. `./tools/check-roadmap`
validates their shared IDs and states.
