# Notes: collection baseline and explicit mutation results

Local macOS arm64 correctness evidence, 2026-09-08. Language base is
`8e6fe1cba4378a136644bdc523fbd4265ded5328`; this change adds no compiler,
runtime or source-language semantics. Notes remains the existing external
repository at `/workspace/Projects/Marimay/marimay-notes`, based on
`08a6576` with an uncommitted JSON/Russian-UX migration. No application commit
or full CP42 acceptance is claimed.

## Observed behavior and changes

Four focused source tests establish the current shared-contract baseline:
add creates a new outer array but preserves existing Note/tag aliases; removal
mutates the existing array and preserves order and retained record identity;
find returns the stored record; rejected add preserves a nonempty input.
These tests intentionally describe today's inconsistent add/remove model.
The forthcoming model migration must change that contract explicitly.

Notes now consumes `Set.add`'s existing `Result<bool, CollectionGrowthError>`:
the bool detects duplicate IDs without a separate membership lookup, and
`mapError`/`?` propagates growth errors. Array pushes in note copying and tag
collection likewise propagate their typed errors. The shrinking truncate's
impossible error is made an explicit invariant panic instead of a silent
discard. D074 is preserved; no mutation API was changed to hide failure.
Void match arms replace three synthetic bool results using the existing D092
surface. These changes use the present language and are not speed measurements.

A closed stdout pipe exposed a separate application defect. Before the fix,
`--help` returned status 0 and empty stderr on VM, native Debug and native
Release, even though the consumer was already closed. `main` now checks
`io.flushStdout()` before returning success. Five scenarios per engine verify
help/list/show/add/remove. Error detection may happen at write or flush;
both are defined failures and status must be 1. The report records the actual
diagnostic for each engine instead of requiring identical host buffering.

For add/remove, the file is already atomically committed when confirmation is
printed. The closed-pipe tests require the complete new JSON to remain on disk;
they do not pretend output failure rolls back persistence. README explains
the retry consequence. Read-only commands preserve exact input bytes.

## Verification

| Command / fixture | Result |
|---|---|
| `mmay fmt . --check` and `mmay check .` | Passed on the captured application sources. |
| `mmay test . --engine both` | 14 source tests passed; 0 failed. |
| `MMAY=.../build/release/mmay python3 tools/acceptance.py` | 27 cases × VM/native Debug/Release passed, comparing exact status/stdout/stderr/JSON bytes. |
| Closed-pipe portion of the same adapter | 5 cases × 3 engines passed; status 1, explicit output diagnostic, exact read-only or committed-file state. |

The process cases include Unicode text/tags/path names, stable removal, missing
IDs, monotonic ID non-reuse, blank text, CLI errors, zero/negative/overflow IDs,
ID exhaustion, invalid version/duplicate ID/missing field/UTF-8, the 1 MiB input
limit and denied read access. Each execution resets synthetic input and checks
the outside fixture and absence of leaked temporary files. The application
adapter vendors the unchanged shared acceptance driver version 1, SHA-256
`e7db6d3bcb596fd8d36a8af14068e83ed928a1c48f67211dfcd2c998a5e546ce`.

The [reproduction packet](cli0-notes-contract-baseline-2026-09-08.json) contains
before/after application source snapshots and hashes, exact verification
outputs, the earlier 27-case baseline, the closed-pipe reproducer result and
the final adapter report with compiler/native executable hashes. Only synthetic
fixtures were used; no private notes database was opened or archived.
The application sources remain in their existing checkout; this packet keeps
the change reviewable without automatically committing other pending work in
that repository.

## Remaining scope

The [shared collection contract packet](../../research/cli0-collection-contracts-2026-09-08.md)
ties shallow copy, predicate search, stable removal, Option binding and the
notes model together, including failure atomicity, callback mutation, ownership
and the tests needed at each boundary. Its new APIs and syntax are proposals,
not accepted normative text or implemented features. Existing Result handling
improvements are already exercised by notes.

Notes still lacks the required search command, consistent functional removal,
the full OS write-failure suite and practical latency/memory acceptance on the
original datasets. This local run does not establish other Tier 1 platforms,
fresh Linux sanitizer coverage, all ten applications, CP41, install or N0.
No C suite rerun is claimed for this documentation/application-only slice.
CP42 and CLI0 remain open. No new idea from the Vladimirov book was used here.
