# Address-level JIT execution attribution

> **Private experiment payloads.** Archived implementations and patches
> referenced in this historical report are retained privately. Commands
> and digests below document the original experiment; they are not public
> reproduction instructions. See the [artifact index](private-artifacts.md).

Status: **diagnosed-local** on accepted parent `af0aa0f`.
This records a measured next action, not a speedup or Go/Rust parity result.
[Summary and exact inputs](cp42-jit-address-attribution-2026-09-11.json); [complete diagnostic records](private-artifacts.md#artifact-005).
The archive is gzip JSON; `gzip -dc` decodes it. Both encoded and decoded hashes
are in the summary. Build scripts, source copies, maps, raw profiles and commands
are preserved for replay and review.

## Method

The accepted compiler remains byte-identical. A diagnostic copy of the JIT
lifecycle owner prints each generated function address, instruction-label offsets,
original image operations and machine bytes. It uses the accepted C17/IPO flags
and linked archives. No production source or workload changes. Each variant uses
three fresh full-size runs for both the accepted and mapped compiler on DSU, BFS,
stack and min-heap, retaining all 114 input hashes, original arguments and stdout,
and 64 MiB heap. All **48** executions return the original expected output.

macOS `sample PID 2 1 -mayDie` captures up to two seconds at 1 ms intervals.
Early startup and tail execution may be missed. The first diagnostic writes each
hex byte separately and incurs substantial output overhead. Its complete data is
retained. The second batches hex output into 1024-byte writes and repeats the
same matrix. Its first DSU mapped run also has 524 loader-thread `_dyld_start`
samples; those remain in the raw totals. No profiled process duration is accepted
as a benchmark observation, and no sample is silently removed.

The parser verifies disjoint function ranges and monotonic labels, and checks
that decoded machine-byte lengths agree with the recorded code sizes. Only a
complete address list resolving to one operation receives that operation label.
Unknown/truncated lists and ambiguous label aliases remain unknown. Exclusive
counts partition stacks; inclusive counts overlap. Generated opcode shares below
use only fully attributed generated-code samples, excluding C helpers and startup.

## Result

| Workload | Accepted samples | Accepted unknown native samples | Mapped generated samples | Mapped unknown samples | Leading generated opcodes (exclusive samples) |
|---|---:|---:|---:|---:|---|
| dsu | 2453 | 1373 | 1580 | 0 | ARRAY_REQUIRE_INDEX: 560, COPY: 290, ARRAY_GET_IN_BOUNDS: 287, RECORD_GET: 245 |
| bfs | 2914 | 1288 | 1381 | 26 | ARRAY_REQUIRE_INDEX: 390, ARRAY_GET_IN_BOUNDS: 279, BRANCH_BOOL: 226, ARRAY_REQUIRE_GENERATION: 140 |
| stack | 707 | 73 | 78 | 3 | RECORD_GET: 20, ARRAY_LENGTH: 19, BRANCH_BOOL: 10, I64_REMAINDER_CHECKED: 8 |
| min-heap | 1670 | 1148 | 1103 | 2 | RECORD_GET: 287, ARRAY_REQUIRE_INDEX: 259, BRANCH_BOOL: 176, COPY: 127 |

In DSU the array-index proof and following proven read account for 847 of 1580
mapped native samples. BFS has 669 of 1381 in those two opcode groups and another
140 in generation checks. Min-heap has 546 of 1103 in record reads and array-index
proofs. Stack remains mostly in C runtime/call owners; this candidate is not
expected to solve that workload's entire gap.

Generated machine bytes were wrapped in ARM64 object text and disassembled with
`otool -tvV`. The hot-address record shows repeated binding-epoch loads, retained
reference/type comparisons, full-reference copies and repeated binding checks at
the next proven access. Ordinary COPY and branch targets are also sampled;
sampling alone does not prove that a particular load is the cause of a stall.

## Selected intervention and proof obligations

The next candidate tracks exact-type/layout managed-reference copies only inside
an uninterrupted native segment. When two registers are known to contain the
same complete reference, a following access can use the canonical register's
existing checked binding. A register overwrite invalidates all relationships
that depend on it. Native call/return and every independently enterable segment
reset the analysis. Source-language aliasing and object mutation remain unchanged.

Bounds and mutation-generation checks stay at their original instructions; this
removes duplicate identity/descriptor work rather than admitting unchecked object
access. Full reference values remain in VM registers for roots and cold exits.
Finite/shared fuel and allocation/code-size failure behavior retain their current
contracts. Focused tests must cover alias chains, overwritten canonical registers,
branch joins, mixed layouts, native calls, growth/GC, poisoned stale bindings and
precise cold exits. The implementation needs its own predeclared early target,
complete unchanged all32 timing gate and ordinary safety/platform validation.
