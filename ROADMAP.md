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

Do not replace evidence-based states with unchecked task-list boxes or treat a
locally implemented slice as publicly shippable.
