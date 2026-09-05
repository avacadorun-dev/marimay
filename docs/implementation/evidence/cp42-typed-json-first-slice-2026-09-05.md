# CP42 typed JSON initial local observation

Scope: local, incomplete. Host: macOS arm64, AppleClang 21, portable C17.
Base: `19cfe17d4715c1bb958a11bee8532819ea050cce`; working branch
`feature/typed-json-ergonomics`. This report records the initial working-tree
slice, not a pushed or release-verified artifact.

Observed commands:

```sh
cmake --preset debug
cmake --build --preset debug -j 8
build/debug/mmay fmt tests/public-alpha/typed-json
build/debug/mmay check tests/public-alpha/typed-json
build/debug/mmay test tests/public-alpha/typed-json --engine both
ctest --test-dir build/debug -R '^mmay_typed-json_unit$' --output-on-failure
./tools/check-architecture
./tools/check-c-memory-policy
```

Build/check succeeded; all four real-source tests passed in both engines.
They cover notes record/array round-trip, strict malformed/missing/unknown/
duplicate/null input rejection, generic records, scalar boundaries and limits.
The focused checker suite passed in 91.64 seconds, including unsupported-type
diagnostics and every deterministic allocation-failure index of its isolated
cursor-signature fixture. Teardown checks both allocation and byte counters.
The fixture does not substitute for real JSON runtime behavior. Architecture
and C memory-policy checks passed.

The first standalone run exposed a mistake in the test entry signature:
`Result<void,string>` does not satisfy the entry error message contract.
That fixture is being corrected to an int-status wrapper around the ordinary
typed transform. Standalone native output/error parity, sanitizer runs, full
Tier 1 coverage, structured JSON error paths, completion metadata, and the
remaining four ergonomics items are not evidenced by this observation.
