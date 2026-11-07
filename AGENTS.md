# Marimay repository rules

This repository defines and implements the Marimay programming language.

## Authority order

When documents disagree, use this order:

1. `docs/spec/` — normative language and toolchain contracts;
2. accepted entries in `docs/design/decisions.md`;
3. `IMPLEMENTATION-BRIEF.md` and `docs/implementation/`;
4. examples and README prose;
5. historical research notes.

Do not silently choose between conflicting normative statements. Record the conflict, add or amend a decision, update the specification, then implement it.

## Non-negotiable product constraints

- The command is `mmay`; the source extension is `.my`.
- Project files are `mmay.mod`, `mmay.work`, and `mmay.lock`; bytecode bundles use `.myb`; tool-owned local state uses `.mmay/`.
- The former command `ma`, source extension `.ma`, `ma.*` project files, `.mab` bundles, and `.ma/` tool-state path are not current compatibility aliases.
- Stage 0 is portable C17. C++ is not permitted in the compiler or runtime.
- A later compiler is self-hosted in Marimay, but the C17 bootstrap remains reproducible.
- VM and native execution consume the same typed MIR and have the same observable semantics.
- `mmay run` and `mmay test` use the VM by default; `mmay build` creates a native executable by default.
- `mmay fmt` and `mmay test` are built in. A release cannot delegate their core behavior to third-party tools.
- Marimay has algebraic enums, exhaustive pattern matching, `Option`, `Result`, and postfix `?`.
- Marimay has no `null`, `undefined`, exceptions for expected failures, inheritance, user-defined implicit conversions, or `any` escape hatch.
- Marimay has no `impl` blocks and no special method declaration. Behavior is declared by ordinary top-level functions.
- A visible function owned by a package is dot-call eligible when its first parameter has an exact nominal type owned by that package: `string(user)` and `user.string()` resolve to one symbol.
- Interfaces are structural contracts over the resulting dot-call set; records and enums remain nominal.
- Direct-style task suspension is inferred by the compiler. `scope` owns child-task lifetimes and `spawn` creates concurrency. Public source code has no `async` function color and no selectable third-party runtime.
- Safe Marimay has no undefined behavior. Raw memory and C FFI are confined to explicit `unsafe` regions.

## Change protocol

Every semantic change must include, in the same change:

1. a decision record or link to an existing accepted decision;
2. normative spec text;
3. parser/type-checker/runtime tests as applicable;
4. VM/native differential tests for observable behavior;
5. formatter fixtures for new syntax;
6. updated user-facing examples if the feature is public.

Implementation convenience is not a reason to change source semantics. Temporary shortcuts must be isolated behind an internal interface, documented in the roadmap, and covered by a removal gate.

## Architecture and context discipline

Checkpoint 0 in `docs/implementation/checkpoints.md` is the historical
architecture prerequisite for every numbered preview checkpoint. The A0
product gate remains independently evidence-scoped; feature work does not
resume while the current architecture check is red.

- Every handwritten production or test C translation unit has one named owner
  and one reviewable responsibility. Coordinators orchestrate owners; they do
  not accumulate their implementations.
- Handwritten `.c` files target 500–1,200 physical lines and have a hard
  ceiling of 1,500 physical lines. Owner-private `.h` files target 50–250
  lines and have a hard ceiling of 400; owner API headers have a tighter hard
  ceiling of 160. Generated files are exempt.
  A temporary exception must be registered in
  `docs/implementation/architecture-exceptions.tsv` with an owner, a tighter
  explicit ceiling, a reason, and a removal checkpoint. New feature logic may
  not be added to an excepted file.
- Splits must create independently compiled translation units behind narrow
  owner-private headers. Handwritten `.inc` shards, `#include` of `.c` files,
  umbrella private headers that expose an owner's full mutable state, and
  rename-only or helper-only splits that leave a semantic god file are
  forbidden.
- Stable coordinators have tighter path-specific ceilings enforced by the
  architecture gate. A coordinator's spare line budget is not a place to put
  the next feature implementation.
- Tests mirror production ownership. Do not append a new semantic family to a
  broad aggregator when an adjacent focused test owner can contain it.
- Dependencies follow `docs/implementation/compiler-architecture.md`.
  Lowering or backends must not repair or duplicate source-language semantics.
- Before changing a subsystem, load its architecture contract, the focused
  owner files, and focused tests. Do not require a whole monolith as the
  minimum context packet.
- `./tools/check-architecture` is a required local and CI gate after every
  feature and every checkpoint. A checkpoint cannot be evidenced, committed,
  pushed, or marked complete while it fails.
- `./tools/check-roadmap` is a required local and CI documentation gate after
  every roadmap, checkpoint, evidence-index, or planning-state change.

Line count is only the mechanical backstop. Passing the count while preserving
a mixed-responsibility god object is still an architecture failure.

## Definition of done

A milestone is `verified` only when its documented commands pass on all supported Tier 1 platforms and the evidence is linked from `ROADMAP.md`. “Implemented” without conformance, negative, sanitizer, and differential coverage is not done.

## C memory ownership

Every C change that adds or modifies an allocation-capable API must cover
successful cleanup, all material error paths, and every reachable deterministic
allocation-failure index. After full teardown, allocator-backed tests must
assert both `live_allocations == 0` and `live_bytes == 0`.

Direct production heap calls outside the central allocator implementation are
forbidden and enforced by `tools/check-c-memory-policy`. Linux LeakSanitizer is
a required CI contract. Sanitizer suppressions require a documented reason and
review; neither sanitizer success nor allocator counters justify an unqualified
claim that all leaks are absent.

## Public contribution hygiene

- Use descriptive branch prefixes such as `fix/`, `feature/`, or `chore/`; never `codex/`.
- Follow [CONTRIBUTING.md](CONTRIBUTING.md) and [docs/agent-workflow.md](docs/agent-workflow.md).
- Keep secrets, personal emails/paths, private remotes, environment dumps, and unrelated process data out of commits and evidence.
- Public CI uses GitHub-hosted runners and requires no repository secrets or private infrastructure.
- Run `./tools/check-publication` before publishing changes; it supplements the secret scanner.
