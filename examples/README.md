# Normative readability examples

These examples are executable specification fixtures once the parser exists. Until then they are reviewed manually and by the documentation consistency checks.

- `tour.ruda` exercises records, algebraic enums, exhaustive match, first-parameter dot calls, structural interfaces, deterministic `f64` arithmetic, and the absence of `impl`.
- `concurrency.ruda` exercises direct-style I/O, `Result`, explicit error mapping, scoped `spawn`, and cancellation by early `?`.
- `service.ruda` sketches a production HTTP handler without async-colored signatures.
- `tour_test.ruda` shows the built-in test syntax.
- `testing_checkpoint_test.ruda` is the runnable checkpoint-11 test example:
  package-local `@test` discovery, record identity, D041 cleanup, and exact
  D046 `expect.output` capture. It runs with VM, native, or `--engine both`.
- `nominal_cp1.ruda` is the focused nominal-engine example: record mutation and
  `identity.same` (including once-only left-to-right operand evaluation),
  nested enum matching, D040 record and named-enum field evaluation order, and
  the equivalence of a visible eligible free call, dot call, and structural
  interface call.
- `matrix.ruda` is the first source-checkable D045 array kernel: a loop-based
  2-by-3 times 3-by-2 `f64` multiplication whose checksum is `415.0`. Under
  D046 it writes that checksum with `println(formatF64(...))?`, which must
  publish `415.0\n` on VM, native, and test capture.
- `local_packages.ruda` shows the D043 local-package surface: named type and
  function aliases, namespace qualification, direct dot calls, and structural
  interface conformance all resolve behavior owned by the imported nominal
  package. Its public `ImportedUsers` and `ImportedUserRows` aliases also show
  canonical `[]User` transport through a package-owned wrapper without a
  boundary conversion.
- `array_checkpoint.ruda` checks the D045 array surface, including construction,
  mutation, checked indexed compound assignment, and nonmutating
  `for const item of array` traversal.
- `array_mutation.ruda` shows the D053 typed `pop`, `truncate`, `clear`, and
  `reserve` surface over scalar and nested managed arrays. Iteration mutation
  is rejected through the separate exact-G4005 acceptance corpus.
- `receiver_collection_names.ruda` demonstrates receiver-directed resolution
  when ordinary `Stack` functions and compiler-known `[]T` operations
  share `push` and `length`: free and dot forms preserve one symbol per exact
  receiver and neither spelling receives reserved precedence.
- The checkpoint-20 reusable algorithm example is the sibling
  `../ruda-algotithms/ruda` module. Its sorting, stack, queue, min-heap, DSU,
  BFS, and O(V²) Dijkstra package owners run through `fmt`, `check`, VM,
  `test --engine both`, and Debug/Release native builds. The main repository's
  independent executable mirror is
  `tests/post-preview/cp20-algorithm-corpus`; exact commands and linked corpus
  revisions are recorded in the checkpoint-20 evidence.

An implementation change that makes an example invalid must update the normative spec and corresponding decision in the same change. After I2, all examples are compiled by `ruda check`; after I6, they run under `ruda test --engine both` with deterministic service fixtures.
