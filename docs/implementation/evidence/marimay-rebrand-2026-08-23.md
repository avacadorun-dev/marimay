# Atomic Marimay repository rebrand — 2026-08-23

Status: **verified locally on Darwin arm64 with AppleClang 17**. N0 remains an
open external distribution-identity blocker; this report proves only the
repository migration defined by [D001](../../design/decisions.md).

## Scope and baseline

The clean tracked baseline contained 944 files. Old identity content appeared
in 706 files; 174 tracked sources used `.ruda`, 26 manifests were `ruda.mod`,
2 workspaces were `ruda.work`, and 310 tracked paths contained the old identity.
The only pre-existing untracked file was the supplied rebranding task brief.

The migration renamed 308 active tracked paths without a case-insensitive
collision. The other two old-identity paths are historical evidence filenames
and remain unchanged. The migration preserves all 174 sources as `.my` and
adds one focused `.my` negative-test fixture, for 175 total; it contains
26 `mmay.mod` files and 2 `mmay.work` files. Four new tracked files add the
historical-evidence policy, this report, and focused legacy-name rejection
fixtures.

## Implemented identity

- Language/product prose uses Marimay; the executable, C/CMake prefix, include
  namespace, install stem, and cache/runtime stem use `mmay`/`MMAY`.
- Sources and bundles use `.my`/`.myb`; project and tool state use
  `mmay.mod`, `mmay.work`, `mmay.lock`, and `.mmay/`.
- Markdown/editor IDs and JSON schemas use `marimay`, including
  `marimay.diagnostic/1` and `marimay.version/1`.
- Function/program bytecode magic is `MMAYBCF\0`/`MMAYBCP\0`. The native cache
  marker is `mmay-native-build-cache-v3`; runtime manifests, profiles, static
  archives, include paths, and relocation paths use only the new identity.
- Generators were updated first and regenerated the committed bytecode,
  diagnostic, syntax, and Unicode artifacts reproducibly.
- `.ruda` source and `ruda.mod` discovery are rejected by focused C-unit tests.
  No compatibility executable, typedef/header wrapper, source fallback,
  manifest fallback, old-magic decoder, or cache fallback was added.

## Historical and negative residues

Pre-rebrand reports under this directory retain the command names, targets,
paths, schemas, and external URLs that were true when they ran. The policy is
documented in [the evidence README](README.md). Only relative links broken by
tracked path renames were repaired.

Outside those reports, old identity text is limited to:

- the historical paragraphs of D001 and the N0 naming audit;
- active-roadmap links whose target is one of the two preserved historical
  evidence filenames;
- negative tests that prove `.ruda` and `ruda.mod` are no longer recognized.

The tracked-path residue is exactly the two historical evidence filenames and
the negative-test `ruda.mod`. Case-folding the complete tracked path set
produces no duplicate. Pre-existing ignored `.ruda/` build/cache directories
on the local machine were left untouched as local artifacts; Marimay neither
discovers nor writes them, and they are not part of the repository diff.

## Verification

| Command | Result |
|---|---|
| `./tools/check-architecture` | pass |
| `./tools/check-c-memory-policy` | pass |
| `./tools/check-generated` | pass after all four generators reproduced committed output |
| `./tools/check-doc-links` | pass |
| `./tools/check-spec-terms` | pass |
| `./tools/check-example-syntax` | pass |
| `./tools/check-syntax-coverage` | pass: 75 syntax kinds, 116 grammar productions, 0 open |
| fresh Debug configure with `MMAY_WARNINGS_AS_ERRORS=ON` and parallel build | pass |
| final `ctest --test-dir build/rebrand-debug --output-on-failure` | pass: `mmay_c_unit` and `mmay_install_relocation`, 269.66s total |
| fresh Release preset configure/build | pass |
| final `ctest --preset release --output-on-failure` | pass: 2/2, 250.87s total |
| fresh ASan+UBSan preset configure/build | pass |
| final ASan+UBSan CTest with halt-on-error options | pass: 2/2, 447.28s total |
| explicit relocated-install assertion for `bin/mmay` and absence of `bin/ruda` | pass in Debug, Release, and ASan+UBSan trees |
| residue audit, case-insensitive collision audit, and `git diff --check` | pass; only classified residues above |

The first sanitizer run exposed hard-coded test path lengths retained from the
longer `.ruda` spelling. Every numeric `.my` path-length site was audited and
replaced with the correct or compile-derived length; the complete sanitizer,
Debug, and Release suites then passed on the final tree.

The user smoke matrix passed:

- `mmay --help` and `mmay version --json` expose Marimay and
  `marimay.version/1`;
- `fmt --check`, `check`, and `run` accept `examples/matrix.my`, with `run`
  producing `415.0`;
- `mmay test tests/c-unit/checkpoint11-tree --engine both` passed 5/5 tests;
- `mmay build tests/run/cp9/void.my --output build/rebrand-smoke` produced an
  executable that exited successfully;
- the fresh Debug tree contains `mmay`, not `ruda`, and CTest lists only the
  renamed test targets.

Linux LeakSanitizer/TSan, MSVC ASan, and the remaining Tier 1 CI host matrix
were not run locally because this machine is Darwin arm64. The repository CI
definitions use the renamed options/targets and retain ownership of those
platform-specific gates.

## Diff review

The final diff is limited to identity-dependent path/content changes, the
accepted D001/N0 record, generated-output regeneration, historical-link
maintenance, and minimal legacy rejection coverage. It contains no source
syntax, language semantics, MIR opcode, compiler architecture, or unrelated
optimization change.
