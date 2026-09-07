# Shared CLI acceptance mechanics

`mmay_acceptance.py` version 1 uses Python's standard library. Copy it unchanged
into each external application's `tools/` directory and record its version/hash.
Keep this compiler-repository file as the authoritative source; update consumers
deliberately. There is no runtime dependency on the language checkout or another
application, and no package registry is required.

`Application(root)` resolves `MMAY` or ordinary `mmay` from PATH. Its `engines`
method builds Debug and Release executables in a caller-owned temporary directory
and returns those commands plus VM execution with identical explicit grants.
`compare` checks exact exit/stdout/stderr for each engine. Use `prepare` to restore
mutable fixtures before each run and `verify(engine, result)` to check expected
business output and file effects after every run. A common wrong result must fail
the application check even when all engines agree. Assertions must stay enabled.

Applications own datasets, schemas, budgets, success/error stream policy, malformed
inputs, permission/symlink tests, broken-pipe driving and expected file contents.
The driver captures no environment variables or application data in metadata;
metadata includes commands, explicit capability paths and executable hashes.
Application/compiler revision and source/fixture hashes remain report obligations.

Run driver regression checks with:

```sh
python3 tools/cli-acceptance/test_driver.py
```

The focused CTest `mmay_cli_acceptance_driver` runs the same checks. They cover
raw byte/argv/cwd preservation, each kind of engine divergence, common wrong
results, fixture reset and verification per engine, empty execution sets,
disabled assertions and timeouts. Actual builds and application semantics are
verified by the external config acceptance adapters.
