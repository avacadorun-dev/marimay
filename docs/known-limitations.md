# Marimay 0.1.0-alpha.1 known limitations

Version: `0.1.0-alpha.1`

Channel: `private-preview`

Claim: `local-checker-backed-slice`

Publication ready: `no`

Standard library: `0.1.0-alpha.1`, scope `bounded-preview`, compatibility
`alpha-source`

Bytecode: `unavailable`, compatibility `none`

## Implemented surface

The portable C17 Stage 0 provides human/JSON version metadata, task help,
canonical `fmt`, authoritative local `check`, verified VM-default `run`,
host-native `build`, and deterministic `test --engine both` for the current
checker-backed private-preview corpus. The implemented language slice includes
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
`Map`/`Set` collections with mutation-checked iterators.

## Exclusions

- The first-party `std/cli` parser and typed JSON remain CP37–CP38.
  User-defined formatting remains deliberately deferred by D065.
- Accepted `==`/`!=` semantics are not yet executable for `string`, closed
  enums, or `Option` payloads. These forms currently reach the generic I0001
  implementation diagnostic; that fallback also lacks the required primary
  source span and construct-specific detail.
- CP36 implements only the closed `@derive(Eq, Hash)` pair required for
  coherent nominal collection keys. General `@derive(Debug)`, standalone
  derived equality, and the corresponding ordinary equality operators remain
  incomplete, so `examples/tour.my` is not yet an executable language tour.
- The executable test surface currently provides `expect.output`; the
  specified `expect.equal`, `expect.true`, and `expect.ok` helpers remain
  unavailable. Preview fixtures continue to use explicit conditions and
  `panic` until that test-library surface is implemented.
- Generic unit constructors now use their expected nominal type, including
  nested contexts such as `Ok(None)`. Positional constructor shorthand and
  annotation-free expression closures are contextual only at compiler-known
  `Result.mapError` in 0.1; they are not general storable function values.
- Native `mmay run`, `mmay init`, `mmay doctor`, and complete install artifacts
  remain CP39. Commands shown as staged are not executable promises.
- Portable public `.myb`/`mmay exec`, networking and direct-style tasks,
  registry/dependencies, production LSP/test tooling, FFI, cross compilation,
  desktop support, and self-hosting are outside CLI0.
- `Array<T>` is not a compatibility spelling. The only managed-array spelling
  in the alpha line is `[]T`.

## Publication blockers

N0 lacks external legal/control/PATH evidence. Full claimed-host architecture,
bootstrap, frontend fuzz, VM/native, sanitizer, installer, and performance
matrices are also incomplete. No public identity, tag, signing operation,
upload, namespace reservation, installer publication, or release is
authorized by this candidate metadata.

## Removal gates

The [CLI0 closure matrix](implementation/cli0-closure-matrix.md) assigns every
known CP29 gap. CP30–CP39 remove the bounded application and install gaps;
CP40 performs the publication-ready audit. Rows explicitly excluded from CLI0
remain owned by their product gate.
