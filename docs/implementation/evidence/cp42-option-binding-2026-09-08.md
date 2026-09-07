# CP42: scoped Option bindings

D096 is implemented and locally checked on macOS arm64. Headers now accept
`if const value = option`, `if let value = option`, and the corresponding
`while` forms. The initializer must be the exact prelude Option family;
Some binds its payload in the successful body, and None selects else/exit.
There is no Result conversion, truthiness, annotation, destructuring or
binding chain. D072 hard keywords remain unchanged.

The focused checker owner emits existing enum test, guarded payload and CFG
operations. No MIR instruction, bytecode version, runtime representation or
backend semantics changed. The CFG owner retains branch/loop edges and
definite initialization. Successful bodies own their binding and defer scope;
initializers, else branches and following statements cannot see that binding.
Parser recovery explicitly keeps malformed annotations/chains in the header;
negative tests caught and prevented loss of a following function declaration.

## Evidence

| Check | Observed result |
|---|---|
| Full local Release CTest suite after production changes | 76/76 passed, 264.08 s. |
| Final focused Release frontend/checker/public-command tests | 3/3 passed after test-only additions for the local-binding limit and generic/falsy payloads. |
| Final focused ASan/UBSan frontend/checker/public-command tests | 3/3 passed on macOS; this is not Linux LeakSanitizer or a full sanitizer matrix. |
| Public Option fixture | 9 source tests with `--engine both`; standalone VM and native Debug/Release success and exact panic/cleanup output. |
| Notes consumer migration | 14 source tests; 27 process cases × three engines and 15 closed-pipe checks passed. |
| Config consumer migration | 7 source tests; 32 process cases × three engines plus atomic/denied/broken-pipe checks and 23 real filesystem cases × three engines passed. |

The public fixture checks Some/None, nested Option, shadowing, mutable and
immutable bindings, reference identity, array aliases, repeated initializers,
zero iterations, continue/break/return, defer cleanup, Result propagation,
retained per-iteration values, contextual mapError use and unchanged exhaustive
match. Generic instantiations preserve zero, false, empty string and record
identity. Panic cleanup emits exactly `option cleanup 7` before the unchanged
G4000 panic on all three execution modes.

Focused C tests reject Result/lookalike enums, void payload bindings, duplicate
names, illegal assignment, initializer/else/post-statement scope leakage and
uninitialized values after None/zero iterations. Parser tests include malformed
RHS, annotations, discard, compound binding and binding chains, losslessness,
following-declaration retention and event limits. Valid, rejected and local-
limit checker paths and each parser fixture run through all reachable
deterministic allocator failure indices; teardown checks both live counters.
Formatter input/expected fixtures and the all-productions corpus include the
new syntax; generated syntax/grammar registries were updated.

The [raw packet](cp42-option-binding-2026-09-08.json) links exact source hashes,
compiler hashes, local logs, application snapshots and process reports. The
full 76-test run preceded only the final generic/local-limit test additions;
those additions passed the final focused runs. Compiler production code was
unchanged between these checks. Application elapsed times are correctness-run
durations under concurrent local verification, not latency/performance evidence.

## Consumer changes and remaining boundaries

Notes binds optional tag occurrences inside its collection loop. Config binds
the optional output filename in its file-writing branch; the absent path still
writes and flushes stdout. Their output, status and persistence expectations
are unchanged. Both external repositories retain their preexisting uncommitted
work; the packet captures the exact tested sources without automatically
committing unrelated changes there.

The [shared contract packet](../../research/cli0-collection-contracts-2026-09-08.md)
continues to govern design preparation for copy/findIndex/removeAt and the
notes mutation model. During this work a stored arbitrary closure remained
unsupported with I0001; the successful contextual mapError case does not prove
a general closure ABI. Predicate search must address that existing dependency
explicitly. Array operations, functional notes removal, notes search and all
ten application acceptances remain open.

No current Tier 1-complete or pushed-candidate sanitizer result is claimed here.
Historical Linux native/toolchain/allocation failures remain unresolved and
are not repaired by this frontend feature. CP42, CP41, install and N0 remain
open. No new idea from the Vladimirov book was used for this change.
