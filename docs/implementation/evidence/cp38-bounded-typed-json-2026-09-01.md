# Checkpoint 38: bounded typed JSON

Date: 2026-09-02 (the implementation and CI sequence began on 2026-09-01).

Status: verified pushed-CI checkpoint evidence.

Verified subject: commit
`4d1b9a9c55444a85ed2bbc65ea40bcb21db9efd3`, including the CP38
implementation, sanitizer repairs, and dedicated sustained JSON fuzz job.
Gitea Actions
[`#102`, run 141](http://ci.example:3000/Marimay/marimay/actions/runs/141)
passed all 11 jobs. No tag, publication, external identity operation, parent
product-gate claim, or complete Tier 1 claim is authorized by this report.

## Authority and delivered contract

Accepted
[D082](../../design/decisions.md#d082-explicit-bounded-typed-json-codecs)
and the normative
[`std/json` specification](../../spec/standard-library.md#11-json-and-serialization)
define this boundary:

- `std/json` is an ordinary first-party source package with stateful
  `Decoder` and `Encoder` cursors, typed errors, half-open byte spans, and
  explicit `Limits`;
- application packages own ordinary nominal codec values plus top-level
  `decode` and `encode` functions; there is no derive syntax, reflection,
  dynamic `any` tree, or host JSON library;
- private fields remain visible only to their owning package;
- strict UTF-8, JSON number grammar, signed 64-bit integer range, duplicate
  rejection, surrogate validation, unknown-field policy, and exact resource
  budgets apply before values are published;
- deterministic compact output follows codec call order and D079 `Map`
  insertion order; it is not a general canonicalization scheme;
- managed-heap or allocator exhaustion remains `I0002`, not a fabricated JSON
  codec error.

The defaults are 1 MiB input and output, 256 KiB strings, 128-byte numbers,
128 nested containers, and 65,536 members per container. JSON itself receives
no filesystem, stream, environment, or network capability.

## Implementation and ownership

`std/json/json.my` owns the public model, limits, errors, and messages.
`std/json/decode.my` owns strict cursor decoding, duplicate tracking, skipping,
and typed primitive reads. `std/json/encode.my` owns the poisoned-state encoder
and deterministic compact form. `cli_standard_json_package.c` embeds exactly
those three ordinary source documents; the compiler has no JSON-specific MIR
or bytecode operation.

`tests/public-alpha/cp38-json/model/api.my` owns explicit codecs for records,
enums, arrays, insertion-ordered maps, strings, integers, and private state.
The runnable transformer composes `std/fs`, `std/bytes`, `std/json`, and
`std/io`, increments the catalog version, adds one label, changes enum state,
and emits this exact compact output:

```json
{"version":2,"items":[{"id":7,"name":"Mara 🌿","tags":["alpha","json"],"labels":{"priority":2,"attempts":1},"status":{"kind":"paused","reason":"review"}}]}
```

`test_cp38_json.c` exhausts every deterministic allocation-failure index in
the three-document embedding boundary and requires zero live allocations and
bytes after teardown. `test_cp38_json.cmake` owns the public format/check/test,
VM/native differential, capability denial, hostile-input, nesting, integer,
Unicode, duplicate, trailing-input, and managed-heap-limit process contract.
`json_fuzz.c` compiles an ordinary `std/json` cursor workload to verified
bytecode and executes arbitrary inputs through the VM.

## Stabilization history

The first pushed implementation, `e1e0e75`, exposed a heap-use-after-free in
recursive imported nominal layout closure. A recursive call could grow a MIR
record or enum vector while its prior element pointer remained live. Commit
`3aeb851` snapshots field or payload counts and reloads vector elements after
each possible growth point.

The repaired sanitizer run then showed that a healthy CP38 end-to-end test
used 1185.54 seconds under ASan, leaving only 14.46 seconds below the former
1200-second test timeout; TSan reached that limit without a sanitizer report.
Commit `87ff784` raised only this test's timeout to 3600 seconds without
removing coverage. Run 140 passed its ten configured jobs, but audit found the
new JSON fuzzer was not yet in the sustained CI matrix. Commit `4d1b9a9` added
that fourth fuzz job, and run 141 supplied the complete evidence below.

## Validation

The 11-job workflow completed in 1h42m11s. Every run-141 job succeeded:

| Job | Result |
|---|---:|
| Documentation and architecture invariants | passed in 44 s |
| Linux x86_64 Clang Debug | passed in 26m36s |
| Linux x86_64 Clang Release | passed in 15m17s |
| Linux x86_64 GCC Debug | passed in 30m25s |
| Linux x86_64 GCC Release | passed in 21m15s |
| Linux x86_64 ASan+UBSan+LSan | passed in 38m40s |
| Linux x86_64 fuzz / bytecode | passed in 11m56s |
| Linux x86_64 fuzz / formatter | passed in 11m59s |
| Linux x86_64 fuzz / frontend | passed in 11m43s |
| Linux x86_64 fuzz / JSON | passed in 11m44s |
| Linux x86_64 TSan | passed in 47m21s |

ASan+UBSan+LSan passed 42/42 tests in 2178.84 seconds; its focused
`mmay_cp38_json` test passed in 1175.63 seconds. TSan passed 42/42 tests in
2764.53 seconds; the same focused test passed in 1703.04 seconds. Neither job
reported a sanitizer, leak, race, timeout, or allocation-counter failure.

The four 601-second libFuzzer campaigns completed without a crash, timeout,
OOM, sanitizer report, or preserved failure artifact:

| Target | Executions | exec/s | Final coverage/features | Corpus | New units | Peak RSS |
|---|---:|---:|---:|---:|---:|---:|
| bytecode | 25,652,082 | 42,682 | 6,572 / 19,094 | 1,866 / 115 KiB | 7,346 | 209 MiB |
| formatter | 1,756,548 | 2,922 | 4,995 / 26,054 | 3,018 / 247 KiB | 18,252 | 175 MiB |
| frontend | 1,326,815 | 2,207 | 2,992 / 16,604 | 2,368 / 288 KiB | 17,180 | 164 MiB |
| JSON | 30,904 | 51 | 7,375 / 22,086 | 441 / 13,472 B | 574 | 179 MiB |

The focused public package reports 8/8 tests under both VM and native engines.
Its exact checks cover missing and unknown fields, reject/skip policy,
duplicate keys, malformed escapes and surrogates, number and byte ranges,
nesting and item budgets, deterministic record/enum/map output, private-field
ownership, round trips, capability denial, and managed-heap exhaustion.

## Validation scope and remaining limitations

This evidence verifies the bounded CP38 checkpoint on the repository's Linux
x86_64 GCC/Clang, sanitizer, and sustained-fuzz CI surface. It does not close
the complete Tier 1 platform matrix or the parent I2, I3, I4, I5, or CLI0
gates. Network-body streaming remains I6/I9; schema registries, reflection,
derive syntax, an unbounded DOM, and dynamic `any` remain absent rather than
stubbed.

CP38 is therefore `verified` with `pushed-ci` evidence. CP41 becomes the sole
`ready` checkpoint before CP39 and CP40. N0 and the later platform, install,
compatibility, and release-audit gates continue to block publication.
