# Working on Marimay with agents

Agents follow the same contracts and review rules as people. Start with
[AGENTS.md](../AGENTS.md), then the [implementation brief](../IMPLEMENTATION-BRIEF.md).
Source code carries no author, agent, trust, or attestation marker under D034.

## Load a bounded context

Read the relevant specification, accepted decision, subsystem
[architecture](implementation/compiler-architecture.md), focused owner files,
and adjacent tests. Inspect Git status and preserve existing work. Reuse the
checkout when suitable; choose a descriptive branch if isolation is needed.
Do not use a `codex/` branch prefix.

Name the responsible owner and expected behavior before editing. A backend
must not repair checker semantics. Narrow responsibilities matter beyond line counts.

## Keep the specification and evidence connected

Use the existing repository documents as the specification-driven workflow:

| Question | Authoritative location |
| --- | --- |
| What behavior is required? | [Normative specifications](spec/) and [requirement IDs](requirements.md) |
| Why was it chosen? | Accepted entries in [design decisions](design/decisions.md) |
| Who implements it? | [Compiler architecture](implementation/compiler-architecture.md) and focused owner files/tests |
| What remains to be delivered? | [Product roadmap](implementation/roadmap.md), [checkpoint ledger](implementation/checkpoints.md), and their [register](implementation/roadmap.tsv) |
| What has actually been checked? | Direct reports in the [evidence index](implementation/evidence/README.md), with exact revisions, commands, and platform scope |

For each proposed semantic change, identify its requirement and decision,
update the normative contract, then implement and verify the affected owners.
Keep implementation, tests, examples, and evidence in the same reviewable
change. Resolve conflicting contracts through a decision and specification
update before implementing them. This workflow uses the existing authority
order; do not create a second competing specification tree.

## Implement and verify

Follow the semantic-change protocol in AGENTS.md. Include the accepted decision,
normative text, focused tests, formatter fixtures, VM/native comparison, and
examples. Use independently compiled C17 owners, never included `.c` or `.inc`
implementation shards.

Run focused checks, then required architecture, roadmap, memory-policy, and
applicable build/test gates. Allocation paths need deterministic failure-index
coverage and zero live allocation/byte counters after teardown. Report actual
commands, results, platform scope, and open acceptance criteria. Historical
benchmarks and job definitions do not establish milestone verification.

## Coordinate and hand off

When multiple contributors or agents are explicitly assigned, give each a
bounded owner and clear interfaces. Avoid concurrent edits to the same files.
The integrating contributor reviews the combined diff and validates the result.
Delegation never removes responsibility for review.

Do not copy credentials, personal paths/emails, private remotes, environment
dumps, chat transcripts, or unrelated process listings into commits or evidence.
Use synthetic fixtures and relative paths. External agent services have their
own data handling; the compiler's lack of telemetry does not cover those services.

Finish with the problem, resulting behavior, changed owners, decisions,
validation, remaining risks, and next concrete step. Never claim tests or
independent review that did not happen.
