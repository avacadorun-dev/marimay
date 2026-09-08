# D101: shared file lines and three streaming CLI consumers

Scope: local macOS arm64, 2026-09-08. CP42 remains `in-progress`; no release,
Tier 1-complete or pushed-CI claim. Parent compiler commit is `c394636`.
[Machine-readable evidence](cp42-file-lines-2026-09-08.json) retains exact source
hashes, application snapshots/revisions, commands, logs and measurement inputs.

## Result and ownership

Accepted D101 adds `std/fs.readLine(File, int): Result<Option<string>, fs.Error>`
with the existing stdin LF/CRLF, lone-CR, NUL, UTF-8 and final-line rules. The
0..1048576-byte limit applies to each returned line, independently of file size.
Invalid UTF-8 offsets are line-relative. No failed line publishes a prefix.
Existing D077 errors, authorization, symlink and close semantics remain intact.

`line_reader.c` shares the byte parser with stdin and grows storage with the
actual line. `fs_handles.c` owns authorized handle slots, lazy 4 KiB buffering,
shared cursor/EOF and teardown. File aliases and mixed read/readLine calls do
not lose prefetched bytes. Path/capability checks remain in `fs.c`. These are
independent C17 owners, not include shards. The append-only wire opcode is 163;
MIR and hostile bytecode validate the nested Result/Option/string layout.

Two existing engine defects were exposed and corrected:

- MIR `closeBestEffort` looked up a Result enum for its `void` destination.
  It now handles cleanup before enum lookup, consistently with VM/native.
- Frequency's ordinary JSON encoding inside a Map loop repeated the first
  record in native. The iterator itself produced the correct pairs. A cached
  record-field pointer survived register redefinition. Native field reads now
  compare the current managed reference before reuse; child-array call hints
  refresh for unstable parent definitions. An immutable field belongs to an
  object, not to a register across iterations. JSON remains ordinary typed MIR.

The original reduced source and old/fixed native artifacts are retained under
`build/d101-json-loop*`; the evidence JSON records their outputs and hashes.
No application workaround substitutes for this native repair.

## Verification

- Full Release: **82/82**, 328.58 s. An earlier 81/82 run failed source packaging
  because new source files were not yet in Git's index; the final full run
  includes them and passes the offline archive configuration.
- Full macOS ASan/UBSan: **81/81**, 823.92 s. A later test-only strengthening
  asserts that hostile nested-layout mutations actually ran; CP34 was repeated
  in Release and ASan/UBSan. No production change followed these full runs.
- Runtime allocation sweeps cover stdin/file empty, CRLF/NUL, lone CR, UTF-8
  across a 4 KiB boundary, 1 MiB lines, limits, mixed reads, close/reopen, host
  open/read/close failure and malformed transfers. Every completed teardown
  checks both live allocation and byte counts.
- File-line MIR/VM/native tests sweep lowering, codec, emission and execution
  allocation indices; VM and native force collection before managed allocations.
  Success, EOF, invalid limit, closed handle, invalid UTF-8 and read failure are
  distinguished. Native view lifetime tests cover loop construction/rebinding,
  records selected from arrays, child arrays and direct calls with the same
  allocation/GC harness. A typed-JSON regression verifies three distinct rows.
- Public `mmay_cp42_file_lines`: **101 exact process checks plus 3 closed pipes**,
  VM/native Debug/Release, including denied paths, symlinks, Unicode filenames,
  read/readLine aliases, 1 MiB lines, and 2,099,200-byte streams under a 16 MiB
  managed-heap limit. Closed pipes may fail at write or flush; both are typed
  errors, not signal termination.
- Architecture, C-memory policy, roadmap, generated artifacts, spec terms,
  example syntax, syntax coverage, release contract and document links pass.
- Three new frontend/formatter/bytecode seeds replay successfully with the
  sanitizer fuzz build. This is fixed-input replay, **not a new fuzz campaign**.

These sanitizer and allocator results are bounded evidence, not an unqualified
claim that all leaks are absent. Linux LSan/TSan and other Tier 1 results for
this revision require CI/platform runs.

## Independent consumers

All three are real independent Git repositories under
`/workspace/Projects/Marimay/Marimay-Examples`, without remotes. Each has
Russian README, `mmay.mod`, business logic in Marimay, synthetic fixtures,
source tests, the unchanged acceptance driver v1, its own adapter, budgets,
measurement script and findings. Existing Notes/config work was not recommitted
or copied; the portfolio navigation was reconciled with the accepted waves.

| Repository | App commit | Public behavior | Local verification |
|---|---|---|---|
| marimay-grep | `4a21fec` | Literal case-sensitive Unicode search, file/line prefixes; 0 match, 1 no match, 2 error | 2 source tests both; 21 process scenarios; 3 closed pipes |
| marimay-frequency | `c04ed62` | First-seen uniq or JSON Lines count, bounded Set/Map keys | 3 source tests both; 26 process scenarios; 3 closed pipes |
| marimay-logstats | `a206f74` | Strict LEVEL TAB EVENT, stable levels/events JSON, malformed-line rejection before output | 2 source tests both; 26 process scenarios; 3 closed pipes |

Process scenarios compare exact status/stdout/stderr across VM and native
Debug/Release; denied builds compare VM and native Release. They include help
and usage failures, CRLF/UTF-8 chunk boundaries, empty/final/NUL lines, configured
limits, missing/denied/symlink I/O and immutable input files. Counter apps also
verify 4096 distinct keys. Source tests cover updates at capacity, aliases,
remove/reinsert order, duplicate Set operations and failure-atomic counters.
They do not replace the entire CP42 combined coverage matrix.

## Measurements and remaining startup failure

Budgets were recorded in each app before timing. Source/test/build work was
completed before these sequential measurements; normal desktop background
applications remained running. No OS cache eviction or fully idle-host claim.
The first native sample follows build directly, without executing the artifact
to warm it. Larger runs use files, 32 MiB managed heaps and fixed repeated keys.

| Workload | grep | frequency | logstats |
|---|---:|---:|---:|
| Native 2 MiB, median of 3 | 30.35 ms | 31.23 ms | 71.94 ms |
| VM 2 MiB, median of 3 | 163.17 ms | 270.29 ms | 420.53 ms |
| Native 20 MiB, one sample | 267.93 ms | 269.86 ms | 662.53 ms |
| VM 20 MiB, one sample | 605.80 ms | 934.07 ms | 2051.80 ms |
| Native peak RSS, 20 MiB | 37.19 MiB | 37.14 MiB | 37.36 MiB |
| VM peak RSS, 20 MiB | 53.61 MiB | 50.19 MiB | 59.48 MiB |
| First native small invocation | **374.87 ms** | **385.38 ms** | **378.37 ms** |

Raw samples in JSON are authoritative over rounded values. The native 2 MiB
budget is 2 s, VM 20 s; RSS bounds are 128/256 MiB. The 20 MiB continuation has
20/200 s bounds with the same RSS limits. These pass; repeated both-test cycles
are 304/339/366 ms under the 2 s budgets. These duplicate-heavy workloads do not
establish performance for every key distribution or replace CP41 profiling.

**First native startup still fails the unchanged 100 ms budget.** Subsequent
small invocations take approximately 6–11 ms. Three fresh ordinary C17 controls,
with a timestamp at the first instruction in main, spend 370.68–512.27 ms before
main and less than 1 ms afterward. This independently reproduces launch latency
without Marimay runtime/business code; it does not identify an OS subsystem or
prove AMFI causation. No security setting, pre-execution warmup, workload or
budget was changed. Startup acceptance and release remain blocked by this
known issue; independent preparation is allowed by the accepted route.

## Remaining work and publication

CP42 still needs the other five applications, the full combined coverage and
claimed-platform acceptance, the escaping-mutable-capture warning, and the
uninhabited enum payload removal gate. A newly recorded checker defect also
needs a normal diagnostic: binding `Ok(value)` of `Result<void,E>` currently
returns status 70 without T0002; the valid `Ok(_)` spelling works. The minimal
reproducer and expected named-void rule are retained in app findings/evidence.
No unsupported behavior was declared normative to hide these limitations.

D100 commit `c394636` was not pushed: the Mac at ci.example could not reach
Gitea ci.example or Proxmox ci.example through its ci.example gateway.
The bounded infra diagnosis found a network-path failure and could not inspect
VM/service state. A pending user question asks about VPN or another Gitea
address. D101 publication is retried after the local gates; this local report
cannot attest that the remote runners saw the revision.

No ideas from Vladimirov's book were used in this change; no page attribution
is claimed. Runtime stream contracts and reproduced engine behavior drove it.
