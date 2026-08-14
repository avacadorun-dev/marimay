#include "test_mir_support.h"

#include <string.h>

#include "ruda/base/allocator.h"

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


void ruda_test_mir_calls_arithmetic(RudaSymbolId symbol) {
    ruda_test_mir_calls(symbol);
    ruda_test_mir_never_calls(symbol);
    ruda_test_mir_bitwise(symbol);
    ruda_test_mir_shifts(symbol);
}
