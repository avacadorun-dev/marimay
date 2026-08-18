# Authoritative batch `ruda check` — preview checkpoint 8 — 2026-08-18

Checkpoint status: locally complete. I2–I5 remain `in-progress`; I6, I7, and
I8 retain their roadmap states. This is authoritative batch checking for the
checker-backed preview surface, not package-manager, workspace-resolution,
runtime, LSP-transport, or release evidence.

Authority: accepted D048; toolchain §§1, 1.1, 2, and 6.1; manifest §1;
diagnostics §§1–4; the compiler architecture contract; and checkpoint 8 in
`checkpoints.md`.

## Delivered surface

- `ruda check`, `ruda check file.ruda`, `ruda check <package>`, and
  `ruda check <module-root>` dispatch through one focused CLI owner into the
  authoritative compiler service. Successful checks are silent.
- No-target discovery finds the nearest ancestor `ruda.mod` and checks the
  whole module even from a descendant package. Explicit `.ruda` files use a
  standalone package; explicit descendant directories select one package.
- D048 defines the minimum pre-I7 module identity. Batch graph construction
  requires exactly one nonempty `module` and `language` value, maps source
  directories to canonical local import paths, and reports `K0005` for a
  syntactically valid but unusable identity.
- Whole-module checking visits every discovered package, including a package
  disconnected from the root, in canonical import-path order. File paths are
  sorted before loading. Default discovery excludes `_test.ruda` and the root
  `tests/` tree; `--all-targets` includes both.
- Traversal is non-following and bounded. It ignores `.git` and `.ruda`, does
  not cross nested modules, and has explicit path, name, depth, directory,
  entry, source-file, and source-byte limits.
- Each invocation owns a fresh compiler service populated only from immutable
  disk snapshots. Editor overlays, incremental reuse, earlier results, and
  persistent semantic caches cannot change batch semantics.
- Lexer, parser, resolver, type, and checker diagnostics retain their stable
  codes and exact snapshots. The central renderer writes human/short or
  versioned `ruda.diagnostic/1` NDJSON only to stderr. Source failure exits 1,
  invalid invocation exits 2, incomplete compiler work exits 70, cancellation
  maps to 130, and stdout remains unpolluted.
- `RUDA_COMPILER_INCOMPLETE_LANGUAGE` is never interpreted as a clean result.
  It emits `I0001` and exits 70. A focused fixture locks this behavior for an
  accepted string-constant shape that is intentionally beyond the current
  checker slice.
- The last direct checkpoint-1–7 blocker found by the service audit,
  `for const item of array`, now lowers through typed MIR. Its focused test
  checks `continue`, `break`, array reads, MIR verification, MIR execution,
  encoded VM execution, and VM/native differential execution. Broader D045
  iterator mutation-generation invalidation remains a later gate.

## Requirements-to-tests coverage

| Checkpoint-8 claim | Executable evidence |
|---|---|
| root/module/package/file discovery | focused CLI checks cover nearest-root behavior from a nested directory, module root, descendant package, and standalone file |
| complete deterministic module graph | a clean multi-file import module succeeds; `--all-targets` reaches disconnected test packages and reports their semantic failure |
| human stderr and NDJSON | semantic human assertions plus exact `check-missing.jsonl` and help snapshots; JSON schema/source-span fields and one-object-per-line behavior are checked |
| stable exits and no stdout | success, source error, invalid manifest/identity, missing target, defensive limit, writer failure, and compiler-incomplete fixtures assert exact exits and empty stdout |
| batch/service separation | the CLI constructs and tears down a fresh service for every run; two identical clean checks remain silent and source bytes remain unchanged |
| manifest and I/O failures | malformed JSON string uses `K0002`, missing identity uses `K0005`, and missing/unreadable target uses `K0004` |
| defensive failures | an over-limit target path uses `K0004`; compiler-service limit mappings remain source failures rather than success |
| allocation ownership | fail-every-allocation runs through target discovery, manifest decoding, document loading, graph construction, checking, and teardown to the first success; every index asserts zero live allocations and bytes |
| accepted syntax cannot pass unchecked | `check-incomplete.ruda` exits 70 with `I0001`; the array iteration acceptance path now returns a verified result and executes through MIR/VM/native |
| no-change behavior | the clean project is checked twice, produces no output, and its source bytes are compared before/after |

The CLI allocation sweep exposed two ownership/progress defects that are now
covered: a trimmed owned path previously retained the allocation's old size at
deallocation, and parser type recovery could recursively re-enter without
token progress after marker-allocation failure. The path owner now allocates
the exact parent bytes, and the parser returns immediately when marker
allocation or a parser limit invalidates the marker.

## Scope boundary

D048 names only local packages. Dependency versions, locks, capabilities,
metadata policy, `ruda.work`, external module resolution, and supply-chain
behavior remain I7. This checkpoint does not implement `ruda run`, `ruda
build`, `ruda test`, runtime stdout, or production LSP transport. Unsupported
accepted forms outside the checker-backed preview surface fail closed with
`I0001`; this evidence does not claim the full I2 language gate is verified.

## Local validation

Environment: macOS Darwin 25.3.0, arm64; Apple Clang 17.0.0; CMake 4.4.2.

| Command | Exit | Result |
|---|---:|---|
| `./tools/check-architecture` | 0 | bounded ownership and line budgets |
| `./tools/check-c-memory-policy` | 0 | central allocator policy |
| `./tools/check-generated` | 0 | generated syntax/opcode/diagnostic artifacts |
| `./tools/check-doc-links` | 0 | documentation links |
| `./tools/check-spec-terms` | 0 | normative terminology and D048 registry |
| `./tools/check-example-syntax` | 0 | public example syntax |
| `./tools/check-syntax-coverage` | 0 | 75/75 syntax kinds and 115/115 grammar productions covered |
| `git diff --check` | 0 | whitespace validation |
| `ctest --preset debug --output-on-failure` | 0 | full CTest, 158.75 seconds |
| `ctest --preset release --output-on-failure` | 0 | full CTest, 173.55 seconds |
| `ctest --preset asan-ubsan --output-on-failure` | 0 | sanitizer CTest, 239.28 seconds |
