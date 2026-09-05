# Void-valued match regression

D067 arm expressions may call a void function. They still reach a join block,
unlike return/panic/never arms, but need no result register. Tests cover boolean,
integer, string, enum, tuple and record scrutinees, guards, explicit block tails,
and return from the enclosing function. A block without an explicit tail stays
invalid under the existing contract.

CTest runs `mmay_match_void` in both engines and `mmay_format_match-void` checks
canonical syntax. `test_checker_expression_statements.c` covers mixed void/value
arms, missing tails, ignored Result diagnostics, and deterministic allocation
failure with full teardown accounting.
