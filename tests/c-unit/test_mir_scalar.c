#include "test_mir_support.h"

#include <stdint.h>
#include <string.h>

#include "ruda/base/allocator.h"

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

static void ruda_test_mir_f64(RudaSymbolId symbol) {
    static const uint64_t positive_zero = UINT64_C(0x0000000000000000);
    static const uint64_t negative_zero = UINT64_C(0x8000000000000000);
    static const uint64_t one = UINT64_C(0x3ff0000000000000);
    static const uint64_t two = UINT64_C(0x4000000000000000);
    static const uint64_t infinity = UINT64_C(0x7ff0000000000000);
    static const uint64_t subnormal = UINT64_C(0x0000000000000001);
    static const uint64_t nan = UINT64_C(0x7ff8000000000042);
    static const uint64_t canonical_nan = UINT64_C(0x7ff8000000000000);
    RudaMirFunction function;
    RudaMirRegister left = RUDA_MIR_NO_REGISTER;
    RudaMirRegister right = RUDA_MIR_NO_REGISTER;
    RudaMirRegister copied = RUDA_MIR_NO_REGISTER;
    RudaMirRegister result_register = RUDA_MIR_NO_REGISTER;
    RudaMirRegister comparison = RUDA_MIR_NO_REGISTER;
    RudaMirBlockId block = RUDA_MIR_NO_BLOCK;
    RudaMirInstruction instruction;
    RudaMirValue result;
    RudaMirBlock *stored_block = NULL;
    RudaTypeId *stored_type = NULL;
    size_t index = 0;
    const RudaMirInstructionKind arithmetic[] = {
        RUDA_MIR_ADD_F64,
        RUDA_MIR_SUBTRACT_F64,
        RUDA_MIR_MULTIPLY_F64,
        RUDA_MIR_DIVIDE_F64
    };
    const uint64_t arithmetic_results[] = {
        UINT64_C(0x4008000000000000),
        UINT64_C(0xbff0000000000000),
        UINT64_C(0x4000000000000000),
        UINT64_C(0x3fe0000000000000)
    };

    memset(&function, 0, sizeof(function));
    RUDA_EXPECT(ruda_mir_function_init(
        &function, ruda_system_allocator(), symbol, NULL, 0, RUDA_TYPE_F64,
        ruda_mir_function_default_options()
    ));
    RUDA_EXPECT(ruda_mir_add_register(&function, RUDA_TYPE_F64, &left));
    RUDA_EXPECT(ruda_mir_add_register(&function, RUDA_TYPE_F64, &right));
    RUDA_EXPECT(ruda_mir_add_register(&function, RUDA_TYPE_F64, &copied));
    RUDA_EXPECT(ruda_mir_add_register(
        &function, RUDA_TYPE_F64, &result_register
    ));
    RUDA_EXPECT(ruda_mir_add_register(
        &function, RUDA_TYPE_BOOL, &comparison
    ));
    RUDA_EXPECT(ruda_mir_add_block(&function, &block));
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_CONSTANT_F64;
    instruction.target = left;
    instruction.f64_bits = one;
    RUDA_EXPECT(ruda_mir_append_instruction(&function, block, &instruction));
    instruction.target = right;
    instruction.f64_bits = two;
    RUDA_EXPECT(ruda_mir_append_instruction(&function, block, &instruction));
    instruction = ruda_test_mir_binary(
        RUDA_MIR_COPY, copied, left, RUDA_MIR_NO_REGISTER
    );
    RUDA_EXPECT(ruda_mir_append_instruction(&function, block, &instruction));
    instruction = ruda_test_mir_binary(
        RUDA_MIR_ADD_F64, result_register, copied, right
    );
    RUDA_EXPECT(ruda_mir_append_instruction(&function, block, &instruction));
    RUDA_EXPECT(ruda_mir_set_terminator(
        &function, block, ruda_test_mir_return(result_register)
    ));
    RUDA_EXPECT(ruda_mir_verify(&function) == RUDA_MIR_VERIFY_OK);
    memset(&result, 0, sizeof(result));
    RUDA_EXPECT(ruda_mir_interpret(
        &function, NULL, 0, ruda_system_allocator(),
        ruda_mir_interpret_default_options(), &result
    ) == RUDA_MIR_INTERPRET_OK);
    RUDA_EXPECT(result.type == RUDA_TYPE_F64
        && result.as.f64_bits == arithmetic_results[0]);

    stored_block = ruda_vector_at(&function.blocks, block);
    stored_type = ruda_vector_at(&function.register_types, left);
    RUDA_EXPECT(stored_block != NULL && stored_type != NULL);
    if (stored_type != NULL) {
        *stored_type = RUDA_TYPE_I64;
        RUDA_EXPECT(ruda_mir_verify(&function) == RUDA_MIR_VERIFY_TYPE_MISMATCH);
        *stored_type = RUDA_TYPE_F64;
    }
    if (stored_block != NULL) {
        RudaMirInstruction *add = ruda_vector_at(
            &stored_block->instructions,
            3
        );
        RUDA_EXPECT(add != NULL);
        if (add != NULL) {
            add->first = RUDA_MIR_NO_REGISTER;
            RUDA_EXPECT(
                ruda_mir_verify(&function) == RUDA_MIR_VERIFY_INVALID_REGISTER
            );
            add->first = copied;
        }
    }
    ruda_mir_function_dispose(&function);

    for (index = 0; index < sizeof(arithmetic) / sizeof(arithmetic[0]);
        index += 1) {
        memset(&function, 0, sizeof(function));
        RUDA_EXPECT(ruda_mir_function_init(
            &function, ruda_system_allocator(), symbol, NULL, 0, RUDA_TYPE_F64,
            ruda_mir_function_default_options()
        ));
        RUDA_EXPECT(ruda_mir_add_register(&function, RUDA_TYPE_F64, &left));
        RUDA_EXPECT(ruda_mir_add_register(&function, RUDA_TYPE_F64, &right));
        RUDA_EXPECT(ruda_mir_add_register(
            &function, RUDA_TYPE_F64, &result_register
        ));
        RUDA_EXPECT(ruda_mir_add_block(&function, &block));
        memset(&instruction, 0, sizeof(instruction));
        instruction.kind = RUDA_MIR_CONSTANT_F64;
        instruction.target = left;
        instruction.f64_bits = one;
        RUDA_EXPECT(ruda_mir_append_instruction(&function, block, &instruction));
        instruction.target = right;
        instruction.f64_bits = two;
        RUDA_EXPECT(ruda_mir_append_instruction(&function, block, &instruction));
        instruction = ruda_test_mir_binary(
            arithmetic[index], result_register, left, right
        );
        RUDA_EXPECT(ruda_mir_append_instruction(&function, block, &instruction));
        RUDA_EXPECT(ruda_mir_set_terminator(
            &function, block, ruda_test_mir_return(result_register)
        ));
        RUDA_EXPECT(ruda_mir_interpret(
            &function, NULL, 0, ruda_system_allocator(),
            ruda_mir_interpret_default_options(), &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.f64_bits == arithmetic_results[index]);
        ruda_mir_function_dispose(&function);
    }

    memset(&function, 0, sizeof(function));
    RUDA_EXPECT(ruda_mir_function_init(
        &function, ruda_system_allocator(), symbol, NULL, 0, RUDA_TYPE_F64,
        ruda_mir_function_default_options()
    ));
    RUDA_EXPECT(ruda_mir_add_register(&function, RUDA_TYPE_F64, &left));
    RUDA_EXPECT(ruda_mir_add_register(&function, RUDA_TYPE_F64, &result_register));
    RUDA_EXPECT(ruda_mir_add_block(&function, &block));
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_CONSTANT_F64;
    instruction.target = left;
    instruction.f64_bits = negative_zero;
    RUDA_EXPECT(ruda_mir_append_instruction(&function, block, &instruction));
    instruction = ruda_test_mir_binary(
        RUDA_MIR_NEGATE_F64, result_register, left, RUDA_MIR_NO_REGISTER
    );
    RUDA_EXPECT(ruda_mir_append_instruction(&function, block, &instruction));
    RUDA_EXPECT(ruda_mir_set_terminator(
        &function, block, ruda_test_mir_return(result_register)
    ));
    RUDA_EXPECT(ruda_mir_interpret(
        &function, NULL, 0, ruda_system_allocator(),
        ruda_mir_interpret_default_options(), &result
    ) == RUDA_MIR_INTERPRET_OK);
    RUDA_EXPECT(result.as.f64_bits == positive_zero);
    ruda_mir_function_dispose(&function);

    memset(&function, 0, sizeof(function));
    RUDA_EXPECT(ruda_mir_function_init(
        &function, ruda_system_allocator(), symbol, NULL, 0, RUDA_TYPE_BOOL,
        ruda_mir_function_default_options()
    ));
    RUDA_EXPECT(ruda_mir_add_register(&function, RUDA_TYPE_F64, &left));
    RUDA_EXPECT(ruda_mir_add_register(&function, RUDA_TYPE_F64, &right));
    RUDA_EXPECT(ruda_mir_add_register(&function, RUDA_TYPE_BOOL, &comparison));
    RUDA_EXPECT(ruda_mir_add_block(&function, &block));
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_CONSTANT_F64;
    instruction.target = left;
    instruction.f64_bits = negative_zero;
    RUDA_EXPECT(ruda_mir_append_instruction(&function, block, &instruction));
    instruction.target = right;
    instruction.f64_bits = positive_zero;
    RUDA_EXPECT(ruda_mir_append_instruction(&function, block, &instruction));
    instruction = ruda_test_mir_binary(
        RUDA_MIR_EQUAL_F64, comparison, left, right
    );
    RUDA_EXPECT(ruda_mir_append_instruction(&function, block, &instruction));
    RUDA_EXPECT(ruda_mir_set_terminator(
        &function, block, ruda_test_mir_return(comparison)
    ));
    RUDA_EXPECT(ruda_mir_interpret(
        &function, NULL, 0, ruda_system_allocator(),
        ruda_mir_interpret_default_options(), &result
    ) == RUDA_MIR_INTERPRET_OK && result.as.boolean);
    stored_block = ruda_vector_at(&function.blocks, block);
    if (stored_block != NULL) {
        RudaMirInstruction *comparison_instruction = ruda_vector_at(
            &stored_block->instructions,
            2
        );
        RUDA_EXPECT(comparison_instruction != NULL);
        if (comparison_instruction != NULL) {
            RudaMirInstruction *left_constant = ruda_vector_at(
                &stored_block->instructions,
                0
            );
            RUDA_EXPECT(left_constant != NULL);
            if (left_constant != NULL) {
                left_constant->f64_bits = nan;
            }
            comparison_instruction->kind = RUDA_MIR_EQUAL_F64;
            RUDA_EXPECT(ruda_mir_interpret(
                &function, NULL, 0, ruda_system_allocator(),
                ruda_mir_interpret_default_options(), &result
            ) == RUDA_MIR_INTERPRET_OK && !result.as.boolean);
            comparison_instruction->kind = RUDA_MIR_LESS_F64;
            RUDA_EXPECT(ruda_mir_interpret(
                &function, NULL, 0, ruda_system_allocator(),
                ruda_mir_interpret_default_options(), &result
            ) == RUDA_MIR_INTERPRET_OK && !result.as.boolean);
            comparison_instruction->kind = RUDA_MIR_LESS_EQUAL_F64;
            RUDA_EXPECT(ruda_mir_interpret(
                &function, NULL, 0, ruda_system_allocator(),
                ruda_mir_interpret_default_options(), &result
            ) == RUDA_MIR_INTERPRET_OK && !result.as.boolean);
        }
    }
    ruda_mir_function_dispose(&function);

    memset(&function, 0, sizeof(function));
    RUDA_EXPECT(ruda_mir_function_init(
        &function, ruda_system_allocator(), symbol, NULL, 0, RUDA_TYPE_F64,
        ruda_mir_function_default_options()
    ));
    RUDA_EXPECT(ruda_mir_add_register(&function, RUDA_TYPE_F64, &left));
    RUDA_EXPECT(ruda_mir_add_register(&function, RUDA_TYPE_F64, &right));
    RUDA_EXPECT(ruda_mir_add_register(&function, RUDA_TYPE_F64, &result_register));
    RUDA_EXPECT(ruda_mir_add_block(&function, &block));
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_CONSTANT_F64;
    instruction.target = left;
    instruction.f64_bits = infinity;
    RUDA_EXPECT(ruda_mir_append_instruction(&function, block, &instruction));
    instruction.target = right;
    instruction.f64_bits = infinity;
    RUDA_EXPECT(ruda_mir_append_instruction(&function, block, &instruction));
    instruction = ruda_test_mir_binary(
        RUDA_MIR_SUBTRACT_F64, result_register, left, right
    );
    RUDA_EXPECT(ruda_mir_append_instruction(&function, block, &instruction));
    RUDA_EXPECT(ruda_mir_set_terminator(
        &function, block, ruda_test_mir_return(result_register)
    ));
    RUDA_EXPECT(ruda_mir_interpret(
        &function, NULL, 0, ruda_system_allocator(),
        ruda_mir_interpret_default_options(), &result
    ) == RUDA_MIR_INTERPRET_OK && result.as.f64_bits == canonical_nan);
    ruda_mir_function_dispose(&function);

    memset(&function, 0, sizeof(function));
    RUDA_EXPECT(ruda_mir_function_init(
        &function, ruda_system_allocator(), symbol, NULL, 0, RUDA_TYPE_F64,
        ruda_mir_function_default_options()
    ));
    RUDA_EXPECT(ruda_mir_add_register(&function, RUDA_TYPE_F64, &left));
    RUDA_EXPECT(ruda_mir_add_block(&function, &block));
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_CONSTANT_F64;
    instruction.target = left;
    instruction.f64_bits = subnormal;
    RUDA_EXPECT(ruda_mir_append_instruction(&function, block, &instruction));
    RUDA_EXPECT(ruda_mir_set_terminator(
        &function, block, ruda_test_mir_return(left)
    ));
    RUDA_EXPECT(ruda_mir_interpret(
        &function, NULL, 0, ruda_system_allocator(),
        ruda_mir_interpret_default_options(), &result
    ) == RUDA_MIR_INTERPRET_OK && result.as.f64_bits == subnormal);
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


void ruda_test_mir_scalar_core(RudaSymbolId symbol) {
    RudaMirFunction function;
    RudaTypeId parameters[] = {RUDA_TYPE_I64, RUDA_TYPE_I64, RUDA_TYPE_I64};
    RudaMirRegister product = RUDA_MIR_NO_REGISTER;
    RudaMirRegister sum = RUDA_MIR_NO_REGISTER;
    RudaMirBlockId entry = RUDA_MIR_NO_BLOCK;
    RudaMirInstruction instruction;
    RudaMirValue arguments[3];
    RudaMirValue result;

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
}
void ruda_test_mir_verify(RudaSymbolId symbol) {
    ruda_test_mir_verify_limits_and_failures(symbol);
    ruda_test_mir_verify_long_cfg(symbol);
}

void ruda_test_mir_strings_f64(RudaSymbolId symbol) {
    ruda_test_mir_strings_and_panic(symbol);
    ruda_test_mir_string_equality(symbol);
    ruda_test_mir_f64(symbol);
}
