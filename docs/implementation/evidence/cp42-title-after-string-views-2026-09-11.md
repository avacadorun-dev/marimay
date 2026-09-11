# Title-search costs after retained string bytes

Status: **completed local diagnosis** of accepted source `78d30702c4ca320cf85d0ec2cab44923b3914938`.
No additional optimization or parity result is claimed. The original title-search
pair remains active. [Protocol and results](cp42-title-after-string-views-2026-09-11-summary.json) and
[complete raw evidence](cp42-title-after-string-views-2026-09-11-raw.json.gz) retain source and binary hashes,
generated diagnostic copies, commands, all observations and publication evidence
for the preceding accepted implementation.

Five fresh phase-timing runs per original workload/engine and two separate
counter replays reproduce all 28 original outcomes under the unchanged 64 MiB
limit and 114 corpus hashes. Timed copies omit operation counters, including the
JIT comparison callback; their symbol table contains no `tp_count`. Coarse phase
markers still perturb execution. These diagnostic times are not acceptance
measurements or exact causal fractions of the uninstrumented process time.

| Original workload | Engine | Process ms | Source preparation ms | Data setup ms | Search ms | Post-search/cleanup ms |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| title-search-10k | native | 7.946 | — | 2.627 | 1.489 | 0.828 |
| title-search-10k | vm | 59.595 | 40.905 | 4.878 | 5.351 | 1.255 |
| title-search-100k | native | 58.494 | — | 26.426 | 21.870 | 7.225 |
| title-search-100k | vm | 165.914 | 39.422 | 44.340 | 65.157 | 10.052 |

Each column is a median of its own observation values and need not add to the
process median. Source preparation sums the six nonoverlapping phases within
each observation before taking its median. VM admission adds about 3.8 ms.
Native post-search includes heap teardown; VM reports execution and CLI teardown
separately in raw phases. Loader/exit overhead remains in process elapsed.

The 100k setup creates **300,016 Native strings** and **400,016 VM strings**.
The search still performs **7,994,208 comparisons**, with zero Native checked-byte
resolutions and **7,994,336 VM resolutions**. No search allocation or GC occurs;
both counter replays agree exactly. These match the accepted candidate's earlier
counter evidence. GC is zero across every observed phase.

The selected next common mechanism is shared string construction/storage and
destruction. The setup phase remains large in both engines. Static inspection
of `runtime/common/managed_string.c` shows a separate header allocation and
nonempty payload allocation, plus a temporary concatenation buffer copied again
into the string. Phase timing does not isolate how much time each allocation or
copy contributes; allocator/copy counters belong in the next candidate evidence.

Before changing that owner, resolve its **D055 budget-preflight and borrowed-byte
lifetime obligations**. A newly declared storage/direct-concatenation experiment
must use the current accepted controls and preserve full logical byte accounting,
failure publication, rooted inputs, all-index allocation teardown and all32
Native/JIT/interpreter checks. The earlier shared-storage rejection remains
historical evidence, not an accepted patch or a promise of the same speedup now.

VM source preparation is an independent unresolved dependency of the same title
family: roughly 10 ms checking, 11 ms MIR optimization, 7 ms lowering, 3 ms encoding
and 6 ms decoding/verification. String runtime work alone cannot close the original
source-run 10k gap. No warm cache or reduced workload replaces that boundary.

The current implementation remains the accepted string-view revision. All-row
parity, known baseline defects and complete Tier 1 evidence remain open.
