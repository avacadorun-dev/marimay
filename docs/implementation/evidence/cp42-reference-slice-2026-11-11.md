# CP42 reference slice: pinned application, derived profile, CI job

Scope: local working-tree evidence plus one pushed-revision CI run, not a checkpoint or
release verification.
Compiler base: `53b216734c8f03118cadeea348b5c12c0048fc72`, release build at
`build/release/mmay` with SHA-256
`4bf288523efa4180d73042964138748bf83b78642afdd89d90a0792bd9088cf8`. Host:
macOS 27.0.1 arm64, AppleClang 21. Date: 2026-11-11.

This delivers issue #27: one real program, built from a fresh checkout of its
own repository by the current compiler, named in the contract, with the
construct profile derived rather than asserted, and one green CI job that
repeats the claim at the compiler's own revision.

## Pinned application

The reference slice is `marimay-notes`, pinned in
`docs/implementation/reference-slice.json`:

| Field | Value |
|---|---|
| Repository | `https://github.com/marimay-lang/marimay-notes` |
| Revision | `c5ec692f3829b4c80facc8864202ae136be3173f` |
| Tree | `8a46ffe1a4a49c6eb41cc51d6fafd6c96d369c15` |
| Content digest | `sha256:7bb57548a2f40451e601878102526af074b234266fe6828957d4695b39e2634d` |
| Module | `local/marimay-notes` |
| Size | 7 source files, 958 lines, 16 tracked files |

The revision was fetched from the public URL, not from the sibling working copy
in `examples/`, so the verification exercised the same path a stranger uses.

The pin has three views on purpose. The revision selects a commit, the tree id is
git's own content address for that commit's files, and the content digest is a
repository-independent SHA-256 over every tracked file, which survives a mirror,
a tarball or a relocated checkout. `verify` refuses a dirty worktree, so the pin
cannot describe uncommitted edits.

## Construct profile

`tools/check-reference-slice derive` applies a declared list of lexical markers
to the application's `.my` sources. The rules are code, the report is data, and
the document block is generated from the report, so none of the three can drift
from the other two without a gate failing.

Present families and occurrence counts at this pin: module imports 25, record
declarations 3, typed `Option`/`Result` 13, postfix `?` 7, contextual
`mapError` 4, `match` 58, string interpolation 41, managed arrays 12,
insertion-ordered sets 2, iterating `for` 8, declared tests 16, explicit panics
16, capability-gated `std/fs` calls 5, byte/UTF-8 conversions 9, public function
surface 12. Packages imported: `std/bytes`, `std/cli`, `std/convert`, `std/fs`,
`std/io`, `std/json`, `std/path`, `std/text`.

Absent families, each asserted to stay at zero by the offline gate: enum
declarations, interface declarations, generic declarations, insertion-ordered
maps, `while`, `defer`, `unsafe`, `extern`, mutable bindings, `as` casts. The
absence table is part of the claim: a green run says nothing about those
families, and the gate fails if one of them silently appears.

## Documented commands, measured

`tools/check-reference-slice run` verifies the pin and then executes the
application's documented commands unchanged, with the compiler directory first on
`PATH` and `MMAY` set. Fresh checkout, release compiler, this host:

| Step | Wall time |
|---|---:|
| `mmay fmt . --check` | ≤ 0.01 s |
| `mmay check .` | 0.08 s |
| `mmay test . --engine both` | 2.2 s |
| `mmay run . -- --help` | 0.03 s |
| `mmay run . ... add` / `list` | 0.06 s each |
| `mmay build . --release` | 4.5 s |
| `sh tools/smoke-ru "$(command -v mmay)"` | 11.6 s |
| `MMAY=... python3 tools/acceptance.py` | 17.7 s |
| Full sequence | 33 s |

Both engines ran: the smoke test compared persisted JSON between VM and native,
and the acceptance harness reported 36 cases across VM/native Debug/Release plus
21 closed-pipe checks. No fixture, environment variable or path outside the
checkout was needed.

## Pushed-revision run

The `reference-slice` job ran on `80d71e6` and passed end to end in 84 s
([run](https://github.com/marimay-lang/marimay/actions/runs/42877531917/job/130047390041)). The compiler was built from that same commit inside the job
(Linux binary `sha256:5bb45171a2f23308cbab9e6f123a66fb3823582ceea6e0107da04fe590e4eae1`),
the application was fetched by the pin rather than by branch, and the uploaded
`reference-slice-evidence` artifact records 36 VM/native Debug/Release process
cases plus 21 closed-pipe checks in 20.8 s on the Linux runner. The offline
`./tools/check-reference-slice` gate passed in the same run's documentation job.

## Defect found while pinning

The application's `tools/smoke-ru` defaulted the compiler to
`../../marimay/build/debug/mmay`, a path that only exists in one local workspace
layout. Passing the compiler explicitly hid the default, so the job would have
passed while the script still assumed a personal directory. Fixed upstream in
`c5ec692`: the script takes the compiler as its first argument or from `MMAY`
and exits with a usage message otherwise. The pin was taken after that fix,
because #27 requires that nothing in the job depend on a personal path. The
application README now states the explicit-compiler contract.

## CI job

`reference-slice` in `.github/workflows/ci.yml` runs on `ubuntu-24.04` after the
documentation job, with no secret:

1. check out the language repository and build a `Release` compiler with
   `BUILD_TESTING=OFF` — the job's own revision is therefore the compiler under
   test;
2. `check-reference-slice fetch --destination "$RUNNER_TEMP/reference-slice"`
   fetches the revision recorded in the report, so the workflow contains no
   second copy of the pin;
3. `check-reference-slice run` proves revision, tree, digest, clean worktree and
   the derived profile, then executes the commands;
4. the process acceptance JSON is uploaded as `reference-slice-evidence`, also on
   failure.

A pin that no longer builds, a profile that changed without the report changing,
or a compiled application that regresses all fail the job. The offline half —
pin format, report/contract consistency, and the requirement that
`known-limitations.md` and the current release contract name the slice — runs in
the documentation job through `./tools/check-reference-slice`.

## Limits

- One program, one CI platform (Linux x86_64). Platform claims stay with the
  checkpoint matrix and the conformance suite.
- The profile is lexical. It detects that a construct appears, not that it is
  used correctly; correctness comes from the application's own tests, which is
  why the documented commands run the both-engine and process suites.
- The slice names what builds today. It is not a compatibility promise, and it
  does not promote any gate; the CLI0 row still depends on the CP42 perf timebox
  and the release.
