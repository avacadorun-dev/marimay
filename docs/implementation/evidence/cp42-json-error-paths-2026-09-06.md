# CP42 structured JSON error paths: local follow-up

Scope: local working-tree evidence for the commit carrying this report, not
checkpoint or release verification. Base: `ca9f153027012c15b796c9c208babaea5e7e4ec2`,
branch `main`. Host: macOS arm64, AppleClang 21, portable C17. This follows the
[notes/startup observation](cp42-notes-startup-2026-09-06.md) and implements the
field/index-error part of D086 without closing CP42.

## Contract and ownership

Each named `json.Error` case now carries `path: []json.PathSegment`.
`InvalidLimits` remains a root-only unit case. Automatic codecs attach enclosing
field names and zero-based array indices only on failure. Cursor errors retain
their original case, payload and byte span; missing, unknown and duplicate
fields identify the offending field. Container grammar errors identify the
container. Explicit codecs can prepend context with `atField` and `atIndex`.

`path` returns an independent shallow array copy. `pathText` produces an escaped
rooted display capped at 4,096 UTF-8 bytes without truncating the structured
path; `message` appends this display to non-root failures. The public corpus
documents the intentional pre-alpha migration for named error patterns and
manual constructors. No new source syntax was introduced.

The checker carries a borrowed stack chain during expansion and restores it on
success and failure. It emits ordinary source-package calls on error branches;
there is no backend-specific error representation or runtime context table.
The source JSON package owns composition and rendering. The architecture and
C memory-policy gates passed before this observation was recorded.

## Local conformance and cleanup

```sh
cmake --build --preset debug -j 4
build/debug/mmay fmt std/json --check
build/debug/mmay fmt tests/public-alpha/typed-json --check
build/debug/mmay fmt tests/public-alpha/cp38-json --check
ctest --test-dir build/debug --parallel 4 --output-on-failure
cmake --fresh --preset asan-ubsan
cmake --build --preset asan-ubsan -j 4
build/asan-ubsan/mmay doctor
ctest --test-dir build/asan-ubsan \
  -R '^(mmay_typed_json|mmay_cp42_unit|mmay_cp38_unit|mmay_cp38_json)$' \
  --parallel 2 --output-on-failure
```

The complete Debug run passed all 67 CTests in 690.02 seconds. The final focused
ASan/UBSan run passed all four tests in 540.79 seconds. These overlapping,
load-dependent runs are correctness observations, not performance baselines.
The typed-JSON process suite checks format idempotence, source checking, all
13 tests in both engines, native debug/release output and hostile-input error
parity. Nine new tests cover nested field/index paths, every named error
payload, spans, root and container failures, limits, escaped/empty field names,
independent copied paths, bounded display and repeated independent calls.

The CP42 signature-only checker fixture still isolates typing and expansion
allocation failure, not JSON parsing. Its every-index failure loop covers the
additional MIR calls and checks both `live_allocations == 0` and
`live_bytes == 0` after teardown. It passed in Debug and ASan/UBSan. The public
source corpus separately executes the actual JSON implementation.

The first CP38 process run encountered its old exact duplicate-field-message
expectation. The expectation now includes the specified path suffix; the
focused rerun and final full run passed. Initial sanitizer native checks also
reported B0002 because their CMake cache recorded AppleClang 17 while the host
had AppleClang 21. Fresh profile configuration restored the correct compiler
identity; doctor and the full focused rerun passed. No assertion, verifier,
allocation-failure index or sanitizer check was disabled to obtain these results.

## Linux fuzz smoke

The existing JSON fuzz owner now also decodes a bounded `Envelope` containing
`[][]string`, renders failed paths and asserts display/schema-depth bounds.
The original cursor/skip/finish checks remain. Three checked-in seeds exercise
nested value, duplicate-field and escaped-field failures.

The harness was built with Ubuntu 24.04 / Clang 18.1.3 in an amd64 container
using a read-only source bind, ASan/UBSan and
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1`. A disposable corpus outside the
repository was seeded from `tests/fuzz/corpus/json`. Running the JSON harness
with `-max_total_time=60 -timeout=10 -max_len=4096` completed 385 executions in
61 seconds, exit zero, without a sanitizer report. The container runs through
the host's amd64 translation support; this is not native Tier 1 evidence, a
sustained campaign, or an unqualified absence-of-leaks claim. Linux CI still
must validate the exact pushed revision and its longer campaigns.

## Installed command and notes handoff

The sibling notes repository remains separately uncommitted. Its Russian error
patterns and messages were migrated to the new path payload. All ten notes
tests passed in both engines, and `tools/smoke-ru` passed in VM and release
native using isolated build-directory databases.

The user's `/opt/homebrew/bin/mmay` links to this repository's
`build/release/mmay`. Its old Release binary lacked the new API; rebuilding the
Release target synchronized the ordinary command with the source used by the
application. From the notes directory, ordinary `mmay check .`,
`mmay test . --engine both` and the reported command
`mmay run . --allow-read . -- show 1` now succeed. The existing user database's
SHA-256 was identical before and after the read-only check. No notes data or
separate notes commit is part of this language change.

## Remaining work

CP42 stays `in-progress`: generic intrinsic signature/completion metadata,
collection helpers, mutation return contracts, Option control-flow binding and
a consistent notes mutation API still need their ordered work and evidence.
Complete Tier 1, Linux CI and sustained fuzz evidence are not supplied by this
report. Prior green CI runs validate their own revisions, not this change.

A sample of the large typed-JSON test process also exposed repeated bytecode
decode/proof verification within the per-test loop. This is a follow-up
performance investigation, not a repaired defect or a reason to bypass the
verification or fresh-test-state contracts.
