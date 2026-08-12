#include "test_support.h"

#include <stdint.h>
#include <string.h>

#include "ruda/base/allocator.h"
#include "ruda/mir/mir.h"

typedef struct RudaMirCallTestContext {
    RudaSymbolId expected;
    size_t calls;
} RudaMirCallTestContext;

typedef struct RudaMirNeverCallTestContext {
    RudaSymbolId expected;
    RudaMirInterpretStatus status;
    size_t calls;
} RudaMirNeverCallTestContext;

static RudaMirInterpretStatus ruda_test_mir_call_handler(
    void *context,
    RudaSymbolId callee,
    const RudaMirValue *arguments,
    size_t argument_count,
    RudaMirValue *result
) {
    RudaMirCallTestContext *state = context;
    if (state == NULL || result == NULL
        || !ruda_symbol_id_equal(callee, state->expected)
        || argument_count != 2 || arguments == NULL
        || arguments[0].type != RUDA_TYPE_I64
        || arguments[1].type != RUDA_TYPE_I64) {
        return RUDA_MIR_INTERPRET_INVALID_ARGUMENT;
    }
    state->calls += 1;
    result->type = RUDA_TYPE_I64;
    result->as.i64 = arguments[0].as.i64 + arguments[1].as.i64;
    return RUDA_MIR_INTERPRET_OK;
}

static RudaMirInterpretStatus ruda_test_mir_never_call_handler(
    void *context,
    RudaSymbolId callee,
    const RudaMirValue *arguments,
    size_t argument_count,
    RudaMirValue *result
) {
    RudaMirNeverCallTestContext *state = context;
    if (state == NULL || result == NULL
        || !ruda_symbol_id_equal(callee, state->expected)
        || argument_count != 2 || arguments == NULL
        || arguments[0].type != RUDA_TYPE_I64
        || arguments[1].type != RUDA_TYPE_I64) {
        return RUDA_MIR_INTERPRET_INVALID_ARGUMENT;
    }
    state->calls += 1;
    if (state->status == RUDA_MIR_INTERPRET_OK) {
        result->type = RUDA_TYPE_I64;
        result->as.i64 = arguments[0].as.i64 + arguments[1].as.i64;
    }
    return state->status;
}

static RudaMirInstruction ruda_test_mir_binary(
    RudaMirInstructionKind kind,
    RudaMirRegister target,
    RudaMirRegister first,
    RudaMirRegister second
) {
    RudaMirInstruction instruction;
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = kind;
    instruction.target = target;
    instruction.first = first;
    instruction.second = second;
    return instruction;
}

static RudaMirTerminator ruda_test_mir_return(RudaMirRegister value) {
    RudaMirTerminator terminator;
    memset(&terminator, 0, sizeof(terminator));
    terminator.kind = RUDA_MIR_RETURN;
    terminator.value = value;
    terminator.target = RUDA_MIR_NO_BLOCK;
    terminator.otherwise = RUDA_MIR_NO_BLOCK;
    return terminator;
}

static RudaMirTerminator ruda_test_mir_goto(RudaMirBlockId target) {
    RudaMirTerminator terminator;
    memset(&terminator, 0, sizeof(terminator));
    terminator.kind = RUDA_MIR_GOTO;
    terminator.value = RUDA_MIR_NO_REGISTER;
    terminator.target = target;
    terminator.otherwise = RUDA_MIR_NO_BLOCK;
    return terminator;
}

static RudaMirTerminator ruda_test_mir_branch(
    RudaMirRegister condition,
    RudaMirBlockId when_true,
    RudaMirBlockId when_false
) {
    RudaMirTerminator terminator;
    memset(&terminator, 0, sizeof(terminator));
    terminator.kind = RUDA_MIR_BRANCH;
    terminator.value = condition;
    terminator.target = when_true;
    terminator.otherwise = when_false;
    return terminator;
}

static RudaMirTerminator ruda_test_mir_panic(
    RudaMirRegister message,
    size_t start,
    size_t end
) {
    RudaMirTerminator terminator;
    memset(&terminator, 0, sizeof(terminator));
    terminator.kind = RUDA_MIR_PANIC;
    terminator.value = message;
    terminator.target = RUDA_MIR_NO_BLOCK;
    terminator.otherwise = RUDA_MIR_NO_BLOCK;
    terminator.span.start = start;
    terminator.span.end = end;
    return terminator;
}

static RudaMirTerminator ruda_test_mir_call_never(
    RudaMirCallId call,
    size_t start,
    size_t end
) {
    RudaMirTerminator terminator;
    memset(&terminator, 0, sizeof(terminator));
    terminator.kind = RUDA_MIR_CALL_NEVER;
    terminator.value = RUDA_MIR_NO_REGISTER;
    terminator.call = call;
    terminator.target = RUDA_MIR_NO_BLOCK;
    terminator.otherwise = RUDA_MIR_NO_BLOCK;
    terminator.span.start = start;
    terminator.span.end = end;
    return terminator;
}

static void ruda_test_mir_strings_and_panic(RudaSymbolId symbol) {
    static const unsigned char message[] = "broken\n\xe2\x9b\x8f";
    RudaMirFunction function;
    RudaMirFunctionOptions function_options =
        ruda_mir_function_default_options();
    RudaMirExecution execution;
    RudaMirExecutionOptions execution_options =
        ruda_mir_execution_default_options();
    RudaMirInterpretOptions interpret_options =
        ruda_mir_interpret_default_options();
    RudaMirPanic panic;
    RudaMirValue result;
    RudaMirStringId string_id = RUDA_MIR_NO_STRING;
    RudaMirRegister string_value = RUDA_MIR_NO_REGISTER;
    RudaMirBlockId block = RUDA_MIR_NO_BLOCK;
    RudaMirInstruction instruction;
    RudaMirBlock *stored_block = NULL;
    RudaMirTerminator *stored_terminator = NULL;
    RudaMirStringConstant *stored_string = NULL;
    RudaTypeId *stored_type = NULL;
    memset(&panic, 0, sizeof(panic));
    RUDA_EXPECT(ruda_mir_function_init(
        &function,
        ruda_system_allocator(),
        symbol,
        NULL,
        0,
        RUDA_TYPE_I64,
        function_options
    ));
    RUDA_EXPECT(ruda_mir_add_register(
        &function,
        RUDA_TYPE_STRING,
        &string_value
    ));
    RUDA_EXPECT(ruda_mir_add_string_constant(
        &function,
        message,
        sizeof(message) - 1,
        &string_id
    ));
    RUDA_EXPECT(ruda_mir_add_block(&function, &block));
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_CONSTANT_STRING;
    instruction.target = string_value;
    instruction.first = RUDA_MIR_NO_REGISTER;
    instruction.second = RUDA_MIR_NO_REGISTER;
    instruction.string_constant = string_id;
    instruction.span.start = 5;
    instruction.span.end = 18;
    RUDA_EXPECT(ruda_mir_append_instruction(&function, block, &instruction));
    RUDA_EXPECT(ruda_mir_set_terminator(
        &function,
        block,
        ruda_test_mir_panic(string_value, 3, 19)
    ));
    RUDA_EXPECT(ruda_mir_verify(&function) == RUDA_MIR_VERIFY_OK);
    RUDA_EXPECT(ruda_mir_execution_init(
        &execution,
        ruda_system_allocator(),
        execution_options
    ));
    interpret_options.execution = &execution;
    interpret_options.panic = &panic;
    RUDA_EXPECT(ruda_mir_interpret(
        &function,
        NULL,
        0,
        ruda_system_allocator(),
        interpret_options,
        &result
    ) == RUDA_MIR_INTERPRET_PANIC);
    RUDA_EXPECT(panic.code != NULL && strcmp(panic.code, "G4000") == 0);
    RUDA_EXPECT(panic.message.length == sizeof(message) - 1);
    RUDA_EXPECT(panic.message.data != NULL && memcmp(
        panic.message.data,
        message,
        sizeof(message) - 1
    ) == 0);
    RUDA_EXPECT(ruda_symbol_id_equal(panic.symbol, symbol));
    RUDA_EXPECT(panic.span.start == 3 && panic.span.end == 19);
    ruda_mir_panic_dispose(&panic);
    RUDA_EXPECT(execution.strings.length == 1);
    ruda_mir_execution_dispose(&execution);

    execution_options = ruda_mir_execution_default_options();
    execution_options.max_total_string_bytes = sizeof(message) - 2;
    RUDA_EXPECT(ruda_mir_execution_init(
        &execution,
        ruda_system_allocator(),
        execution_options
    ));
    interpret_options.execution = &execution;
    interpret_options.panic = &panic;
    RUDA_EXPECT(ruda_mir_interpret(
        &function,
        NULL,
        0,
        ruda_system_allocator(),
        interpret_options,
        &result
    ) == RUDA_MIR_INTERPRET_OBJECT_LIMIT);
    RUDA_EXPECT(execution.strings.length == 0);
    RUDA_EXPECT(execution.total_string_bytes == 0);
    RUDA_EXPECT(panic.message.data == NULL);
    ruda_mir_execution_dispose(&execution);
    execution_options = ruda_mir_execution_default_options();

    {
        size_t failure_offset = 0;
        bool reached_success = false;
        for (failure_offset = 0; failure_offset < 16; failure_offset += 1) {
            RudaFailAllocator state;
            RudaAllocator allocator;
            RudaMirExecution failed_execution;
            RudaMirPanic failed_panic;
            RudaMirInterpretStatus status;
            memset(&failed_panic, 0, sizeof(failed_panic));
            ruda_fail_allocator_init(
                &state,
                ruda_system_allocator(),
                SIZE_MAX
            );
            allocator = ruda_fail_allocator(&state);
            RUDA_EXPECT(ruda_mir_execution_init(
                &failed_execution,
                allocator,
                execution_options
            ));
            state.failure_index = state.attempts + failure_offset;
            interpret_options.execution = &failed_execution;
            interpret_options.panic = &failed_panic;
            status = ruda_mir_interpret(
                &function,
                NULL,
                0,
                allocator,
                interpret_options,
                &result
            );
            if (status == RUDA_MIR_INTERPRET_PANIC) {
                reached_success = true;
                RUDA_EXPECT(failed_panic.message.length == sizeof(message) - 1);
            } else {
                RUDA_EXPECT(status == RUDA_MIR_INTERPRET_ALLOCATION_FAILED);
            }
            ruda_mir_panic_dispose(&failed_panic);
            ruda_mir_execution_dispose(&failed_execution);
            RUDA_EXPECT(state.live_allocations == 0);
            RUDA_EXPECT(state.live_bytes == 0);
            if (reached_success) {
                break;
            }
        }
        RUDA_EXPECT(reached_success);
    }

    {
        static const unsigned char invalid_utf8[] = { 0xffU };
        size_t original_count = function.string_constants.length;
        size_t original_bytes = function.string_bytes;
        RudaMirStringId invalid_id = RUDA_MIR_NO_STRING;
        RUDA_EXPECT(!ruda_mir_add_string_constant(
            &function,
            invalid_utf8,
            sizeof(invalid_utf8),
            &invalid_id
        ));
        RUDA_EXPECT(function.string_constants.length == original_count);
        RUDA_EXPECT(function.string_bytes == original_bytes);
        RUDA_EXPECT(invalid_id == RUDA_MIR_NO_STRING);
    }

    stored_block = ruda_vector_at(&function.blocks, block);
    stored_terminator = stored_block != NULL ? &stored_block->terminator : NULL;
    stored_type = ruda_vector_at(&function.register_types, string_value);
    stored_string = ruda_vector_at(&function.string_constants, string_id);
    RUDA_EXPECT(stored_terminator != NULL && stored_type != NULL
        && stored_string != NULL);
    if (stored_type != NULL) {
        *stored_type = RUDA_TYPE_I64;
        RUDA_EXPECT(ruda_mir_verify(&function) == RUDA_MIR_VERIFY_TYPE_MISMATCH);
        *stored_type = RUDA_TYPE_STRING;
    }
    if (stored_terminator != NULL) {
        stored_terminator->value = RUDA_MIR_NO_REGISTER;
        RUDA_EXPECT(ruda_mir_verify(&function) == RUDA_MIR_VERIFY_INVALID_REGISTER);
        stored_terminator->value = string_value;
    }
    if (stored_string != NULL && stored_string->value.data != NULL) {
        unsigned char saved = stored_string->value.data[0];
        stored_string->value.data[0] = 0xffU;
        RUDA_EXPECT(ruda_mir_verify(&function) == RUDA_MIR_VERIFY_INVALID_ARGUMENT);
        stored_string->value.data[0] = saved;
    }
    {
        RudaMirInstruction *stored_instruction = stored_block != NULL
            ? ruda_vector_at(&stored_block->instructions, 0)
            : NULL;
        RUDA_EXPECT(stored_instruction != NULL);
        if (stored_instruction != NULL) {
            stored_instruction->string_constant = RUDA_MIR_NO_STRING;
            RUDA_EXPECT(
                ruda_mir_verify(&function) == RUDA_MIR_VERIFY_INVALID_ARGUMENT
            );
            stored_instruction->string_constant = string_id;
        }
    }
    function.options.max_verify_work = 1;
    RUDA_EXPECT(ruda_mir_verify(&function) == RUDA_MIR_VERIFY_LIMIT_REACHED);
    ruda_mir_function_dispose(&function);
}

static void ruda_test_mir_string_equality(RudaSymbolId symbol) {
    static const unsigned char embedded[] = {'a', 0, 'b'};
    RudaMirFunction function;
    RudaMirExecution execution;
    RudaMirInterpretOptions options = ruda_mir_interpret_default_options();
    RudaMirValue result;
    RudaMirStringId left_id = RUDA_MIR_NO_STRING;
    RudaMirStringId right_id = RUDA_MIR_NO_STRING;
    RudaMirRegister left = RUDA_MIR_NO_REGISTER;
    RudaMirRegister right = RUDA_MIR_NO_REGISTER;
    RudaMirRegister equal = RUDA_MIR_NO_REGISTER;
    RudaMirRegister uninitialized = RUDA_MIR_NO_REGISTER;
    RudaMirBlockId block = RUDA_MIR_NO_BLOCK;
    RudaMirInstruction instruction;
    RudaMirBlock *stored_block = NULL;
    RudaMirInstruction *comparison = NULL;
    RudaMirStringConstant *right_constant = NULL;
    RudaTypeId *right_type = NULL;

    RUDA_EXPECT(ruda_mir_function_init(
        &function,
        ruda_system_allocator(),
        symbol,
        NULL,
        0,
        RUDA_TYPE_BOOL,
        ruda_mir_function_default_options()
    ));
    RUDA_EXPECT(ruda_mir_add_register(&function, RUDA_TYPE_STRING, &left));
    RUDA_EXPECT(ruda_mir_add_register(&function, RUDA_TYPE_STRING, &right));
    RUDA_EXPECT(ruda_mir_add_register(&function, RUDA_TYPE_BOOL, &equal));
    RUDA_EXPECT(ruda_mir_add_register(
        &function,
        RUDA_TYPE_STRING,
        &uninitialized
    ));
    RUDA_EXPECT(ruda_mir_add_string_constant(
        &function,
        embedded,
        sizeof(embedded),
        &left_id
    ));
    RUDA_EXPECT(ruda_mir_add_string_constant(
        &function,
        embedded,
        sizeof(embedded),
        &right_id
    ));
    RUDA_EXPECT(ruda_mir_add_block(&function, &block));
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_CONSTANT_STRING;
    instruction.target = left;
    instruction.string_constant = left_id;
    RUDA_EXPECT(ruda_mir_append_instruction(&function, block, &instruction));
    instruction.target = right;
    instruction.string_constant = right_id;
    RUDA_EXPECT(ruda_mir_append_instruction(&function, block, &instruction));
    instruction = ruda_test_mir_binary(
        RUDA_MIR_EQUAL_STRING,
        equal,
        left,
        right
    );
    RUDA_EXPECT(ruda_mir_append_instruction(&function, block, &instruction));
    RUDA_EXPECT(ruda_mir_set_terminator(
        &function,
        block,
        ruda_test_mir_return(equal)
    ));
    RUDA_EXPECT(ruda_mir_verify(&function) == RUDA_MIR_VERIFY_OK);
    RUDA_EXPECT(ruda_mir_execution_init(
        &execution,
        ruda_system_allocator(),
        ruda_mir_execution_default_options()
    ));
    options.execution = &execution;
    RUDA_EXPECT(ruda_mir_interpret(
        &function,
        NULL,
        0,
        ruda_system_allocator(),
        options,
        &result
    ) == RUDA_MIR_INTERPRET_OK);
    RUDA_EXPECT(result.type == RUDA_TYPE_BOOL && result.as.boolean);
    ruda_mir_execution_dispose(&execution);

    right_constant = ruda_vector_at(&function.string_constants, right_id);
    RUDA_EXPECT(right_constant != NULL && right_constant->value.length == 3);
    if (right_constant != NULL && right_constant->value.length == 3) {
        right_constant->value.data[2] = 'c';
        RUDA_EXPECT(ruda_mir_execution_init(
            &execution,
            ruda_system_allocator(),
            ruda_mir_execution_default_options()
        ));
        options.execution = &execution;
        RUDA_EXPECT(ruda_mir_interpret(
            &function,
            NULL,
            0,
            ruda_system_allocator(),
            options,
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.type == RUDA_TYPE_BOOL && !result.as.boolean);
        ruda_mir_execution_dispose(&execution);
        right_constant->value.data[2] = 'b';
    }

    stored_block = ruda_vector_at(&function.blocks, block);
    comparison = stored_block != NULL
        ? ruda_vector_at(&stored_block->instructions, 2)
        : NULL;
    right_type = ruda_vector_at(&function.register_types, right);
    RUDA_EXPECT(comparison != NULL && right_type != NULL);
    if (right_type != NULL) {
        *right_type = RUDA_TYPE_I64;
        RUDA_EXPECT(ruda_mir_verify(&function) == RUDA_MIR_VERIFY_TYPE_MISMATCH);
        *right_type = RUDA_TYPE_STRING;
    }
    if (comparison != NULL) {
        comparison->second = uninitialized;
        RUDA_EXPECT(
            ruda_mir_verify(&function)
                == RUDA_MIR_VERIFY_UNINITIALIZED_REGISTER
        );
        comparison->second = right;
    }
    RUDA_EXPECT(ruda_mir_verify(&function) == RUDA_MIR_VERIFY_OK);
    options = ruda_mir_interpret_default_options();
    RUDA_EXPECT(ruda_mir_interpret(
        &function,
        NULL,
        0,
        ruda_system_allocator(),
        options,
        &result
    ) == RUDA_MIR_INTERPRET_INVALID_ARGUMENT);
    ruda_mir_function_dispose(&function);
}

static void ruda_test_mir_verify_limits_and_failures(RudaSymbolId symbol) {
    RudaTypeId parameter = RUDA_TYPE_I64;
    RudaMirFunction function;
    RudaMirBlockId block = RUDA_MIR_NO_BLOCK;
    RudaMirFunctionOptions options = ruda_mir_function_default_options();
    size_t failure_offset = 0;
    bool reached_success = false;

    options.max_verify_work = 1;
    RUDA_EXPECT(ruda_mir_function_init(
        &function,
        ruda_system_allocator(),
        symbol,
        &parameter,
        1,
        RUDA_TYPE_I64,
        options
    ));
    RUDA_EXPECT(ruda_mir_add_block(&function, &block));
    RUDA_EXPECT(ruda_mir_set_terminator(
        &function,
        block,
        ruda_test_mir_return(0)
    ));
    RUDA_EXPECT(ruda_mir_verify(&function) == RUDA_MIR_VERIFY_LIMIT_REACHED);
    ruda_mir_function_dispose(&function);

    for (failure_offset = 0; failure_offset < 16; failure_offset += 1) {
        RudaFailAllocator state;
        RudaAllocator allocator;
        RudaMirVerifyStatus status;
        size_t baseline_allocations = 0;
        size_t baseline_bytes = 0;
        ruda_fail_allocator_init(
            &state,
            ruda_system_allocator(),
            SIZE_MAX
        );
        allocator = ruda_fail_allocator(&state);
        RUDA_EXPECT(ruda_mir_function_init(
            &function,
            allocator,
            symbol,
            &parameter,
            1,
            RUDA_TYPE_I64,
            ruda_mir_function_default_options()
        ));
        RUDA_EXPECT(ruda_mir_add_block(&function, &block));
        RUDA_EXPECT(ruda_mir_set_terminator(
            &function,
            block,
            ruda_test_mir_return(0)
        ));
        baseline_allocations = state.live_allocations;
        baseline_bytes = state.live_bytes;
        state.failure_index = state.attempts + failure_offset;
        status = ruda_mir_verify(&function);
        if (status == RUDA_MIR_VERIFY_OK) {
            reached_success = true;
        } else {
            RUDA_EXPECT(status == RUDA_MIR_VERIFY_ALLOCATION_FAILED);
            RUDA_EXPECT(state.live_allocations == baseline_allocations);
            RUDA_EXPECT(state.live_bytes == baseline_bytes);
        }
        ruda_mir_function_dispose(&function);
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
        if (reached_success) {
            break;
        }
    }
    RUDA_EXPECT(reached_success);
}

static void ruda_test_mir_verify_long_cfg(RudaSymbolId symbol) {
    enum { BLOCK_COUNT = 256 };
    RudaMirFunction function;
    RudaMirFunctionOptions options = ruda_mir_function_default_options();
    RudaMirRegister value = RUDA_MIR_NO_REGISTER;
    RudaMirBlockId blocks[BLOCK_COUNT];
    RudaMirInstruction instruction;
    size_t index = 0;
    options.max_verify_work = 10000;
    RUDA_EXPECT(ruda_mir_function_init(
        &function,
        ruda_system_allocator(),
        symbol,
        NULL,
        0,
        RUDA_TYPE_I64,
        options
    ));
    RUDA_EXPECT(ruda_mir_add_register(&function, RUDA_TYPE_I64, &value));
    for (index = 0; index < BLOCK_COUNT; index += 1) {
        RUDA_EXPECT(ruda_mir_add_block(&function, &blocks[index]));
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_CONSTANT_I64;
    instruction.target = value;
    instruction.first = RUDA_MIR_NO_REGISTER;
    instruction.second = RUDA_MIR_NO_REGISTER;
    instruction.integer = 7;
    RUDA_EXPECT(ruda_mir_append_instruction(
        &function,
        blocks[0],
        &instruction
    ));
    for (index = 0; index + 1 < BLOCK_COUNT; index += 1) {
        RUDA_EXPECT(ruda_mir_set_terminator(
            &function,
            blocks[index],
            ruda_test_mir_goto(blocks[index + 1])
        ));
    }
    RUDA_EXPECT(ruda_mir_set_terminator(
        &function,
        blocks[BLOCK_COUNT - 1],
        ruda_test_mir_return(value)
    ));
    RUDA_EXPECT(ruda_mir_verify(&function) == RUDA_MIR_VERIFY_OK);
    {
        size_t verify_work = function.options.max_verify_work;
        function.options.max_verify_work = 1;
        RUDA_EXPECT(
            ruda_mir_verify(&function) == RUDA_MIR_VERIFY_LIMIT_REACHED
        );
        function.options.max_verify_work = verify_work;
    }
    ruda_mir_function_dispose(&function);
}

static void ruda_test_mir_calls(RudaSymbolId symbol) {
    RudaSymbolId callee;
    RudaTypeId parameters[] = {RUDA_TYPE_I64, RUDA_TYPE_I64};
    RudaMirFunction function;
    RudaMirRegister value = RUDA_MIR_NO_REGISTER;
    RudaMirBlockId block = RUDA_MIR_NO_BLOCK;
    RudaMirCallArgument call_arguments[2];
    RudaMirCallId call = RUDA_MIR_NO_CALL;
    RudaMirInstruction instruction;
    RudaMirValue arguments[2];
    RudaMirValue result;
    RudaMirInterpretOptions options = ruda_mir_interpret_default_options();
    RudaMirCallTestContext context;
    size_t failure_offset = 0;
    bool reached_success = false;
    RUDA_EXPECT(ruda_symbol_id_from_parts(
        "test",
        "mir.ruda",
        "callee",
        0,
        &callee
    ));
    RUDA_EXPECT(ruda_mir_function_init(
        &function,
        ruda_system_allocator(),
        symbol,
        parameters,
        2,
        RUDA_TYPE_I64,
        ruda_mir_function_default_options()
    ));
    RUDA_EXPECT(ruda_mir_add_register(&function, RUDA_TYPE_I64, &value));
    RUDA_EXPECT(ruda_mir_add_block(&function, &block));
    call_arguments[0].register_id = 0;
    call_arguments[0].type = RUDA_TYPE_I64;
    call_arguments[1].register_id = 1;
    call_arguments[1].type = RUDA_TYPE_I64;
    RUDA_EXPECT(ruda_mir_add_call(
        &function,
        callee,
        RUDA_TYPE_I64,
        call_arguments,
        2,
        &call
    ));
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_CALL;
    instruction.target = value;
    instruction.first = RUDA_MIR_NO_REGISTER;
    instruction.second = RUDA_MIR_NO_REGISTER;
    instruction.call = call;
    RUDA_EXPECT(ruda_mir_append_instruction(&function, block, &instruction));
    RUDA_EXPECT(ruda_mir_set_terminator(
        &function,
        block,
        ruda_test_mir_return(value)
    ));
    RUDA_EXPECT(ruda_mir_verify(&function) == RUDA_MIR_VERIFY_OK);
    {
        size_t verify_work = function.options.max_verify_work;
        function.options.max_verify_work = 1;
        RUDA_EXPECT(
            ruda_mir_verify(&function) == RUDA_MIR_VERIFY_LIMIT_REACHED
        );
        function.options.max_verify_work = verify_work;
    }
    arguments[0].type = RUDA_TYPE_I64;
    arguments[0].as.i64 = 3;
    arguments[1].type = RUDA_TYPE_I64;
    arguments[1].as.i64 = 4;
    RUDA_EXPECT(ruda_mir_interpret(
        &function,
        arguments,
        2,
        ruda_system_allocator(),
        options,
        &result
    ) == RUDA_MIR_INTERPRET_INVALID_ARGUMENT);
    context.expected = callee;
    context.calls = 0;
    options.call_context = &context;
    options.call = ruda_test_mir_call_handler;
    RUDA_EXPECT(ruda_mir_interpret(
        &function,
        arguments,
        2,
        ruda_system_allocator(),
        options,
        &result
    ) == RUDA_MIR_INTERPRET_OK);
    RUDA_EXPECT(context.calls == 1);
    RUDA_EXPECT(result.type == RUDA_TYPE_I64);
    RUDA_EXPECT(result.as.i64 == 7);
    {
        RudaMirCallArgument *argument = ruda_vector_at(
            &function.call_arguments,
            0
        );
        RUDA_EXPECT(argument != NULL);
        if (argument != NULL) {
            argument->type = RUDA_TYPE_BOOL;
            RUDA_EXPECT(
                ruda_mir_verify(&function) == RUDA_MIR_VERIFY_TYPE_MISMATCH
            );
            argument->type = RUDA_TYPE_I64;
            argument->register_id = value;
            RUDA_EXPECT(
                ruda_mir_verify(&function)
                    == RUDA_MIR_VERIFY_UNINITIALIZED_REGISTER
            );
            argument->register_id = 0;
        }
    }
    reached_success = false;
    for (failure_offset = 0; failure_offset < 8; failure_offset += 1) {
        RudaFailAllocator state;
        RudaAllocator allocator;
        RudaMirInterpretStatus status;
        ruda_fail_allocator_init(
            &state,
            ruda_system_allocator(),
            failure_offset
        );
        allocator = ruda_fail_allocator(&state);
        status = ruda_mir_interpret(
            &function,
            arguments,
            2,
            allocator,
            options,
            &result
        );
        if (status == RUDA_MIR_INTERPRET_OK) {
            reached_success = true;
            RUDA_EXPECT(result.as.i64 == 7);
        } else {
            RUDA_EXPECT(status == RUDA_MIR_INTERPRET_ALLOCATION_FAILED);
        }
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
        if (reached_success) {
            break;
        }
    }
    RUDA_EXPECT(reached_success);
    ruda_mir_function_dispose(&function);

    reached_success = false;
    for (failure_offset = 0; failure_offset < 8; failure_offset += 1) {
        RudaFailAllocator state;
        RudaAllocator allocator;
        ruda_fail_allocator_init(
            &state,
            ruda_system_allocator(),
            SIZE_MAX
        );
        allocator = ruda_fail_allocator(&state);
        RUDA_EXPECT(ruda_mir_function_init(
            &function,
            allocator,
            symbol,
            parameters,
            2,
            RUDA_TYPE_I64,
            ruda_mir_function_default_options()
        ));
        state.failure_index = state.attempts + failure_offset;
        if (ruda_mir_add_call(
                &function,
                callee,
                RUDA_TYPE_I64,
                call_arguments,
                2,
                &call
            )) {
            reached_success = true;
        } else {
            RUDA_EXPECT(function.calls.length == 0);
            RUDA_EXPECT(function.call_arguments.length == 0);
        }
        ruda_mir_function_dispose(&function);
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
        if (reached_success) {
            break;
        }
    }
    RUDA_EXPECT(reached_success);
}

static void ruda_test_mir_never_calls(RudaSymbolId symbol) {
    RudaSymbolId callee;
    RudaTypeId parameters[] = { RUDA_TYPE_I64, RUDA_TYPE_I64 };
    RudaMirFunction function;
    RudaMirBlockId block = RUDA_MIR_NO_BLOCK;
    RudaMirRegister unused = RUDA_MIR_NO_REGISTER;
    RudaMirCallArgument call_arguments[2];
    RudaMirCallId call = RUDA_MIR_NO_CALL;
    RudaMirValue arguments[2];
    RudaMirValue result;
    RudaMirInterpretOptions options = ruda_mir_interpret_default_options();
    RudaMirNeverCallTestContext context;
    RudaMirCall *stored_call = NULL;
    RudaMirCallArgument *stored_argument = NULL;
    RudaMirBlock *stored_block = NULL;
    size_t failure_offset = 0;
    bool reached_status = false;
    RUDA_EXPECT(ruda_symbol_id_from_parts(
        "test",
        "mir.ruda",
        "never-callee",
        0,
        &callee
    ));
    RUDA_EXPECT(ruda_mir_function_init(
        &function,
        ruda_system_allocator(),
        symbol,
        parameters,
        2,
        RUDA_TYPE_I64,
        ruda_mir_function_default_options()
    ));
    RUDA_EXPECT(ruda_mir_add_register(&function, RUDA_TYPE_I64, &unused));
    RUDA_EXPECT(ruda_mir_add_block(&function, &block));
    call_arguments[0].register_id = 0;
    call_arguments[0].type = RUDA_TYPE_I64;
    call_arguments[1].register_id = 1;
    call_arguments[1].type = RUDA_TYPE_I64;
    RUDA_EXPECT(ruda_mir_add_call(
        &function,
        callee,
        RUDA_TYPE_NEVER,
        call_arguments,
        2,
        &call
    ));
    RUDA_EXPECT(ruda_mir_set_terminator(
        &function,
        block,
        ruda_test_mir_call_never(call, 4, 15)
    ));
    RUDA_EXPECT(ruda_mir_verify(&function) == RUDA_MIR_VERIFY_OK);

    arguments[0].type = RUDA_TYPE_I64;
    arguments[0].as.i64 = 3;
    arguments[1].type = RUDA_TYPE_I64;
    arguments[1].as.i64 = 4;
    RUDA_EXPECT(ruda_mir_interpret(
        &function,
        arguments,
        2,
        ruda_system_allocator(),
        options,
        &result
    ) == RUDA_MIR_INTERPRET_INVALID_ARGUMENT);
    context.expected = callee;
    context.status = RUDA_MIR_INTERPRET_PANIC;
    context.calls = 0;
    options.call_context = &context;
    options.call = ruda_test_mir_never_call_handler;
    RUDA_EXPECT(ruda_mir_interpret(
        &function,
        arguments,
        2,
        ruda_system_allocator(),
        options,
        &result
    ) == RUDA_MIR_INTERPRET_PANIC);
    RUDA_EXPECT(context.calls == 1);
    context.status = RUDA_MIR_INTERPRET_OK;
    RUDA_EXPECT(ruda_mir_interpret(
        &function,
        arguments,
        2,
        ruda_system_allocator(),
        options,
        &result
    ) == RUDA_MIR_INTERPRET_INVALID_MIR);

    context.status = RUDA_MIR_INTERPRET_PANIC;
    reached_status = false;
    for (failure_offset = 0; failure_offset < 8; failure_offset += 1) {
        RudaFailAllocator state;
        RudaAllocator allocator;
        RudaMirInterpretStatus status;
        ruda_fail_allocator_init(
            &state,
            ruda_system_allocator(),
            failure_offset
        );
        allocator = ruda_fail_allocator(&state);
        status = ruda_mir_interpret(
            &function,
            arguments,
            2,
            allocator,
            options,
            &result
        );
        if (status == RUDA_MIR_INTERPRET_PANIC) {
            reached_status = true;
        } else {
            RUDA_EXPECT(status == RUDA_MIR_INTERPRET_ALLOCATION_FAILED);
        }
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
        if (reached_status) {
            break;
        }
    }
    RUDA_EXPECT(reached_status);

    stored_call = ruda_vector_at(&function.calls, call);
    stored_argument = ruda_vector_at(&function.call_arguments, 0);
    stored_block = ruda_vector_at(&function.blocks, block);
    RUDA_EXPECT(stored_call != NULL && stored_argument != NULL
        && stored_block != NULL);
    if (stored_call != NULL) {
        stored_call->result_type = RUDA_TYPE_I64;
        RUDA_EXPECT(ruda_mir_verify(&function) == RUDA_MIR_VERIFY_INVALID_ARGUMENT);
        stored_call->result_type = RUDA_TYPE_NEVER;
    }
    if (stored_argument != NULL) {
        stored_argument->type = RUDA_TYPE_BOOL;
        RUDA_EXPECT(ruda_mir_verify(&function) == RUDA_MIR_VERIFY_TYPE_MISMATCH);
        stored_argument->type = RUDA_TYPE_I64;
        stored_argument->register_id = unused;
        RUDA_EXPECT(
            ruda_mir_verify(&function) == RUDA_MIR_VERIFY_UNINITIALIZED_REGISTER
        );
        stored_argument->register_id = 0;
    }
    if (stored_block != NULL) {
        stored_block->terminator.call = RUDA_MIR_NO_CALL;
        RUDA_EXPECT(ruda_mir_verify(&function) == RUDA_MIR_VERIFY_INVALID_ARGUMENT);
        stored_block->terminator.call = call;
    }
    ruda_mir_function_dispose(&function);

    RUDA_EXPECT(ruda_mir_function_init(
        &function,
        ruda_system_allocator(),
        symbol,
        NULL,
        0,
        RUDA_TYPE_NEVER,
        ruda_mir_function_default_options()
    ));
    RUDA_EXPECT(ruda_mir_add_block(&function, &block));
    RUDA_EXPECT(ruda_mir_set_terminator(
        &function,
        block,
        ruda_test_mir_return(RUDA_MIR_NO_REGISTER)
    ));
    RUDA_EXPECT(ruda_mir_verify(&function) == RUDA_MIR_VERIFY_TYPE_MISMATCH);
    ruda_mir_function_dispose(&function);

    RUDA_EXPECT(ruda_mir_function_init(
        &function,
        ruda_system_allocator(),
        symbol,
        parameters,
        2,
        RUDA_TYPE_NEVER,
        ruda_mir_function_default_options()
    ));
    RUDA_EXPECT(ruda_mir_add_register(&function, RUDA_TYPE_NEVER, &unused));
    RUDA_EXPECT(ruda_mir_add_block(&function, &block));
    RUDA_EXPECT(ruda_mir_add_call(
        &function,
        callee,
        RUDA_TYPE_NEVER,
        call_arguments,
        2,
        &call
    ));
    {
        RudaMirInstruction instruction;
        memset(&instruction, 0, sizeof(instruction));
        instruction.kind = RUDA_MIR_CALL;
        instruction.target = unused;
        instruction.first = RUDA_MIR_NO_REGISTER;
        instruction.second = RUDA_MIR_NO_REGISTER;
        instruction.call = call;
        RUDA_EXPECT(ruda_mir_append_instruction(
            &function,
            block,
            &instruction
        ));
    }
    RUDA_EXPECT(ruda_mir_set_terminator(
        &function,
        block,
        ruda_test_mir_call_never(call, 0, 0)
    ));
    RUDA_EXPECT(ruda_mir_verify(&function) == RUDA_MIR_VERIFY_INVALID_ARGUMENT);
    ruda_mir_function_dispose(&function);
}

static void ruda_test_mir_bitwise(RudaSymbolId symbol) {
    RudaTypeId parameters[] = {RUDA_TYPE_I64, RUDA_TYPE_I64};
    RudaMirFunction function;
    RudaMirRegister and_value = RUDA_MIR_NO_REGISTER;
    RudaMirRegister xor_value = RUDA_MIR_NO_REGISTER;
    RudaMirRegister or_value = RUDA_MIR_NO_REGISTER;
    RudaMirBlockId block = RUDA_MIR_NO_BLOCK;
    RudaMirInstruction instruction;
    RudaMirValue arguments[2];
    RudaMirValue result;
    RUDA_EXPECT(ruda_mir_function_init(
        &function,
        ruda_system_allocator(),
        symbol,
        parameters,
        2,
        RUDA_TYPE_I64,
        ruda_mir_function_default_options()
    ));
    RUDA_EXPECT(ruda_mir_add_register(
        &function,
        RUDA_TYPE_I64,
        &and_value
    ));
    RUDA_EXPECT(ruda_mir_add_register(
        &function,
        RUDA_TYPE_I64,
        &xor_value
    ));
    RUDA_EXPECT(ruda_mir_add_register(
        &function,
        RUDA_TYPE_I64,
        &or_value
    ));
    RUDA_EXPECT(ruda_mir_add_block(&function, &block));
    instruction = ruda_test_mir_binary(
        RUDA_MIR_BITWISE_AND_I64,
        and_value,
        0,
        1
    );
    RUDA_EXPECT(ruda_mir_append_instruction(&function, block, &instruction));
    instruction = ruda_test_mir_binary(
        RUDA_MIR_BITWISE_XOR_I64,
        xor_value,
        and_value,
        1
    );
    RUDA_EXPECT(ruda_mir_append_instruction(&function, block, &instruction));
    instruction = ruda_test_mir_binary(
        RUDA_MIR_BITWISE_OR_I64,
        or_value,
        xor_value,
        0
    );
    RUDA_EXPECT(ruda_mir_append_instruction(&function, block, &instruction));
    RUDA_EXPECT(ruda_mir_set_terminator(
        &function,
        block,
        ruda_test_mir_return(or_value)
    ));
    RUDA_EXPECT(ruda_mir_verify(&function) == RUDA_MIR_VERIFY_OK);
    arguments[0].type = RUDA_TYPE_I64;
    arguments[0].as.i64 = -16;
    arguments[1].type = RUDA_TYPE_I64;
    arguments[1].as.i64 = 10;
    RUDA_EXPECT(ruda_mir_interpret(
        &function,
        arguments,
        2,
        ruda_system_allocator(),
        ruda_mir_interpret_default_options(),
        &result
    ) == RUDA_MIR_INTERPRET_OK);
    RUDA_EXPECT(result.type == RUDA_TYPE_I64);
    RUDA_EXPECT(result.as.i64 == -6);
    ruda_mir_function_dispose(&function);

    {
        RudaTypeId invalid_parameters[] = {RUDA_TYPE_BOOL, RUDA_TYPE_BOOL};
        RudaMirRegister invalid_value = RUDA_MIR_NO_REGISTER;
        RUDA_EXPECT(ruda_mir_function_init(
            &function,
            ruda_system_allocator(),
            symbol,
            invalid_parameters,
            2,
            RUDA_TYPE_I64,
            ruda_mir_function_default_options()
        ));
        RUDA_EXPECT(ruda_mir_add_register(
            &function,
            RUDA_TYPE_I64,
            &invalid_value
        ));
        RUDA_EXPECT(ruda_mir_add_block(&function, &block));
        instruction = ruda_test_mir_binary(
            RUDA_MIR_BITWISE_AND_I64,
            invalid_value,
            0,
            1
        );
        RUDA_EXPECT(ruda_mir_append_instruction(
            &function,
            block,
            &instruction
        ));
        RUDA_EXPECT(ruda_mir_set_terminator(
            &function,
            block,
            ruda_test_mir_return(invalid_value)
        ));
        RUDA_EXPECT(
            ruda_mir_verify(&function) == RUDA_MIR_VERIFY_TYPE_MISMATCH
        );
        ruda_mir_function_dispose(&function);
    }
}

static void ruda_test_mir_shifts(RudaSymbolId symbol) {
    RudaTypeId parameters[] = {RUDA_TYPE_I64, RUDA_TYPE_I64};
    RudaMirFunction function;
    RudaMirRegister shifted = RUDA_MIR_NO_REGISTER;
    RudaMirBlockId block = RUDA_MIR_NO_BLOCK;
    RudaMirInstruction instruction;
    RudaMirValue arguments[2];
    RudaMirValue result;

    RUDA_EXPECT(ruda_mir_function_init(
        &function,
        ruda_system_allocator(),
        symbol,
        parameters,
        2,
        RUDA_TYPE_I64,
        ruda_mir_function_default_options()
    ));
    RUDA_EXPECT(ruda_mir_add_register(
        &function,
        RUDA_TYPE_I64,
        &shifted
    ));
    RUDA_EXPECT(ruda_mir_add_block(&function, &block));
    instruction = ruda_test_mir_binary(
        RUDA_MIR_SHIFT_LEFT_CHECKED_I64,
        shifted,
        0,
        1
    );
    RUDA_EXPECT(ruda_mir_append_instruction(&function, block, &instruction));
    RUDA_EXPECT(ruda_mir_set_terminator(
        &function,
        block,
        ruda_test_mir_return(shifted)
    ));
    RUDA_EXPECT(ruda_mir_verify(&function) == RUDA_MIR_VERIFY_OK);
    arguments[0].type = RUDA_TYPE_I64;
    arguments[1].type = RUDA_TYPE_I64;
    arguments[0].as.i64 = -1;
    arguments[1].as.i64 = 63;
    RUDA_EXPECT(ruda_mir_interpret(
        &function,
        arguments,
        2,
        ruda_system_allocator(),
        ruda_mir_interpret_default_options(),
        &result
    ) == RUDA_MIR_INTERPRET_OK);
    RUDA_EXPECT(result.as.i64 == INT64_MIN);
    arguments[0].as.i64 = -2;
    arguments[1].as.i64 = 62;
    RUDA_EXPECT(ruda_mir_interpret(
        &function,
        arguments,
        2,
        ruda_system_allocator(),
        ruda_mir_interpret_default_options(),
        &result
    ) == RUDA_MIR_INTERPRET_OK);
    RUDA_EXPECT(result.as.i64 == INT64_MIN);
    arguments[0].as.i64 = INT64_MIN;
    arguments[1].as.i64 = 1;
    RUDA_EXPECT(ruda_mir_interpret(
        &function,
        arguments,
        2,
        ruda_system_allocator(),
        ruda_mir_interpret_default_options(),
        &result
    ) == RUDA_MIR_INTERPRET_OVERFLOW);
    arguments[0].as.i64 = 1;
    arguments[1].as.i64 = 63;
    RUDA_EXPECT(ruda_mir_interpret(
        &function,
        arguments,
        2,
        ruda_system_allocator(),
        ruda_mir_interpret_default_options(),
        &result
    ) == RUDA_MIR_INTERPRET_OVERFLOW);
    arguments[0].as.i64 = 0;
    arguments[1].as.i64 = 63;
    RUDA_EXPECT(ruda_mir_interpret(
        &function,
        arguments,
        2,
        ruda_system_allocator(),
        ruda_mir_interpret_default_options(),
        &result
    ) == RUDA_MIR_INTERPRET_OK);
    RUDA_EXPECT(result.as.i64 == 0);
    arguments[1].as.i64 = -1;
    RUDA_EXPECT(ruda_mir_interpret(
        &function,
        arguments,
        2,
        ruda_system_allocator(),
        ruda_mir_interpret_default_options(),
        &result
    ) == RUDA_MIR_INTERPRET_INVALID_SHIFT);
    arguments[1].as.i64 = 64;
    RUDA_EXPECT(ruda_mir_interpret(
        &function,
        arguments,
        2,
        ruda_system_allocator(),
        ruda_mir_interpret_default_options(),
        &result
    ) == RUDA_MIR_INTERPRET_INVALID_SHIFT);
    ruda_mir_function_dispose(&function);

    RUDA_EXPECT(ruda_mir_function_init(
        &function,
        ruda_system_allocator(),
        symbol,
        parameters,
        2,
        RUDA_TYPE_I64,
        ruda_mir_function_default_options()
    ));
    RUDA_EXPECT(ruda_mir_add_register(
        &function,
        RUDA_TYPE_I64,
        &shifted
    ));
    RUDA_EXPECT(ruda_mir_add_block(&function, &block));
    instruction = ruda_test_mir_binary(
        RUDA_MIR_SHIFT_RIGHT_I64,
        shifted,
        0,
        1
    );
    RUDA_EXPECT(ruda_mir_append_instruction(&function, block, &instruction));
    RUDA_EXPECT(ruda_mir_set_terminator(
        &function,
        block,
        ruda_test_mir_return(shifted)
    ));
    RUDA_EXPECT(ruda_mir_verify(&function) == RUDA_MIR_VERIFY_OK);
    arguments[0].as.i64 = -7;
    arguments[1].as.i64 = 1;
    RUDA_EXPECT(ruda_mir_interpret(
        &function,
        arguments,
        2,
        ruda_system_allocator(),
        ruda_mir_interpret_default_options(),
        &result
    ) == RUDA_MIR_INTERPRET_OK);
    RUDA_EXPECT(result.as.i64 == -4);
    arguments[0].as.i64 = INT64_MIN;
    arguments[1].as.i64 = 63;
    RUDA_EXPECT(ruda_mir_interpret(
        &function,
        arguments,
        2,
        ruda_system_allocator(),
        ruda_mir_interpret_default_options(),
        &result
    ) == RUDA_MIR_INTERPRET_OK);
    RUDA_EXPECT(result.as.i64 == -1);
    arguments[0].as.i64 = 7;
    arguments[1].as.i64 = 1;
    RUDA_EXPECT(ruda_mir_interpret(
        &function,
        arguments,
        2,
        ruda_system_allocator(),
        ruda_mir_interpret_default_options(),
        &result
    ) == RUDA_MIR_INTERPRET_OK);
    RUDA_EXPECT(result.as.i64 == 3);
    arguments[1].as.i64 = 64;
    RUDA_EXPECT(ruda_mir_interpret(
        &function,
        arguments,
        2,
        ruda_system_allocator(),
        ruda_mir_interpret_default_options(),
        &result
    ) == RUDA_MIR_INTERPRET_INVALID_SHIFT);
    ruda_mir_function_dispose(&function);

    {
        RudaTypeId invalid_parameters[] = {RUDA_TYPE_BOOL, RUDA_TYPE_BOOL};
        RUDA_EXPECT(ruda_mir_function_init(
            &function,
            ruda_system_allocator(),
            symbol,
            invalid_parameters,
            2,
            RUDA_TYPE_I64,
            ruda_mir_function_default_options()
        ));
        RUDA_EXPECT(ruda_mir_add_register(
            &function,
            RUDA_TYPE_I64,
            &shifted
        ));
        RUDA_EXPECT(ruda_mir_add_block(&function, &block));
        instruction = ruda_test_mir_binary(
            RUDA_MIR_SHIFT_RIGHT_I64,
            shifted,
            0,
            1
        );
        RUDA_EXPECT(ruda_mir_append_instruction(
            &function,
            block,
            &instruction
        ));
        RUDA_EXPECT(ruda_mir_set_terminator(
            &function,
            block,
            ruda_test_mir_return(shifted)
        ));
        RUDA_EXPECT(
            ruda_mir_verify(&function) == RUDA_MIR_VERIFY_TYPE_MISMATCH
        );
        ruda_mir_function_dispose(&function);
    }
}

static void ruda_test_mir_records(RudaSymbolId symbol) {
    const RudaTypeId record_type = RUDA_TYPE_FIRST_DYNAMIC;
    RudaMirRecordField fields[2];
    RudaMirFunction function;
    RudaMirRegister first = RUDA_MIR_NO_REGISTER;
    RudaMirRegister second = RUDA_MIR_NO_REGISTER;
    RudaMirRegister record_value = RUDA_MIR_NO_REGISTER;
    RudaMirRegister alias = RUDA_MIR_NO_REGISTER;
    RudaMirRegister replacement = RUDA_MIR_NO_REGISTER;
    RudaMirRegister set_result = RUDA_MIR_NO_REGISTER;
    RudaMirRegister projected = RUDA_MIR_NO_REGISTER;
    RudaMirRegister uninitialized = RUDA_MIR_NO_REGISTER;
    RudaMirBlockId block = RUDA_MIR_NO_BLOCK;
    RudaMirRecordId record = RUDA_MIR_NO_RECORD;
    RudaMirRecordConstructionId construction =
        RUDA_MIR_NO_RECORD_CONSTRUCTION;
    RudaMirRecordArgument construction_arguments[2];
    RudaMirInstruction instruction;
    RudaMirExecution execution;
    RudaMirInterpretOptions interpret_options =
        ruda_mir_interpret_default_options();
    RudaMirValue result;

    fields[0].type = RUDA_TYPE_I64;
    fields[0].is_mutable = false;
    fields[1].type = RUDA_TYPE_I64;
    fields[1].is_mutable = true;
    RUDA_EXPECT(ruda_mir_function_init(
        &function,
        ruda_system_allocator(),
        symbol,
        NULL,
        0,
        RUDA_TYPE_I64,
        ruda_mir_function_default_options()
    ));
    RUDA_EXPECT(ruda_mir_add_register(&function, RUDA_TYPE_I64, &first));
    RUDA_EXPECT(ruda_mir_add_register(&function, RUDA_TYPE_I64, &second));
    RUDA_EXPECT(ruda_mir_add_register(
        &function,
        record_type,
        &record_value
    ));
    RUDA_EXPECT(ruda_mir_add_register(&function, record_type, &alias));
    RUDA_EXPECT(ruda_mir_add_register(
        &function,
        RUDA_TYPE_I64,
        &replacement
    ));
    RUDA_EXPECT(ruda_mir_add_register(
        &function,
        RUDA_TYPE_I64,
        &set_result
    ));
    RUDA_EXPECT(ruda_mir_add_register(
        &function,
        RUDA_TYPE_I64,
        &projected
    ));
    RUDA_EXPECT(ruda_mir_add_register(
        &function,
        RUDA_TYPE_I64,
        &uninitialized
    ));
    RUDA_EXPECT(ruda_mir_add_block(&function, &block));
    RUDA_EXPECT(ruda_mir_add_record(
        &function,
        record_type,
        fields,
        2,
        &record
    ));
    {
        RudaMirRecordId duplicate = RUDA_MIR_NO_RECORD;
        RudaMirRecordField mismatch[2] = {fields[0], fields[1]};
        RUDA_EXPECT(ruda_mir_add_record(
            &function,
            record_type,
            fields,
            2,
            &duplicate
        ));
        RUDA_EXPECT(duplicate == record);
        mismatch[1].is_mutable = false;
        RUDA_EXPECT(!ruda_mir_add_record(
            &function,
            record_type,
            mismatch,
            2,
            &duplicate
        ));
    }
    construction_arguments[0].register_id = first;
    construction_arguments[0].type = RUDA_TYPE_I64;
    construction_arguments[1].register_id = second;
    construction_arguments[1].type = RUDA_TYPE_I64;
    RUDA_EXPECT(ruda_mir_add_record_construction(
        &function,
        record,
        construction_arguments,
        2,
        &construction
    ));
    {
        RudaMirRecordConstructionId invalid =
            RUDA_MIR_NO_RECORD_CONSTRUCTION;
        RUDA_EXPECT(!ruda_mir_add_record_construction(
            &function,
            record,
            construction_arguments,
            1,
            &invalid
        ));
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_CONSTANT_I64;
    instruction.target = first;
    instruction.integer = 10;
    RUDA_EXPECT(ruda_mir_append_instruction(&function, block, &instruction));
    instruction.target = second;
    instruction.integer = 20;
    RUDA_EXPECT(ruda_mir_append_instruction(&function, block, &instruction));
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_RECORD_NEW;
    instruction.target = record_value;
    instruction.record = record;
    instruction.record_construction = construction;
    RUDA_EXPECT(ruda_mir_append_instruction(&function, block, &instruction));
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_COPY;
    instruction.target = alias;
    instruction.first = record_value;
    RUDA_EXPECT(ruda_mir_append_instruction(&function, block, &instruction));
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_CONSTANT_I64;
    instruction.target = replacement;
    instruction.integer = 30;
    RUDA_EXPECT(ruda_mir_append_instruction(&function, block, &instruction));
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_RECORD_SET;
    instruction.target = set_result;
    instruction.first = record_value;
    instruction.second = replacement;
    instruction.record = record;
    instruction.field = 1;
    RUDA_EXPECT(ruda_mir_append_instruction(&function, block, &instruction));
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_RECORD_GET;
    instruction.target = projected;
    instruction.first = alias;
    instruction.record = record;
    instruction.field = 1;
    RUDA_EXPECT(ruda_mir_append_instruction(&function, block, &instruction));
    RUDA_EXPECT(ruda_mir_set_terminator(
        &function,
        block,
        ruda_test_mir_return(projected)
    ));
    RUDA_EXPECT(ruda_mir_verify(&function) == RUDA_MIR_VERIFY_OK);
    {
        RudaMirRecordId other = RUDA_MIR_NO_RECORD;
        RudaMirRecord *other_layout = NULL;
        RUDA_EXPECT(ruda_mir_add_record(
            &function,
            record_type + 1,
            fields,
            2,
            &other
        ));
        other_layout = ruda_vector_at(&function.records, other);
        RUDA_EXPECT(other_layout != NULL);
        if (other_layout != NULL) {
            other_layout->type = record_type;
            RUDA_EXPECT(
                ruda_mir_verify(&function)
                    == RUDA_MIR_VERIFY_INVALID_ARGUMENT
            );
            other_layout->type = record_type + 1;
        }
        RUDA_EXPECT(ruda_mir_verify(&function) == RUDA_MIR_VERIFY_OK);
    }
    RUDA_EXPECT(ruda_mir_interpret(
        &function,
        NULL,
        0,
        ruda_system_allocator(),
        interpret_options,
        &result
    ) == RUDA_MIR_INTERPRET_INVALID_ARGUMENT);
    RUDA_EXPECT(ruda_mir_execution_init(
        &execution,
        ruda_system_allocator(),
        ruda_mir_execution_default_options()
    ));
    interpret_options.execution = &execution;
    RUDA_EXPECT(ruda_mir_interpret(
        &function,
        NULL,
        0,
        ruda_system_allocator(),
        interpret_options,
        &result
    ) == RUDA_MIR_INTERPRET_OK);
    RUDA_EXPECT(result.type == RUDA_TYPE_I64);
    RUDA_EXPECT(result.as.i64 == 30);
    RUDA_EXPECT(execution.objects.length == 1);
    RUDA_EXPECT(execution.total_fields == 2);
    {
        RudaMirFunction mismatch;
        RudaTypeId parameter = record_type;
        RudaMirRecordField mismatch_fields[2];
        RudaMirRecordId mismatch_record = RUDA_MIR_NO_RECORD;
        RudaMirRegister mismatch_value = RUDA_MIR_NO_REGISTER;
        RudaMirBlockId mismatch_block = RUDA_MIR_NO_BLOCK;
        RudaMirValue mismatch_argument;
        mismatch_fields[0].type = RUDA_TYPE_I64;
        mismatch_fields[0].is_mutable = true;
        mismatch_fields[1].type = RUDA_TYPE_I64;
        mismatch_fields[1].is_mutable = true;
        RUDA_EXPECT(ruda_mir_function_init(
            &mismatch,
            ruda_system_allocator(),
            symbol,
            &parameter,
            1,
            RUDA_TYPE_I64,
            ruda_mir_function_default_options()
        ));
        RUDA_EXPECT(ruda_mir_add_register(
            &mismatch,
            RUDA_TYPE_I64,
            &mismatch_value
        ));
        RUDA_EXPECT(ruda_mir_add_block(&mismatch, &mismatch_block));
        RUDA_EXPECT(ruda_mir_add_record(
            &mismatch,
            record_type,
            mismatch_fields,
            2,
            &mismatch_record
        ));
        memset(&instruction, 0, sizeof(instruction));
        instruction.kind = RUDA_MIR_RECORD_GET;
        instruction.target = mismatch_value;
        instruction.first = 0;
        instruction.record = mismatch_record;
        instruction.field = 0;
        RUDA_EXPECT(ruda_mir_append_instruction(
            &mismatch,
            mismatch_block,
            &instruction
        ));
        RUDA_EXPECT(ruda_mir_set_terminator(
            &mismatch,
            mismatch_block,
            ruda_test_mir_return(mismatch_value)
        ));
        RUDA_EXPECT(ruda_mir_verify(&mismatch) == RUDA_MIR_VERIFY_OK);
        mismatch_argument.type = record_type;
        mismatch_argument.as.reference = 0;
        RUDA_EXPECT(ruda_mir_interpret(
            &mismatch,
            &mismatch_argument,
            1,
            ruda_system_allocator(),
            interpret_options,
            &result
        ) == RUDA_MIR_INTERPRET_INVALID_MIR);
        ruda_mir_function_dispose(&mismatch);
    }
    ruda_mir_execution_dispose(&execution);

    {
        RudaMirBlock *mutable_block = ruda_vector_at(
            &function.blocks,
            block
        );
        RudaMirInstruction *set = mutable_block != NULL
            ? ruda_vector_at(&mutable_block->instructions, 5)
            : NULL;
        RudaTypeId *projected_type = ruda_vector_at(
            &function.register_types,
            projected
        );
        RudaMirRecordArgument *argument = ruda_vector_at(
            &function.record_arguments,
            0
        );
        RUDA_EXPECT(set != NULL);
        RUDA_EXPECT(projected_type != NULL);
        RUDA_EXPECT(argument != NULL);
        if (set != NULL) {
            set->field = 0;
            RUDA_EXPECT(
                ruda_mir_verify(&function) == RUDA_MIR_VERIFY_TYPE_MISMATCH
            );
            set->field = 1;
        }
        if (projected_type != NULL) {
            *projected_type = RUDA_TYPE_BOOL;
            RUDA_EXPECT(
                ruda_mir_verify(&function) == RUDA_MIR_VERIFY_TYPE_MISMATCH
            );
            *projected_type = RUDA_TYPE_I64;
        }
        if (argument != NULL) {
            argument->register_id = uninitialized;
            RUDA_EXPECT(
                ruda_mir_verify(&function)
                    == RUDA_MIR_VERIFY_UNINITIALIZED_REGISTER
            );
            argument->register_id = first;
        }
        RUDA_EXPECT(ruda_mir_verify(&function) == RUDA_MIR_VERIFY_OK);
    }

    {
        size_t failure_offset = 0;
        bool reached_success = false;
        for (failure_offset = 0; failure_offset < 8; failure_offset += 1) {
            RudaFailAllocator state;
            RudaAllocator allocator;
            RudaMirInterpretStatus status;
            ruda_fail_allocator_init(
                &state,
                ruda_system_allocator(),
                SIZE_MAX
            );
            allocator = ruda_fail_allocator(&state);
            RUDA_EXPECT(ruda_mir_execution_init(
                &execution,
                allocator,
                ruda_mir_execution_default_options()
            ));
            state.failure_index = state.attempts + failure_offset;
            interpret_options.execution = &execution;
            status = ruda_mir_interpret(
                &function,
                NULL,
                0,
                ruda_system_allocator(),
                interpret_options,
                &result
            );
            if (status == RUDA_MIR_INTERPRET_OK) {
                reached_success = true;
                RUDA_EXPECT(result.as.i64 == 30);
            } else {
                RUDA_EXPECT(
                    status == RUDA_MIR_INTERPRET_ALLOCATION_FAILED
                );
            }
            ruda_mir_execution_dispose(&execution);
            RUDA_EXPECT(state.live_allocations == 0);
            RUDA_EXPECT(state.live_bytes == 0);
            if (reached_success) {
                break;
            }
        }
        RUDA_EXPECT(reached_success);
    }
    ruda_mir_function_dispose(&function);

    {
        RudaTypeId parameter = record_type;
        RudaMirRegister value = RUDA_MIR_NO_REGISTER;
        RudaMirValue argument;
        RUDA_EXPECT(ruda_mir_function_init(
            &function,
            ruda_system_allocator(),
            symbol,
            &parameter,
            1,
            RUDA_TYPE_I64,
            ruda_mir_function_default_options()
        ));
        RUDA_EXPECT(ruda_mir_add_register(
            &function,
            RUDA_TYPE_I64,
            &value
        ));
        RUDA_EXPECT(ruda_mir_add_block(&function, &block));
        RUDA_EXPECT(ruda_mir_add_record(
            &function,
            record_type,
            fields,
            2,
            &record
        ));
        memset(&instruction, 0, sizeof(instruction));
        instruction.kind = RUDA_MIR_RECORD_GET;
        instruction.target = value;
        instruction.first = 0;
        instruction.record = record;
        instruction.field = 0;
        RUDA_EXPECT(ruda_mir_append_instruction(
            &function,
            block,
            &instruction
        ));
        RUDA_EXPECT(ruda_mir_set_terminator(
            &function,
            block,
            ruda_test_mir_return(value)
        ));
        RUDA_EXPECT(ruda_mir_verify(&function) == RUDA_MIR_VERIFY_OK);
        RUDA_EXPECT(ruda_mir_execution_init(
            &execution,
            ruda_system_allocator(),
            ruda_mir_execution_default_options()
        ));
        interpret_options.execution = &execution;
        argument.type = record_type;
        argument.as.reference = RUDA_MIR_INVALID_REFERENCE;
        RUDA_EXPECT(ruda_mir_interpret(
            &function,
            &argument,
            1,
            ruda_system_allocator(),
            interpret_options,
            &result
        ) == RUDA_MIR_INTERPRET_INVALID_REFERENCE);
        ruda_mir_execution_dispose(&execution);
        ruda_mir_function_dispose(&function);
    }

    {
        RudaMirExecutionOptions execution_options =
            ruda_mir_execution_default_options();
        RudaMirRegister value = RUDA_MIR_NO_REGISTER;
        RudaMirRegister first_record = RUDA_MIR_NO_REGISTER;
        RudaMirRegister second_record = RUDA_MIR_NO_REGISTER;
        execution_options.max_objects = 1;
        RUDA_EXPECT(ruda_mir_function_init(
            &function,
            ruda_system_allocator(),
            symbol,
            NULL,
            0,
            RUDA_TYPE_I64,
            ruda_mir_function_default_options()
        ));
        RUDA_EXPECT(ruda_mir_add_register(&function, RUDA_TYPE_I64, &value));
        RUDA_EXPECT(ruda_mir_add_register(
            &function,
            record_type,
            &first_record
        ));
        RUDA_EXPECT(ruda_mir_add_register(
            &function,
            record_type,
            &second_record
        ));
        RUDA_EXPECT(ruda_mir_add_block(&function, &block));
        RUDA_EXPECT(ruda_mir_add_record(
            &function,
            record_type,
            fields,
            2,
            &record
        ));
        construction_arguments[0].register_id = value;
        construction_arguments[1].register_id = value;
        RUDA_EXPECT(ruda_mir_add_record_construction(
            &function,
            record,
            construction_arguments,
            2,
            &construction
        ));
        memset(&instruction, 0, sizeof(instruction));
        instruction.kind = RUDA_MIR_CONSTANT_I64;
        instruction.target = value;
        instruction.integer = 1;
        RUDA_EXPECT(ruda_mir_append_instruction(
            &function,
            block,
            &instruction
        ));
        memset(&instruction, 0, sizeof(instruction));
        instruction.kind = RUDA_MIR_RECORD_NEW;
        instruction.target = first_record;
        instruction.record = record;
        instruction.record_construction = construction;
        RUDA_EXPECT(ruda_mir_append_instruction(
            &function,
            block,
            &instruction
        ));
        instruction.target = second_record;
        RUDA_EXPECT(ruda_mir_append_instruction(
            &function,
            block,
            &instruction
        ));
        RUDA_EXPECT(ruda_mir_set_terminator(
            &function,
            block,
            ruda_test_mir_return(value)
        ));
        RUDA_EXPECT(ruda_mir_execution_init(
            &execution,
            ruda_system_allocator(),
            execution_options
        ));
        interpret_options.execution = &execution;
        RUDA_EXPECT(ruda_mir_interpret(
            &function,
            NULL,
            0,
            ruda_system_allocator(),
            interpret_options,
            &result
        ) == RUDA_MIR_INTERPRET_OBJECT_LIMIT);
        RUDA_EXPECT(execution.objects.length == 1);
        RUDA_EXPECT(execution.total_fields == 2);
        ruda_mir_execution_dispose(&execution);
        ruda_mir_function_dispose(&function);
    }

    {
        RudaMirFunctionOptions options = ruda_mir_function_default_options();
        RudaMirExecutionOptions execution_options =
            ruda_mir_execution_default_options();
        options.max_record_layouts = 0;
        RUDA_EXPECT(!ruda_mir_function_init(
            &function,
            ruda_system_allocator(),
            symbol,
            NULL,
            0,
            RUDA_TYPE_I64,
            options
        ));
        execution_options.max_objects = 0;
        RUDA_EXPECT(!ruda_mir_execution_init(
            &execution,
            ruda_system_allocator(),
            execution_options
        ));
    }
}

static void ruda_test_mir_record_identity(RudaSymbolId symbol) {
    const RudaTypeId record_type = RUDA_TYPE_FIRST_DYNAMIC + 10;
    RudaMirRecordField field;
    RudaMirRecordArgument construction_argument;
    RudaMirFunction constructor;
    RudaMirFunction comparison;
    RudaMirRecordId constructor_record = RUDA_MIR_NO_RECORD;
    RudaMirRecordId comparison_record = RUDA_MIR_NO_RECORD;
    RudaMirRecordConstructionId construction =
        RUDA_MIR_NO_RECORD_CONSTRUCTION;
    RudaMirRegister value = RUDA_MIR_NO_REGISTER;
    RudaMirRegister object = RUDA_MIR_NO_REGISTER;
    RudaMirRegister same = RUDA_MIR_NO_REGISTER;
    RudaMirRegister uninitialized = RUDA_MIR_NO_REGISTER;
    RudaMirBlockId block = RUDA_MIR_NO_BLOCK;
    RudaMirInstruction instruction;
    RudaTypeId parameters[2] = {record_type, record_type};
    RudaMirExecution execution;
    RudaMirInterpretOptions options = ruda_mir_interpret_default_options();
    RudaMirValue first;
    RudaMirValue second;
    RudaMirValue arguments[2];
    RudaMirValue result;

    field.type = RUDA_TYPE_I64;
    field.is_mutable = false;
    RUDA_EXPECT(ruda_mir_function_init(
        &constructor,
        ruda_system_allocator(),
        symbol,
        NULL,
        0,
        record_type,
        ruda_mir_function_default_options()
    ));
    RUDA_EXPECT(ruda_mir_add_register(&constructor, RUDA_TYPE_I64, &value));
    RUDA_EXPECT(ruda_mir_add_register(&constructor, record_type, &object));
    RUDA_EXPECT(ruda_mir_add_block(&constructor, &block));
    RUDA_EXPECT(ruda_mir_add_record(
        &constructor,
        record_type,
        &field,
        1,
        &constructor_record
    ));
    construction_argument.register_id = value;
    construction_argument.type = RUDA_TYPE_I64;
    RUDA_EXPECT(ruda_mir_add_record_construction(
        &constructor,
        constructor_record,
        &construction_argument,
        1,
        &construction
    ));
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_CONSTANT_I64;
    instruction.target = value;
    instruction.integer = 42;
    RUDA_EXPECT(ruda_mir_append_instruction(
        &constructor,
        block,
        &instruction
    ));
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_RECORD_NEW;
    instruction.target = object;
    instruction.record = constructor_record;
    instruction.record_construction = construction;
    RUDA_EXPECT(ruda_mir_append_instruction(
        &constructor,
        block,
        &instruction
    ));
    RUDA_EXPECT(ruda_mir_set_terminator(
        &constructor,
        block,
        ruda_test_mir_return(object)
    ));
    RUDA_EXPECT(ruda_mir_verify(&constructor) == RUDA_MIR_VERIFY_OK);

    block = RUDA_MIR_NO_BLOCK;
    RUDA_EXPECT(ruda_mir_function_init(
        &comparison,
        ruda_system_allocator(),
        symbol,
        parameters,
        2,
        RUDA_TYPE_BOOL,
        ruda_mir_function_default_options()
    ));
    RUDA_EXPECT(ruda_mir_add_register(&comparison, RUDA_TYPE_BOOL, &same));
    RUDA_EXPECT(ruda_mir_add_register(
        &comparison,
        record_type,
        &uninitialized
    ));
    RUDA_EXPECT(ruda_mir_add_block(&comparison, &block));
    RUDA_EXPECT(ruda_mir_add_record(
        &comparison,
        record_type,
        &field,
        1,
        &comparison_record
    ));
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_RECORD_IDENTITY_SAME;
    instruction.target = same;
    instruction.first = 0;
    instruction.second = 1;
    instruction.record = comparison_record;
    RUDA_EXPECT(ruda_mir_append_instruction(
        &comparison,
        block,
        &instruction
    ));
    RUDA_EXPECT(ruda_mir_set_terminator(
        &comparison,
        block,
        ruda_test_mir_return(same)
    ));
    RUDA_EXPECT(ruda_mir_verify(&comparison) == RUDA_MIR_VERIFY_OK);
    {
        RudaMirBlock *mutable_block = ruda_vector_at(
            &comparison.blocks,
            block
        );
        RudaMirInstruction *identity = mutable_block != NULL
            ? ruda_vector_at(&mutable_block->instructions, 0)
            : NULL;
        RudaTypeId *target_type = ruda_vector_at(
            &comparison.register_types,
            same
        );
        RudaTypeId *second_type = ruda_vector_at(
            &comparison.register_types,
            1
        );
        RUDA_EXPECT(identity != NULL);
        RUDA_EXPECT(target_type != NULL);
        RUDA_EXPECT(second_type != NULL);
        if (identity != NULL) {
            identity->second = uninitialized;
            RUDA_EXPECT(
                ruda_mir_verify(&comparison)
                    == RUDA_MIR_VERIFY_UNINITIALIZED_REGISTER
            );
            identity->second = 1;
            identity->record = RUDA_MIR_NO_RECORD;
            RUDA_EXPECT(
                ruda_mir_verify(&comparison)
                    == RUDA_MIR_VERIFY_INVALID_ARGUMENT
            );
            identity->record = comparison_record;
        }
        if (target_type != NULL) {
            *target_type = RUDA_TYPE_I64;
            RUDA_EXPECT(
                ruda_mir_verify(&comparison)
                    == RUDA_MIR_VERIFY_TYPE_MISMATCH
            );
            *target_type = RUDA_TYPE_BOOL;
        }
        if (second_type != NULL) {
            *second_type = record_type + 1;
            RUDA_EXPECT(
                ruda_mir_verify(&comparison)
                    == RUDA_MIR_VERIFY_TYPE_MISMATCH
            );
            *second_type = record_type;
        }
    }
    RUDA_EXPECT(ruda_mir_verify(&comparison) == RUDA_MIR_VERIFY_OK);
    RUDA_EXPECT(ruda_mir_execution_init(
        &execution,
        ruda_system_allocator(),
        ruda_mir_execution_default_options()
    ));
    options.execution = &execution;
    RUDA_EXPECT(ruda_mir_interpret(
        &constructor,
        NULL,
        0,
        ruda_system_allocator(),
        options,
        &first
    ) == RUDA_MIR_INTERPRET_OK);
    RUDA_EXPECT(ruda_mir_interpret(
        &constructor,
        NULL,
        0,
        ruda_system_allocator(),
        options,
        &second
    ) == RUDA_MIR_INTERPRET_OK);
    RUDA_EXPECT(first.as.reference != second.as.reference);
    arguments[0] = first;
    arguments[1] = first;
    RUDA_EXPECT(ruda_mir_interpret(
        &comparison,
        arguments,
        2,
        ruda_system_allocator(),
        options,
        &result
    ) == RUDA_MIR_INTERPRET_OK);
    RUDA_EXPECT(result.type == RUDA_TYPE_BOOL);
    RUDA_EXPECT(result.as.boolean);
    arguments[1] = second;
    RUDA_EXPECT(ruda_mir_interpret(
        &comparison,
        arguments,
        2,
        ruda_system_allocator(),
        options,
        &result
    ) == RUDA_MIR_INTERPRET_OK);
    RUDA_EXPECT(!result.as.boolean);
    arguments[1].as.reference = RUDA_MIR_INVALID_REFERENCE;
    RUDA_EXPECT(ruda_mir_interpret(
        &comparison,
        arguments,
        2,
        ruda_system_allocator(),
        options,
        &result
    ) == RUDA_MIR_INTERPRET_INVALID_REFERENCE);
    field.is_mutable = true;
    {
        RudaMirRecordField *mismatched = ruda_vector_at(
            &comparison.record_fields,
            0
        );
        RUDA_EXPECT(mismatched != NULL);
        if (mismatched != NULL) {
            *mismatched = field;
        }
    }
    arguments[1] = first;
    RUDA_EXPECT(ruda_mir_interpret(
        &comparison,
        arguments,
        2,
        ruda_system_allocator(),
        options,
        &result
    ) == RUDA_MIR_INTERPRET_INVALID_MIR);
    ruda_mir_execution_dispose(&execution);
    ruda_mir_function_dispose(&comparison);
    ruda_mir_function_dispose(&constructor);
}

static void ruda_test_mir_enums(RudaSymbolId symbol) {
    const RudaTypeId enum_type = RUDA_TYPE_FIRST_DYNAMIC + 20;
    RudaMirEnumVariant variants[3];
    RudaMirEnumPayload payloads[4];
    RudaMirFunction function;
    RudaMirRegister first = RUDA_MIR_NO_REGISTER;
    RudaMirRegister ready = RUDA_MIR_NO_REGISTER;
    RudaMirRegister x = RUDA_MIR_NO_REGISTER;
    RudaMirRegister y = RUDA_MIR_NO_REGISTER;
    RudaMirRegister uninitialized = RUDA_MIR_NO_REGISTER;
    RudaMirRegister unit_value = RUDA_MIR_NO_REGISTER;
    RudaMirRegister positional_value = RUDA_MIR_NO_REGISTER;
    RudaMirRegister named_value = RUDA_MIR_NO_REGISTER;
    RudaMirBlockId block = RUDA_MIR_NO_BLOCK;
    RudaMirEnumId enum_layout = RUDA_MIR_NO_ENUM;
    RudaMirEnumConstructionId unit = RUDA_MIR_NO_ENUM_CONSTRUCTION;
    RudaMirEnumConstructionId positional = RUDA_MIR_NO_ENUM_CONSTRUCTION;
    RudaMirEnumConstructionId named = RUDA_MIR_NO_ENUM_CONSTRUCTION;
    RudaMirEnumArgument positional_arguments[2];
    RudaMirEnumArgument named_arguments[2];
    RudaMirInstruction instruction;
    RudaMirExecution execution;
    RudaMirExecutionOptions execution_options =
        ruda_mir_execution_default_options();
    RudaMirInterpretOptions interpret_options =
        ruda_mir_interpret_default_options();
    RudaMirValue result;
    RudaMirValue payload;
    size_t selected_variant = SIZE_MAX;
    size_t payload_count = SIZE_MAX;

    variants[0].kind = RUDA_MIR_ENUM_VARIANT_UNIT;
    variants[0].payload_start = 0;
    variants[0].payload_count = 0;
    variants[1].kind = RUDA_MIR_ENUM_VARIANT_POSITIONAL;
    variants[1].payload_start = 0;
    variants[1].payload_count = 2;
    variants[2].kind = RUDA_MIR_ENUM_VARIANT_NAMED;
    variants[2].payload_start = 2;
    variants[2].payload_count = 2;
    payloads[0].type = RUDA_TYPE_I64;
    payloads[1].type = RUDA_TYPE_BOOL;
    payloads[2].type = RUDA_TYPE_I64;
    payloads[3].type = RUDA_TYPE_I64;
    RUDA_EXPECT(ruda_mir_function_init(
        &function,
        ruda_system_allocator(),
        symbol,
        NULL,
        0,
        enum_type,
        ruda_mir_function_default_options()
    ));
    RUDA_EXPECT(ruda_mir_add_register(&function, RUDA_TYPE_I64, &first));
    RUDA_EXPECT(ruda_mir_add_register(&function, RUDA_TYPE_BOOL, &ready));
    RUDA_EXPECT(ruda_mir_add_register(&function, RUDA_TYPE_I64, &x));
    RUDA_EXPECT(ruda_mir_add_register(&function, RUDA_TYPE_I64, &y));
    RUDA_EXPECT(ruda_mir_add_register(
        &function,
        RUDA_TYPE_I64,
        &uninitialized
    ));
    RUDA_EXPECT(ruda_mir_add_register(
        &function,
        enum_type,
        &unit_value
    ));
    RUDA_EXPECT(ruda_mir_add_register(
        &function,
        enum_type,
        &positional_value
    ));
    RUDA_EXPECT(ruda_mir_add_register(
        &function,
        enum_type,
        &named_value
    ));
    RUDA_EXPECT(ruda_mir_add_block(&function, &block));
    RUDA_EXPECT(ruda_mir_add_enum(
        &function,
        enum_type,
        variants,
        3,
        payloads,
        4,
        &enum_layout
    ));
    {
        RudaMirEnumId duplicate = RUDA_MIR_NO_ENUM;
        RudaMirEnumVariant mismatch[3] = {
            variants[0], variants[1], variants[2]
        };
        RUDA_EXPECT(ruda_mir_add_enum(
            &function,
            enum_type,
            variants,
            3,
            payloads,
            4,
            &duplicate
        ));
        RUDA_EXPECT(duplicate == enum_layout);
        mismatch[2].kind = RUDA_MIR_ENUM_VARIANT_POSITIONAL;
        RUDA_EXPECT(!ruda_mir_add_enum(
            &function,
            enum_type,
            mismatch,
            3,
            payloads,
            4,
            &duplicate
        ));
    }
    RUDA_EXPECT(ruda_mir_add_enum_construction(
        &function,
        enum_layout,
        0,
        NULL,
        0,
        &unit
    ));
    positional_arguments[0].register_id = first;
    positional_arguments[0].type = RUDA_TYPE_I64;
    positional_arguments[1].register_id = ready;
    positional_arguments[1].type = RUDA_TYPE_BOOL;
    RUDA_EXPECT(ruda_mir_add_enum_construction(
        &function,
        enum_layout,
        1,
        positional_arguments,
        2,
        &positional
    ));
    named_arguments[0].register_id = x;
    named_arguments[0].type = RUDA_TYPE_I64;
    named_arguments[1].register_id = y;
    named_arguments[1].type = RUDA_TYPE_I64;
    RUDA_EXPECT(ruda_mir_add_enum_construction(
        &function,
        enum_layout,
        2,
        named_arguments,
        2,
        &named
    ));
    {
        RudaMirEnumConstructionId invalid = RUDA_MIR_NO_ENUM_CONSTRUCTION;
        RUDA_EXPECT(!ruda_mir_add_enum_construction(
            &function,
            enum_layout,
            2,
            named_arguments,
            1,
            &invalid
        ));
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_ENUM_NEW;
    instruction.target = unit_value;
    instruction.enum_id = enum_layout;
    instruction.enum_construction = unit;
    RUDA_EXPECT(ruda_mir_append_instruction(&function, block, &instruction));
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_CONSTANT_I64;
    instruction.target = first;
    instruction.integer = 41;
    RUDA_EXPECT(ruda_mir_append_instruction(&function, block, &instruction));
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_CONSTANT_BOOL;
    instruction.target = ready;
    instruction.boolean = true;
    RUDA_EXPECT(ruda_mir_append_instruction(&function, block, &instruction));
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_ENUM_NEW;
    instruction.target = positional_value;
    instruction.enum_id = enum_layout;
    instruction.enum_construction = positional;
    RUDA_EXPECT(ruda_mir_append_instruction(&function, block, &instruction));
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_CONSTANT_I64;
    instruction.target = x;
    instruction.integer = 10;
    RUDA_EXPECT(ruda_mir_append_instruction(&function, block, &instruction));
    instruction.target = y;
    instruction.integer = 20;
    RUDA_EXPECT(ruda_mir_append_instruction(&function, block, &instruction));
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_ENUM_NEW;
    instruction.target = named_value;
    instruction.enum_id = enum_layout;
    instruction.enum_construction = named;
    RUDA_EXPECT(ruda_mir_append_instruction(&function, block, &instruction));
    RUDA_EXPECT(ruda_mir_set_terminator(
        &function,
        block,
        ruda_test_mir_return(named_value)
    ));
    RUDA_EXPECT(ruda_mir_verify(&function) == RUDA_MIR_VERIFY_OK);
    RUDA_EXPECT(ruda_mir_interpret(
        &function,
        NULL,
        0,
        ruda_system_allocator(),
        interpret_options,
        &result
    ) == RUDA_MIR_INTERPRET_INVALID_ARGUMENT);
    RUDA_EXPECT(ruda_mir_execution_init(
        &execution,
        ruda_system_allocator(),
        execution_options
    ));
    interpret_options.execution = &execution;
    RUDA_EXPECT(ruda_mir_interpret(
        &function,
        NULL,
        0,
        ruda_system_allocator(),
        interpret_options,
        &result
    ) == RUDA_MIR_INTERPRET_OK);
    RUDA_EXPECT(result.type == enum_type);
    RUDA_EXPECT(result.as.reference == 2);
    RUDA_EXPECT(execution.enums.length == 3);
    RUDA_EXPECT(ruda_mir_execution_inspect_enum(
        &execution,
        result,
        &selected_variant,
        &payload_count
    ));
    RUDA_EXPECT(selected_variant == 2);
    RUDA_EXPECT(payload_count == 2);
    RUDA_EXPECT(ruda_mir_execution_enum_payload(
        &execution,
        result,
        0,
        &payload
    ));
    RUDA_EXPECT(payload.type == RUDA_TYPE_I64);
    RUDA_EXPECT(payload.as.i64 == 10);
    RUDA_EXPECT(ruda_mir_execution_enum_payload(
        &execution,
        result,
        1,
        &payload
    ));
    RUDA_EXPECT(payload.as.i64 == 20);
    RUDA_EXPECT(!ruda_mir_execution_enum_payload(
        &execution,
        result,
        2,
        &payload
    ));
    ruda_mir_execution_dispose(&execution);

    {
        RudaMirEnumArgument *argument = ruda_vector_at(
            &function.enum_arguments,
            2
        );
        RudaTypeId *target_type = ruda_vector_at(
            &function.register_types,
            named_value
        );
        RUDA_EXPECT(argument != NULL);
        RUDA_EXPECT(target_type != NULL);
        if (argument != NULL) {
            argument->register_id = uninitialized;
            RUDA_EXPECT(
                ruda_mir_verify(&function)
                    == RUDA_MIR_VERIFY_UNINITIALIZED_REGISTER
            );
            argument->register_id = x;
        }
        if (target_type != NULL) {
            *target_type = RUDA_TYPE_BOOL;
            RUDA_EXPECT(
                ruda_mir_verify(&function) == RUDA_MIR_VERIFY_TYPE_MISMATCH
            );
            *target_type = enum_type;
        }
        RUDA_EXPECT(ruda_mir_verify(&function) == RUDA_MIR_VERIFY_OK);
    }

    execution_options = ruda_mir_execution_default_options();
    execution_options.max_objects = 2;
    RUDA_EXPECT(ruda_mir_execution_init(
        &execution,
        ruda_system_allocator(),
        execution_options
    ));
    interpret_options.execution = &execution;
    RUDA_EXPECT(ruda_mir_interpret(
        &function,
        NULL,
        0,
        ruda_system_allocator(),
        interpret_options,
        &result
    ) == RUDA_MIR_INTERPRET_OBJECT_LIMIT);
    RUDA_EXPECT(execution.enums.length == 2);
    ruda_mir_execution_dispose(&execution);

    {
        size_t failure_offset = 0;
        bool reached_success = false;
        for (failure_offset = 0; failure_offset < 16; failure_offset += 1) {
            RudaFailAllocator state;
            RudaAllocator allocator;
            RudaMirInterpretStatus status;
            ruda_fail_allocator_init(
                &state,
                ruda_system_allocator(),
                SIZE_MAX
            );
            allocator = ruda_fail_allocator(&state);
            RUDA_EXPECT(ruda_mir_execution_init(
                &execution,
                allocator,
                ruda_mir_execution_default_options()
            ));
            state.failure_index = state.attempts + failure_offset;
            interpret_options.execution = &execution;
            status = ruda_mir_interpret(
                &function,
                NULL,
                0,
                ruda_system_allocator(),
                interpret_options,
                &result
            );
            if (status == RUDA_MIR_INTERPRET_OK) {
                reached_success = true;
            } else {
                RUDA_EXPECT(
                    status == RUDA_MIR_INTERPRET_ALLOCATION_FAILED
                );
            }
            ruda_mir_execution_dispose(&execution);
            RUDA_EXPECT(state.live_allocations == 0);
            RUDA_EXPECT(state.live_bytes == 0);
            if (reached_success) {
                break;
            }
        }
        RUDA_EXPECT(reached_success);
    }
    ruda_mir_function_dispose(&function);
}

static void ruda_test_mir_enum_projection(RudaSymbolId symbol) {
    const RudaTypeId enum_type = RUDA_TYPE_FIRST_DYNAMIC + 21;
    RudaMirEnumVariant variants[3];
    RudaMirEnumPayload payloads[4];
    RudaMirFunction constructor;
    RudaMirFunction projection;
    RudaMirEnumArgument arguments[2];
    RudaMirEnumId constructor_layout = RUDA_MIR_NO_ENUM;
    RudaMirEnumId projection_layout = RUDA_MIR_NO_ENUM;
    RudaMirEnumConstructionId construction = RUDA_MIR_NO_ENUM_CONSTRUCTION;
    RudaMirRegister left = RUDA_MIR_NO_REGISTER;
    RudaMirRegister right = RUDA_MIR_NO_REGISTER;
    RudaMirRegister value = RUDA_MIR_NO_REGISTER;
    RudaMirRegister matched = RUDA_MIR_NO_REGISTER;
    RudaMirRegister guarded = RUDA_MIR_NO_REGISTER;
    RudaMirRegister projected = RUDA_MIR_NO_REGISTER;
    RudaMirBlockId constructor_block = RUDA_MIR_NO_BLOCK;
    RudaMirBlockId projection_block = RUDA_MIR_NO_BLOCK;
    RudaMirInstruction instruction;
    RudaMirExecution execution;
    RudaMirInterpretOptions options = ruda_mir_interpret_default_options();
    RudaMirValue enum_value;
    RudaMirValue result;
    RudaMirValue forged;

    variants[0].kind = RUDA_MIR_ENUM_VARIANT_UNIT;
    variants[0].payload_start = 0;
    variants[0].payload_count = 0;
    variants[1].kind = RUDA_MIR_ENUM_VARIANT_POSITIONAL;
    variants[1].payload_start = 0;
    variants[1].payload_count = 2;
    variants[2].kind = RUDA_MIR_ENUM_VARIANT_NAMED;
    variants[2].payload_start = 2;
    variants[2].payload_count = 2;
    payloads[0].type = RUDA_TYPE_I64;
    payloads[1].type = RUDA_TYPE_BOOL;
    payloads[2].type = RUDA_TYPE_I64;
    payloads[3].type = RUDA_TYPE_I64;

    RUDA_EXPECT(ruda_mir_function_init(
        &constructor,
        ruda_system_allocator(),
        symbol,
        NULL,
        0,
        enum_type,
        ruda_mir_function_default_options()
    ));
    RUDA_EXPECT(ruda_mir_add_register(&constructor, RUDA_TYPE_I64, &left));
    RUDA_EXPECT(ruda_mir_add_register(&constructor, RUDA_TYPE_I64, &right));
    RUDA_EXPECT(ruda_mir_add_register(&constructor, enum_type, &value));
    RUDA_EXPECT(ruda_mir_add_block(&constructor, &constructor_block));
    RUDA_EXPECT(ruda_mir_add_enum(
        &constructor,
        enum_type,
        variants,
        3,
        payloads,
        4,
        &constructor_layout
    ));
    arguments[0].register_id = left;
    arguments[0].type = RUDA_TYPE_I64;
    arguments[1].register_id = right;
    arguments[1].type = RUDA_TYPE_I64;
    RUDA_EXPECT(ruda_mir_add_enum_construction(
        &constructor,
        constructor_layout,
        2,
        arguments,
        2,
        &construction
    ));
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_CONSTANT_I64;
    instruction.target = left;
    instruction.integer = 10;
    RUDA_EXPECT(ruda_mir_append_instruction(
        &constructor,
        constructor_block,
        &instruction
    ));
    instruction.target = right;
    instruction.integer = 20;
    RUDA_EXPECT(ruda_mir_append_instruction(
        &constructor,
        constructor_block,
        &instruction
    ));
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_ENUM_NEW;
    instruction.target = value;
    instruction.enum_id = constructor_layout;
    instruction.enum_construction = construction;
    RUDA_EXPECT(ruda_mir_append_instruction(
        &constructor,
        constructor_block,
        &instruction
    ));
    RUDA_EXPECT(ruda_mir_set_terminator(
        &constructor,
        constructor_block,
        ruda_test_mir_return(value)
    ));
    RUDA_EXPECT(ruda_mir_verify(&constructor) == RUDA_MIR_VERIFY_OK);

    RUDA_EXPECT(ruda_mir_function_init(
        &projection,
        ruda_system_allocator(),
        symbol,
        &enum_type,
        1,
        RUDA_TYPE_I64,
        ruda_mir_function_default_options()
    ));
    RUDA_EXPECT(ruda_mir_add_register(
        &projection,
        RUDA_TYPE_BOOL,
        &matched
    ));
    RUDA_EXPECT(ruda_mir_add_register(
        &projection,
        enum_type,
        &guarded
    ));
    RUDA_EXPECT(ruda_mir_add_register(
        &projection,
        RUDA_TYPE_I64,
        &projected
    ));
    RUDA_EXPECT(ruda_mir_add_block(&projection, &projection_block));
    RUDA_EXPECT(ruda_mir_add_enum(
        &projection,
        enum_type,
        variants,
        3,
        payloads,
        4,
        &projection_layout
    ));
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_ENUM_IS_VARIANT;
    instruction.target = matched;
    instruction.first = 0;
    instruction.second = RUDA_MIR_NO_REGISTER;
    instruction.enum_id = projection_layout;
    instruction.variant = 2;
    RUDA_EXPECT(ruda_mir_append_instruction(
        &projection,
        projection_block,
        &instruction
    ));
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_ENUM_REQUIRE_VARIANT;
    instruction.target = guarded;
    instruction.first = 0;
    instruction.second = RUDA_MIR_NO_REGISTER;
    instruction.enum_id = projection_layout;
    instruction.variant = 2;
    RUDA_EXPECT(ruda_mir_append_instruction(
        &projection,
        projection_block,
        &instruction
    ));
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_ENUM_GET_PAYLOAD;
    instruction.target = projected;
    instruction.first = guarded;
    instruction.second = RUDA_MIR_NO_REGISTER;
    instruction.enum_id = projection_layout;
    instruction.variant = 2;
    instruction.field = 1;
    RUDA_EXPECT(ruda_mir_append_instruction(
        &projection,
        projection_block,
        &instruction
    ));
    RUDA_EXPECT(ruda_mir_set_terminator(
        &projection,
        projection_block,
        ruda_test_mir_return(projected)
    ));
    RUDA_EXPECT(ruda_mir_verify(&projection) == RUDA_MIR_VERIFY_OK);

    RUDA_EXPECT(ruda_mir_execution_init(
        &execution,
        ruda_system_allocator(),
        ruda_mir_execution_default_options()
    ));
    options.execution = &execution;
    RUDA_EXPECT(ruda_mir_interpret(
        &constructor,
        NULL,
        0,
        ruda_system_allocator(),
        options,
        &enum_value
    ) == RUDA_MIR_INTERPRET_OK);
    RUDA_EXPECT(ruda_mir_interpret(
        &projection,
        &enum_value,
        1,
        ruda_system_allocator(),
        options,
        &result
    ) == RUDA_MIR_INTERPRET_OK);
    RUDA_EXPECT(result.type == RUDA_TYPE_I64 && result.as.i64 == 20);
    forged = enum_value;
    forged.as.reference = RUDA_MIR_INVALID_REFERENCE;
    RUDA_EXPECT(ruda_mir_interpret(
        &projection,
        &forged,
        1,
        ruda_system_allocator(),
        options,
        &result
    ) == RUDA_MIR_INTERPRET_INVALID_REFERENCE);
    {
        RudaMirBlock *stored_block = ruda_vector_at(
            &projection.blocks,
            projection_block
        );
        RudaMirInstruction *require = stored_block != NULL
            ? ruda_vector_at(&stored_block->instructions, 1)
            : NULL;
        RudaMirInstruction *get = stored_block != NULL
            ? ruda_vector_at(&stored_block->instructions, 2)
            : NULL;
        RUDA_EXPECT(require != NULL && get != NULL);
        if (require != NULL) {
            require->variant = 1;
            RUDA_EXPECT(ruda_mir_verify(&projection) == RUDA_MIR_VERIFY_OK);
            RUDA_EXPECT(ruda_mir_interpret(
                &projection,
                &enum_value,
                1,
                ruda_system_allocator(),
                options,
                &result
            ) == RUDA_MIR_INTERPRET_INVALID_MIR);
            require->variant = 2;
        }
        if (get != NULL) {
            get->field = 2;
            RUDA_EXPECT(
                ruda_mir_verify(&projection)
                    == RUDA_MIR_VERIFY_INVALID_ARGUMENT
            );
            get->field = 1;
        }
        {
            RudaMirEnumPayload *layout_payload = ruda_vector_at(
                &projection.enum_payloads,
                3
            );
            RudaTypeId *projected_type = ruda_vector_at(
                &projection.register_types,
                projected
            );
            RUDA_EXPECT(layout_payload != NULL && projected_type != NULL);
            if (layout_payload != NULL && projected_type != NULL) {
                layout_payload->type = RUDA_TYPE_BOOL;
                *projected_type = RUDA_TYPE_BOOL;
                projection.result_type = RUDA_TYPE_BOOL;
                RUDA_EXPECT(
                    ruda_mir_verify(&projection) == RUDA_MIR_VERIFY_OK
                );
                RUDA_EXPECT(ruda_mir_interpret(
                    &projection,
                    &enum_value,
                    1,
                    ruda_system_allocator(),
                    options,
                    &result
                ) == RUDA_MIR_INTERPRET_INVALID_MIR);
                layout_payload->type = RUDA_TYPE_I64;
                *projected_type = RUDA_TYPE_I64;
                projection.result_type = RUDA_TYPE_I64;
            }
        }
        RUDA_EXPECT(ruda_mir_verify(&projection) == RUDA_MIR_VERIFY_OK);
    }
    ruda_mir_execution_dispose(&execution);
    ruda_mir_function_dispose(&projection);
    ruda_mir_function_dispose(&constructor);
}

void ruda_test_mir(void) {
    RudaMirFunction function;
    RudaSymbolId symbol;
    RudaTypeId parameters[] = {RUDA_TYPE_I64, RUDA_TYPE_I64, RUDA_TYPE_I64};
    RudaMirRegister product = RUDA_MIR_NO_REGISTER;
    RudaMirRegister sum = RUDA_MIR_NO_REGISTER;
    RudaMirBlockId entry = RUDA_MIR_NO_BLOCK;
    RudaMirInstruction instruction;
    RudaMirValue arguments[3];
    RudaMirValue result;

    RUDA_EXPECT(ruda_symbol_id_from_parts(
        "example.test/core",
        "src/calc.ruda",
        "calc",
        0,
        &symbol
    ));
    RUDA_EXPECT(ruda_mir_function_init(
        &function,
        ruda_system_allocator(),
        symbol,
        parameters,
        3,
        RUDA_TYPE_I64,
        ruda_mir_function_default_options()
    ));
    RUDA_EXPECT(ruda_mir_add_register(&function, RUDA_TYPE_I64, &product));
    RUDA_EXPECT(ruda_mir_add_register(&function, RUDA_TYPE_I64, &sum));
    RUDA_EXPECT(ruda_mir_add_block(&function, &entry));
    instruction = ruda_test_mir_binary(
        RUDA_MIR_MULTIPLY_CHECKED_I64,
        product,
        1,
        2
    );
    RUDA_EXPECT(ruda_mir_append_instruction(&function, entry, &instruction));
    instruction = ruda_test_mir_binary(
        RUDA_MIR_ADD_CHECKED_I64,
        sum,
        0,
        product
    );
    RUDA_EXPECT(ruda_mir_append_instruction(&function, entry, &instruction));
    RUDA_EXPECT(ruda_mir_set_terminator(
        &function,
        entry,
        ruda_test_mir_return(sum)
    ));
    RUDA_EXPECT(ruda_mir_verify(&function) == RUDA_MIR_VERIFY_OK);
    arguments[0].type = RUDA_TYPE_I64;
    arguments[0].as.i64 = 2;
    arguments[1].type = RUDA_TYPE_I64;
    arguments[1].as.i64 = 3;
    arguments[2].type = RUDA_TYPE_I64;
    arguments[2].as.i64 = 4;
    RUDA_EXPECT(ruda_mir_interpret(
        &function,
        arguments,
        3,
        ruda_system_allocator(),
        ruda_mir_interpret_default_options(),
        &result
    ) == RUDA_MIR_INTERPRET_OK);
    RUDA_EXPECT(result.type == RUDA_TYPE_I64);
    RUDA_EXPECT(result.as.i64 == 14);
    ruda_mir_function_dispose(&function);

    {
        RudaTypeId overflow_parameters[] = {RUDA_TYPE_I64};
        RudaMirRegister one = RUDA_MIR_NO_REGISTER;
        RudaMirRegister added = RUDA_MIR_NO_REGISTER;
        RudaMirBlockId block = RUDA_MIR_NO_BLOCK;
        RudaMirValue maximum;
        RUDA_EXPECT(ruda_mir_function_init(
            &function,
            ruda_system_allocator(),
            symbol,
            overflow_parameters,
            1,
            RUDA_TYPE_I64,
            ruda_mir_function_default_options()
        ));
        RUDA_EXPECT(ruda_mir_add_register(&function, RUDA_TYPE_I64, &one));
        RUDA_EXPECT(ruda_mir_add_register(&function, RUDA_TYPE_I64, &added));
        RUDA_EXPECT(ruda_mir_add_block(&function, &block));
        memset(&instruction, 0, sizeof(instruction));
        instruction.kind = RUDA_MIR_CONSTANT_I64;
        instruction.target = one;
        instruction.integer = 1;
        RUDA_EXPECT(ruda_mir_append_instruction(&function, block, &instruction));
        instruction = ruda_test_mir_binary(
            RUDA_MIR_ADD_CHECKED_I64,
            added,
            0,
            one
        );
        RUDA_EXPECT(ruda_mir_append_instruction(&function, block, &instruction));
        RUDA_EXPECT(ruda_mir_set_terminator(
            &function,
            block,
            ruda_test_mir_return(added)
        ));
        maximum.type = RUDA_TYPE_I64;
        maximum.as.i64 = INT64_MAX;
        RUDA_EXPECT(ruda_mir_verify(&function) == RUDA_MIR_VERIFY_OK);
        RUDA_EXPECT(ruda_mir_interpret(
            &function,
            &maximum,
            1,
            ruda_system_allocator(),
            ruda_mir_interpret_default_options(),
            &result
        ) == RUDA_MIR_INTERPRET_OVERFLOW);
        ruda_mir_function_dispose(&function);
    }

    {
        RudaTypeId invalid_parameters[] = {RUDA_TYPE_BOOL};
        RudaMirRegister value = RUDA_MIR_NO_REGISTER;
        RudaMirBlockId block = RUDA_MIR_NO_BLOCK;
        RUDA_EXPECT(ruda_mir_function_init(
            &function,
            ruda_system_allocator(),
            symbol,
            invalid_parameters,
            1,
            RUDA_TYPE_I64,
            ruda_mir_function_default_options()
        ));
        RUDA_EXPECT(ruda_mir_add_register(&function, RUDA_TYPE_I64, &value));
        RUDA_EXPECT(ruda_mir_add_block(&function, &block));
        instruction = ruda_test_mir_binary(
            RUDA_MIR_ADD_CHECKED_I64,
            value,
            0,
            0
        );
        RUDA_EXPECT(ruda_mir_append_instruction(&function, block, &instruction));
        RUDA_EXPECT(ruda_mir_set_terminator(
            &function,
            block,
            ruda_test_mir_return(value)
        ));
        RUDA_EXPECT(ruda_mir_verify(&function) == RUDA_MIR_VERIFY_TYPE_MISMATCH);
        ruda_mir_function_dispose(&function);
    }

    {
        RudaTypeId branch_parameters[] = {RUDA_TYPE_BOOL};
        RudaMirRegister selected = RUDA_MIR_NO_REGISTER;
        RudaMirBlockId branch = RUDA_MIR_NO_BLOCK;
        RudaMirBlockId when_true = RUDA_MIR_NO_BLOCK;
        RudaMirBlockId when_false = RUDA_MIR_NO_BLOCK;
        RudaMirBlockId join = RUDA_MIR_NO_BLOCK;
        RudaMirValue condition;
        RUDA_EXPECT(ruda_mir_function_init(
            &function,
            ruda_system_allocator(),
            symbol,
            branch_parameters,
            1,
            RUDA_TYPE_I64,
            ruda_mir_function_default_options()
        ));
        RUDA_EXPECT(ruda_mir_add_register(&function, RUDA_TYPE_I64, &selected));
        RUDA_EXPECT(ruda_mir_add_block(&function, &branch));
        RUDA_EXPECT(ruda_mir_add_block(&function, &when_true));
        RUDA_EXPECT(ruda_mir_add_block(&function, &when_false));
        RUDA_EXPECT(ruda_mir_add_block(&function, &join));
        RUDA_EXPECT(ruda_mir_set_terminator(
            &function,
            branch,
            ruda_test_mir_branch(0, when_true, when_false)
        ));
        memset(&instruction, 0, sizeof(instruction));
        instruction.kind = RUDA_MIR_CONSTANT_I64;
        instruction.target = selected;
        instruction.integer = 10;
        RUDA_EXPECT(ruda_mir_append_instruction(
            &function,
            when_true,
            &instruction
        ));
        RUDA_EXPECT(ruda_mir_set_terminator(
            &function,
            when_true,
            ruda_test_mir_goto(join)
        ));
        instruction.integer = 20;
        RUDA_EXPECT(ruda_mir_append_instruction(
            &function,
            when_false,
            &instruction
        ));
        RUDA_EXPECT(ruda_mir_set_terminator(
            &function,
            when_false,
            ruda_test_mir_goto(join)
        ));
        RUDA_EXPECT(ruda_mir_set_terminator(
            &function,
            join,
            ruda_test_mir_return(selected)
        ));
        RUDA_EXPECT(ruda_mir_verify(&function) == RUDA_MIR_VERIFY_OK);
        condition.type = RUDA_TYPE_BOOL;
        condition.as.boolean = true;
        RUDA_EXPECT(ruda_mir_interpret(
            &function,
            &condition,
            1,
            ruda_system_allocator(),
            ruda_mir_interpret_default_options(),
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 10);
        condition.as.boolean = false;
        RUDA_EXPECT(ruda_mir_interpret(
            &function,
            &condition,
            1,
            ruda_system_allocator(),
            ruda_mir_interpret_default_options(),
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 20);
        ruda_mir_function_dispose(&function);
    }

    {
        RudaTypeId branch_parameters[] = {RUDA_TYPE_BOOL};
        RudaMirRegister selected = RUDA_MIR_NO_REGISTER;
        RudaMirBlockId branch = RUDA_MIR_NO_BLOCK;
        RudaMirBlockId when_true = RUDA_MIR_NO_BLOCK;
        RudaMirBlockId when_false = RUDA_MIR_NO_BLOCK;
        RudaMirBlockId join = RUDA_MIR_NO_BLOCK;
        RUDA_EXPECT(ruda_mir_function_init(
            &function,
            ruda_system_allocator(),
            symbol,
            branch_parameters,
            1,
            RUDA_TYPE_I64,
            ruda_mir_function_default_options()
        ));
        RUDA_EXPECT(ruda_mir_add_register(&function, RUDA_TYPE_I64, &selected));
        RUDA_EXPECT(ruda_mir_add_block(&function, &branch));
        RUDA_EXPECT(ruda_mir_add_block(&function, &when_true));
        RUDA_EXPECT(ruda_mir_add_block(&function, &when_false));
        RUDA_EXPECT(ruda_mir_add_block(&function, &join));
        RUDA_EXPECT(ruda_mir_set_terminator(
            &function,
            branch,
            ruda_test_mir_branch(0, when_true, when_false)
        ));
        memset(&instruction, 0, sizeof(instruction));
        instruction.kind = RUDA_MIR_CONSTANT_I64;
        instruction.target = selected;
        instruction.integer = 10;
        RUDA_EXPECT(ruda_mir_append_instruction(
            &function,
            when_true,
            &instruction
        ));
        RUDA_EXPECT(ruda_mir_set_terminator(
            &function,
            when_true,
            ruda_test_mir_goto(join)
        ));
        RUDA_EXPECT(ruda_mir_set_terminator(
            &function,
            when_false,
            ruda_test_mir_goto(join)
        ));
        RUDA_EXPECT(ruda_mir_set_terminator(
            &function,
            join,
            ruda_test_mir_return(selected)
        ));
        RUDA_EXPECT(
            ruda_mir_verify(&function)
                == RUDA_MIR_VERIFY_UNINITIALIZED_REGISTER
        );
        ruda_mir_function_dispose(&function);
    }

    ruda_test_mir_verify_limits_and_failures(symbol);
    ruda_test_mir_verify_long_cfg(symbol);
    ruda_test_mir_calls(symbol);
    ruda_test_mir_never_calls(symbol);
    ruda_test_mir_bitwise(symbol);
    ruda_test_mir_shifts(symbol);
    ruda_test_mir_records(symbol);
    ruda_test_mir_record_identity(symbol);
    ruda_test_mir_enums(symbol);
    ruda_test_mir_enum_projection(symbol);
    ruda_test_mir_strings_and_panic(symbol);
    ruda_test_mir_string_equality(symbol);
}
