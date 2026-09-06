# Typed JSON and useful failures

Describe public data once, then use `json.decode<Database>(contents)` and
`json.encode(database)`. This corpus exercises the strict codec in the VM and
native debug/release executables.

For a nested tag that is not a string, `json.message(error)` identifies both
the byte and the value: `expected a JSON string at byte 97 at $["notes"][1]["tags"][1]`.

Use `json.path(error)` for a fresh array of `json.PathSegment.Field { name }`
and `json.PathSegment.Index { index }` values. Indices start at zero.
`json.pathText(error)` is a bounded, escaped display, not a machine-readable
replacement for those segments. Missing, duplicate and unknown fields include
their name; malformed container separators identify the container. Input
validation and trailing-input failures identify the root `$`.

Explicit codecs add context with `json.atField(error, "notes")` and
`json.atIndex(error, index)` without mutating the original error. Automatic
codecs add the enclosing context without application loops.

Pre-alpha migration: named `json.Error` cases gained `path`. Exhaustive
patterns must bind it or write `path: _`; manually constructed named errors
must supply it (usually `path: []`). `InvalidLimits` remains a root-only unit
case. Code using `json.message` does not need a signature change.

Compiler-service consumers can query intrinsic file-import completions and
generic signatures, including named aliases and namespace spellings. The
metadata states the eligible type family, explicit/inferred type-argument
policy and direct-call-only restriction. These are not first-class functions
or fabricated source declarations. File-import candidates do not imply local
cursor-scope availability or a production LSP implementation.

From the repository root:

```sh
build/debug/mmay fmt tests/public-alpha/typed-json --check
build/debug/mmay test tests/public-alpha/typed-json --engine both
ctest --test-dir build/debug -R '^mmay_typed_json$' --output-on-failure
```
