# Configuration migration integration workload

`config-migrate INPUT.json OUTPUT.json` strictly decodes a version-1 catalog,
including records, algebraic enums, arrays and insertion-ordered maps, then
atomically writes version 2. Its package-owned codec derives from the CP38
typed JSON example. Unknown fields and unsupported versions are rejected.

`mmay_cli_config_migrate` in CTest checks VM/native output and verifies that
invalid input cannot replace an existing destination file.
