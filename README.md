# Marimay

Latin spelling: **Marimay**. Cyrillic spelling: **Ру́да**, pronounced «ру́да». The toolchain command is `mmay`, and source files use `.my`.

> “Marimay is a statically typed, garbage-collected programming language for reliable native applications—from CLIs and network services to desktop software. It combines algebraic data types, structural interfaces, direct-style structured concurrency, and a unified VM/native toolchain, without null, async coloring, or exceptions for expected failures.”

> «Marimay — статически типизированный язык программирования с управляемой памятью для создания надёжных нативных приложений: от CLI и сетевых сервисов до десктопных программ. Он сочетает алгебраические типы данных, структурные интерфейсы, прямой стиль структурированной конкурентности и единый VM/native toolchain — без null, async-окраски и исключений для ожидаемых ошибок».

The name means ore: source material transformed by a clear language, strict types, and a cohesive toolchain into reliable native software.

This is the canonical positioning of the intended product, not a claim that every subsystem is implemented today. The current portable C17 Stage 0 builds the `mmay` executable and provides help/version output, a lossless frontend, canonical formatting, authoritative batch `mmay check`, verified local VM execution, host-native `mmay build`, and deterministic VM/native `mmay test` for the checker-backed local/private preview surface. Portable bundles, native `mmay run`, networking, and desktop support remain gated roadmap work. [D001](docs/design/decisions.md) accepts the repository identity; [N0](docs/design/naming-audit.md) still blocks public distribution until legal review and external domains, accounts, namespaces, signing identity, and command/PATH coexistence have evidence. None of those external identities is claimed or reserved here.

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

## Read the language in one minute

```marimay
import { Error as IoError, println } from "std/io";

enum Lookup<T> {
  Found(T),
  Missing,
  Denied { reason: string; },
}

type User {
  id: int;
  name: string;
}

interface Stringer {
  string(): string;
}

fn string(user: User): string {
  return $"{user.name} (#{user.id})";
}

fn describe(result: Lookup<User>): string {
  return match result {
    case Found(user) => user.string();
    case Missing => "not found";
    case Denied { reason } => $"denied: {reason}";
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

## Target execution contract: two engines, one language

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

These commands are the normative product workflow. In the current bootstrap, `mmay fmt`, `mmay check`, VM-default `mmay run`, host-native `mmay build`, and deterministic `mmay test` with VM/native comparison (plus help/version inspection) are implemented for the local/private preview surface; the roadmap gates the remaining options and commands. `mmay check` refuses success when accepted source reaches a checker path that is not implemented yet. The VM is not a looser or dynamically typed version of the language: both implemented execution paths consume code that passed the same parser, resolver, type checker, exhaustiveness checker, and typed MIR verifier.

`mmay run`, `mmay test`, and `mmay build` accept one decimal
`--max-heap-bytes`; the unchanged default is 64 MiB. Native builds embed the
selected value, and configured-budget versus host-allocation failures have
distinct `I0002` diagnostics with exit `70`.

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
