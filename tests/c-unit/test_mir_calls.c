#include "test_mir_support.h"

#include <string.h>

#include "mmay/base/allocator.h"

typedef struct MarimayMirCallTestContext {
    MarimaySymbolId expected;
    size_t calls;
} MarimayMirCallTestContext;

typedef struct MarimayMirNeverCallTestContext {
    MarimaySymbolId expected;
    MarimayMirInterpretStatus status;
    size_t calls;
} MarimayMirNeverCallTestContext;

static MarimayMirInterpretStatus mmay_test_mir_call_handler(
    void *context,
    MarimaySymbolId callee,
    const MarimayMirValue *arguments,
    size_t argument_count,
    MarimayMirValue *result
) {
    MarimayMirCallTestContext *state = context;
    if (state == NULL || result == NULL
        || !mmay_symbol_id_equal(callee, state->expected)
        || argument_count != 2 || arguments == NULL
        || arguments[0].type != MMAY_TYPE_I64
        || arguments[1].type != MMAY_TYPE_I64) {
        return MMAY_MIR_INTERPRET_INVALID_ARGUMENT;
    }
    state->calls += 1;
    result->type = MMAY_TYPE_I64;
    result->as.i64 = arguments[0].as.i64 + arguments[1].as.i64;
    return MMAY_MIR_INTERPRET_OK;
}

static MarimayMirInterpretStatus mmay_test_mir_never_call_handler(
    void *context,
    MarimaySymbolId callee,
    const MarimayMirValue *arguments,
    size_t argument_count,
    MarimayMirValue *result
) {
    MarimayMirNeverCallTestContext *state = context;
    if (state == NULL || result == NULL
        || !mmay_symbol_id_equal(callee, state->expected)
        || argument_count != 2 || arguments == NULL
        || arguments[0].type != MMAY_TYPE_I64
        || arguments[1].type != MMAY_TYPE_I64) {
        return MMAY_MIR_INTERPRET_INVALID_ARGUMENT;
    }
    state->calls += 1;
    if (state->status == MMAY_MIR_INTERPRET_OK) {
        result->type = MMAY_TYPE_I64;
        result->as.i64 = arguments[0].as.i64 + arguments[1].as.i64;
    }
    return state->status;
}


static void mmay_test_mir_calls(MarimaySymbolId symbol) {
    MarimaySymbolId callee;
    MarimayTypeId parameters[] = {MMAY_TYPE_I64, MMAY_TYPE_I64};
    MarimayMirFunction function;
    MarimayMirRegister value = MMAY_MIR_NO_REGISTER;
    MarimayMirBlockId block = MMAY_MIR_NO_BLOCK;
    MarimayMirCallArgument call_arguments[2];
    MarimayMirCallId call = MMAY_MIR_NO_CALL;
    MarimayMirInstruction instruction;
    MarimayMirValue arguments[2];
    MarimayMirValue result;
    MarimayMirInterpretOptions options = mmay_mir_interpret_default_options();
    MarimayMirCallTestContext context;
    size_t failure_offset = 0;
    bool reached_success = false;
    MMAY_EXPECT(mmay_symbol_id_from_parts(
        "test",
        "mir.my",
        "callee",
        0,
        &callee
    ));
    MMAY_EXPECT(mmay_mir_function_init(
        &function,
        mmay_system_allocator(),
        symbol,
        parameters,
        2,
        MMAY_TYPE_I64,
        mmay_mir_function_default_options()
    ));
    MMAY_EXPECT(mmay_mir_add_register(&function, MMAY_TYPE_I64, &value));
    MMAY_EXPECT(mmay_mir_add_block(&function, &block));
    call_arguments[0].register_id = 0;
    call_arguments[0].type = MMAY_TYPE_I64;
    call_arguments[1].register_id = 1;
    call_arguments[1].type = MMAY_TYPE_I64;
    MMAY_EXPECT(mmay_mir_add_call(
        &function,
        callee,
        MMAY_TYPE_I64,
        call_arguments,
        2,
        &call
    ));
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MMAY_MIR_CALL;
    instruction.target = value;
    instruction.first = MMAY_MIR_NO_REGISTER;
    instruction.second = MMAY_MIR_NO_REGISTER;
    instruction.call = call;
    MMAY_EXPECT(mmay_mir_append_instruction(&function, block, &instruction));
    MMAY_EXPECT(mmay_mir_set_terminator(
        &function,
        block,
        mmay_test_mir_return(value)
    ));
    MMAY_EXPECT(mmay_mir_verify(&function) == MMAY_MIR_VERIFY_OK);
    {
        size_t verify_work = function.options.max_verify_work;
        function.options.max_verify_work = 1;
        MMAY_EXPECT(
            mmay_mir_verify(&function) == MMAY_MIR_VERIFY_LIMIT_REACHED
        );
        function.options.max_verify_work = verify_work;
    }
    arguments[0].type = MMAY_TYPE_I64;
    arguments[0].as.i64 = 3;
    arguments[1].type = MMAY_TYPE_I64;
    arguments[1].as.i64 = 4;
    MMAY_EXPECT(mmay_mir_interpret(
        &function,
        arguments,
        2,
        mmay_system_allocator(),
        options,
        &result
    ) == MMAY_MIR_INTERPRET_INVALID_ARGUMENT);
    context.expected = callee;
    context.calls = 0;
    options.call_context = &context;
    options.call = mmay_test_mir_call_handler;
    MMAY_EXPECT(mmay_mir_interpret(
        &function,
        arguments,
        2,
        mmay_system_allocator(),
        options,
        &result
    ) == MMAY_MIR_INTERPRET_OK);
    MMAY_EXPECT(context.calls == 1);
    MMAY_EXPECT(result.type == MMAY_TYPE_I64);
    MMAY_EXPECT(result.as.i64 == 7);
    {
        MarimayMirCallArgument *argument = mmay_vector_at(
            &function.call_arguments,
            0
        );
        MMAY_EXPECT(argument != NULL);
        if (argument != NULL) {
            argument->type = MMAY_TYPE_BOOL;
            MMAY_EXPECT(
                mmay_mir_verify(&function) == MMAY_MIR_VERIFY_TYPE_MISMATCH
            );
            argument->type = MMAY_TYPE_I64;
            argument->register_id = value;
            MMAY_EXPECT(
                mmay_mir_verify(&function)
                    == MMAY_MIR_VERIFY_UNINITIALIZED_REGISTER
            );
            argument->register_id = 0;
        }
    }
    reached_success = false;
    for (failure_offset = 0; failure_offset < 8; failure_offset += 1) {
        MarimayFailAllocator state;
        MarimayAllocator allocator;
        MarimayMirInterpretStatus status;
        mmay_fail_allocator_init(
            &state,
            mmay_system_allocator(),
            failure_offset
        );
        allocator = mmay_fail_allocator(&state);
        status = mmay_mir_interpret(
            &function,
            arguments,
            2,
            allocator,
            options,
            &result
        );
        if (status == MMAY_MIR_INTERPRET_OK) {
            reached_success = true;
            MMAY_EXPECT(result.as.i64 == 7);
        } else {
            MMAY_EXPECT(status == MMAY_MIR_INTERPRET_ALLOCATION_FAILED);
        }
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
        if (reached_success) {
            break;
        }
    }
    MMAY_EXPECT(reached_success);
    mmay_mir_function_dispose(&function);

    reached_success = false;
    for (failure_offset = 0; failure_offset < 8; failure_offset += 1) {
        MarimayFailAllocator state;
        MarimayAllocator allocator;
        mmay_fail_allocator_init(
            &state,
            mmay_system_allocator(),
            SIZE_MAX
        );
        allocator = mmay_fail_allocator(&state);
        MMAY_EXPECT(mmay_mir_function_init(
            &function,
            allocator,
            symbol,
            parameters,
            2,
            MMAY_TYPE_I64,
            mmay_mir_function_default_options()
        ));
        state.failure_index = state.attempts + failure_offset;
        if (mmay_mir_add_call(
                &function,
                callee,
                MMAY_TYPE_I64,
                call_arguments,
                2,
                &call
            )) {
            reached_success = true;
        } else {
            MMAY_EXPECT(function.calls.length == 0);
            MMAY_EXPECT(function.call_arguments.length == 0);
        }
        mmay_mir_function_dispose(&function);
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
        if (reached_success) {
            break;
        }
    }
    MMAY_EXPECT(reached_success);
}

static void mmay_test_mir_never_calls(MarimaySymbolId symbol) {
    MarimaySymbolId callee;
    MarimayTypeId parameters[] = { MMAY_TYPE_I64, MMAY_TYPE_I64 };
    MarimayMirFunction function;
    MarimayMirBlockId block = MMAY_MIR_NO_BLOCK;
    MarimayMirRegister unused = MMAY_MIR_NO_REGISTER;
    MarimayMirCallArgument call_arguments[2];
    MarimayMirCallId call = MMAY_MIR_NO_CALL;
    MarimayMirValue arguments[2];
    MarimayMirValue result;
    MarimayMirInterpretOptions options = mmay_mir_interpret_default_options();
    MarimayMirNeverCallTestContext context;
    MarimayMirCall *stored_call = NULL;
    MarimayMirCallArgument *stored_argument = NULL;
    MarimayMirBlock *stored_block = NULL;
    size_t failure_offset = 0;
    bool reached_status = false;
    MMAY_EXPECT(mmay_symbol_id_from_parts(
        "test",
        "mir.my",
        "never-callee",
        0,
        &callee
    ));
    MMAY_EXPECT(mmay_mir_function_init(
        &function,
        mmay_system_allocator(),
        symbol,
        parameters,
        2,
        MMAY_TYPE_I64,
        mmay_mir_function_default_options()
    ));
    MMAY_EXPECT(mmay_mir_add_register(&function, MMAY_TYPE_I64, &unused));
    MMAY_EXPECT(mmay_mir_add_block(&function, &block));
    call_arguments[0].register_id = 0;
    call_arguments[0].type = MMAY_TYPE_I64;
    call_arguments[1].register_id = 1;
    call_arguments[1].type = MMAY_TYPE_I64;
    MMAY_EXPECT(mmay_mir_add_call(
        &function,
        callee,
        MMAY_TYPE_NEVER,
        call_arguments,
        2,
        &call
    ));
    MMAY_EXPECT(mmay_mir_set_terminator(
        &function,
        block,
        mmay_test_mir_call_never(call, 4, 15)
    ));
    MMAY_EXPECT(mmay_mir_verify(&function) == MMAY_MIR_VERIFY_OK);

    arguments[0].type = MMAY_TYPE_I64;
    arguments[0].as.i64 = 3;
    arguments[1].type = MMAY_TYPE_I64;
    arguments[1].as.i64 = 4;
    MMAY_EXPECT(mmay_mir_interpret(
        &function,
        arguments,
        2,
        mmay_system_allocator(),
        options,
        &result
    ) == MMAY_MIR_INTERPRET_INVALID_ARGUMENT);
    context.expected = callee;
    context.status = MMAY_MIR_INTERPRET_PANIC;
    context.calls = 0;
    options.call_context = &context;
    options.call = mmay_test_mir_never_call_handler;
    MMAY_EXPECT(mmay_mir_interpret(
        &function,
        arguments,
        2,
        mmay_system_allocator(),
        options,
        &result
    ) == MMAY_MIR_INTERPRET_PANIC);
    MMAY_EXPECT(context.calls == 1);
    context.status = MMAY_MIR_INTERPRET_OK;
    MMAY_EXPECT(mmay_mir_interpret(
        &function,
        arguments,
        2,
        mmay_system_allocator(),
        options,
        &result
    ) == MMAY_MIR_INTERPRET_INVALID_MIR);

    context.status = MMAY_MIR_INTERPRET_PANIC;
    reached_status = false;
    for (failure_offset = 0; failure_offset < 8; failure_offset += 1) {
        MarimayFailAllocator state;
        MarimayAllocator allocator;
        MarimayMirInterpretStatus status;
        mmay_fail_allocator_init(
            &state,
            mmay_system_allocator(),
            failure_offset
        );
        allocator = mmay_fail_allocator(&state);
        status = mmay_mir_interpret(
            &function,
            arguments,
            2,
            allocator,
            options,
            &result
        );
        if (status == MMAY_MIR_INTERPRET_PANIC) {
            reached_status = true;
        } else {
            MMAY_EXPECT(status == MMAY_MIR_INTERPRET_ALLOCATION_FAILED);
        }
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
        if (reached_status) {
            break;
        }
    }
    MMAY_EXPECT(reached_status);

    stored_call = mmay_vector_at(&function.calls, call);
    stored_argument = mmay_vector_at(&function.call_arguments, 0);
    stored_block = mmay_vector_at(&function.blocks, block);
    MMAY_EXPECT(stored_call != NULL && stored_argument != NULL
        && stored_block != NULL);
    if (stored_call != NULL) {
        stored_call->result_type = MMAY_TYPE_I64;
        MMAY_EXPECT(mmay_mir_verify(&function) == MMAY_MIR_VERIFY_INVALID_ARGUMENT);
        stored_call->result_type = MMAY_TYPE_NEVER;
    }
    if (stored_argument != NULL) {
        stored_argument->type = MMAY_TYPE_BOOL;
        MMAY_EXPECT(mmay_mir_verify(&function) == MMAY_MIR_VERIFY_TYPE_MISMATCH);
        stored_argument->type = MMAY_TYPE_I64;
        stored_argument->register_id = unused;
        MMAY_EXPECT(
            mmay_mir_verify(&function) == MMAY_MIR_VERIFY_UNINITIALIZED_REGISTER
        );
        stored_argument->register_id = 0;
    }
    if (stored_block != NULL) {
        stored_block->terminator.call = MMAY_MIR_NO_CALL;
        MMAY_EXPECT(mmay_mir_verify(&function) == MMAY_MIR_VERIFY_INVALID_ARGUMENT);
        stored_block->terminator.call = call;
    }
    mmay_mir_function_dispose(&function);

    MMAY_EXPECT(mmay_mir_function_init(
        &function,
        mmay_system_allocator(),
        symbol,
        NULL,
        0,
        MMAY_TYPE_NEVER,
        mmay_mir_function_default_options()
    ));
    MMAY_EXPECT(mmay_mir_add_block(&function, &block));
    MMAY_EXPECT(mmay_mir_set_terminator(
        &function,
        block,
        mmay_test_mir_return(MMAY_MIR_NO_REGISTER)
    ));
    MMAY_EXPECT(mmay_mir_verify(&function) == MMAY_MIR_VERIFY_TYPE_MISMATCH);
    mmay_mir_function_dispose(&function);

    MMAY_EXPECT(mmay_mir_function_init(
        &function,
        mmay_system_allocator(),
        symbol,
        parameters,
        2,
        MMAY_TYPE_NEVER,
        mmay_mir_function_default_options()
    ));
    MMAY_EXPECT(mmay_mir_add_register(&function, MMAY_TYPE_NEVER, &unused));
    MMAY_EXPECT(mmay_mir_add_block(&function, &block));
    MMAY_EXPECT(mmay_mir_add_call(
        &function,
        callee,
        MMAY_TYPE_NEVER,
        call_arguments,
        2,
        &call
    ));
    {
        MarimayMirInstruction instruction;
        memset(&instruction, 0, sizeof(instruction));
        instruction.kind = MMAY_MIR_CALL;
        instruction.target = unused;
        instruction.first = MMAY_MIR_NO_REGISTER;
        instruction.second = MMAY_MIR_NO_REGISTER;
        instruction.call = call;
        MMAY_EXPECT(mmay_mir_append_instruction(
            &function,
            block,
            &instruction
        ));
    }
    MMAY_EXPECT(mmay_mir_set_terminator(
        &function,
        block,
        mmay_test_mir_call_never(call, 0, 0)
    ));
    /* The invalid never value register is rejected before call validation. */
    MMAY_EXPECT(mmay_mir_verify(&function) == MMAY_MIR_VERIFY_INVALID_REGISTER);
    mmay_mir_function_dispose(&function);
}

static void mmay_test_mir_bitwise(MarimaySymbolId symbol) {
    MarimayTypeId parameters[] = {MMAY_TYPE_I64, MMAY_TYPE_I64};
    MarimayMirFunction function;
    MarimayMirRegister and_value = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister xor_value = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister or_value = MMAY_MIR_NO_REGISTER;
    MarimayMirBlockId block = MMAY_MIR_NO_BLOCK;
    MarimayMirInstruction instruction;
    MarimayMirValue arguments[2];
    MarimayMirValue result;
    MMAY_EXPECT(mmay_mir_function_init(
        &function,
        mmay_system_allocator(),
        symbol,
        parameters,
        2,
        MMAY_TYPE_I64,
        mmay_mir_function_default_options()
    ));
    MMAY_EXPECT(mmay_mir_add_register(
        &function,
        MMAY_TYPE_I64,
        &and_value
    ));
    MMAY_EXPECT(mmay_mir_add_register(
        &function,
        MMAY_TYPE_I64,
        &xor_value
    ));
    MMAY_EXPECT(mmay_mir_add_register(
        &function,
        MMAY_TYPE_I64,
        &or_value
    ));
    MMAY_EXPECT(mmay_mir_add_block(&function, &block));
    instruction = mmay_test_mir_binary(
        MMAY_MIR_BITWISE_AND_I64,
        and_value,
        0,
        1
    );
    MMAY_EXPECT(mmay_mir_append_instruction(&function, block, &instruction));
    instruction = mmay_test_mir_binary(
        MMAY_MIR_BITWISE_XOR_I64,
        xor_value,
        and_value,
        1
    );
    MMAY_EXPECT(mmay_mir_append_instruction(&function, block, &instruction));
    instruction = mmay_test_mir_binary(
        MMAY_MIR_BITWISE_OR_I64,
        or_value,
        xor_value,
        0
    );
    MMAY_EXPECT(mmay_mir_append_instruction(&function, block, &instruction));
    MMAY_EXPECT(mmay_mir_set_terminator(
        &function,
        block,
        mmay_test_mir_return(or_value)
    ));
    MMAY_EXPECT(mmay_mir_verify(&function) == MMAY_MIR_VERIFY_OK);
    arguments[0].type = MMAY_TYPE_I64;
    arguments[0].as.i64 = -16;
    arguments[1].type = MMAY_TYPE_I64;
    arguments[1].as.i64 = 10;
    MMAY_EXPECT(mmay_mir_interpret(
        &function,
        arguments,
        2,
        mmay_system_allocator(),
        mmay_mir_interpret_default_options(),
        &result
    ) == MMAY_MIR_INTERPRET_OK);
    MMAY_EXPECT(result.type == MMAY_TYPE_I64);
    MMAY_EXPECT(result.as.i64 == -6);
    mmay_mir_function_dispose(&function);

    {
        MarimayTypeId invalid_parameters[] = {MMAY_TYPE_BOOL, MMAY_TYPE_BOOL};
        MarimayMirRegister invalid_value = MMAY_MIR_NO_REGISTER;
        MMAY_EXPECT(mmay_mir_function_init(
            &function,
            mmay_system_allocator(),
            symbol,
            invalid_parameters,
            2,
            MMAY_TYPE_I64,
            mmay_mir_function_default_options()
        ));
        MMAY_EXPECT(mmay_mir_add_register(
            &function,
            MMAY_TYPE_I64,
            &invalid_value
        ));
        MMAY_EXPECT(mmay_mir_add_block(&function, &block));
        instruction = mmay_test_mir_binary(
            MMAY_MIR_BITWISE_AND_I64,
            invalid_value,
            0,
            1
        );
        MMAY_EXPECT(mmay_mir_append_instruction(
            &function,
            block,
            &instruction
        ));
        MMAY_EXPECT(mmay_mir_set_terminator(
            &function,
            block,
            mmay_test_mir_return(invalid_value)
        ));
        MMAY_EXPECT(
            mmay_mir_verify(&function) == MMAY_MIR_VERIFY_TYPE_MISMATCH
        );
        mmay_mir_function_dispose(&function);
    }
}

static void mmay_test_mir_shifts(MarimaySymbolId symbol) {
    MarimayTypeId parameters[] = {MMAY_TYPE_I64, MMAY_TYPE_I64};
    MarimayMirFunction function;
    MarimayMirRegister shifted = MMAY_MIR_NO_REGISTER;
    MarimayMirBlockId block = MMAY_MIR_NO_BLOCK;
    MarimayMirInstruction instruction;
    MarimayMirValue arguments[2];
    MarimayMirValue result;

    MMAY_EXPECT(mmay_mir_function_init(
        &function,
        mmay_system_allocator(),
        symbol,
        parameters,
        2,
        MMAY_TYPE_I64,
        mmay_mir_function_default_options()
    ));
    MMAY_EXPECT(mmay_mir_add_register(
        &function,
        MMAY_TYPE_I64,
        &shifted
    ));
    MMAY_EXPECT(mmay_mir_add_block(&function, &block));
    instruction = mmay_test_mir_binary(
        MMAY_MIR_SHIFT_LEFT_CHECKED_I64,
        shifted,
        0,
        1
    );
    MMAY_EXPECT(mmay_mir_append_instruction(&function, block, &instruction));
    MMAY_EXPECT(mmay_mir_set_terminator(
        &function,
        block,
        mmay_test_mir_return(shifted)
    ));
    MMAY_EXPECT(mmay_mir_verify(&function) == MMAY_MIR_VERIFY_OK);
    arguments[0].type = MMAY_TYPE_I64;
    arguments[1].type = MMAY_TYPE_I64;
    arguments[0].as.i64 = -1;
    arguments[1].as.i64 = 63;
    MMAY_EXPECT(mmay_mir_interpret(
        &function,
        arguments,
        2,
        mmay_system_allocator(),
        mmay_mir_interpret_default_options(),
        &result
    ) == MMAY_MIR_INTERPRET_OK);
    MMAY_EXPECT(result.as.i64 == INT64_MIN);
    arguments[0].as.i64 = -2;
    arguments[1].as.i64 = 62;
    MMAY_EXPECT(mmay_mir_interpret(
        &function,
        arguments,
        2,
        mmay_system_allocator(),
        mmay_mir_interpret_default_options(),
        &result
    ) == MMAY_MIR_INTERPRET_OK);
    MMAY_EXPECT(result.as.i64 == INT64_MIN);
    arguments[0].as.i64 = INT64_MIN;
    arguments[1].as.i64 = 1;
    MMAY_EXPECT(mmay_mir_interpret(
        &function,
        arguments,
        2,
        mmay_system_allocator(),
        mmay_mir_interpret_default_options(),
        &result
    ) == MMAY_MIR_INTERPRET_OVERFLOW);
    arguments[0].as.i64 = 1;
    arguments[1].as.i64 = 63;
    MMAY_EXPECT(mmay_mir_interpret(
        &function,
        arguments,
        2,
        mmay_system_allocator(),
        mmay_mir_interpret_default_options(),
        &result
    ) == MMAY_MIR_INTERPRET_OVERFLOW);
    arguments[0].as.i64 = 0;
    arguments[1].as.i64 = 63;
    MMAY_EXPECT(mmay_mir_interpret(
        &function,
        arguments,
        2,
        mmay_system_allocator(),
        mmay_mir_interpret_default_options(),
        &result
    ) == MMAY_MIR_INTERPRET_OK);
    MMAY_EXPECT(result.as.i64 == 0);
    arguments[1].as.i64 = -1;
    MMAY_EXPECT(mmay_mir_interpret(
        &function,
        arguments,
        2,
        mmay_system_allocator(),
        mmay_mir_interpret_default_options(),
        &result
    ) == MMAY_MIR_INTERPRET_INVALID_SHIFT);
    arguments[1].as.i64 = 64;
    MMAY_EXPECT(mmay_mir_interpret(
        &function,
        arguments,
        2,
        mmay_system_allocator(),
        mmay_mir_interpret_default_options(),
        &result
    ) == MMAY_MIR_INTERPRET_INVALID_SHIFT);
    mmay_mir_function_dispose(&function);

    MMAY_EXPECT(mmay_mir_function_init(
        &function,
        mmay_system_allocator(),
        symbol,
        parameters,
        2,
        MMAY_TYPE_I64,
        mmay_mir_function_default_options()
    ));
    MMAY_EXPECT(mmay_mir_add_register(
        &function,
        MMAY_TYPE_I64,
        &shifted
    ));
    MMAY_EXPECT(mmay_mir_add_block(&function, &block));
    instruction = mmay_test_mir_binary(
        MMAY_MIR_SHIFT_RIGHT_I64,
        shifted,
        0,
        1
    );
    MMAY_EXPECT(mmay_mir_append_instruction(&function, block, &instruction));
    MMAY_EXPECT(mmay_mir_set_terminator(
        &function,
        block,
        mmay_test_mir_return(shifted)
    ));
    MMAY_EXPECT(mmay_mir_verify(&function) == MMAY_MIR_VERIFY_OK);
    arguments[0].as.i64 = -7;
    arguments[1].as.i64 = 1;
    MMAY_EXPECT(mmay_mir_interpret(
        &function,
        arguments,
        2,
        mmay_system_allocator(),
        mmay_mir_interpret_default_options(),
        &result
    ) == MMAY_MIR_INTERPRET_OK);
    MMAY_EXPECT(result.as.i64 == -4);
    arguments[0].as.i64 = INT64_MIN;
    arguments[1].as.i64 = 63;
    MMAY_EXPECT(mmay_mir_interpret(
        &function,
        arguments,
        2,
        mmay_system_allocator(),
        mmay_mir_interpret_default_options(),
        &result
    ) == MMAY_MIR_INTERPRET_OK);
    MMAY_EXPECT(result.as.i64 == -1);
    arguments[0].as.i64 = 7;
    arguments[1].as.i64 = 1;
    MMAY_EXPECT(mmay_mir_interpret(
        &function,
        arguments,
        2,
        mmay_system_allocator(),
        mmay_mir_interpret_default_options(),
        &result
    ) == MMAY_MIR_INTERPRET_OK);
    MMAY_EXPECT(result.as.i64 == 3);
    arguments[1].as.i64 = 64;
    MMAY_EXPECT(mmay_mir_interpret(
        &function,
        arguments,
        2,
        mmay_system_allocator(),
        mmay_mir_interpret_default_options(),
        &result
    ) == MMAY_MIR_INTERPRET_INVALID_SHIFT);
    mmay_mir_function_dispose(&function);

    {
        MarimayTypeId invalid_parameters[] = {MMAY_TYPE_BOOL, MMAY_TYPE_BOOL};
        MMAY_EXPECT(mmay_mir_function_init(
            &function,
            mmay_system_allocator(),
            symbol,
            invalid_parameters,
            2,
            MMAY_TYPE_I64,
            mmay_mir_function_default_options()
        ));
        MMAY_EXPECT(mmay_mir_add_register(
            &function,
            MMAY_TYPE_I64,
            &shifted
        ));
        MMAY_EXPECT(mmay_mir_add_block(&function, &block));
        instruction = mmay_test_mir_binary(
            MMAY_MIR_SHIFT_RIGHT_I64,
            shifted,
            0,
            1
        );
        MMAY_EXPECT(mmay_mir_append_instruction(
            &function,
            block,
            &instruction
        ));
        MMAY_EXPECT(mmay_mir_set_terminator(
            &function,
            block,
            mmay_test_mir_return(shifted)
        ));
        MMAY_EXPECT(
            mmay_mir_verify(&function) == MMAY_MIR_VERIFY_TYPE_MISMATCH
        );
        mmay_mir_function_dispose(&function);
    }
}


void mmay_test_mir_calls_arithmetic(MarimaySymbolId symbol) {
    mmay_test_mir_calls(symbol);
    mmay_test_mir_never_calls(symbol);
    mmay_test_mir_bitwise(symbol);
    mmay_test_mir_shifts(symbol);
}
