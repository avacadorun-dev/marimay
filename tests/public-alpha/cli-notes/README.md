# Local notes integration workload

An ordinary multi-file application composing `std/cli`, `std/json`, typed
records, arrays, sets, filesystem capabilities, and standard streams.
The module name intentionally matches the external `marimay-notes` repository
so the same source can be exercised outside the compiler tree.

`mmay_cli_notes` in CTest runs add/list/show/remove across fresh VM and native
processes and compares persistent JSON bytes. It rejects malformed databases,
unknown versions, invalid counters and duplicate note IDs, and checks that
failed operations do not overwrite stored data. `mmay test --engine both`
exercises the model and codec independently of host filesystem state.
Process tests also compare errors for denied read/write capabilities, a missing
parent directory, and a directory used as the database path. Capability-denied
operations preserve the existing JSON bytes.

Example from the repository root (the data directory must already exist):

```sh
build/debug/mmay run tests/public-alpha/cli-notes --allow-read build --allow-write build -- --db build/notes.json add "Read docs" --tag work
```

The application deliberately uses the existing bounded JSON contract D082 and
atomic filesystem replacement D077. It does not promise concurrent writers,
locking, automatic backups, timestamps or databases larger than 1 MiB.
