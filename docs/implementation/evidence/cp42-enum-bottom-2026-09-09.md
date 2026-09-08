# CP42 direct-bottom enum transport

Scope: local macOS arm64 implementation evidence, 2026-09-09, parent `af08f37`.
[Raw commands, outputs and source hashes](cp42-enum-bottom-2026-09-09.json)
identify the candidate. D102 completes the direct-bottom transport gap recorded
in D100; it does not close CP42 or certify another host's execution.

`Result<T, never>` retains an impossible Err payload in typed metadata. A mapper
factory still evaluates on either input variant; its never-returning callback
runs only on Err. `Result<never, E>`, `Option<never>` and positional/named generic
user enums use the same representation. Values can cross generic functions,
closures and array storage. Matching excludes direct-never variants and rejects
explicit patterns for them with T0014. An empty match on an enum whose every
variant is directly impossible diverges. Postfix `?` preserves exact E and
never creates an impossible error/success value. Void remains distinct.

The checker owns classification and impossible CFG edges. MIR/wire keep exact
type metadata and variant ordinals; no new opcode or wire field is introduced.
The existing NEVER bytecode tag is allowed in enum payload metadata with no
layout id, while value registers and constructor arguments remain invalid.
Native/VM descriptor placeholders are not source values or references. The
native runtime and public .myb contract do not change. Source parameter type
metadata of never remains expressible in MIR; a forged use of that parameter
to construct a never payload is explicitly rejected.

Validation includes five source tests with `--engine both`, 30 recorded process
observations (including check/run/build negative equivalence), native Debug and
Release, exact mapper-factory/callback/defer/panic order, nested Option, void,
array aliases, generic transport and unused all-bottom functions. Focused C tests
sweep every reachable checker allocation-failure index on the positive program
and seven rejection forms. The common callable harness additionally exercises
MIR oracle/VM, wire encode/decode, native emission, forced collection and runtime
allocation failures, asserting both allocator live counters after teardown.
Hostile MIR/bytecode checks reject an impossible construction, a never value
register, void payload substitution and a forged layout id.

The full Release invocation ran 83 tests: 81 initially passed. Two failures were
resolved without weakening product behavior: the malformed-MIR test now expects
early INVALID_REGISTER rejection, and new source files were staged so the
tracked-source packager included them. The subsequent checker/runtime/packaging
recheck passed 3/3. Thus all 83 tests have passing results across these invocations;
this is not a claim of a single 83/83 run. The full native shard passed in 248.88 s;
an earlier focused native invocation also passed in 234.79 s. The final expanded enum-bottom owner was also run directly through the same
MIR/VM/wire/native allocation/GC harness; its runner and result are retained.
Final focused ASan/UBSan checker and process results are retained in the raw packet. Linux
LeakSanitizer and all other required Tier 1 hosts remain CI/host obligations.

This is the direct-bottom rule, not general recursive inhabitation inference,
value-dependent unreachable-code analysis or an extension to first-class
functions with never parameters. Source-lint/delimited, nonfatal mutable-capture
warnings, startup budgets and the complete CP42 → CP41 → CP39 → CP40/N0 route
remain open. No benchmark budget, platform requirement or release gate is relaxed.
The Vladimirov book was not used in this change; no page attribution is claimed.
