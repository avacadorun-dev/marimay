# Fresh string references over shared bodies: rejected early

> **Private experiment payloads.** Archived implementations and patches
> referenced in this historical report are retained privately. Commands
> and digests below document the original experiment; they are not public
> reproduction instructions. See the [artifact index](private-artifacts.md).

Status: **rejected**. Neither engine improves in the fixed title-pair screen:
Native pair ratio **1.011506** and VM+JIT
pair ratio **1.010651**. The requirement was
at least one ratio at or below 0.97 and the other at or below 1.00. There is no
favorable retry, full validation or full 32-workload acceptance timing after this failure.
Production is restored. The original all 32 Native/VM parity goal remains open.

[Predeclared protocol](cp42-string-body-sharing-2026-09-13-plan.md),
[results and hashes](cp42-string-body-sharing-2026-09-13-rejected-summary.json), [raw evidence](private-artifacts.md#artifact-048),
[candidate patch](private-artifacts.md#artifact-047).
Apply the zero-context patch to `451d98c33e049391b219986bf515154bf7b47e6b` with
`git apply --unidiff-zero` to reproduce tree `e4955107fbb3ad2687c2b1afbec0eefc3b0ca713`.
The [latest accepted-production table](cp42-jit-string-search-loop-2026-09-13-rejected-accepted-comprehensive.md)
remains authoritative; the diagnostic rows below do not replace it.

## Candidate and checks

The shared C17 string owner keeps the public string header as the first member
of a private physical body with one owner count. A fixed 256-entry weak cache
considers at most 64 bytes and checks complete live reference identity, length
and exact content. Every construction still publishes a fresh logical slot and
charges the complete original header/payload and slot growth. Weak hints do
not retain a slot or body. A pending owner survives collection of an old alias
and is released on publication failure. The original collector and public
string/slot layouts are unchanged. D055, spec and architecture amendments are
archived with the implementation and restored on rejection.

The first build and **7/7 focused tests** pass, including runtime, bytecode,
JIT strings and source VM/native differential lifetime checks. An additional
wrong-length weak-hint case was added before freezing, then the final storage
test and all local gates passed. Tests cover distinct handles over equal bytes,
empty/NUL/Unicode and concat boundaries, long-string bypass, all six alias
collection orders, stale/foreign/wrong-kind/wrong-length hints, exact budgets,
cached and cold slot growth, retirement, borrowed output aliasing, every
reachable allocation failure and both zero teardown ledgers. Existing cold
growth assertions remain; distinct input contents keep those allocations cold.
A new source test retains repeated strings through containers, partial
replacement and allocation churn in both engines under 8 MiB.

All original 32 Native, ON and OFF results pass before timing. Architecture,
C memory policy, roadmap, links, spec terms, generated and diff checks pass.
The full ON/OFF suites, sanitizer and x86 matrix were deliberately not run
after failed early admission. Complete Tier1 and Linux LSan are not claimed.

## Fixed early results

Three warmups and eleven fresh-process pairs rotate rows and alternate
participants. Original 114 corpus/harness hashes, arguments, stdout and 64 MiB
heap are unchanged. VM timing retains source preparation. Quiet preflight and
monitoring show no disallowed foreign compute or observation gaps.

| Engine | Workload | Control ms | Candidate ms | Time change |
| --- | --- | ---: | ---: | ---: |
| native | title-search-10k | 6.510 | 6.561 | +0.78% |
| native | title-search-100k | 43.308 | 43.968 | +1.53% |
| on | title-search-10k | 55.624 | 56.538 | +1.64% |
| on | title-search-100k | 143.271 | 143.973 | +0.49% |

## Later text-row diagnosis

After rejection, one separately declared series measures the two previously
unmeasured text rows. It uses the same frozen binaries and process protocol,
cannot rescue acceptance and does not replace or close the active title family.

| Engine | Workload | Control ms | Candidate ms | Time change |
| --- | --- | ---: | ---: | ---: |
| native | normalize-title | 59.148 | 57.526 | -2.74% |
| native | word-frequency | 144.957 | 138.189 | -4.67% |
| on | normalize-title | 138.020 | 133.122 | -3.55% |
| on | word-frequency | 272.893 | 264.091 | -3.23% |

## Actual physical allocation counts

Private source copies count successful original constructions and actual body
allocation/reuse. Four unchanged workloads run in Native and VM, each with
control/candidate and plain/allocator-ledger variants: **32 original results**,
exact plain/ledger counter agreement and **16 zero allocation/byte ledgers**.
Logical construction counts remain identical. Counter variants are diagnostic
only; their elapsed times do not enter any performance series.

| Workload | Engine | Logical constructions | Original body allocations | Candidate body allocations |
| --- | --- | ---: | ---: | ---: |
| title-search-10k | vm | 40556 | 40556 | 21522 |
| title-search-10k | native | 30310 | 30310 | 21424 |
| title-search-100k | vm | 400556 | 400556 | 212944 |
| title-search-100k | native | 300310 | 300310 | 212121 |
| normalize-title | vm | 162656 | 162656 | 22665 |
| normalize-title | native | 152632 | 152632 | 22642 |
| word-frequency | vm | 500122 | 500122 | 40 |
| word-frequency | native | 400100 | 400100 | 19 |

Thus allocation elimination really occurs, including almost all word-frequency
string bodies, without the required end-to-end title gain. The cache also adds
hashing, equality, validation and owner accounting; these measurements do not
isolate the original allocator's CPU share or prove an irreducible language
cost. Do not start another cache-size/hash microvariant series from these counts.

The next measured title-family dependency is redundant MIR preparation and
lowering verification. The preceding accepted-control census found 358 verifier
calls for 67 catalog functions and about 13–14 ms process CPU. The concrete
handoff audit is preserved in the raw archive. It must retain public raw-MIR
admission and D049's fresh encode/decode/verify chain. In particular, the CLI
allows optimizer resource failures as checked-MIR fallback, and range passes
can return OK without verification when no candidate exists. Any new proof
handoff must track actual verification, mutation, rollback and fallback;
missing or failed proof cannot become trusted input. No such implementation,
startup cache or new performance claim is accepted here.

Evidence prepared after 43.8 minutes against the four-hour stage deadline; commit/push is recorded separately.
