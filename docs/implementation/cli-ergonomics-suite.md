# External CLI ergonomics acceptance suite

Status: required CP42 acceptance scope, added at the user's request on
2026-09-06; dependency waves accepted on 2026-09-07. This is a workload/acceptance contract, not a second execution queue
or evidence that the applications work. The [checkpoint ledger](checkpoints.md#cp42-notes-driven-cli-ergonomics)
retains sequencing and the [product roadmap](roadmap.md#cli0-boundary) retains
release authority. All ten workloads must be evidenced before CP42/CLI0 can
close; a successful notes application alone is insufficient.

## Workspace and repository boundaries

The local portfolio lives at
`/workspace/Projects/Marimay/Marimay-Examples/`, outside the language checkout.
It is a navigation directory, not a monorepo or another compiler worktree.
Each utility has its own Git repository, `mmay.mod`, Russian README, source,
tests, synthetic fixtures and process acceptance script. Create repositories
as implementation starts, not as empty completed-looking placeholders.
No remotes, publication or copies of the compiler are implicit in this plan.

Reuse the existing `marimay-notes` repository and its history; do not duplicate
it or copy its private database into fixtures. Until its location is deliberately
reconciled with the portfolio, the index links to the existing sibling path.
The final portfolio must have one authoritative checkout of each utility,
not parallel old/new copies. Existing in-tree public-alpha regression programs
remain language tests and possible starting material, not proof that these
independent user-facing applications are finished.

## Required applications and coverage

| ID | Repository | Useful application contract | Main ergonomics pressure |
|---|---|---|---|
| 1 | `marimay-config` | Validate a versioned JSON configuration and migrate one documented schema to another; validate-only is read-only, migration publishes only complete valid output. | Nominal/generic records, typed JSON, Result/Option, field/index/span errors, defaults explicitly chosen by the application, limits and atomic replacement. |
| 2 | `marimay-manifest` | Generate and verify a deterministic JSON file inventory from a directory tree: relative paths, metadata, sorted entries and useful mismatch diagnostics. | Directory walking, arrays, sorting, paths, missing files, permission errors and explicit symlink policy. Cryptographic integrity is not implied by a metadata manifest. |
| 3 | `marimay-logstats` | Read a documented line-based log format from stdin/files, count levels and events, and produce a stable JSON report. | Chunked input, partial/final lines, text parsing, records, Map counters, bounded memory and malformed-line policy. |
| 4 | `marimay-grep` | Literal substring search across stdin/files with line numbers, useful match/no-match/error statuses and Unicode fixtures. | Text/byte boundaries, arrays, search, empty patterns, long lines and streaming. No implicit regex engine. |
| 5 | `marimay-frequency` | Stable `uniq` and frequency modes, preserving first-seen order or applying a documented deterministic count ordering. | Set membership, Map updates, iteration, ties, copying, insertion/removal and empty/duplicate-heavy input. |
| 6 | `marimay-delimited` | Convert documented TSV and CSV dialects to/from a typed tabular JSON representation. Support CSV quotes, escaped quotes, delimiters/newlines inside quoted fields, or reject unsupported forms explicitly. | Stateful parsing, nested arrays, enums, Option/Result, builders, UTF-8, round trips and bounded fields/rows. Do not label naive comma splitting as general CSV support. |
| 7 | `marimay-project-check` | Check required files, extensions, size limits and a typed configuration; report all applicable findings in deterministic text/JSON. | Composition across packages, arrays of diagnostics, collections, traversal, validation and useful aggregated errors. |
| 8 | `marimay-notes` | Retain add/list/show/search/remove and strict JSON persistence; reconcile mutation/copy semantics and Russian UX using the existing repository. | Stable removal/search/copy, aliasing, records, tags, Set/Map where useful, ID boundaries and failure-atomic storage. |
| 9 | `marimay-env-paths` | Inspect only explicitly named allowed environment variables and normalize/explain selected paths and process context. | Option values, nominal paths, explicit capabilities, absent versus denied access and redacted error output. Never dump the ambient environment or secrets into fixtures/reports. |
| 10 | `marimay-source-lint` | Apply a documented set of lightweight lexical/text source checks, with file/line/column diagnostics and stable JSON output. | Scanner state, strings/bytes, arrays/records/enums, maps/sets, diagnostic spans and false-positive fixtures. No claim of full AST or semantic analysis. |

These workloads target the bounded local CLI surface. They do not replace
language conformance or prove ergonomics for unimplemented concurrency,
networking, FFI, package-registry or full LSP features. E0 external-developer
feedback remains a separate gate; applications written by the implementer are
not a substitute for it.

## Per-repository acceptance

1. A reader can follow the Russian README using ordinary installed `mmay`:
   initialize/inspect the project, format, check, test, run and build it without
   private compiler APIs, host-language business logic or deleted worktree paths.
2. Source tests pass with `mmay test . --engine both`; process tests compare
   status, stdout, stderr and output files for VM and native debug/release.
   The test harness may drive processes and compare fixtures, but the utility's
   actual parsing, transformation and persistence are written in Marimay.
3. Fixture coverage includes empty/minimal/typical input, Cyrillic and other
   Unicode, large input, malformed input, configured resource boundaries and
   missing/denied I/O. Streaming cases include chunk boundaries, EOF without a
   final newline and broken-pipe behavior. Writes retain the original on failure.
4. Collection workloads exercise element/reference identity, empty and boundary
   indices, copy versus alias, stable removal, duplicate insertion, update/delete/
   reinsert, deterministic iteration and iterator invalidation. Record the exact
   test mapping; using a Map once does not establish this coverage.
5. Record cold/warm `run`, already-built native execution, test-loop duration,
   large-input throughput and bounded-memory behavior against named datasets,
   compiler/application revisions and host/build profiles. Establish practical
   latency and memory budgets before optimizing each workload; explain material
   regressions rather than weakening verification or shrinking the workload.
   A performance failure blocks acceptance of affected workloads and release:
   reproduce it, profile the responsible layer, fix the cause, rerun correctness
   tests and measure the unchanged workload again. Independent preparation may
   proceed under the accepted route. A shared native correctness failure blocks
   the affected native candidate across applications. A findings entry alone
   does not close either defect. Separate measured host startup costs from compiler/runtime work without
   hiding either; do not disable host security or run user code during a build
   to manufacture a warm-start result.
6. Keep an ergonomics findings log with the smallest reproducer, expected user
   expression, actual workaround/boilerplate, severity, owner and resolution.
   Distinguish language/library defects, missing abstractions, toolchain/install
   issues and application design. Recurring manual loops, needless discards,
   duplicated codecs and inconsistent mutation APIs require explicit review.
7. Rework affected utilities after each accepted language/library improvement,
   retaining all behavioral tests. Evidence must show the resulting application
   and resolved finding, not just a compiler feature or reduced source line count.

## Execution and completion

The [accepted route](checkpoints.md#accepted-cli0-execution-route) replaces the
former numeric implementation order with dependency waves. Table IDs identify
the ten required applications; they do not impose a sequence.

| Wave | Applications | Shared work and distinct responsibilities |
|---|---|---|
| Data and persistence | config, existing notes | Start shared collection/mutation/Option contracts with these real consumers; typed JSON, limits and failure-atomic storage fixtures. Keep each domain model separate. |
| Streaming and counters | logstats, grep, frequency | Chunked UTF-8/line boundaries, EOF, broken pipe, collections and stable output. Each utility keeps its own format and exit contract. |
| Files and context | manifest, project-check, env-paths | Traversal, path/metadata/capability fixtures; project-check uses the established config and manifest experience. |
| Stateful parsing | delimited, source-lint | Reuse input/builders/span/report mechanics; quoted CSV newlines and lexical checks remain separate parsers. |

Use one versioned acceptance driver for common process/build/fixture/measurement
mechanics with small per-application adapters. Every repository remains useful
independently of adjacent checkouts and keeps its own README, tests and fixtures.
Shared harness code must not implement the utilities' business logic or create
a new registry/dependency requirement. Run every application's own acceptance;
one shared unit test does not replace ten end-to-end results.

The [JSON metadata follow-up](evidence/cp42-json-intrinsics-2026-09-06.md) is
historical first-application evidence. Current implementation and gaps are
tracked in the checkpoint/evidence ledger; a passing slice is not full acceptance.

The portfolio index records exact repository paths and application/compiler
revisions. Each report links commands, fixtures, measured limits, test results
and findings. CP42 completion requires all ten useful applications, the combined
coverage matrix, resolved release-blocking ergonomics defects, and an explicit
disposition for every remaining material limitation. Incomplete applications
stay incomplete; the scope cannot be reduced to the easiest passing subset.
CP41 performance evidence, CP39 installation, CP40 audit, Tier 1 and N0 remain
independent obligations before public release.
