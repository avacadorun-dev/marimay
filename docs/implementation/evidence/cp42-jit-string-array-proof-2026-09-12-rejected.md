# Adaptive checked string-array proof: rejected early

Status: **rejected at the fixed early gate**, against `d3c502b4d793fb84c1f43cb7740b61204f69d73c`.
The original VM title pair takes **3.546934% longer**,
with exact geometric median latency ratio **1.035469337919**.
It misses the minimum 3% improvement; the 100k median also exceeds the 1.05 row
guard. No favorable rerun, new sample selection or threshold adjustment follows.
Production and normative/architecture changes are restored. Accepted
[string publication V2](cp42-string-publication-v2-2026-09-12.md) and its
[all 32 comparison](cp42-string-publication-v2-2026-09-12-comprehensive.md) remain
current. Original Native/VM parity, complete Tier1 and Linux LSan remain open.

[Protocol, fixed results and counter analysis](cp42-jit-string-array-proof-2026-09-12-rejected-summary.json),
[exact sources, commands, counters and assembly](cp42-jit-string-array-proof-2026-09-12-rejected-raw.json.gz),
[replayable candidate patch](cp42-jit-string-array-proof-2026-09-12-rejected-candidate.patch).
Applying the patch with `git apply --unidiff-zero` on the parent reproduces
candidate tree `e69bc031cd0b09483980d27a543ead964aacb06f`, verified using private indexes. The raw archive and
every recovered file were roundtrip-verified. Candidate, control and all 114
original source/harness hashes match the frozen protocol.

## Candidate and correctness

The new independently compiled `vm_jit_string_arrays.c` owner uses bounded
per-binding metadata to count successful reads of an already checked canonical
STRING/Cell parent. After twice the current length, one nonallocating scan checks
every full child reference and string kind. An invalid unvisited child silently
denies reuse. A failed selected-child read still uses the unchanged C bridge,
retaining target, PC, fuel and resource record. Matching parent lifetime/identity
and live generation/length permit direct string-byte projection through current
storage/slots. A binding miss clears evidence; saturation disables the proof.
No copied child/storage address, extra root or managed allocation is introduced.
The existing invocation owner accounts for larger bounded JIT binding metadata.

D111, execution spec and architecture describe the optional proof and distinguish
it from allocating native child caches. These candidate allowances are restored
with the rejected code. There is no public syntax or source-result change.
The original checked string-cell bridge, dispatcher and runtime mutation/string
validation owners remain unchanged. Earlier rejected local-view or inline-Cell
implementations are not part of this candidate.

The first build and **18/18 focused ON tests** pass; **5/5 OFF tests** pass.
Expanded proof coverage then passes in its separate recorded build/test step.
All32 original Native/ON/OFF outputs agree and every Native executable is
byte-identical to accepted control. Six architecture, memory and documentation
gates pass before the fixed series. Full 105/89 CTest, ASan/UBSan, Rosetta,
full 32 paired timing and a fresh five-way comparison are not run after rejection.

The focused proof owner executes real JIT code in checked/proven and
metered/unmetered modes. It compares the complete destination VM value/binding
with the unchanged C oracle. Tests cover two-length admission, one-pass exclusion,
denied unvisited malformed children and retained denial, all reference components,
wrong-kind/null objects, mutation set/push/pop/truncate/clear/remove and view/
checked-access variants, array replacement, reserve and forced storage relocation,
successful/failed collection, stale parents and saturation. Seeded valid proofs
with fifteen different failed parent-key conditions must rebind or reject before
dereferencing deliberately invalid old object-field pointers. Differential
ordinary/program execution checks every bounded fuel step, shared counters and
all reachable deterministic allocation-failure indices with zero allocation/byte
ledgers. Existing call/root/ownership owners retain recursion and global wrap
coverage. These are focused local checks, not complete Tier1 acceptance.

## Fixed early timing

Three process warmups and eleven rotating/alternating fresh-process pairs retain
all original work, arguments, outputs, source-run preparation and 64MiB heap.
Quiet preflight and five-second monitoring report no known foreign compute or
observation gaps. These local Apple M2 measurements do not establish independent
host isolation. P90 uses the predeclared tenth of eleven ordered observations.

| Workload | Control median ms | Candidate median ms | Median ratio | P90 ratio |
| --- | ---: | ---: | ---: | ---: |
| title-search-10k | 56.816084 | 57.716041 | 1.015839828 | 1.016944710 |
| title-search-100k | 153.047459 | 161.538250 | 1.055478157 | 0.931560325 |

## Actual proof usage after rejection

A separately declared diagnostic instruments ignored copies of the frozen
candidate projection/binding owners. It changes no proof branch, heap state or
logical work and does not supply performance acceptance times. Each of 128 fresh
processes runs one original main: all 32 workloads, two repeats for plain and
central-allocator-ledger variants. All original outputs pass; four replicas per
workload have identical counters and all 64 ledger returns have zero live
allocations/bytes. Failed projections and disabled-path calls are zero in these
original inputs. Exact private source/commands/raw counters remain in the archive.

| Workload | Child reads | Successful scans | Scanned cells | Fast reads | Checked reads | Binding-discarded read evidence |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| title-search-10k | 814,304 | 2 | 10,016 | 794,272 | 20,032 | 0 |
| title-search-100k | 8,094,304 | 2 | 100,016 | 7,894,272 | 200,032 | 0 |
| normalize-title | 152,618 | 2 | 8 | 19,984 | 132,634 | 132,482 |
| word-frequency | 500,090 | 1 | 4 | 99,992 | 400,098 | 400,077 |

Every workload's actual fast-read, scan and scanned-cell counts match the earlier
independent stable-window model. Lost title evidence is therefore refuted:
title100k really executes 7,894,272 certified reads, with 200,032 checked reads
and 100,016 scanned cells. Approximately 96.3% of reference validations disappear,
but the fixed elapsed-time series does not improve. One-pass text arrays remain
excluded exactly as predicted. No instrumented duration is used as a speed claim.

## Changed next action

Disassembly of the frozen release C helpers makes the limited instruction
reduction concrete. The accepted successful helper path has 54 ARM64 instructions,
8 conditional branches and 17 load instructions. The candidate's already-certified
path has 52 instructions, 5 conditional branches and 22 loads. It still executes a
C call on every read. The archive records exact addresses, path conditions,
commands and binary/assembly hashes; these counts exclude the generated caller,
scans and hardware/cache effects. Whole-function sizes 56 versus 227 instructions
must not be confused with executed path lengths. Binding stride also grows from
104 to 136 bytes; neither static counts nor counter agreement isolate the cause
of the full measured regression.

The next bounded candidate should lower the successful proof guard and direct
projection into the JIT segment, taking the C proof/checked owner only on a miss.
This changes the hot execution boundary instead of repeating the failed timing
or loosening its threshold. It must retain complete parent lifetime validation,
live generation/length checks, every destination store, exact source exits/fuel,
and the unchanged C oracle. New actual-machine tests must exercise certified
success and each failed proof guard before any trusted child dereference. Declare
a fresh SMART and apply the same fixed early/full 32 acceptance rules. No such
inline certified implementation is accepted by this rejection record.

Stage began 2026-09-12T11:40:00.396128+00:00; due 2026-09-12T14:40:00.396128+00:00; evidence prepared
2026-09-12T12:10:23.870447+00:00; deadline met: **True**. Commit/push is
recorded separately.
