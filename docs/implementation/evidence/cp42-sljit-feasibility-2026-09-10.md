# CP42: SLJIT feasibility with the reused ARM64 prototype

Date: 2026-09-10. Status: **experimental; not production acceptance or parity**.
Source base: `f43647eeef4728eed96ab4855eb7ce02549dae7a`.
Host: Apple M2/macOS, `local-unverified`. SLJIT commit:
`d6ec3d07dcdbf2256ccae2b84d0d8a2d019f392e`.

The user selected JIT as the primary next route and explicitly identified the
previous ARM64 prototype as the work to reuse. The experiment retains its
region plan, nonallocating VM bindings, exact source PCs and segment-fuel
reservation/refund model. A shared SLJIT adapter replaces handwritten CPU
emission. Source loading, checking, typed MIR, bytecode serialization,
decoding and verification remain inside every measured `--engine vm` run.

## Reproducible results

The [paired report](cp42-sljit-feasibility-paired-2026-09-10.json) contains
168 fresh-process observations: six unchanged workloads × two immutable
compilers × (3 warmups + 11 samples). Participants alternate and workload
order rotates. Every checksum passed. All 114 reference input/harness hashes
and all pinned compiler/runtime/header hashes were checked before/after.
Neither host isolation nor Tier 1 acceptance is asserted.

| Workload | Current main, ms | Experimental JIT, ms | Control/JIT |
|---|---:|---:|---:|
| `dijkstra` | 2468.83 | 320.72 | 7.70× |
| `bfs` | 5714.52 | 3275.62 | 1.74× |
| `min-heap` | 6156.20 | 1221.93 | 5.04× |
| `dsu` | 4313.47 | 2853.56 | 1.51× |
| `insertion` | 248.90 | 627.26 | 0.40× |
| `heap-sort` | 1723.65 | 459.15 | 3.75× |

The [32-workload correctness report](cp42-sljit-feasibility-correctness-2026-09-10.json)
contains one fresh-process observation per unchanged workload, including all
catalogue cases. All 32 stdout checksums match, stderr is empty, and the
67,108,864-byte managed heap limit is unchanged. These single observations
are correctness probes, not median performance evidence.

Candidate SHA-256: `caa978d22850e6a3ee035083cda632c84aab13c4de2d3551b36dd2cb210b4748`.
A fresh rebuild with the committed reproduction script produced the same
candidate SHA-256 byte for byte, with the same baseline archive and pinned
SLJIT checkout. The build manifest records all compile/link commands and input
hashes under the requested build output.

The bytecode C unit suite linked against the experimental VM passed
(`mmay_c_unit_2: ok`); log SHA-256: `01b84b06176d7a97ae834a5a743b6300c2a1ad8fe9dbf8acfba2ed49014d7913`.
This exercises existing finite-fuel, image and frame/allocation contracts;
it does not establish full JIT operation/failure coverage. ASan/UBSan/LSan and
full platform coverage have not been run for this experimental JIT.

Standalone SLJIT smoke programs passed checked signed addition, overflow
branches, memory access and destination preservation on ARM64 (72 code bytes)
and x86-64 under Rosetta (40 bytes). This verifies that both emitters are usable,
not full VM conformance on x86-64 or Linux/Windows support.

## Correctness defect and shared runtime cost

The earlier Dijkstra checksum failure reproduced with the new backend. The
bytecode operand structure stores signed and unsigned members separately;
compact image construction read a record field index from the unsigned member.
Both emitters therefore read `Edge.to` when source semantics required
`Edge.weight`. Source-PC tracing and a comparison against the same incorrectly
projected operand could not detect this; the authoritative original instruction
was needed. The corrected record selector makes the complete Dijkstra checksum
`38672` with JIT execution enabled.

A pre-existing uncommitted fix and record-field regressions were found in the
main checkout. They were read and copied for evaluation; foreign files were
neither modified nor staged. That broader signed-third-operand conversion
caused 64-bit enum payload selectors to reject the entire image, silently
removing both the JIT and the existing composed loops. The isolated adapter
therefore fixes record selectors specifically; enum helpers read the original
instruction. Production integration must cover both nonzero record fields and
wide enum selectors without silently excluding every function's image.

After the field fix, a diagnostic Dijkstra run executed 501,987,240 JIT steps
in its main algorithm function across 310,120 entries, plus 25,015 JIT steps
in the benchmark caller. Coverage was already high; removing dispatch alone
was insufficient. The inherited prototype invalidated views after every
managed COPY/record read, even for the identical reference. Retaining the
view only when all provenance/slot/generation bits match removed repeated
binding calls. Every return to the VM still changes the epoch, and changed
references invalidate their slot. The paired table measures that version.

## Reproduction and next acceptance gates

The [experiment sources and build instructions](../../../tools/performance/sljit-prototype/README.md)
record the original prototype reuse, source hashes, pinned SLJIT revision and
narrow patches applied only under `build/`. Use experiment snapshot `0145177bd4c6366128a2039fd094ba1fede30f3a`
with a matching Release archive to reproduce this historical candidate, then run:

```sh
python3 tools/performance/compare-vm-preparation.py \
  --benchmarks ../algorithm-benchmarks \
  --reference docs/implementation/evidence/cp42-vm-preparation-comprehensive-2026-09-09.json \
  --before /absolute/path/to/pinned-control/mmay \
  --after build/sljit-prototype/mmay \
  --output build/sljit-prototype/paired-six.json \
  --workload dijkstra --workload bfs --workload min-heap \
  --workload dsu --workload insertion --workload heap-sort
```

Production adoption is blocked by the measured insertion regression, ordinary
call/activation costs, missing central hooks for SLJIT compiler allocations,
platform executable-page ownership and deterministic OS/allocation failure
coverage. Code-budget exhaustion and unsupported-host fallback also need a
production contract and tests; this prototype may report compilation failure
as allocation failure. No benchmark or source semantics change is proposed.

The next implementation work is to retain D109's exact composed-loop semantics
inside the new execution tier, support cheaper ordinary calls with existing
logical depth/roots/limits, and finish the memory/platform owners before the
complete acceptance matrix. Go/Rust parity remains open. Bun remains an
intermediate comparator only.
