# Line frequency integration workload

`frequency [--trim]` reads bounded UTF-8 lines from stdin, counts them in an
insertion-ordered `Map<string, int>`, and emits one compact JSON object.
It composes the ordinary CLI and JSON packages with text, collections, and
stdin/stdout/stderr. Limits are 10,000 lines, 65,536 bytes per line and the
standard JSON output limit. Failure publishes diagnostics on stderr.

`mmay_cli_frequency` in CTest compares VM/native status and exact output for
empty input, Unicode, CRLF, trimming, unknown options and oversized lines.
