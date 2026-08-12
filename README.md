# Ruda

Latin spelling: **Ruda**. Cyrillic spelling: **Ру́да**, pronounced «ру́да». The toolchain command is `ruda`, and source files use `.ruda`.

> “Ruda is a statically typed, garbage-collected programming language for reliable native applications—from CLIs and network services to desktop software. It combines algebraic data types, structural interfaces, direct-style structured concurrency, and a unified VM/native toolchain, without null, async coloring, or exceptions for expected failures.”

> «Ruda — статически типизированный язык программирования с управляемой памятью для создания надёжных нативных приложений: от CLI и сетевых сервисов до десктопных программ. Он сочетает алгебраические типы данных, структурные интерфейсы, прямой стиль структурированной конкурентности и единый VM/native toolchain — без null, async-окраски и исключений для ожидаемых ошибок».

The name means ore: source material transformed by a clear language, strict types, and a cohesive toolchain into reliable native software.

This is the canonical positioning of the intended product, not a claim that every subsystem is implemented today. The current portable C17 Stage 0 builds the `ruda` executable and provides help/version output, a lossless frontend, and canonical formatting. Type checking, VM execution, native builds, networking, and desktop support remain gated roadmap work. [D001](docs/design/decisions.md) accepts the repository identity; [N0](docs/design/naming-audit.md) still blocks public distribution until legal review and external domains, accounts, namespaces, signing identity, and command/PATH coexistence have evidence. None of those external identities is claimed or reserved here.

Ruda is not a dialect or a collage of borrowed keywords. It uses a compact C-family surface, but has its own semantic center:

- closed algebraic enums and exhaustive `match`;
- `Option<T>` instead of nullable references;
- `Result<T, E>` and `?` instead of exceptions or repeated error branches;
- ordinary functions with restricted first-parameter dot calls, without a second method declaration system or `impl` blocks;
- structural interfaces over nominal records and enums;
- one compiler frontend and typed MIR, with both a portable bytecode VM and C17-based native AOT;
- direct-style lightweight tasks and structured concurrency, without `async`-colored APIs;
- a single `ruda` tool containing the formatter, test runner, build system, package workflow, docs, fuzzing, and diagnostics.

The first implementation is written in portable C17. After the language and bootstrap contract stabilize, the compiler is rewritten in Ruda and verified against the C17 bootstrap.

## Read the language in one minute

```ruda
import { println } from "std/io";

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

fn main(): void {
  println(describe(Lookup.Found(User { id: 7, name: "Mara" })));
}
```

The dot call and ordinary function call are the same symbol:

```ruda
const a = user.string();
const b = string(user);
```

There is no hidden method body and no second behavior system. Because `User` is owned by the package and is the exact first-parameter type of `string`, the compiler adds `string(): string` to its dot-call set. That same set satisfies `Stringer`; no conformance declaration is needed.

## Target execution contract: two engines, one language

```sh
ruda run .                         # fast typed bytecode VM
ruda run --engine native .         # compile and execute native
ruda build --release               # standalone native executable
ruda build --engine vm             # portable .rudab bytecode bundle
ruda test                          # fast VM test loop
ruda test --engine both            # VM/native semantic conformance
ruda fmt                           # canonical formatting
```

These commands are the normative product workflow. In the current bootstrap, only `ruda fmt` (plus help/version inspection) is implemented; the roadmap gates the remaining commands. Once present, the VM is not a looser or dynamically typed version of the language: both engines run code that passed the same parser, resolver, type checker, exhaustiveness checker, and typed MIR verifier.

## Repository map

- [`docs/research/evidence.md`](docs/research/evidence.md) — primary evidence and the pain-point matrix;
- [`docs/design/principles.md`](docs/design/principles.md) — the design constitution;
- [`docs/design/decisions.md`](docs/design/decisions.md) — accepted and deferred architectural decisions;
- [`docs/spec/language.md`](docs/spec/language.md) — normative language baseline;
- [`docs/spec/execution.md`](docs/spec/execution.md) — VM, AOT, runtime, tasks, and equivalence;
- [`docs/spec/toolchain.md`](docs/spec/toolchain.md) — the `ruda` command contract;
- [`docs/spec/standard-library.md`](docs/spec/standard-library.md) — required first-party APIs and production behavior;
- [`docs/spec/diagnostics.md`](docs/spec/diagnostics.md) — stable human and machine diagnostics;
- [`docs/requirements.md`](docs/requirements.md) — requirement-to-gate traceability;
- [`IMPLEMENTATION-BRIEF.md`](IMPLEMENTATION-BRIEF.md) — complete handoff to an implementation agent;
- [`docs/implementation/roadmap.md`](docs/implementation/roadmap.md) — gates from C17 bootstrap to production and self-hosting;
- [`docs/implementation/bootstrap.md`](docs/implementation/bootstrap.md) — build and verify the current C17 bootstrap slice;
- [`docs/implementation/test-strategy.md`](docs/implementation/test-strategy.md) — required verification matrix;
- [`examples/`](examples) — readability and conformance examples.

The roadmap status labels and linked evidence are authoritative for implementation readiness.
