# I2 local packages and D043 — preview checkpoint 2 — 2026-08-16

Gate: deterministic local-package checking and non-generic D043 import-form
invariance. Checkpoint status: locally complete; I2 remains `in-progress`.

Baseline: `d34864ae9acb6dde8965c9c04e12ffbf43c710a2`
(`docs: close nominal engine checkpoint`).

Authority: language §3, §5.8, and §6.2; diagnostics §7 and §10; accepted
D003, D004, and D043; the compiler architecture vertical-feature contract;
and the test-strategy import matrix. This checkpoint implements accepted
semantics and does not add a package manager, network resolver, general
overload resolution, or a second completion/resolution engine.

## Delivered vertical slice

- The public bounded `ruda_check_package_graph` API checks an explicitly
  supplied local package closure. Exact decoded import paths are canonical
  package identities. Packages, files, dependency edges, and import bindings
  are processed deterministically; cycles include the canonical path in
  `R0009`.
- One graph check owns one `RudaCheckResult`, type table, function index, and
  symbol universe. Package results are not separately checked and
  concatenated, preventing result-local `RudaTypeId` collisions in MIR,
  bytecode nominal schemas, and native descriptors.
- Named type-only, named type-plus-function (including aliases), and namespace
  imports feed the existing D003 resolver. All direct imports of an owning
  package expose its public eligible functions to exact nominal dot lookup and
  structural interface conformance. Only the available free spelling changes.
- Imported public record fields, record/enum layouts, interface boxing, direct
  calls, and overloaded owned functions use the same canonical types and
  symbols. Foreign same-spelled nominal types do not cross-match, and a local
  package cannot add D043 behavior to an imported nominal type.
- Parser coverage already included both import forms and recovery. Formatter
  fixtures now cover namespace/named mixtures, aliases, canonical path groups,
  comment barriers, compact D031 lists, and the 100-column multiline boundary
  without reordering named members.
- `examples/local_packages.ruda` documents named aliases, namespace spelling,
  dot calls, and structural conformance as a public source example.

## Conformance and negative coverage

The positive matrix uses a two-file `acme/store` package with a public record,
an algebraic enum, two same-name owned `score` functions, and factories. A
second package imports the types in each of the three normative forms, passes
record and enum values through the same structural interface, and resolves
the same owned symbols. The root package executes the graph to exact `i64`
value `14` in the MIR oracle, collect-before-allocation VM, and generated C17.

Each form verifies every MIR function. A field-by-field canonical serialization
of typed MIR is byte-identical across the three source spellings after removing
only source spans, which remain form-specific by design. The span-normalized
encoded bytecode is byte-identical as an independent lowering assertion. A
separate permutation test reverses package/file input order and import
declaration order and compares complete encoded program bytes; diagnostic code,
message, source path, and primary span are likewise stable under input
enumeration reversal.

Negative tests cover unknown package and export, private named and namespace
functions/types, private record fields, unavailable type-only and namespace
free spellings, alias preservation, duplicate bindings, same-file and
multi-file declaration conflicts, dependency cycles, interface mismatch,
foreign-extension exclusion, and same-spelled nominal types owned by distinct
packages. Diagnostic results retain one exact diagnostic and publish no
partial semantic vectors.

Explicit bounds cover package count, total imports, imports per file, items per
import, total bindings, dependency depth, package-path bytes, graph-wide
function count, and graph-wide symbol count. Success, cycle-message, and
late-checker-diagnostic paths run deterministic fail-every-allocation sweeps;
every teardown asserts both `live_allocations == 0` and `live_bytes == 0`.
The canonical MIR serializer used by the form-invariance assertion has its own
deterministic fail-every-allocation sweep with the same zero-live counters.

## Architecture

`package_graph.c` owns graph coordination and binding construction;
`checker_lookup_imports.c` owns imported spelling and D043 lookup;
`checker_imported_nominal.c` adapts already checked record/enum layouts to a
consumer function's MIR. `checker.c` remains phase coordination, and
`resolution.c` remains the shared local/imported call-resolution boundary.
The feature does not filter published function vectors in compiler service or
repair nominal identity in either backend.

All new production and test owners remain within the architecture limits.
`package_graph.c` is 1,006 lines, the public graph header is 52 lines, and the
owner-private import header is 93 lines. The architecture exception registry
remains empty.

## Local validation

Environment:

- macOS Darwin 25.3.0, arm64;
- Apple Clang 17.0.0;
- CMake 4.4.2.

Commands and results:

| Command | Exit | Result |
|---|---:|---|
| `./tools/check-architecture` | 0 | owner/header/dependency/registration policy clean |
| `./tools/check-c-memory-policy` | 0 | allocator boundary clean |
| `./tools/check-generated` | 0 | bytecode, diagnostic, Unicode, and syntax artifacts current |
| `./tools/check-doc-links` | 0 | documentation links valid |
| `./tools/check-spec-terms` | 0 | normative terminology valid |
| `./tools/check-example-syntax` | 0 | all public examples accepted |
| `./tools/check-syntax-coverage` | 0 | 74 syntax kinds and 115 grammar productions covered, 0 open |
| `git diff --check` | 0 | patch whitespace clean |
| `cmake --preset debug` + build + full CTest | 0 | 1/1 passed in 40.83 s on the final checkpoint diff |
| `cmake --preset release` + build + full CTest | 0 | 1/1 passed in 41.94 s on the final checkpoint diff |
| `cmake --preset asan-ubsan` + build + strict full CTest | 0 | 1/1 passed in 63.93 s on the final checkpoint diff without sanitizer report; Apple leak detection disabled |
| `clang --analyze` over new graph/import/result owners and focused tests | 0 | no analyzer finding, including the canonical MIR comparison and its failure sweep |

## Remaining boundary

I2 remains `in-progress`. Compiler-service occurrence facts, import-aware
invalidation, definition-from-reference, search, and checker-backed completion
belong to preview checkpoint 3. Generic imports and payloads belong to
checkpoint 4. Filesystem/module discovery, manifests, lock resolution,
registries, network access, public `ruda check`, and complete Tier 1 CI evidence
remain later gates.
