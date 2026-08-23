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
scalar control flow, checked integer behavior, deterministic `f64`, literal
strings and `println(string)`, nominal records/enums/interfaces,
`Option`/`Result`/`?`, lexical `defer`, local packages and owned dot calls, and
managed `[]T` arrays.

## Exclusions

- Dynamic interpolation and complete primitive formatting, program argument
  access, typed standard streams, `Bytes`/builders, paths/files/environment,
  `Map`/`Set`, `std/cli`, and typed JSON remain CP30–CP38.
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
