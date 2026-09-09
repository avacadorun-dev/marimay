# Roadmap

Start with the [current product dashboard](docs/implementation/roadmap.md#current)
and [Now / Next / Later](docs/implementation/roadmap.md#now-next-later).

The [product roadmap](docs/implementation/roadmap.md) is the only authority for
gate states and release sequencing. The
[checkpoint ledger](docs/implementation/checkpoints.md) is the only execution
queue, and the [evidence index](docs/implementation/evidence/README.md) keeps
direct reports out of status cells. Their IDs and states are checked against
[`roadmap.tsv`](docs/implementation/roadmap.tsv).

Current execution links: [CLI0](docs/implementation/roadmap.md#cli0-boundary),
[CP41/I6/E0 sequencing](docs/implementation/roadmap.md#cli0-cp41-boundary-and-post-cli0-i6e0), and the
active ergonomics card,
[CP42](docs/implementation/checkpoints.md#cp42-notes-driven-cli-ergonomics).

The user accepted the [five-cycle CLI0 execution route](docs/implementation/checkpoints.md#accepted-cli0-execution-route)
on 2026-09-07. It groups shared API and application work while preserving
checkpoint verification order and all release obligations.
Its required pre-alpha acceptance includes the
[ten-application external CLI portfolio](docs/implementation/cli-ergonomics-suite.md).

For continuation context, see the dated
[CLI-alpha agent handoff](docs/implementation/agent-handoff-cli-alpha-2026-09-07.md).
It is a snapshot and reading guide, not a replacement for the authorities above.

Latest local runtime evidence: [managed VM string lifetimes and the catalogue
regression matrix](docs/implementation/evidence/cp42-vm-string-lifetime-2026-09-09.md).
This fixes the report-concat defect; release-gate states remain unchanged.

The [full catalogue performance investigation](docs/implementation/evidence/cp42-catalog-performance-2026-09-09.md)
records all 32 standard workloads, the native object-queue regression and open
shared text/collection/VM execution costs. These findings remain part of the
pre-alpha performance work; no existing acceptance budget is waived.

[VM execution regions and frame reuse](docs/implementation/evidence/cp42-vm-regions-2026-09-09.md)
remove repeated view validation and activation allocation from supported paths.
All 32 full-size VM diagnostics and 86 Release tests pass locally. Instruction
counts expose the next shared code-preparation boundary; performance parity,
startup budgets and Tier 1 acceptance remain open.

[Scalar/control preparation](docs/implementation/evidence/cp42-scalar-flow-2026-09-09.md)
records D106, all 32 unchanged workloads on the system build, the native bulk-fill
composition regression and its repair, and allocation/rollback checks. Native
text/collection costs and the large VM gap remain open; parity is not achieved.

[Scalar array VM regions](docs/implementation/evidence/cp42-vm-scalar-arrays-2026-09-09.md)
cover every scalar width and reduce paired Dijkstra from 6.63 to 2.41 seconds.
All 32 VM workloads and 86 Release tests pass locally. System `mmay` uses an
independent installed copy; catalogue runs pin their tools. Record/reference
access, call preparation and native text/collection costs remain open.

Do not replace evidence-based states with unchecked task-list boxes or treat a
locally implemented slice as publicly shippable.
