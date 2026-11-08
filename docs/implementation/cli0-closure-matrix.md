# CLI0 closure matrix

Status: authoritative CP29 assignment of open N0, A0, and applicable I0–I5
criteria. Initial assignment: 2026-08-23; implementation/evidence reconciliation:
2026-09-07. Decision: [D064](../design/decisions.md#d064-pre-beta-releases-carry-an-explicit-bounded-claim).

The row-level matrix is [`cli0-closure-matrix.tsv`](cli0-closure-matrix.tsv).
It is machine checked by `./tools/check-release-contract`. Each row names the
current evidence scope, one still-missing criterion, one owning checkpoint or
gate, an acceptance command or manual evidence action, platform scope, and the
largest claim that may eventually use the result.

This matrix assigns work; it does not close a product gate. Until the named
owner and CP40 audit pass, the current permitted implementation claim is still
only the checker-backed local slice. D114 separately authorizes publishing
that slice as an experimental macOS prerelease; it does not complete this
matrix or the `cli-alpha` gate. `excluded-from-cli0` rows remain product-gate
work and cannot be pulled into the bounded alpha by documentation wording.

The [baseline audit](evidence/cli0-baseline-audit-2026-09-07.md) reconciles all
42 assignments. The 26 CP30–CP38 rows and two CP39 install rows now distinguish
existing implementation from incomplete exact-candidate/platform acceptance.
No owner, requirement, platform scope, release claim or checkpoint state is
removed or promoted by this reconciliation. Evidence-scope fields retain their
previous bounded scope; they do not certify a fresh all-host result.

## Ownership summary

| Gate | Current scope | Missing families | Owners |
|---|---|---|---|
| N0 | `local` | legal/confusable review, controlled identities, PATH lifecycle | N0 |
| A0 | `tier-1-partial` | complete Tier 1 architecture evidence | A0 |
| I0 | `pushed-ci` | bootstrap/compiler, memory tools, install matrix | I0, CP39 |
| I1 | `pushed-ci` | remaining production fixtures and sustained fuzz | I1 |
| I2 | `pushed-ci` | full baseline closure plus CLI0 formatting, entry, collections, JSON | I2, I8, CP30, CP31, CP36, CP38 |
| I3 | `pushed-ci` | public bundle gate plus CLI0 bytecode/VM verticals | I3, CP30–CP33, CP36, CP38, CP40 |
| I4 | `pushed-ci` | CLI0 native verticals, install lifecycle, full Tier 1 differential | CP30–CP36, CP38–CP40 |
| I5 | `pushed-ci` | strings, streams, text/bytes, files/env, collections, CLI, JSON, workload gates | CP30, CP32–CP38, CP40 |

## Boundary decisions

- Portable public `.myb`, production LSP/tooling, networking/tasks, registry,
  FFI, cross compilation, desktop support, and self-hosting stay outside CLI0.
- CP18/D054 left no `Array<T>` compatibility window. The alpha surface starts
  with `[]T`; the old spelling is neither an alias nor a staged migration.
- CP29 does not mark N0 or I0–I5 verified. N0 and claimed-host evidence still
  block the completed CLI0/`cli-alpha` claim even after bounded implementation
  cards land. D114's explicitly experimental publication is a separate scope;
  no owner, platform obligation or missing criterion is waived by it.
- Acceptance commands in rows are the focused minimum. Each owner also runs
  the common validation tail and its card-specific sanitizer/fuzz/platform
  gates.
