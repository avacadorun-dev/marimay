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
[post-CLI0 I6/E0](docs/implementation/roadmap.md#post-cli0-i6-and-e0), and the
sole ready card,
[CP31](docs/implementation/checkpoints.md#cp31-program-arguments-and-controlled-exit).

Do not replace evidence-based states with unchecked task-list boxes or treat a
locally implemented slice as publicly shippable.
