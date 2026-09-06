# Local execution policy regression

D087 separates local command execution from the embedding VM's finite fuel.
`mmay run . -- loop` and `mmay test . --engine both` finish a computation with
over ten million loop iterations. Without `loop`, the program reads stdin and
strictly decodes and re-encodes a `Config` record.

`tests/c-unit/test_cli_local_execution.cmake` generates exact 1 MiB JSON input,
an over-limit input and an invalid final token. It compares VM, debug native
and release native status/stdout/stderr, with explicit process timeouts. No
large generated fixture or test-output cache is checked in.
