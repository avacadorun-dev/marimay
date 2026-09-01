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
