#ifndef MMAY_CLI_TEST_BYTECODE_H
#define MMAY_CLI_TEST_BYTECODE_H

#include "mmay/bytecode/mir_lower.h"

/*
 * Prepares the complete immutable function set once per CLI test invocation.
 * MIR is borrowed only during this call. NULL output or failure_reason returns
 * false without mutation. Output must be zero-initialized or an owned program;
 * its old value is disposed even on failure. Success owns
 * decoded, verified bytecode; failure leaves output empty. The caller disposes
 * it with mmay_bytecode_program_dispose after all per-test executions end.
 */
bool mmay_cli_test_bytecode_prepare(
    const MarimayMirFunction *const *functions,
    size_t function_count,
    MarimaySymbolId entry,
    MarimayAllocator allocator,
    MarimayBytecodeProgram *output,
    const char **failure_reason
);

/*
 * Changes only the entry index, then verifies the complete program. Failure
 * restores the previous index. Bodies must not be mutated between tests.
 * This retains no execution, heap, argument, panic, stream or capability state.
 * Verification can allocate through the program's allocator.
 */
bool mmay_cli_test_bytecode_select(
    MarimayBytecodeProgram *program,
    MarimaySymbolId entry,
    const char **failure_reason
);

#endif
