# Checkpoint 25: exact and reserved array capacity

Date: 2026-08-22

Scope: internal array construction and growth planning. No Ruda source
spelling, public capacity observation, portable bytecode identity, evaluation
order, managed-budget boundary, or VM/native result changes.

## Decision and implementation

Accepted D061 gives capacity policy one owner:
`runtime/common/managed_array_capacity.c`. Construction with a known final
count bypasses geometric growth and allocates exactly
`count * descriptor.storage_stride` bytes, including exact zero capacity.
Unknown growth starts at four, doubles with checked arithmetic, clamps once to
the configured maximum, and performs at most one resize for a `reserve`
minimum. A covered reserve is an allocation-free no-op.

The planner completes all length, capacity, and D060 stride arithmetic before
D055 budget preflight or allocator entry. A collection retry keeps the array
and temporary reference element rooted, re-resolves the stable handle, and
recomputes the same deterministic plan. Resize publication is failure-atomic
and preserves contents, identity, length, generation, and live accounting on
every failure.

## Coverage

Focused capacity coverage includes counts 0, 1, 3, 4, 5, powers of two, raw,
reference, and nested elements; exact constructor capacity; geometric pushes;
covered and growing reserve; maximum clamp and overflow; exact allocation and
resize attempt counts; budget/host failure classification; iterator stability;
and zero allocator counters after teardown. Bytecode construction and native
emission continue to converge on the same runtime constructor/planner.

No formatter fixture or public example changes apply because capacity remains
unobservable. On the final checkpoint-28 artifact, growth measured 82.329 ms
and reserved-growth 78.888 ms; the exact report/hash and dispersion are
recorded in checkpoint-28 evidence.

## Validation

The final local command/result record is shared with checkpoint 28. The local
macOS arm64 result is supplemental and does not satisfy the separate D057
five-platform public-alpha registry.
