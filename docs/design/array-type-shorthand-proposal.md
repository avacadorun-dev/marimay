# Proposal: compact managed-array type spelling

Status: non-normative design proposal; not accepted.
Motivation snapshot: algorithm comparison on 2026-08-18.

## Problem

Nested managed arrays are common in graph, matrix, grid, and dynamic-programming
code. The current spelling exposes the generic implementation at every level:

```marimay
public type IntAdjacency = Array<Array<int>>;
public type WeightedAdjacency = Array<Array<Edge>>;

let adjacency = arrayOf<Array<int>>();
```

The punctuation grows outward and makes the element type harder to scan. In the
algorithm experiment, aliases were introduced partly to avoid repeating these
signatures. The desired source shape is closer to:

```marimay
public type IntAdjacency = [][]int;
public type WeightedAdjacency = [][]Edge;

let adjacency: [][]int = [];
```

## Recommended end state

Define prefix `[]T` as the canonical source spelling of the existing managed
array type whose element type is `T`:

```text
[]int       == Array<int>
[][]int     == Array<Array<int>>
[][]Edge    == Array<Array<Edge>>
[]pkg.Edge  == Array<pkg.Edge>
```

The form is right-associative: `[][]int` applies the `[]` constructor twice to
`int`. It is a type spelling only. It does not add a new runtime type,
collection, ownership model, variance rule, conversion, MIR layout, bytecode
tag, or ABI identity.

It composes with existing type forms:

```marimay
Option<[]int>
Result<[][]Edge, GraphError>
Map<string, []int>
fn flatten(rows: [][]int): []int
```

`[]T` must mean managed `Array<T>`, not `Slice<T>`. Although the punctuation is
familiar from Go, Marimay's `Array<T>` has stable managed-reference identity and
mutable growth, while `Slice<T>` is a non-owning scope-bounded view. Documentation
and hover text must make that distinction explicit.

## Grammar feasibility

The current parser uses `[T, U]` for tuple types. A left bracket immediately
followed by a right bracket is not a valid current tuple type, so `[]T` can be
recognized in type position without changing the meaning of valid existing
source:

```ebnf
array_type = "[" "]" type ;
tuple_type = "[" type { "," type } "]" ;
```

Expression `[]` remains an empty array literal because expression and type
positions are already distinguished by the parser. No lexer token is required.

The implementation still needs deliberate recovery rules for incomplete
source such as `[]`, `[][]`, `[][`, and `[]Map<string,` so editor diagnostics do
not degrade.

## Canonical formatting and migration

Marimay's design constitution rejects permanent overlapping spellings by default.
The preferred end state is therefore:

- `[]T` is the canonical user-facing spelling and the formatter always emits
  adjacent brackets with no intervening space;
- diagnostics, signatures, documentation, and IDE hover use `[]T`;
- internal compiler and runtime code may continue to call the semantic type
  `Array`;
- existing pre-release `Array<T>` source is migrated mechanically rather than
  keeping two equally canonical spellings forever.

There are two viable transition policies that require an explicit decision:

1. Pre-release replacement: accept `[]T`, migrate repository source, and stop
   accepting `Array<T>` as source syntax before 1.0.
2. One-preview migration: accept both temporarily, warn on `Array<T>`, provide a
   syntax-aware migration command, and remove the old source spelling at the
   next preview boundary.

`mmay fmt` should not silently rewrite an arbitrary identifier named `Array`
without a syntax or semantic guarantee that it denotes the prelude type. If the
old form remains ordinary identifier-based generic syntax during migration, a
dedicated migration action is safer than a context-free formatter rewrite.

## Construction should become shorter too

Changing only signatures leaves empty construction verbose. The existing D045
contract already permits an empty array literal when an expected array type is
available, so examples should prefer:

```marimay
let rows: [][]int = [];
let values: []int = [];
```

When no expected type exists, the explicit constructor can use the shorthand
for its element type:

```marimay
let rows = arrayOf<[]int>();
```

A future inference proposal may reduce that further, but it should not be
bundled into the type-spelling change.

## Alternatives considered

### Keep `Array<Array<int>>`

This has no grammar cost and names the semantic type explicitly, but it is the
verbosity that triggered the experiment. Aliases hide repetition only after a
user invents and navigates another name.

### `int[][]`

This is familiar from Java and C#, but postfix nesting reads in the opposite
direction from Marimay's existing prefix/container type forms and becomes harder
to compose with qualified, generic, tuple, and function element types.

### `[int]`

This is compact but collides with the existing one-element tuple type shape and
with the broader tuple grammar.

### `Array2<int>` or a library alias

Dimension-specific names do not compose, create an arbitrary family of aliases,
and do not help `Array<Map<string, Array<Edge>>>`.

### Make `[]T` mean `Slice<T>` as in Go

That would optimize for visual familiarity at the cost of misrepresenting
Marimay's owning managed array. It would also leave the original nested `Array`
verbosity unresolved. This proposal explicitly rejects that mapping.

## Adjacent findings kept out of this syntax change

The algorithm exercise exposed several other ergonomics gaps. They should be
tracked independently so accepting `[]T` does not silently approve unrelated
semantics.

### Collection method names

Marimay stack/heap examples had to use `add`/`take` and
`insert`/`removeMinimum` because user-defined `push`/`pop` paths collided with
compiler-known array behavior in the current preview. The accepted dot-call
model should already allow receiver-directed names such as `stack.push(value)`
and `stack.pop()` for an owned nominal `Stack`. Treat this first as a name
resolution/toolchain conformance issue, not a request for special method
syntax.

### Prelude `Option` and `Result` across packages

Imported package code did not resolve the ordinary prelude constructors
consistently, so the experiment introduced `IntOption` and `SearchResult`
workarounds. Fixing generic prelude visibility and imported enum lowering is
preferable to adding shorter absence syntax now.

### Nested generics at package boundaries

Reusable graph signatures using `Array<Array<T>>` could pass checking while the
current run/test lowering path still failed. A compact spelling is valuable only
if the same canonical type works through package metadata, checker, MIR,
bytecode, VM, and native execution.

### Missing collection operations

The current executable preview lacks a complete shrink/pop surface. Stack and
heap implementations therefore retain backing elements and maintain a logical
size. Candidate library operations include `pop`, `truncate`, `clear`, and
non-observable `reserve`; their failure and iteration-invalidation contracts
need separate standard-library decisions.

### Other possible punctuation aliases

This proposal is not precedent for immediately adding `T?`, `map[K]V`, or a
short spelling for every generic. `?` already carries propagation meaning, and
each extra spelling pays parser, diagnostics, formatter, documentation, and
teaching cost. Add only forms supported by repeated real-code evidence.

## Required work if accepted

Acceptance would require the normal semantic-change protocol in one coherent
change:

1. a new accepted decision or an amendment to D045 choosing the end-state and
   migration policy;
2. normative language and standard-library text defining `[]T` and its exact
   equivalence to the managed array type;
3. parser tests for nesting, generics, qualified element types, tuples,
   functions, malformed input, and recovery;
4. checker/import tests proving one canonical type identity across both source
   spellings during any migration window;
5. formatter fixtures and idempotence tests for `[]T`, `[][]T`, and mixed type
   expressions;
6. VM/native differential tests showing no observable semantic or layout
   change;
7. diagnostics and IDE display rules using the chosen canonical spelling;
8. migrated public examples, standard-library signatures, and teaching
   material;
9. an explicit removal gate if `Array<T>` is accepted temporarily.

These are impact areas for later planning, not pre-sliced atomic checkpoints.
