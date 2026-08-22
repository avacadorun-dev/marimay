# Checkpoint 27: verifier-reconstructed integer proofs

Date: 2026-08-22

Scope: internal checked-i64 elimination for proven add/subtract/multiply. No
source arithmetic, wrapping/saturating, division, shift, width, or fast-math
change.

## Decision and implementation

Accepted D062 gives the interval lattice to
`bootstrap/src/base/integer_range.c`, MIR adaptation and transactional rewrite
to `bootstrap/src/mir/integer_range.c`, and hostile-bytecode reconstruction to
`bootstrap/src/bytecode/bytecode_verify_integer_range.c`. Proven MIR and wire
operations contain no serialized bounds: both verifiers reconstruct the exact
fact at the instruction. Unsupported facts, work exhaustion, or optimizer
allocation failure retain checked MIR.

The fixed point uses monotone convex-hull joins, widens only loop headers after
two expanding updates, then performs a bounded narrowing phase so finite true
edges recover useful loop-body ranges. This replaced an initially oscillating
incoming-fact update that exhausted the analysis budget on real loops.
Execution uses unsigned-domain C17 arithmetic and portable D035
two's-complement conversion; retained checked operations keep their original
panic, span, and first-failure order. Generated C inlines an equivalent checked
or proven helper without changing proof authority.

## Coverage and measurement

Focused tests cover constants/extrema, positive/negative intervals,
conversions, remainder, true/false edges, joins, converging loops, retained
overflow, MIR/bytecode corruption, encode/decode, VM/native O0/O2, optimizer
and verifier fail-index sweeps, and zero allocator counters. Inline helpers are
compared with the out-of-line semantic owner at success, overflow,
division-by-zero, and null-result boundaries.

The first corrected 11/3 checked-loop report measured 247.869 ms versus the
checkpoint-21 local 310.763 ms median, a 20.24% reduction, with 0.186 ms MAD
and 0.341 ms IQR. Its JSON SHA-256 is
`674f8f872e81b9dbd23b130dcd1bb06e29615b1396e3e2136f2cc65a288a0b89`.
The final corrected compiler artifact remeasured checked-loop at 68.357 ms
median with 0.354 ms MAD and 1.593 ms IQR; checkpoint 28 records the exact
artifact and report hashes.

This is local macOS arm64 evidence, not the separate five-platform public-alpha
registry.
