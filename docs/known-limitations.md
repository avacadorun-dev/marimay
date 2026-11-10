# Marimay 0.1.0-alpha.3 known limitations

Version: `0.1.0-alpha.3`

Channel: `experimental-alpha`

Claim: `local-checker-backed-slice`

Publication ready: `no`

This field records incomplete CLI0/CP40 readiness. The owner separately
authorized D114's bounded experimental macOS prerelease; neither authorization
nor publication promotes a product milestone.

Standard library: `0.1.0-alpha.3`, scope `bounded-preview`, compatibility
`alpha-source`

Bytecode: `unavailable`, compatibility `none`

## Implemented surface

The portable C17 Stage 0 provides human/JSON version metadata, task help,
canonical `fmt`, authoritative local `check`, verified VM-default `run`,
native `run`, host-native `build`, deterministic `test --engine both`, local
atomic `init`, and offline human/JSON `doctor` for the current
checker-backed experimental corpus. The implemented language slice includes
scalar control flow, checked integer behavior, deterministic `f64`, dynamic
primitive interpolation, integer parse/format, and `println(string)`, nominal
records/enums/interfaces, `Option`/`Result`/`?`, lexical `defer`, local packages
and owned dot calls, managed `[]T` arrays, parenthesized tuples, comma-delimited
data declarations and match arms with block-tail values, unified iterable
`for`, compiler-known integer ranges, total `as`, `tryConvert`, contextual
`mapError` mappers, explicit diagnostics for discarded `Result` values,
bounded program arguments and controlled exit, typed standard streams,
owned UTF-8 text and `Bytes` builders, capability-confined paths/files and
environment access, normalized process context, and insertion-ordered
`Map`/`Set` collections with mutation-checked iterators. The ordinary
first-party `std/cli` package provides bounded declarative options,
positionals, typed usage errors, and deterministic help. The ordinary
first-party `std/json` package provides bounded strict decoding, deterministic
compact encoding, explicit package-owned typed codecs, byte-span errors, and
unknown-field policies without reflection or a dynamic value tree. Counted string,
closed enum, `Option`/`Result`, and immutable `@derive(Eq)` record equality are
executable through typed MIR on VM and native. The built-in preview test
surface includes `expect.output`, `equal`, `notEqual`, `true`, `false`,
`some`, `none`, `ok`, and `err`. `examples/tour` is an executable module with
five VM/native differential tests.

## Exclusions

- JSON derives, a schema registry, a dynamic DOM, network-body streaming, and
  arbitrary/private reflection are absent; D082 deliberately keeps explicit
  package-owned codecs. User-defined formatting remains deferred by D065.
- Recursively self-referential nominal equality remains staged. Preview
  `@derive(Debug)` publishes compiler metadata but does not create a
  user-callable formatter; user-defined formatting remains deferred by D065.
  The richer `expect.matches` and `expect.panics` helpers are also staged.
- Accepted constructs that are still checker-incomplete produce an exact
  source-owned `I0001` diagnostic and exit `70`; they remain exclusions rather
  than silent feature claims.
- Generic unit constructors now use their expected nominal type, including
  nested contexts such as `Ok(None)`. Positional constructor shorthand and
  annotation-free expression closures are contextual only at compiler-known
  `Result.mapError` in 0.1; they are not general storable function values.
- Cross compilation, automatic PATH editing, signing and notarization remain
  absent. D114 separately permits the experimental macOS package and release
  workflow; this does not establish CP39/CP40 completion. Commands shown as
  staged are not executable promises.
- Portable public `.myb`/`mmay exec`, networking and direct-style tasks,
  registry/dependencies, production LSP/test tooling, FFI, cross compilation,
  desktop support, and self-hosting are outside CLI0.
- `Array<T>` is not a compatibility spelling. The only managed-array spelling
  in the alpha line is `[]T`.

- The experimental `mmay lsp` supports diagnostics, navigation for top-level
  symbols/locals/record fields and standard-library source/API views, completion,
  validated rename and formatting. Current analyses are cached; fresh checking
  and unfinished-expression completion remain synchronous.
  Generic-local specializations, enum payload identities, complex call/index
  receiver completion, semantic tokens, background indexing and asynchronous
  cancellation remain unavailable. See [editor support](editor-support.md).

## Known gaps carried as debt

These gaps are reproduced or recorded and accepted for the alpha line, each with
a workaround or a bounded scope. None of them is a silent exclusion.

- **Matching a `byte` value against a literal fails `I0001`.** Reproduction:
  `fn f(v: byte): int { return match v { 44 => 1, _ => 0 }; }`. The same
  function with `v: int` works; the source-lint scanner avoids the construct
  with boolean comparisons. Evidence:
  [portfolio matrix](implementation/evidence/cp42-portfolio-matrix-2026-11-11.md).
- **A VM frame hits a 10,000,000-instruction limit (`I0002`) on pathological
  inputs where native succeeds** (E08). D087 removed embedder fuel from local
  commands; the engine asymmetry remains for extreme inputs.
- **General VM string-pool retention is open.** The `report-concat` defect is
  fixed by the D055 follow-up; other long-lived concatenation shapes are not
  measured.
- **`Version` discovery needs a second decode pass** (E04) and **localization has
  no universal position accessor** (E05). Both are application-ergonomics debts,
  excluded from CLI0.
- **Native startup is measured on repeated execution** (D125). A freshly linked
  binary costs about 240 ms on this platform before `main` runs — page-in and
  code-signing validation, measured with a trivial C program — so the budget row
  reports 5-11 ms of application startup and carries the first-execution number
  beside the baseline it must be compared with.

## Publication blockers

N0 lacks external legal/control/PATH evidence; D123 excludes N0 from the bounded
CLI claim, records the residual risk, and returns the gate for I7, signing and
the 1.0 audit. Full broader-release architecture,
bootstrap, frontend fuzz, VM/native, sanitizer, installer, and performance
acceptance remains incomplete. The experimental binary scope is only macOS 15+
ARM64/Intel, with compatible Apple Command Line Tools required for native
compilation. Packages are unsigned/unnotarized and same-version installation
is deliberately rejected rather than overwriting an existing prefix; see
[installation and recovery](install-macos.md).

The [D114](design/decisions.md#d114-publish-a-bounded-experimental-macos-alpha)
owner authorization and verified release workflow permit only that bounded
experimental prerelease. Candidate metadata alone authorizes no tag, upload,
signing operation, namespace reservation or release. Broader CLI0/N0/CP40
requirements remain unchanged.

## Removal gates

The [CLI0 closure matrix](implementation/cli0-closure-matrix.md) assigns every
known CP29 gap. CP30–CP39 remove the bounded application and install gaps;
CP40 performs the publication-ready audit. Rows explicitly excluded from CLI0
remain owned by their product gate.
