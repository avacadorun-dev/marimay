# Title-search phase diagnosis, 2026-09-11

Status: diagnostic evidence on accepted implementation `51e0be3812410f2a3a3df89089a3a7770043ad1d`, documentation parent `d7f48b65550adebcf6e00974f14a99fe8926df37`. These instrumented observations are not candidate acceptance times and do not replace the five-way comparison.

Five fresh timer runs per engine and original workload, plus two independent counter runs each. All 28 runs returned the original stdout under the unchanged 67108864-byte limit; all 114 corpus hashes match. Every run observed setup once, 129 query-overhead intervals, 128 scan intervals and one post-search interval. Counters match exactly between their two runs.

| Workload | Engine | Process median ms | Data setup ms | Query overhead ms | Search ms | Cleanup/post-search ms |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| title-search-10k | native | 9.700 | 2.780 | 0.038 | 3.213 | 0.759 |
| title-search-10k | vm | 94.135 | 4.790 | 0.091 | 40.866 | 1.409 |
| title-search-100k | native | 75.345 | 27.616 | 0.064 | 36.562 | 6.997 |
| title-search-100k | vm | 516.176 | 47.583 | 0.239 | 413.298 | 9.931 |

Phase medians are computed independently and need not add to the process median. Native post-search includes heap teardown; VM has separately marked execution/CLI teardown. Unclocked loader/exit overhead remains in parent process elapsed. No profiler or counter increments occur inside timed per-comparison code; coarse VM dispatch marker checks and emitted boundary calls still perturb execution, so these are diagnosis only.

## Exact search counts

| Workload | Comparisons | Native checked byte resolutions | VM checked byte resolutions | Managed allocations in search | GC in entire run |
| --- | ---: | ---: | ---: | ---: | ---: |
| title-search-10k | 804208 | 804208 | 2412624 | 0 | 0 |
| title-search-100k | 7994208 | 7994208 | 23982624 | 0 | 0 |

VM resolves one string at array-cell conversion and both operands again for equality. The same operations currently leave machine code. Native resolves the selected child once per comparison, even though its typed rooted parent already proves child validity. The shared target is array-to-string projection and retained immutable byte access, preserving checks at the VM boundary.

VM setup for 100k creates 400016 strings versus Native300016; repeated search itself creates none. The title10k source pipeline separately costs approximately40ms before VM admission/JIT preparation (another3.7ms). Search-only acceleration cannot establish end-to-end title10k parity. Data creation and source preparation remain dependencies of the same active family.

## Retained diagnostic corrections

The initial instrumented VM aborted because its function symbol was selected from a different source-path/cwd form. Two explicit marker probes and regenerated C identify the original benchmark-command symbol. Complete generated function bodies match after normalizing symbol IDs and immediate-enum registration IDs; all block labels and operations match. Final Native and VM copies use the original command symbol. The initial failed observations are retained and are not merged with final evidence.

Before any accepted diagnostic observations, disabled counters were made inline no-ops and the VM equality counter was added. The initial build and initial marker failure remain archived. Native first-launch loader delay is visible in raw elapsed observations and is not relabeled as workload time. All five final observations are retained without filtering.

The next predeclared candidate is `build/jit-string-views/SMART.json`; its early acceptance minima are pair geometric latency reductions of15% Native and20% VM, at least5% improvement per title row, followed by all32 checks and the declared5% confirmation policy. No new speed claim follows from this diagnosis alone.
