# Checkpoint 29: public release contract and closure matrix

Date: 2026-08-23

Evidence scope: `local` on macOS arm64. Source base:
`45ead300367488c0e85b95eb47777ab8051b3349` (`main`). The verified subject is
that exact clean base plus the CP29 worktree change described here. No commit,
tag, signature, upload, namespace reservation, push, or publication was
authorized or performed.

Host: Darwin arm64, AppleClang 17.0.0, CMake 4.4.2.

## Accepted contract and closure ownership

[D064](../../design/decisions.md#d064-pre-beta-releases-carry-an-explicit-bounded-claim)
accepts the `0.1.0-alpha.N` line, explicit release channels and claims,
pre-beta source/API removal policy, versioned diagnostic metadata stability,
no public bytecode compatibility while `.myb` is unavailable, bounded standard
library versioning, and publication authorization separation. Normative text
is in [toolchain §10](../../spec/toolchain.md#10-toolchain-versioning-and-compatibility).

The [CLI0 closure matrix](../cli0-closure-matrix.md) contains 42 machine-
validated rows. Every row names one open N0, A0, or applicable I0–I5 criterion,
its current evidence scope, one owner, acceptance command/action, platform
scope, and permitted eventual claim. Product gates retain their previous
states. Portable public bytecode, production compiler-service tooling, I6,
registry, FFI, cross compilation, desktop support, and self-hosting remain
outside CLI0.

D054/CP18 predates this compatibility contract. `Array<T>` has no alpha alias
or migration window; `[]T` is the only managed-array spelling in this line.
`mmay explain` remains staged under I8 and is absent from everyday inspection
help.

## Binary and installed metadata

The compiler, runtime, and bounded standard-library slice report
`0.1.0-alpha.1`. The source language remains honestly classified
`0.1-design`, selected by manifest `language "0.1"`. JSON schema
`marimay.version/2` reports channel `private-preview`, claim
`local-checker-backed-slice`, `publicationReady: false`, bytecode
`unavailable`/`none`, and standard library
`0.1.0-alpha.1`/`bounded-preview`/`alpha-source`.

Available commands are exactly `build`, `check`, `fmt`, `help`, `run`, `test`,
and `version`. Seventeen recognized commands are staged. Every staged help
surface names its owner, and ordinary staged invocations, including
`mmay explain T0017`, return `U0003` rather than an argument-shape diagnostic.

`bootstrap/src/cli/cli_release.c` is the focused release-metadata owner. The
stable CLI coordinator fell from 850 to 720 physical lines; the new owner is
273 lines behind a 26-line private header. CMake generates and installs exact
release JSON plus offline README/docs/examples and the source/table link
targets required by those documents. The relocation test moves the complete
prefix into a path containing spaces and compares installed metadata byte for
byte with `mmay version --json` while all proxy variables point at a rejecting
local endpoint.

Artifact SHA-256 values from focused acceptance:

| Artifact | SHA-256 |
|---|---|
| `build/debug/mmay` | `391b988ec5b6128425208636c560a528fc8f7f51a410917f13dc3663ce4632cc` |
| generated release metadata | `d298deec845a9f56f2cd0dbdec2685a46d2901662e0d338e7b9f00d06bf6b3e7` |
| CP29 native quick start | `e4aaefbb15d4b09fbbeb21bdd1e33a2868f8e83c2be295d95bd3f3e537569a52` |

## Coverage

- Human help and version plus JSON version have exact snapshots. Write
  failures are swept through every reachable writer-call index.
- Every available and staged command help surface is exercised. Every staged
  ordinary invocation is required to return `U0003`.
- `./tools/check-release-contract` validates the command inventories, release
  docs, generated/install metadata templates, README extraction, all 42 matrix
  rows, requirement IDs, owners, scopes, and claims. Mutation self-tests prove
  duplicate rows, scope drift, unknown owners/requirements, missing acceptance,
  and gate coverage are rejected.
- `tests/public-alpha/cp29-quick-start` passes formatter check, authoritative
  check, VM run, `test --engine both`, native build, and direct artifact run.
  Both execution paths print `Marimay CP29 quick start`; both tests pass.
- CTest installs and relocates the tool, validates offline docs and metadata,
  runs help/version without a usable proxy, and detects metadata/output drift.
- No parser, checker, MIR, bytecode, VM, native semantic owner, runtime API, or
  allocation-capable C API changed. Parser/formatter/bytecode fuzz campaigns
  are therefore not applicable to CP29; their still-open product gates remain
  explicit in the matrix.

## Validation

| Command or gate | Result |
|---|---|
| exact CP29 version/help/fmt/check/run/test-both/build/artifact sequence | passed; VM/native output exact, 2 tests passed |
| `./tools/check-release-contract` | passed; 42 rows, 24 commands, mutation self-tests |
| `./tools/check-roadmap` | passed after CP29/CP30 state transition |
| architecture, C-memory, generated, docs, spec, example, syntax gates | passed; 75 syntax kinds and 116 grammar productions covered |
| debug build and `ctest --preset debug` | 5/5 passed; 336.55 s |
| release build and `ctest --preset release` | 5/5 passed; 320.35 s |
| ASan+UBSan build and `ctest --preset asan-ubsan` | 5/5 passed; 478.22 s |
| `git diff --check` | passed |

Apple's ASan configuration does not provide the Linux LeakSanitizer contract.
This local evidence therefore does not close I0, the complete Tier 1 matrix,
or CP40. No allocation-capable API changed, and the existing deterministic
allocation and teardown assertions remained green under all three profiles.

## Remaining limitations and handoff

N0 and every product gate remain open exactly as recorded in the closure
matrix. CP29 verification makes no public release claim. CP30 is the sole ready
checkpoint and receives only D064/toolchain §10, the closure matrix and this
report, `cli_release.c` plus focused tests, roadmap/register, and the CP30 card.
