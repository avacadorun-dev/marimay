# CP42: JIT fuel matches invocation options

Date: 2026-09-10. Status: **experimental; not enabled in production CMake**.
Control: owned-memory JIT snapshot `35155570616f3a0677117db34938e473dc6bb278`.
Production base and SLJIT pin are unchanged from that snapshot.
Host: Apple M2/macOS, `local-unverified`.

## Change and limits

D087 already configures ordinary local source runs with `max_steps=SIZE_MAX`
and no shared step counter. The interpreter does not count steps in that mode,
but the JIT still reserved and refunded fuel for every supported segment.
The generator now selects its mode once from the invocation options. It omits
segment accounting only for unlimited frame steps **and no shared-counter
pointer**. Every metered invocation retains the existing exact source-PC,
frame/shared-fuel and side-exit accounting.

Unmetered code refuses entry if passed any finite frame limit or any shared
counter, including one initially equal to SIZE_MAX; ordinary VM execution
then applies the supplied limits. This is a conservative tier-admission guard,
not a change to default limits, bytecode semantics or workload arguments.
Code-memory ownership, allocation failure behavior, call-depth accounting,
managed roots and D109 composition remain unchanged.

## Validation

The [validation record](cp42-sljit-fuel-mode-validation-2026-09-10.json) contains
exact commands, hashes and logs. The bytecode suite and unchanged 32-workload
correctness replay passed on macOS ARM64, macOS x86-64 under Rosetta and ARM64
ASan/UBSan. All 114 corpus/harness hashes and each compiler hash matched.
The benchmark managed heap remains 67,108,864 bytes.

`test_jit_fuel.c` checks a terminating loop, exact accounting with frame/shared
limits, a SIZE_MAX-valued shared counter, unmetered source exit and refusal
of incompatible invocation options. Its emitted code is 284/176 bytes for
metered/unmetered ARM64, and 207/133 bytes for x86-64. These sizes describe the
test fixture, not a complete benchmark's code size.

The two-function ownership test exhausts 12 allocation and 4 platform failure
indices independently in each mode. Existing publication/relocation tests also
pass. All paths assert zero allocator and code-mapping counters after teardown.
ASan/UBSan instrument the adapter, owners and SLJIT; macOS leak detection remains
disabled. Linux LSan, Linux/Windows execution and full Tier 1 conformance are
still open. Rosetta is not physical x86-64 performance evidence.

## Paired performance

The [six-workload report](cp42-sljit-fuel-mode-paired-2026-09-10.json) contains
168 fresh processes: six unchanged workloads × two pinned compilers ×
(3 warmups + 11 samples), with alternating participants and rotating workload
order. Every checksum and pinned input/toolchain hash passed.

| Workload | Owned-memory JIT, ms | Invocation-selected fuel, ms | Change |
|---|---:|---:|---:|
| `dijkstra` | 314.23 | 266.23 | -15.3% |
| `min-heap` | 1127.44 | 1126.21 | -0.1% |
| `heap-sort` | 444.49 | 434.60 | -2.2% |
| `dsu` | 2637.99 | 2651.40 | +0.5% |
| `insertion` | 231.79 | 232.00 | +0.1% |
| `title-search-10k` | 123.64 | 123.11 | -0.4% |

Dijkstra improves 15.3%; the other five changes are small and no broad speedup
claim is made for them. The complete 32-workload median table remains the
separate owned-memory snapshot's measurement; this six-workload follow-up does
not replace it or establish Go/Rust parity.

Candidate SHA-256: `2e5cc530109304597b6b7d5ef40f50ed49c7355d04ce0f12e61760c513fdfd8c`.

## Reproduction

Use the [prototype builder and test instructions](../../../tools/performance/sljit-prototype/README.md),
including `--unit --memory-test` and `test_jit_fuel`. Compare this compiler with
the separately pinned owned-memory compiler using
`tools/performance/compare-vm-preparation.py` and these six `--workload` names:
`dijkstra`, `min-heap`, `heap-sort`, `dsu`, `insertion`, `title-search-10k`.
The default protocol is 3 warmups and 11 samples. The full correctness replay
uses the same immutable 32-workload reference as the owned-memory report.
