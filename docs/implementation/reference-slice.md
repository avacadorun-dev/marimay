# Reference slice: the program that answers "what does Marimay do today?"

This page names one real application as the reference slice for the current
alpha. The slice exists because the supported language surface used to be
described only by exclusions: `known-limitations.md`, the CLI0 closure matrix
and the release contract each said what Marimay does not do. One program, built
from a fresh checkout of its own repository by the current compiler, answers the
question positively and keeps the answer honest as the language moves.

The slice is not a product, a compatibility promise or a second home for the
application source. The application lives in its own repository
([`marimay-lang/marimay-notes`](https://github.com/marimay-lang/marimay-notes));
this page records the exact revision the compiler is expected to build and
verifies that expectation in CI.

## Pinned state

Everything below the markers is generated from
`docs/implementation/reference-slice.json` by
`tools/check-reference-slice derive --write`. Do not edit the block by hand: the
offline gate `./tools/check-reference-slice` fails when the block and the report
disagree.

Each family row is a lexical marker applied to the application's `.my` sources.
A count is the number of marker matches in that file, not a semantic census: the
markers are deliberately conservative, they are printed next to every count so a
reader can audit them, and a family is listed as absent only while its marker
stays at zero.

<!-- reference-slice:begin -->
```text
repository      https://github.com/marimay-lang/marimay-notes
revision        c5ec692f3829b4c80facc8864202ae136be3173f
tree            8a46ffe1a4a49c6eb41cc51d6fafd6c96d369c15
content digest  sha256:7bb57548a2f40451e601878102526af074b234266fe6828957d4695b39e2634d
module          local/marimay-notes
size            7 source files, 958 lines, 16 tracked files
```

| Family | Marker | Occurrences | Files |
|---|---|---:|---|
| module imports | `^\s*import\s+(?:\*\s+as\s+\w+|\{[^}]*\})\s+from\b` | 25 | `main.my` (8), `main_test.my` (3), `messages/json.my` (1), `messages/system.my` (3), `messages_test.my` (6), `notes/api.my` (4) |
| record declarations | `^\s*(?:public\s+)?type\s+\w+(?:<[^>]*>)?\s*\{` | 3 | `notes/api.my` (3) |
| typed Option/Result values | `\b(?:Option|Result)<` | 13 | `main.my` (6), `main_test.my` (1), `notes/api.my` (6) |
| `?` propagation of fallible values | `\)\s*\?[;,]` | 7 | `main.my` (1), `notes/api.my` (6) |
| contextual error-type mapping through `mapError` | `\.mapError\(` | 4 | `main.my` (1), `notes/api.my` (3) |
| match expressions | `\bmatch\b` | 58 | `main.my` (27), `main_test.my` (13), `messages/json.my` (3), `messages/system.my` (4), `messages_test.my` (6), `notes/api.my` (1), `notes/collection_baseline_test.my` (4) |
| string interpolation | `\$"` | 41 | `main.my` (9), `messages/json.my` (12), `messages/system.my` (18), `notes/api.my` (2) |
| managed arrays | `\[\]\s*\w` | 12 | `main.my` (8), `main_test.my` (1), `notes/api.my` (3) |
| insertion-ordered sets | `\b(?:Set|setOf)<` | 2 | `notes/api.my` (2) |
| iterating for loops | `\bfor\s+\w+\s+in\b` | 8 | `main.my` (4), `main_test.my` (2), `notes/api.my` (1), `notes/collection_baseline_test.my` (1) |
| declared tests | `^\s*@test\b` | 16 | `main_test.my` (7), `messages_test.my` (3), `notes/collection_baseline_test.my` (6) |
| explicit panics | `\bpanic\(` | 16 | `main.my` (1), `main_test.my` (8), `messages_test.my` (3), `notes/api.my` (1), `notes/collection_baseline_test.my` (3) |
| capability-gated filesystem calls | `\bfs\.\w+` | 5 | `main.my` (3), `messages/system.my` (1), `messages_test.my` (1) |
| UTF-8/byte buffer conversions | `\bbytes\.\w+` | 9 | `main_test.my` (5), `messages_test.my` (2), `notes/api.my` (2) |
| public function surface | `^\s*public\s+fn\b` | 12 | `messages/json.my` (1), `messages/system.my` (4), `notes/api.my` (7) |

Standard-library packages imported by the slice: `std/bytes`, `std/cli`, `std/convert`, `std/fs`, `std/io`, `std/json`, `std/path`, `std/text`.

Families the slice does not exercise, asserted to stay at zero:

| Family | Marker |
|---|---|
| enum declarations | `^\s*(?:public\s+)?enum\s+\w+` |
| interface declarations | `^\s*(?:public\s+)?interface\s+\w+` |
| generic declarations | `^\s*(?:public\s+)?(?:fn|type|enum|interface)\s+\w+\s*<` |
| insertion-ordered maps | `\b(?:Map|mapOf)<` |
| while loops | `\bwhile\b` |
| lexical defer | `\bdefer\b` |
| unsafe blocks and foreign functions | `\bunsafe\b` |
| extern declarations | `^\s*extern\b` |
| mutable bindings | `\bmutable\b` |
| total `as` casts | `\bas\s+(?:int|u8|u32|u64|i8|i32|i64|f64|bool|string|byte)\b` |

The commands the slice is built and verified with, in order:

```sh
mmay fmt . --check
mmay check .
mmay test . --engine both
mmay run . -- --help
mkdir -p build
mmay run . --allow-read . --allow-write . -- --db build/reference-slice.json add "Read the docs" --tag study
mmay run . --allow-read . --allow-write . -- --db build/reference-slice.json list
mmay build . --release --allow-read . --allow-write . --output build/marimay-notes
./build/marimay-notes --db build/reference-slice.json list
sh tools/smoke-ru "$(command -v mmay)"
MMAY="$(command -v mmay)" python3 tools/acceptance.py
```
<!-- reference-slice:end -->

The revision, the tree id and the content digest are three views of the same
committed state. The revision selects the commit, the tree id selects its
content in git's own content addressing, and the content digest is a
repository-independent SHA-256 over every tracked file, so a mirror, a tarball
or a relocated checkout can be checked without trusting a remote.

## Reproducing the claim

Two commands, both offline after the fetch. They read the pin from the report
instead of naming a branch or a personal path, and they fail rather than warn.

```sh
./tools/check-reference-slice fetch --destination build/reference-slice
./tools/check-reference-slice run --source build/reference-slice --compiler build/debug/mmay
```

`fetch` initialises an empty directory, fetches the pinned revision with
`--depth 1` from the public repository, and refuses to continue when the result
is not the pinned commit. `run` first proves the checkout is that revision, tree
and digest with a clean worktree, re-derives the construct profile from the
source and compares it to the committed report, and only then executes the
documented commands in order. A regression in any command fails the run; a
change in what the program exercises fails verification before a command runs.

The CI job `reference-slice` in `.github/workflows/ci.yml` does exactly this
against the compiler built from the same revision as the job, so the claim and
the compiler move together. The job needs no repository secret, no personal path
and no private fixture; the only network access is the same public fetch anyone
runs locally.

## Updating the pin

The pin moves deliberately, in a pull request that does all of the following:

1. fetch the candidate revision and make it build with the current compiler;
2. run `derive --write` so the report, the digest and the generated block
   describe the new source;
3. record what changed in the profile in the milestone evidence, especially a
   family moving between the present and absent tables;
4. update the revision named in `docs/known-limitations.md` and in the current
   release contract, because both quote the pin.

A pin update is never merged to make a red job green. If the application no
longer builds, the failure is a compiler regression until proven otherwise; the
profile diff is how that judgement stays visible.

## What the slice does not prove

- It is one program on one CI platform (Linux x86_64). The checkpoint matrix and
  the conformance suite carry the platform claims; this job carries the
  end-to-end construction claim.
- It does not promote a gate. Gate state lives in
  [`roadmap.tsv`](roadmap.tsv) and the claim ladder in
  [`roadmap.md`](roadmap.md); this page is evidence input for the CLI0 row.
- It does not promise source compatibility. The application is written for the
  current alpha and may be adapted when the language moves, as D114 requires.
- The absent-families table is as important as the present one: a green run says
  nothing about enums, interfaces, generic declarations, maps, `while`, `defer`,
  `unsafe`, `extern`, mutable bindings or `as` casts, because the slice does not
  use them.
