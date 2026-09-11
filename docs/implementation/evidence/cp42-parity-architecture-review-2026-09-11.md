# Parity feasibility and ordered workload work

Status: **architecture review and user-directed work-order update**. No new
production optimization or timing observation is claimed. Accepted source:
`51e0be3812410f2a3a3df89089a3a7770043ad1d`. The previous goal turn made progress:
it accepted, committed and pushed the enum/bounds change with measurements and
validation. Overall parity remains unachieved; this review does not narrow it.

## Evidence and architectural limits

The [current comparison](cp42-jit-enum-bounds-2026-09-11-comprehensive.md) gives
Native/Go 2.0225, Native/Rust 2.1732, VM+JIT/Go 13.5951 and VM+JIT/Rust 14.6082
as geometric latency ratios. Native executables are unchanged. The separate
[paired experiment](cp42-jit-enum-bounds-2026-09-11.md) reduces summed VM medians
by 9.76%, with a 1.0618 geometric speedup. Neither statistic proves row parity.

No inspected normative contract or measurement proves that language semantics
make parity impossible on this corpus. This is also not proof that every row
can reach parity, and no completion date has been established.

| Concern | Evidence and implication |
| --- | --- |
| GC and reference identity | These have real costs. Language rules allow stack placement/scalar replacement only when identity and lifetime observations are preserved. Different ownership/GC models preclude a blanket promise to equal Rust on every workload. |
| Handles, string storage, frame work | Repeated descriptor/reference resolution, universal VM values, frame rebuilding and separate string header/payload allocations are current implementation choices. Their overhead is not a demonstrated irreducible language cost. |
| JIT architecture | The emitter loads/stores VM register memory and routes calls through the activation owner. SLJIT provides portable low-level code generation; Marimay must supply stronger analysis to remove its runtime transactions. Register retention, call optimization and earlier safety proofs are possible work, not claimed features. |
| End-to-end VM | Source loading/checking/lowering and JIT preparation are additional work excluded from native columns. Retain that acceptance metric and separately measure phases; do not substitute a warm cache or remove startup. |
| Memory limits | Marimay keeps 67108864 managed bytes. Current metadata records no explicit GOGC/GOMEMLIMIT overrides and no identical enforced heap policy for all peers. This is a real comparison constraint, not proof that GC causes the current gap. Keep all limits and comparator settings unchanged. |

Source anchors: [language](../../spec/language.md),
[execution](../../spec/execution.md), [decisions](../../design/decisions.md),
[JIT emitter](../../../bootstrap/src/bytecode/vm_jit_sljit.c),
[activation](../../../bootstrap/src/bytecode/vm_activation.c),
[strings](../../../runtime/common/managed_string.c).
Primary external references checked on 2026-09-11:
[SLJIT](https://github.com/zherczeg/sljit),
[Go GC cost model](https://go.dev/doc/gc-guide),
[Rust ownership](https://doc.rust-lang.org/book/ch04-01-what-is-ownership.html).
These explain mechanisms, not benchmark results for Marimay.

## Text evidence and correction

The [text diagnosis](cp42-text-runtime-diagnosis-2026-09-11.md) remains relevant
to unchanged Native executables. It found 1057/2529 exclusive repeated-entry
samples in checked string-byte resolution during title search. These are
samples, not an exact elapsed-time fraction or a proved maximum speedup.
Native word-frequency creates 400100 managed strings; normalization creates
152632. The string owner allocates header and payload separately, and concat
copies through temporary storage.

The [shared-string experiment](cp42-shared-string-storage-2026-09-11-rejected.md)
did measure a 9.36% reduction in summed Native medians. It failed the then-current
2% median rule on min-heap. Its rejection does not establish that string work
lacks benefit. The later user-directed 5% tradeoff policy does not retroactively
accept that experiment; any revised candidate requires predeclared acceptance,
current controls and all validation. Existing string-budget and F32 descriptor
gaps remain open and must not be silently weakened to facilitate optimization.

## Ordered work protocol

The user requests visible progress in workload order. The active family is
`title-search-10k` plus `title-search-100k`, followed by `normalize-title`, then
`word-frequency`; continue in the original 32-row manifest order thereafter.
The search sizes share an algorithm. A change may improve later rows without
changing the active family or closing a row on aggregate evidence alone.

1. Pin accepted Native and VM+JIT controls, source hashes, arguments and outcomes.
2. Separate setup, useful work, allocation/GC and teardown in diagnostics; for
   source-run VM also identify frontend/checking/lowering/JIT preparation.
3. Record one common cause, proposed change, numerical targets, validation and
   deadline before candidate timing. Diagnostic cases never replace the corpus.
4. Use focused trials first, then full correctness and all32 regression checks
   for promising candidates. Commit/push only with the required evidence.
5. Keep the family active while pursuing necessary compiler/runtime dependencies.
   Record the dependency before changing subsystem. Failed attempts do not close
   a row or justify quietly moving to an unrelated easy gain. Reassess the cause.

The existing 1.10-times-faster-peer tolerance remains explicitly a proposal,
not a user-ratified completion criterion. The goal still covers every workload,
both Native and source-run VM, semantics, safety, limits and platform evidence.
A favorable aggregate cannot replace contemporaneous per-workload peer results.

The next title-search question is the split between title creation and repeated
search in both engines. Checked byte resolution, array-to-string projection,
equality and VM cold exits are the first mechanisms to distinguish. No workload
algorithm rewrite or benchmark-specific optimization is authorized.

The [frame counters](cp42-jit-enum-bounds-2026-09-11.md) still identify 15 million
DSU frame initializations and about 65 million reference clears/relinks. A
transitive noncollecting-call analysis was investigated by reading image, frame,
activation, enum and array-mutation owners; it is not implemented or benchmarked.
It requires proofs over recursive call graphs, cold execution, allocation
failures, panic/defer cleanup, roots and exact depth/fuel. Preserve it as follow-up,
not a competing active candidate while the text family is being addressed.
