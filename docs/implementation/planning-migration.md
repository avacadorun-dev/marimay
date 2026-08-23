# Roadmap consolidation migration record

Status: historical planning migration record, not planning authority.
Audit revision: 2026-08-23 working tree.

The authoritative product state is in [the product roadmap](roadmap.md), the
authoritative execution queue is in [the checkpoint ledger](checkpoints.md),
and direct reports are indexed by [the evidence index](evidence/README.md).
This file records how the retired top-level planning documents were disposed;
it does not create another queue.

## Source identity

| Retired file | SHA-256 before migration |
|---|---|
| `checkpoints.md` | `1550083dd3795f82c5991d149109ff9afe1cc2b756656ebf60b737ab9a74f604` |
| `algorithm-followup-checkpoints.md` | `2234026d7ccf85defaced49be1b6c68ff97b18facd7dd9b9dbf139d93b96f95f` |
| `public-cli-alpha-checkpoints.md` | `66dabd1c9eef4b780ef3da8835baaf87d514b03fe7d454f75bc77dd1d439b4d3` |
| `goal-objective.md` | `3dbd5ae5e8d85161b5af7834cd9d04303997233264088242c1a5ede6bc5eacdf` |
| `algorithm-followup-checkpoint-task.md` | `4b00eee7c2f0696b249f5e93e48f0278003ba20bf940d613628f959827cacdef` |

## Section traceability

`historical-evidence` means the original instruction was executed and its
durable result is represented by linked evidence rather than copied
instructions. `duplicate-rule` means the rule already exists in repository
governance or the ledger's common contract. `superseded` names its replacement.

| Old file and section | New file and anchor | Disposition |
|---|---|---|
| `checkpoints.md` opening order and status rules | `checkpoints.md#status-and-evidence-model` and `#common-execution-contract` | `duplicate-rule` — replaced by the closed register vocabulary and one common contract |
| `checkpoints.md` CP0 | `checkpoints.md#cp0-bounded-compiler-architecture` | `historical-evidence` |
| `checkpoints.md` CP1 | `checkpoints.md#cp1-nominal-record-enum-and-interface-engines` | `historical-evidence` |
| `checkpoints.md` CP2 | `checkpoints.md#cp2-local-packages-imports-and-d043` | `historical-evidence` |
| `checkpoints.md` CP3 | `checkpoints.md#cp3-compiler-service-facts-and-completion` | `historical-evidence` |
| `checkpoints.md` CP4 | `checkpoints.md#cp4-generic-option-result-and-question` | `historical-evidence` |
| `checkpoints.md` CP5 | `checkpoints.md#cp5-generic-managed-arrays` | `historical-evidence` |
| `checkpoints.md` CP6 | `checkpoints.md#cp6-numeric-widths-conversions-and-formatters` | `historical-evidence` |
| `checkpoints.md` CP7 | `checkpoints.md#cp7-lexical-defer` | `historical-evidence` |
| `checkpoints.md` CP8 | `checkpoints.md#cp8-authoritative-batch-check` | `historical-evidence` |
| `checkpoints.md` CP9 | `checkpoints.md#cp9-verified-vm-run-and-stdout` | `historical-evidence` |
| `checkpoints.md` CP10 | `checkpoints.md#cp10-native-build` | `historical-evidence` |
| `checkpoints.md` CP11 | `checkpoints.md#cp11-built-in-test-loop` | `historical-evidence` |
| `checkpoints.md` CP12 | `checkpoints.md#cp12-private-preview-integration` | `historical-evidence` |
| `algorithm-followup-checkpoints.md` opening evidence | `evidence/README.md#historical-checkpoint-evidence` | `migrated` |
| `algorithm-followup-checkpoints.md` issue traceability | `checkpoints.md#historical-checkpoints-13-28` | `migrated` |
| `algorithm-followup-checkpoints.md` CP13–CP19 | `checkpoints.md#cp13-profile-coherent-native-runtime` through `#cp19-managed-heap-resource-policy` | `historical-evidence` |
| `algorithm-followup-checkpoints.md` CP20–CP28 | `checkpoints.md#cp20-reusable-algorithm-corpus` through `#cp28-algorithm-integration-boundary` | `historical-evidence` |
| repeated validation, allocation, architecture, commit and push rules in CP13–CP28 | `checkpoints.md#common-execution-contract` | `duplicate-rule` |
| `public-cli-alpha-checkpoints.md` release condition | `roadmap.md#release-claims-and-ladder` and `checkpoints.md#cli0-boundary` | `migrated` |
| `public-cli-alpha-checkpoints.md` common rules | `checkpoints.md#common-execution-contract` | `migrated` |
| `public-cli-alpha-checkpoints.md` issue traceability and dependency graph | `checkpoints.md#active-sequence-cp29-cp40` | `migrated` |
| `public-cli-alpha-checkpoints.md` CP29 | `checkpoints.md#cp29-public-release-contract-and-closure-matrix` | `migrated` |
| `public-cli-alpha-checkpoints.md` CP30 | `checkpoints.md#cp30-dynamic-strings-interpolation-and-primitive-formatting` | `migrated` |
| `public-cli-alpha-checkpoints.md` CP31 | `checkpoints.md#cp31-program-arguments-and-controlled-exit` | `migrated` |
| `public-cli-alpha-checkpoints.md` CP32 | `checkpoints.md#cp32-typed-standard-streams-and-flush` | `migrated` |
| `public-cli-alpha-checkpoints.md` CP33 | `checkpoints.md#cp33-utf-8-text-bytes-and-bounded-builders` | `migrated` |
| `public-cli-alpha-checkpoints.md` CP34 | `checkpoints.md#cp34-paths-and-filesystem-slice` | `migrated` |
| `public-cli-alpha-checkpoints.md` CP35 | `checkpoints.md#cp35-environment-and-process-context` | `migrated` |
| `public-cli-alpha-checkpoints.md` CP36 | `checkpoints.md#cp36-map-set-and-iterator-core` | `migrated` |
| `public-cli-alpha-checkpoints.md` CP37 | `checkpoints.md#cp37-first-party-cli-parser` | `migrated` |
| `public-cli-alpha-checkpoints.md` CP38 | `checkpoints.md#cp38-bounded-typed-json` | `migrated` |
| `public-cli-alpha-checkpoints.md` CP39 | `checkpoints.md#cp39-native-run-init-doctor-and-installable-artifacts` | `migrated` |
| `public-cli-alpha-checkpoints.md` CP40 | `checkpoints.md#cp40-cli-alpha-integration-and-publication-ready-audit` | `migrated` |
| `public-cli-alpha-checkpoints.md` deferred-after-alpha list | `roadmap.md#post-cli0-i6-and-e0` and `checkpoints.md#cli0-boundary` | `migrated` |
| `goal-objective.md` outcome, V0 spine, preview language and acceptance programs | `checkpoints.md#historical-checkpoints-0-12` and evidence index | `historical-evidence` |
| `goal-objective.md` coordination/model-routing/delegation contract | `AGENTS.md` plus current Codex runtime policy | `superseded` — execution policy is session/repository governance, not roadmap state |
| `goal-objective.md` quality checks and Git checkpoints | `AGENTS.md` and `checkpoints.md#common-execution-contract` | `duplicate-rule` |
| `goal-objective.md` exclusions and final report | `roadmap.md#current-limitations` and `#release-claims-and-ladder` | `migrated` |
| `algorithm-followup-checkpoint-task.md` objective and edit boundary | this migration record | `historical-evidence` |
| `algorithm-followup-checkpoint-task.md` mandatory reading and benchmark/code audit | CP13–CP28 evidence index | `historical-evidence` |
| `algorithm-followup-checkpoint-task.md` problem inventory and atomicity rules | `checkpoints.md#historical-checkpoints-13-28` and `#common-execution-contract` | `migrated` |
| `algorithm-followup-checkpoint-task.md` required card structure | `checkpoints.md#active-checkpoint-card-contract` | `superseded` — standardized by the consolidated ledger template |
| `algorithm-followup-checkpoint-task.md` sequence closure and self-review | CP28 evidence plus `tools/check-roadmap` | `historical-evidence` |
| `algorithm-followup-checkpoint-task.md` final response format | this migration record and evidence index | `superseded` — no longer an executable task brief |

## Preserved history

All reports below `docs/implementation/evidence/` remain byte-preserved. Some
reports accurately refer to the filenames or identities that existed when the
commands ran. The evidence index explains those historical references; they
are not active planning authority and are not rewritten as if the consolidated
ledger existed at the time.
