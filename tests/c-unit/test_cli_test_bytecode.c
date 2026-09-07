#include "test_support.h"

#include <string.h>
#include <stdio.h>
#include "mmay/platform/directory.h"

#include "cli_test_bytecode.h"
#include "mmay/bytecode/vm.h"
#include "mmay/cli.h"
#include "mmay/session.h"

static bool mmay_test_prepare_leaf(
    MarimayMirFunction *function, unsigned char id, int64_t value,
    MarimayAllocator allocator
) {
    MarimayMirRegister result = MMAY_MIR_NO_REGISTER;
    MarimayMirBlockId block = MMAY_MIR_NO_BLOCK;
    MarimayMirInstruction instruction = {0};
    MarimayMirTerminator terminator = {0};
    if (!mmay_mir_function_init(
            function, allocator, (MarimaySymbolId){{id}},
            NULL, 0, MMAY_TYPE_I64, mmay_mir_function_default_options()
        ) || !mmay_mir_add_register(function, MMAY_TYPE_I64, &result)
        || !mmay_mir_add_block(function, &block)) return false;
    instruction.kind = MMAY_MIR_CONSTANT_I64;
    instruction.target = result;
    instruction.integer = value;
    instruction.span = (MarimayMirSpan){10, 11};
    terminator.kind = MMAY_MIR_RETURN;
    terminator.value = result;
    terminator.span = (MarimayMirSpan){11, 12};
    return mmay_mir_append_instruction(function, block, &instruction)
        && mmay_mir_set_terminator(function, block, terminator);
}

static void mmay_test_prepared_entries(
    const MarimayMirFunction *const *functions
) {
    MarimayFailAllocator state;
    MarimayAllocator allocator;
    MarimayBytecodeProgram program = {0};
    const char *reason = NULL;
    size_t index;
    mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
    allocator = mmay_fail_allocator(&state);
    MMAY_EXPECT(mmay_cli_test_bytecode_prepare(
        functions, 2, functions[0]->symbol, allocator, &program, &reason
    ));
    MMAY_EXPECT(program.functions.length == 2 && program.entry_function == 1);
    for (index = 0; index < 4; index += 1) {
        MarimayBytecodeProgram reference = {0};
        MarimayOwnedBytes reused = {0};
        MarimayOwnedBytes independent = {0};
        MarimayBytecodeValue result = {0};
        const MarimayMirFunction *selected = functions[index % 2];
        const void *bodies = program.functions.data;
        MMAY_EXPECT(mmay_cli_test_bytecode_select(
            &program, selected->symbol, &reason
        ));
        MMAY_EXPECT(program.functions.data == bodies);
        MMAY_EXPECT(mmay_bytecode_run_program(
            &program, NULL, 0, allocator,
            mmay_bytecode_run_default_options(), &result
        ) == MMAY_BYTECODE_RUN_OK);
        MMAY_EXPECT(result.type == MMAY_BYTECODE_TYPE_I64
            && result.as.i64 == (index % 2 == 0 ? 43 : 42));
        MMAY_EXPECT(mmay_cli_test_bytecode_prepare(
            functions, 2, selected->symbol, allocator, &reference, &reason
        ));
        MMAY_EXPECT(mmay_bytecode_encode_program(&program, allocator, &reused)
            == MMAY_BYTECODE_CODEC_OK);
        MMAY_EXPECT(mmay_bytecode_encode_program(&reference, allocator, &independent)
            == MMAY_BYTECODE_CODEC_OK);
        MMAY_EXPECT(reused.length == independent.length
            && reused.length != 0
            && memcmp(reused.data, independent.data, reused.length) == 0);
        mmay_owned_bytes_dispose(&independent);
        mmay_owned_bytes_dispose(&reused);
        mmay_bytecode_program_dispose(&reference);
    }
    {
        uint32_t entry = program.entry_function;
        MMAY_EXPECT(!mmay_cli_test_bytecode_select(
            &program, (MarimaySymbolId){{99}}, &reason
        ));
        MMAY_EXPECT(program.entry_function == entry && reason != NULL);
        /* Selection must not turn a damaged body into trusted bytecode. */
        MarimayBytecodeProgramFunction *function = mmay_vector_at(&program.functions, 0);
        if (function != NULL) {
            MarimayBytecodeType saved = function->body.result_type;
            function->body.result_type = MMAY_BYTECODE_TYPE_BOOL;
            MMAY_EXPECT(!mmay_cli_test_bytecode_select(
                &program, functions[0]->symbol, &reason
            ));
            MMAY_EXPECT(program.entry_function == entry);
            function->body.result_type = saved;
        } else MMAY_EXPECT(false);
        MMAY_EXPECT(mmay_cli_test_bytecode_select(
            &program, functions[0]->symbol, &reason
        ));
    }
    /* Repreparation failure releases an already owned program as documented. */
    MMAY_EXPECT(!mmay_cli_test_bytecode_prepare(
        functions, 2, (MarimaySymbolId){{99}}, allocator, &program, &reason
    ));
    MMAY_EXPECT(!program.initialized && program.functions.data == NULL);
    mmay_bytecode_program_dispose(&program);
    mmay_bytecode_program_dispose(&program);
    MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
}

static void mmay_test_preparation_failures(
    const MarimayMirFunction *const *functions
) {
    size_t failure_index = 0;
    size_t attempts = 0;
    bool reached_success = false;
    /* Include every allocation in lowering, encoding and verified decoding. */
    for (failure_index = 0; failure_index < 10000; failure_index += 1) {
        MarimayFailAllocator state;
        MarimayAllocator allocator;
        MarimayMirFunction first = {0};
        MarimayMirFunction second = {0};
        const MarimayMirFunction *owned[] = {&second, &first};
        MarimayBytecodeProgram program = {0};
        const char *reason = NULL;
        bool ready;
        mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
        allocator = mmay_fail_allocator(&state);
        ready = mmay_test_prepare_leaf(&first, 1, 42, allocator)
            && mmay_test_prepare_leaf(&second, 2, 43, allocator);
        MMAY_EXPECT(ready);
        if (!ready) {
            mmay_mir_function_dispose(&second);
            mmay_mir_function_dispose(&first);
            MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
            break;
        }
        /* MIR verification uses the borrowed function's allocator, so include
           that allocation stream in the same sweep as the resulting bytecode. */
        state.attempts = 0;
        state.failure_index = failure_index;
        ready = mmay_cli_test_bytecode_prepare(
            owned, 2, second.symbol, allocator,
            &program, &reason
        );
        attempts = state.attempts;
        if (ready) {
            MMAY_EXPECT(failure_index >= attempts);
            reached_success = true;
        } else {
            MMAY_EXPECT(reason != NULL && !program.initialized
                && program.functions.data == NULL);
            MMAY_EXPECT(attempts > failure_index);
        }
        mmay_bytecode_program_dispose(&program);
        mmay_mir_function_dispose(&second);
        mmay_mir_function_dispose(&first);
        MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
        if (ready) break;
    }
    MMAY_EXPECT(reached_success && attempts > 0);
    reached_success = false;
    /* Sweep the full verification at entry changes independently of prepare. */
    for (failure_index = 0; failure_index < 10000; failure_index += 1) {
        MarimayFailAllocator state;
        MarimayBytecodeProgram program = {0};
        const char *reason = NULL;
        bool ready;
        mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
        ready = mmay_cli_test_bytecode_prepare(
            functions, 2, functions[0]->symbol, mmay_fail_allocator(&state),
            &program, &reason
        );
        MMAY_EXPECT(ready);
        state.attempts = 0;
        state.failure_index = failure_index;
        ready = ready && mmay_cli_test_bytecode_select(
            &program, functions[1]->symbol, &reason
        );
        if (ready) {
            MMAY_EXPECT(failure_index >= state.attempts);
            MMAY_EXPECT(program.entry_function == 0);
            reached_success = true;
        } else {
            MMAY_EXPECT(state.attempts > failure_index && reason != NULL);
            MMAY_EXPECT(program.entry_function == 1);
        }
        mmay_bytecode_program_dispose(&program);
        MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
        if (ready) break;
    }
    MMAY_EXPECT(reached_success);
}

static void mmay_test_preparation_invalid(
    const MarimayMirFunction *const *functions
) {
    MarimayFailAllocator state;
    MarimayAllocator allocator;
    MarimayBytecodeProgram program = {0};
    const MarimayMirFunction *duplicate[] = {functions[0], functions[0]};
    MarimayMirFunction invalid = {0};
    const MarimayMirFunction *bad[] = {&invalid, functions[1]};
    const char *reason = NULL;
    mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
    allocator = mmay_fail_allocator(&state);
    MMAY_EXPECT(!mmay_cli_test_bytecode_select(&program, functions[0]->symbol, &reason));
    MMAY_EXPECT(!mmay_cli_test_bytecode_prepare(
        NULL, 0, functions[0]->symbol, allocator, &program, &reason
    ));
    MMAY_EXPECT(!mmay_cli_test_bytecode_prepare(
        duplicate, 2, functions[0]->symbol, allocator, &program, &reason
    ));
    MMAY_EXPECT(!mmay_cli_test_bytecode_prepare(
        bad, 2, functions[0]->symbol, allocator, &program, &reason
    ));
    MMAY_EXPECT(!program.initialized && reason != NULL);
    mmay_bytecode_program_dispose(&program);
    MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
}

static void mmay_test_prepared_isolation(void) {
    static const char format[] =
        "test local/prepared-tests.argumentsMutateThenPanic ... FAILED\n"
        "  stdout:\nfirst-output\ncleanup-one\n"
        "  stderr:\npanic[G4000]: first-panic\n"
        "  --> %s/checkpoint11-prepared/reuse_test.my:21:3-21:23\n"
        "test local/prepared-tests.noArgumentsAfterPanic ... ok\n"
        "test local/prepared-tests.argumentsAreFresh ... ok\n"
        "test local/prepared-tests.lastTestStillHasFreshArguments ... ok\n"
        "test result: FAILED. 3 passed; 1 failed\n";
    static const char *const engines[] = {"vm", "native", "both"};
    char expected[16384];
    MarimayOwnedBytes current = {0};
    int length;
    MarimaySession session;
    size_t index;
    mmay_session_init(&session, mmay_system_allocator());
    MMAY_EXPECT(mmay_directory_current(mmay_system_allocator(), 4096, &current) == MMAY_FILE_OK);
    if (current.data == NULL) return;
    for (index = 0; index < current.length; index += 1) {
        if (current.data[index] == '\\') current.data[index] = '/';
    }
    length = snprintf(expected, sizeof(expected), format, (const char *)current.data);
    mmay_owned_bytes_dispose(&current);
    MMAY_EXPECT(length > 0 && (size_t)length < sizeof(expected));
    if (length <= 0 || (size_t)length >= sizeof(expected)) return;
    for (index = 0; index < 3; index += 1) {
        const char *arguments[] = {
            mmay_test_toolchain_executable(), "test", "checkpoint11-prepared",
            "--engine", engines[index], "--", "Мир", "two words"
        };
        MarimayTestBuffer output;
        MarimayTestBuffer error;
        mmay_test_buffer_init(&output);
        mmay_test_buffer_init(&error);
        MMAY_EXPECT(mmay_cli_run(
            &session, 8, arguments, (MarimayReadStream){0},
            mmay_test_buffer_writer(&output), mmay_test_buffer_writer(&error)
        ) == MMAY_EXIT_SOURCE_FAILURE);
        MMAY_EXPECT(output.length == (size_t)length
            && memcmp(output.bytes, expected, (size_t)length) == 0);
        MMAY_EXPECT(error.length == 0);
    }
}

void mmay_test_cli_test_bytecode(void) {
    MarimayMirFunction first = {0};
    MarimayMirFunction second = {0};
    const MarimayMirFunction *functions[] = {&second, &first};
    bool built = mmay_test_prepare_leaf(&first, 1, 42, mmay_system_allocator())
        && mmay_test_prepare_leaf(&second, 2, 43, mmay_system_allocator());
    MMAY_EXPECT(built);
    if (built) {
        mmay_test_prepared_entries(functions);
        mmay_test_preparation_failures(functions);
        mmay_test_preparation_invalid(functions);
    }
    mmay_mir_function_dispose(&second);
    mmay_mir_function_dispose(&first);
    mmay_test_prepared_isolation();
}
