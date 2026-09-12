# Diagnosis of string-publication regressions

Status: **completed local diagnosis** on `65220b0c1f685ecaa1fca0be2bb196cca7740939`. Production is unchanged;
the [string-publication candidate](cp42-string-publication-2026-09-11-rejected.md)
remains rejected for Native record-create median and OFF Dijkstra P90. No
diagnostic result accepts that candidate or establishes parity or full Tier1.

[Protocols and results](cp42-string-publication-regressions-2026-09-12-summary.json), [complete raw evidence](cp42-string-publication-regressions-2026-09-12-raw.json.gz).
The raw archive retains diagnostic sources, original generated benchmark C,
commands, object/archive/executable identities, all observations, host journals,
sampled stacks, the first measurement method and unsuccessful setup attempts.
The summary provides compressed and decoded archive hashes. Rebuilding requires
the frozen runtime/compiler artifacts identified there and in the rejected stage.

## Native registration path

The diagnostic replaces only the candidate's `mmay_managed_publish_object`
wrapper with its exact parent implementation. Both variants keep the candidate
string constructor, prepare/commit API, collector and runtime archive and link
the same generated C object. The original frozen Native control and candidate
are measured alongside both relinked diagnostics. Source algorithms, arguments,
stdout and the 64MiB limit remain unchanged.

Two counter-only replays agree exactly for each variant/workload. Record-create
registers **132096 records and 1024 enums**; record-reference-copy registers
**1024 records and 1024 enums**. Both have **zero collections**. Neither title
row enters the already-allocated-object wrapper, and both also have zero GC.
This locates the extra registration volume without attributing it to collection.

The first timing script accumulated full host snapshots in memory and rewrote
its entire JSON result after each child. The result grew to 35,332,396 bytes;
the two series took 42.8s and 88.3s and small-process latencies increased. That
measurement-method confound is retained and is not acceptance evidence. A
declared correction writes complete snapshots to an append-only journal and
keeps UTC references in a compact 243,974-byte result. Two fresh diagnostic
series retain the same 3 warmups, 11 samples, balanced/rotating order, exact
binaries and original arguments. None repeats or changes the acceptance gate.

| Series | Workload | Frozen control ms | Frozen rejected candidate ms | Relinked candidate ms | Prior wrapper ms |
| --- | --- | ---: | ---: | ---: | ---: |
| 1 | record-create | 23.345 | 24.958 | 24.735 | 22.989 |
| 1 | record-reference-copy | 6.881 | 6.933 | 6.856 | 6.879 |
| 1 | title-search-10k | 7.604 | 6.436 | 6.604 | 6.474 |
| 1 | title-search-100k | 59.066 | 44.286 | 47.196 | 44.413 |
| 2 | record-create | 23.315 | 24.996 | 24.743 | 23.173 |
| 2 | record-reference-copy | 6.944 | 6.960 | 6.936 | 6.959 |
| 2 | title-search-10k | 7.724 | 6.457 | 6.516 | 6.424 |
| 2 | title-search-100k | 64.568 | 44.916 | 47.817 | 44.377 |

The prior wrapper reduces record-create medians by **7.06%** and
**6.35%** versus the relinked candidate, supporting a registration-path
intervention. This is not a unique instruction-level causal proof: title100k
also changes between the relinked binaries despite zero calls to that wrapper,
so code layout and other local effects remain relevant. Simply importing the
diagnostic's duplicate legacy algorithm is not the proposed production design;
a revision must retain one owned budget/slot contract and its failure coverage.

## OFF Dijkstra CPU tail

The rejected full series has a control near 2.22s but candidate observations
near both 2.22s and 2.68s. P90 increases **20.28%**, then **20.86%** in the single
allowed confirmation. The corresponding median increase is small; it does not
invalidate the P90 rejection.

A separate declared diagnostic runs twelve alternating original-workload pairs.
Six pairs take a one-second macOS sample at 1ms intervals of each exact owned
VM child; six do not sample. `wait4` records that VM's CPU time, faults and context
switches independently of the sampler. All24 outputs and binary/corpus hashes
match. Sampling can perturb timings; these are diagnostic observations only.

| Pair | Sampled | Control elapsed / user CPU ms | Candidate elapsed / user CPU ms |
| --- | --- | ---: | ---: |
| 1 | True | 2333.8 / 2231.7 | 2335.9 / 2233.2 |
| 2 | False | 2408.7 / 2225.5 | 2252.9 / 2214.8 |
| 3 | True | 2333.8 / 2239.2 | 2331.2 / 2242.5 |
| 4 | False | 2266.5 / 2236.6 | 2280.0 / 2243.4 |
| 5 | True | 2353.2 / 2256.0 | 2783.1 / 2698.6 |
| 6 | False | 2250.7 / 2217.1 | 2264.1 / 2224.6 |
| 7 | True | 2325.4 / 2227.4 | 2337.9 / 2233.2 |
| 8 | False | 2267.4 / 2220.8 | 2246.7 / 2212.8 |
| 9 | True | 2337.7 / 2244.2 | 2348.5 / 2245.8 |
| 10 | False | 2262.9 / 2225.5 | 2272.0 / 2228.9 |
| 11 | True | 2310.9 / 2228.8 | 2796.1 / 2695.8 |
| 12 | False | 2423.4 / 2239.7 | 2247.4 / 2210.0 |

Two sampled candidate runs use **2698.6ms** and **2695.8ms** of user CPU, versus
roughly 2230ms in ordinary runs. Extra user CPU accounts for most of their
approximately 0.47s excess; descheduling alone does not explain these cases.
Samples remain concentrated in hot array dispatch, scalar-view access and the
hot loop, with no collection/publication frame among the dominant stacks. A
one-second statistical sample does not prove the absence of any particular call.

Archived `vm_hot.c.o` and `vm_hot_strings.c.o` are byte-identical between OFF
control and candidate. Observed relative entry offsets also agree for hot array
dispatch (0x26958) and the hot loop (0x265d8). These observations do not establish
whole-process code/data/stack layout equivalence. Image addresses are retained;
ASLR, cache placement and changed dynamic instruction paths remain hypotheses,
not a diagnosed cause. The next bounded investigation counts hot-array cache
lookups/probes and executed paths before selecting a general remedy.

Both diagnostics retain three quiet preflight snapshots, sampled foreign-compute
checks and 15s interruption guards. Profiles, builds and diagnostic series run
sequentially after the rejected acceptance timing. These local probes do not
establish dedicated-host isolation, Linux LSan, physical x86 speed or full Tier1.
The original string-publication deadline remains missed. D055, title-family
closure, original all32 parity and the known F32/B0001 gaps remain open.
