# Catalogue runs retain their own toolchain and executables

> **Private experiment payloads.** Archived implementations and patches
> referenced in this historical report are retained privately. Commands
> and digests below document the original experiment; they are not public
> reproduction instructions. See the [artifact index](private-artifacts.md).

Local correctness evidence, 2026-09-09; external benchmark repository
`algorithm-benchmarks`, parent `3eda605` with the user's existing catalogue work.
[Snapshot metadata and report hashes](cp42-catalog-snapshot-2026-09-09.json).
[Exact external harness patch](private-artifacts.md#artifact-002).

The user's comprehensive run was interrupted again when this task rebuilt the
system compiler during measurement. Input-integrity checks correctly preserved
the partial observations as diagnostic only. Merely retaining that check did
not prevent the disruption; the run now owns its compiler/runtime/header and
executable copies before measuring.

Catalogue compiler selection follows `MMAY`, then the system `PATH`, then
the sibling build fallback. Each catalogue invocation uses a unique `.build/catalog/runs/run-*` directory.
An installed toolchain layout contains copied compiler bytes, the selected
release runtime archive/manifest and matching C headers. Embedded standard
packages remain in the compiler. Shared Rust outputs are also copied before
measurement; native and Go outputs are built inside the unique workspace.
Copies use independent files, not hard links. The original input hashes are
checked before/after copying, so a change during capture stops preparation.
Subsequent checks watch the copies and the original workload/harness sources.
Updating the original compiler after capture no longer invalidates the series.
Source changes and tampering with a run's copies still invalidate it.

Validation:

- `python3 -m unittest discover -s benchmarks -p 'test_*.py'`: 95 passed.
  New tests cover build-tree/installed layouts, executable permissions,
  MMAY/PATH/build-fallback selection, independent copies, pre-capture and mid-copy mutation, missing headers,
  per-run directories and deduplication of shared external executables.
- `MMAY=/opt/homebrew/bin/mmay python3 benchmarks/catalog_bench.py --profile smoke
  --engine both --verify-only --output benchmarks/results/catalog-snapshot-smoke-2026-09-09.md`:
  21 workloads × 4 participants × 3 checksum checks passed.
- A second real run used a disposable installed copy as its original toolchain.
  After its first complete round, the original compiler, runtime archive and
  one header were overwritten. Its separately pinned copies completed all
  252 checks with valid hashes. The actual system installation was untouched.
  Report: `catalog-snapshot-origin-replaced-2026-09-09.json`.

These are smoke correctness checks, not performance measurements. Metadata
retains the initial compiler revision/version and source/copy paths. Copies and
journals are retained for diagnosis and can consume disk space. The old standalone
`benchmarks/run.py` entry has not acquired this isolation; the change covers
`catalog_bench.py`, including `make benchmark-comprehensive` and its anchors.

The external repository already contains substantial uncommitted catalogue
work. Its working files are fixed in place; the patch records only this task's
isolation changes without folding that pre-existing work into a compiler commit.
No workload source, size, checksum, language semantics or release gate changes.
No new idea from Vladimirov's book was used for the isolation mechanism.
