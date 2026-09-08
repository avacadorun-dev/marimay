# CP42 array-search CMake policy correction

Scope: test harness portability, 2026-09-08, parent `2fa6693`.
[Reproducer and outputs](cp42-array-search-cmake-policy-2026-09-08.json) retain
commands, statuses, source/tool hashes and the bounded CI observation.

Run 220 at `271604a` failed `mmay_cp42_array_search` in ASan/UBSan/LSan and
Clang Debug/Release. The script had no explicit policy version. Under CMake
3.25's unset CMP0054 policy, its quoted `"panic"` comparison operand was
reinterpreted as the variable of the same name. The panic-mode expected error
therefore remained G4005, while the program correctly produced the expected
cleanup followed by G4000 `predicate panic`. This was an expectation-selection
failure, not evidence of a sanitizer finding or a runtime semantic defect.

The script now declares the repository's existing CMake 3.25 minimum, like the
CP39 distribution/packaging scripts. This selects CMP0054 NEW for this standalone
`cmake -P` invocation; the root project's policies do not automatically apply
there. No scenario, expected panic, engine, budget or sanitizer was removed.

Verification uses an isolated CMake 3.25.2 wheel extracted under ignored build
state, without replacing the host CMake. The original script fails under 3.25.2
with the same CMP0054 warning and actual G4000; the corrected complete script
passes under both 3.25.2 and local 4.4.2. Each passing script checks formatting,
checking, ten source tests with both engines, then normal execution and all nine
mutation/panic modes in VM/native Debug/Release. Compiler/runtime code is
unchanged from the preceding file/context wave. Required architecture and
roadmap gates are run before committing this correction.

Read-only CI inspection found run 221 (`94d1e85`) and run 222 (`2fa6693`) still
queued. Their statuses do not certify this correction; its exact pushed
revision needs its own CI result. No job was restarted or cancelled. CP42 and
all remaining CLI0 obligations retain their existing states.
