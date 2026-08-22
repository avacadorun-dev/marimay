#include "test_mir_support.h"

#include <stdint.h>
#include <string.h>

#include "mmay/base/allocator.h"

static void mmay_test_mir_strings_and_panic(MarimaySymbolId symbol) {
    static const unsigned char message[] = "broken\n\xe2\x9b\x8f";
    MarimayMirFunction function;
    MarimayMirFunctionOptions function_options =
        mmay_mir_function_default_options();
    MarimayMirExecution execution;
    MarimayMirExecutionOptions execution_options =
        mmay_mir_execution_default_options();
    MarimayMirInterpretOptions interpret_options =
        mmay_mir_interpret_default_options();
    MarimayMirPanic panic;
    MarimayMirValue result;
    MarimayMirStringId string_id = MMAY_MIR_NO_STRING;
    MarimayMirRegister string_value = MMAY_MIR_NO_REGISTER;
    MarimayMirBlockId block = MMAY_MIR_NO_BLOCK;
    MarimayMirInstruction instruction;
    MarimayMirBlock *stored_block = NULL;
    MarimayMirTerminator *stored_terminator = NULL;
    MarimayMirStringConstant *stored_string = NULL;
    MarimayTypeId *stored_type = NULL;
    memset(&panic, 0, sizeof(panic));
    MMAY_EXPECT(mmay_mir_function_init(
        &function,
        mmay_system_allocator(),
        symbol,
        NULL,
        0,
        MMAY_TYPE_I64,
        function_options
    ));
    MMAY_EXPECT(mmay_mir_add_register(
        &function,
        MMAY_TYPE_STRING,
        &string_value
    ));
    MMAY_EXPECT(mmay_mir_add_string_constant(
        &function,
        message,
        sizeof(message) - 1,
        &string_id
    ));
    MMAY_EXPECT(mmay_mir_add_block(&function, &block));
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MMAY_MIR_CONSTANT_STRING;
    instruction.target = string_value;
    instruction.first = MMAY_MIR_NO_REGISTER;
    instruction.second = MMAY_MIR_NO_REGISTER;
    instruction.string_constant = string_id;
    instruction.span.start = 5;
    instruction.span.end = 18;
    MMAY_EXPECT(mmay_mir_append_instruction(&function, block, &instruction));
    MMAY_EXPECT(mmay_mir_set_terminator(
        &function,
        block,
        mmay_test_mir_panic(string_value, 3, 19)
    ));
    MMAY_EXPECT(mmay_mir_verify(&function) == MMAY_MIR_VERIFY_OK);
    MMAY_EXPECT(mmay_mir_execution_init(
        &execution,
        mmay_system_allocator(),
        execution_options
    ));
    interpret_options.execution = &execution;
    interpret_options.panic = &panic;
    MMAY_EXPECT(mmay_mir_interpret(
        &function,
        NULL,
        0,
        mmay_system_allocator(),
        interpret_options,
        &result
    ) == MMAY_MIR_INTERPRET_PANIC);
    MMAY_EXPECT(panic.code != NULL && strcmp(panic.code, "G4000") == 0);
    MMAY_EXPECT(panic.message.length == sizeof(message) - 1);
    MMAY_EXPECT(panic.message.data != NULL && memcmp(
        panic.message.data,
        message,
        sizeof(message) - 1
    ) == 0);
    MMAY_EXPECT(mmay_symbol_id_equal(panic.symbol, symbol));
    MMAY_EXPECT(panic.span.start == 3 && panic.span.end == 19);
    mmay_mir_panic_dispose(&panic);
    MMAY_EXPECT(execution.strings.length == 1);
    mmay_mir_execution_dispose(&execution);

    execution_options = mmay_mir_execution_default_options();
    execution_options.max_total_string_bytes = sizeof(message) - 2;
    MMAY_EXPECT(mmay_mir_execution_init(
        &execution,
        mmay_system_allocator(),
        execution_options
    ));
    interpret_options.execution = &execution;
    interpret_options.panic = &panic;
    MMAY_EXPECT(mmay_mir_interpret(
        &function,
        NULL,
        0,
        mmay_system_allocator(),
        interpret_options,
        &result
    ) == MMAY_MIR_INTERPRET_OBJECT_LIMIT);
    MMAY_EXPECT(execution.strings.length == 0);
    MMAY_EXPECT(execution.total_string_bytes == 0);
    MMAY_EXPECT(panic.message.data == NULL);
    mmay_mir_execution_dispose(&execution);
    execution_options = mmay_mir_execution_default_options();

    {
        size_t failure_offset = 0;
        bool reached_success = false;
        for (failure_offset = 0; failure_offset < 16; failure_offset += 1) {
            MarimayFailAllocator state;
            MarimayAllocator allocator;
            MarimayMirExecution failed_execution;
            MarimayMirPanic failed_panic;
            MarimayMirInterpretStatus status;
            memset(&failed_panic, 0, sizeof(failed_panic));
            mmay_fail_allocator_init(
                &state,
                mmay_system_allocator(),
                SIZE_MAX
            );
            allocator = mmay_fail_allocator(&state);
            MMAY_EXPECT(mmay_mir_execution_init(
                &failed_execution,
                allocator,
                execution_options
            ));
            state.failure_index = state.attempts + failure_offset;
            interpret_options.execution = &failed_execution;
            interpret_options.panic = &failed_panic;
            status = mmay_mir_interpret(
                &function,
                NULL,
                0,
                allocator,
                interpret_options,
                &result
            );
            if (status == MMAY_MIR_INTERPRET_PANIC) {
                reached_success = true;
                MMAY_EXPECT(failed_panic.message.length == sizeof(message) - 1);
            } else {
                MMAY_EXPECT(status == MMAY_MIR_INTERPRET_ALLOCATION_FAILED);
            }
            mmay_mir_panic_dispose(&failed_panic);
            mmay_mir_execution_dispose(&failed_execution);
            MMAY_EXPECT(state.live_allocations == 0);
            MMAY_EXPECT(state.live_bytes == 0);
            if (reached_success) {
                break;
            }
        }
        MMAY_EXPECT(reached_success);
    }

    {
        static const unsigned char invalid_utf8[] = { 0xffU };
        size_t original_count = function.string_constants.length;
        size_t original_bytes = function.string_bytes;
        MarimayMirStringId invalid_id = MMAY_MIR_NO_STRING;
        MMAY_EXPECT(!mmay_mir_add_string_constant(
            &function,
            invalid_utf8,
            sizeof(invalid_utf8),
            &invalid_id
        ));
        MMAY_EXPECT(function.string_constants.length == original_count);
        MMAY_EXPECT(function.string_bytes == original_bytes);
        MMAY_EXPECT(invalid_id == MMAY_MIR_NO_STRING);
    }

    stored_block = mmay_vector_at(&function.blocks, block);
    stored_terminator = stored_block != NULL ? &stored_block->terminator : NULL;
    stored_type = mmay_vector_at(&function.register_types, string_value);
    stored_string = mmay_vector_at(&function.string_constants, string_id);
    MMAY_EXPECT(stored_terminator != NULL && stored_type != NULL
        && stored_string != NULL);
    if (stored_type != NULL) {
        *stored_type = MMAY_TYPE_I64;
        MMAY_EXPECT(mmay_mir_verify(&function) == MMAY_MIR_VERIFY_TYPE_MISMATCH);
        *stored_type = MMAY_TYPE_STRING;
    }
    if (stored_terminator != NULL) {
        stored_terminator->value = MMAY_MIR_NO_REGISTER;
        MMAY_EXPECT(mmay_mir_verify(&function) == MMAY_MIR_VERIFY_INVALID_REGISTER);
        stored_terminator->value = string_value;
    }
    if (stored_string != NULL && stored_string->value.data != NULL) {
        unsigned char saved = stored_string->value.data[0];
        stored_string->value.data[0] = 0xffU;
        MMAY_EXPECT(mmay_mir_verify(&function) == MMAY_MIR_VERIFY_INVALID_ARGUMENT);
        stored_string->value.data[0] = saved;
    }
    {
        MarimayMirInstruction *stored_instruction = stored_block != NULL
            ? mmay_vector_at(&stored_block->instructions, 0)
            : NULL;
        MMAY_EXPECT(stored_instruction != NULL);
        if (stored_instruction != NULL) {
            stored_instruction->string_constant = MMAY_MIR_NO_STRING;
            MMAY_EXPECT(
                mmay_mir_verify(&function) == MMAY_MIR_VERIFY_INVALID_ARGUMENT
            );
            stored_instruction->string_constant = string_id;
        }
    }
    function.options.max_verify_work = 1;
    MMAY_EXPECT(mmay_mir_verify(&function) == MMAY_MIR_VERIFY_LIMIT_REACHED);
    mmay_mir_function_dispose(&function);
}

static void mmay_test_mir_f64(MarimaySymbolId symbol) {
    static const uint64_t positive_zero = UINT64_C(0x0000000000000000);
    static const uint64_t negative_zero = UINT64_C(0x8000000000000000);
    static const uint64_t one = UINT64_C(0x3ff0000000000000);
    static const uint64_t two = UINT64_C(0x4000000000000000);
    static const uint64_t infinity = UINT64_C(0x7ff0000000000000);
    static const uint64_t subnormal = UINT64_C(0x0000000000000001);
    static const uint64_t nan = UINT64_C(0x7ff8000000000042);
    static const uint64_t canonical_nan = UINT64_C(0x7ff8000000000000);
    MarimayMirFunction function;
    MarimayMirRegister left = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister right = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister copied = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister result_register = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister comparison = MMAY_MIR_NO_REGISTER;
    MarimayMirBlockId block = MMAY_MIR_NO_BLOCK;
    MarimayMirInstruction instruction;
    MarimayMirValue result;
    MarimayMirBlock *stored_block = NULL;
    MarimayTypeId *stored_type = NULL;
    size_t index = 0;
    const MarimayMirInstructionKind arithmetic[] = {
        MMAY_MIR_ADD_F64,
        MMAY_MIR_SUBTRACT_F64,
        MMAY_MIR_MULTIPLY_F64,
        MMAY_MIR_DIVIDE_F64
    };
    const uint64_t arithmetic_results[] = {
        UINT64_C(0x4008000000000000),
        UINT64_C(0xbff0000000000000),
        UINT64_C(0x4000000000000000),
        UINT64_C(0x3fe0000000000000)
    };

    memset(&function, 0, sizeof(function));
    MMAY_EXPECT(mmay_mir_function_init(
        &function, mmay_system_allocator(), symbol, NULL, 0, MMAY_TYPE_F64,
        mmay_mir_function_default_options()
    ));
    MMAY_EXPECT(mmay_mir_add_register(&function, MMAY_TYPE_F64, &left));
    MMAY_EXPECT(mmay_mir_add_register(&function, MMAY_TYPE_F64, &right));
    MMAY_EXPECT(mmay_mir_add_register(&function, MMAY_TYPE_F64, &copied));
    MMAY_EXPECT(mmay_mir_add_register(
        &function, MMAY_TYPE_F64, &result_register
    ));
    MMAY_EXPECT(mmay_mir_add_register(
        &function, MMAY_TYPE_BOOL, &comparison
    ));
    MMAY_EXPECT(mmay_mir_add_block(&function, &block));
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MMAY_MIR_CONSTANT_F64;
    instruction.target = left;
    instruction.f64_bits = one;
    MMAY_EXPECT(mmay_mir_append_instruction(&function, block, &instruction));
    instruction.target = right;
    instruction.f64_bits = two;
    MMAY_EXPECT(mmay_mir_append_instruction(&function, block, &instruction));
    instruction = mmay_test_mir_binary(
        MMAY_MIR_COPY, copied, left, MMAY_MIR_NO_REGISTER
    );
    MMAY_EXPECT(mmay_mir_append_instruction(&function, block, &instruction));
    instruction = mmay_test_mir_binary(
        MMAY_MIR_ADD_F64, result_register, copied, right
    );
    MMAY_EXPECT(mmay_mir_append_instruction(&function, block, &instruction));
    MMAY_EXPECT(mmay_mir_set_terminator(
        &function, block, mmay_test_mir_return(result_register)
    ));
    MMAY_EXPECT(mmay_mir_verify(&function) == MMAY_MIR_VERIFY_OK);
    memset(&result, 0, sizeof(result));
    MMAY_EXPECT(mmay_mir_interpret(
        &function, NULL, 0, mmay_system_allocator(),
        mmay_mir_interpret_default_options(), &result
    ) == MMAY_MIR_INTERPRET_OK);
    MMAY_EXPECT(result.type == MMAY_TYPE_F64
        && result.as.f64_bits == arithmetic_results[0]);

    stored_block = mmay_vector_at(&function.blocks, block);
    stored_type = mmay_vector_at(&function.register_types, left);
    MMAY_EXPECT(stored_block != NULL && stored_type != NULL);
    if (stored_type != NULL) {
        *stored_type = MMAY_TYPE_I64;
        MMAY_EXPECT(mmay_mir_verify(&function) == MMAY_MIR_VERIFY_TYPE_MISMATCH);
        *stored_type = MMAY_TYPE_F64;
    }
    if (stored_block != NULL) {
        MarimayMirInstruction *add = mmay_vector_at(
            &stored_block->instructions,
            3
        );
        MMAY_EXPECT(add != NULL);
        if (add != NULL) {
            add->first = MMAY_MIR_NO_REGISTER;
            MMAY_EXPECT(
                mmay_mir_verify(&function) == MMAY_MIR_VERIFY_INVALID_REGISTER
            );
            add->first = copied;
        }
    }
    mmay_mir_function_dispose(&function);

    for (index = 0; index < sizeof(arithmetic) / sizeof(arithmetic[0]);
        index += 1) {
        memset(&function, 0, sizeof(function));
        MMAY_EXPECT(mmay_mir_function_init(
            &function, mmay_system_allocator(), symbol, NULL, 0, MMAY_TYPE_F64,
            mmay_mir_function_default_options()
        ));
        MMAY_EXPECT(mmay_mir_add_register(&function, MMAY_TYPE_F64, &left));
        MMAY_EXPECT(mmay_mir_add_register(&function, MMAY_TYPE_F64, &right));
        MMAY_EXPECT(mmay_mir_add_register(
            &function, MMAY_TYPE_F64, &result_register
        ));
        MMAY_EXPECT(mmay_mir_add_block(&function, &block));
        memset(&instruction, 0, sizeof(instruction));
        instruction.kind = MMAY_MIR_CONSTANT_F64;
        instruction.target = left;
        instruction.f64_bits = one;
        MMAY_EXPECT(mmay_mir_append_instruction(&function, block, &instruction));
        instruction.target = right;
        instruction.f64_bits = two;
        MMAY_EXPECT(mmay_mir_append_instruction(&function, block, &instruction));
        instruction = mmay_test_mir_binary(
            arithmetic[index], result_register, left, right
        );
        MMAY_EXPECT(mmay_mir_append_instruction(&function, block, &instruction));
        MMAY_EXPECT(mmay_mir_set_terminator(
            &function, block, mmay_test_mir_return(result_register)
        ));
        MMAY_EXPECT(mmay_mir_interpret(
            &function, NULL, 0, mmay_system_allocator(),
            mmay_mir_interpret_default_options(), &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(result.as.f64_bits == arithmetic_results[index]);
        mmay_mir_function_dispose(&function);
    }

    memset(&function, 0, sizeof(function));
    MMAY_EXPECT(mmay_mir_function_init(
        &function, mmay_system_allocator(), symbol, NULL, 0, MMAY_TYPE_F64,
        mmay_mir_function_default_options()
    ));
    MMAY_EXPECT(mmay_mir_add_register(&function, MMAY_TYPE_F64, &left));
    MMAY_EXPECT(mmay_mir_add_register(&function, MMAY_TYPE_F64, &result_register));
    MMAY_EXPECT(mmay_mir_add_block(&function, &block));
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MMAY_MIR_CONSTANT_F64;
    instruction.target = left;
    instruction.f64_bits = negative_zero;
    MMAY_EXPECT(mmay_mir_append_instruction(&function, block, &instruction));
    instruction = mmay_test_mir_binary(
        MMAY_MIR_NEGATE_F64, result_register, left, MMAY_MIR_NO_REGISTER
    );
    MMAY_EXPECT(mmay_mir_append_instruction(&function, block, &instruction));
    MMAY_EXPECT(mmay_mir_set_terminator(
        &function, block, mmay_test_mir_return(result_register)
    ));
    MMAY_EXPECT(mmay_mir_interpret(
        &function, NULL, 0, mmay_system_allocator(),
        mmay_mir_interpret_default_options(), &result
    ) == MMAY_MIR_INTERPRET_OK);
    MMAY_EXPECT(result.as.f64_bits == positive_zero);
    mmay_mir_function_dispose(&function);

    memset(&function, 0, sizeof(function));
    MMAY_EXPECT(mmay_mir_function_init(
        &function, mmay_system_allocator(), symbol, NULL, 0, MMAY_TYPE_BOOL,
        mmay_mir_function_default_options()
    ));
    MMAY_EXPECT(mmay_mir_add_register(&function, MMAY_TYPE_F64, &left));
    MMAY_EXPECT(mmay_mir_add_register(&function, MMAY_TYPE_F64, &right));
    MMAY_EXPECT(mmay_mir_add_register(&function, MMAY_TYPE_BOOL, &comparison));
    MMAY_EXPECT(mmay_mir_add_block(&function, &block));
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MMAY_MIR_CONSTANT_F64;
    instruction.target = left;
    instruction.f64_bits = negative_zero;
    MMAY_EXPECT(mmay_mir_append_instruction(&function, block, &instruction));
    instruction.target = right;
    instruction.f64_bits = positive_zero;
    MMAY_EXPECT(mmay_mir_append_instruction(&function, block, &instruction));
    instruction = mmay_test_mir_binary(
        MMAY_MIR_EQUAL_F64, comparison, left, right
    );
    MMAY_EXPECT(mmay_mir_append_instruction(&function, block, &instruction));
    MMAY_EXPECT(mmay_mir_set_terminator(
        &function, block, mmay_test_mir_return(comparison)
    ));
    MMAY_EXPECT(mmay_mir_interpret(
        &function, NULL, 0, mmay_system_allocator(),
        mmay_mir_interpret_default_options(), &result
    ) == MMAY_MIR_INTERPRET_OK && result.as.boolean);
    stored_block = mmay_vector_at(&function.blocks, block);
    if (stored_block != NULL) {
        MarimayMirInstruction *comparison_instruction = mmay_vector_at(
            &stored_block->instructions,
            2
        );
        MMAY_EXPECT(comparison_instruction != NULL);
        if (comparison_instruction != NULL) {
            MarimayMirInstruction *left_constant = mmay_vector_at(
                &stored_block->instructions,
                0
            );
            MMAY_EXPECT(left_constant != NULL);
            if (left_constant != NULL) {
                left_constant->f64_bits = nan;
            }
            comparison_instruction->kind = MMAY_MIR_EQUAL_F64;
            MMAY_EXPECT(mmay_mir_interpret(
                &function, NULL, 0, mmay_system_allocator(),
                mmay_mir_interpret_default_options(), &result
            ) == MMAY_MIR_INTERPRET_OK && !result.as.boolean);
            comparison_instruction->kind = MMAY_MIR_LESS_F64;
            MMAY_EXPECT(mmay_mir_interpret(
                &function, NULL, 0, mmay_system_allocator(),
                mmay_mir_interpret_default_options(), &result
            ) == MMAY_MIR_INTERPRET_OK && !result.as.boolean);
            comparison_instruction->kind = MMAY_MIR_LESS_EQUAL_F64;
            MMAY_EXPECT(mmay_mir_interpret(
                &function, NULL, 0, mmay_system_allocator(),
                mmay_mir_interpret_default_options(), &result
            ) == MMAY_MIR_INTERPRET_OK && !result.as.boolean);
        }
    }
    mmay_mir_function_dispose(&function);

    memset(&function, 0, sizeof(function));
    MMAY_EXPECT(mmay_mir_function_init(
        &function, mmay_system_allocator(), symbol, NULL, 0, MMAY_TYPE_F64,
        mmay_mir_function_default_options()
    ));
    MMAY_EXPECT(mmay_mir_add_register(&function, MMAY_TYPE_F64, &left));
    MMAY_EXPECT(mmay_mir_add_register(&function, MMAY_TYPE_F64, &right));
    MMAY_EXPECT(mmay_mir_add_register(&function, MMAY_TYPE_F64, &result_register));
    MMAY_EXPECT(mmay_mir_add_block(&function, &block));
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MMAY_MIR_CONSTANT_F64;
    instruction.target = left;
    instruction.f64_bits = infinity;
    MMAY_EXPECT(mmay_mir_append_instruction(&function, block, &instruction));
    instruction.target = right;
    instruction.f64_bits = infinity;
    MMAY_EXPECT(mmay_mir_append_instruction(&function, block, &instruction));
    instruction = mmay_test_mir_binary(
        MMAY_MIR_SUBTRACT_F64, result_register, left, right
    );
    MMAY_EXPECT(mmay_mir_append_instruction(&function, block, &instruction));
    MMAY_EXPECT(mmay_mir_set_terminator(
        &function, block, mmay_test_mir_return(result_register)
    ));
    MMAY_EXPECT(mmay_mir_interpret(
        &function, NULL, 0, mmay_system_allocator(),
        mmay_mir_interpret_default_options(), &result
    ) == MMAY_MIR_INTERPRET_OK && result.as.f64_bits == canonical_nan);
    mmay_mir_function_dispose(&function);

    memset(&function, 0, sizeof(function));
    MMAY_EXPECT(mmay_mir_function_init(
        &function, mmay_system_allocator(), symbol, NULL, 0, MMAY_TYPE_F64,
        mmay_mir_function_default_options()
    ));
    MMAY_EXPECT(mmay_mir_add_register(&function, MMAY_TYPE_F64, &left));
    MMAY_EXPECT(mmay_mir_add_block(&function, &block));
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MMAY_MIR_CONSTANT_F64;
    instruction.target = left;
    instruction.f64_bits = subnormal;
    MMAY_EXPECT(mmay_mir_append_instruction(&function, block, &instruction));
    MMAY_EXPECT(mmay_mir_set_terminator(
        &function, block, mmay_test_mir_return(left)
    ));
    MMAY_EXPECT(mmay_mir_interpret(
        &function, NULL, 0, mmay_system_allocator(),
        mmay_mir_interpret_default_options(), &result
    ) == MMAY_MIR_INTERPRET_OK && result.as.f64_bits == subnormal);
    mmay_mir_function_dispose(&function);
}

static void mmay_test_mir_string_equality(MarimaySymbolId symbol) {
    static const unsigned char embedded[] = {'a', 0, 'b'};
    MarimayMirFunction function;
    MarimayMirExecution execution;
    MarimayMirInterpretOptions options = mmay_mir_interpret_default_options();
    MarimayMirValue result;
    MarimayMirStringId left_id = MMAY_MIR_NO_STRING;
    MarimayMirStringId right_id = MMAY_MIR_NO_STRING;
    MarimayMirRegister left = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister right = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister equal = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister uninitialized = MMAY_MIR_NO_REGISTER;
    MarimayMirBlockId block = MMAY_MIR_NO_BLOCK;
    MarimayMirInstruction instruction;
    MarimayMirBlock *stored_block = NULL;
    MarimayMirInstruction *comparison = NULL;
    MarimayMirStringConstant *right_constant = NULL;
    MarimayTypeId *right_type = NULL;

    MMAY_EXPECT(mmay_mir_function_init(
        &function,
        mmay_system_allocator(),
        symbol,
        NULL,
        0,
        MMAY_TYPE_BOOL,
        mmay_mir_function_default_options()
    ));
    MMAY_EXPECT(mmay_mir_add_register(&function, MMAY_TYPE_STRING, &left));
    MMAY_EXPECT(mmay_mir_add_register(&function, MMAY_TYPE_STRING, &right));
    MMAY_EXPECT(mmay_mir_add_register(&function, MMAY_TYPE_BOOL, &equal));
    MMAY_EXPECT(mmay_mir_add_register(
        &function,
        MMAY_TYPE_STRING,
        &uninitialized
    ));
    MMAY_EXPECT(mmay_mir_add_string_constant(
        &function,
        embedded,
        sizeof(embedded),
        &left_id
    ));
    MMAY_EXPECT(mmay_mir_add_string_constant(
        &function,
        embedded,
        sizeof(embedded),
        &right_id
    ));
    MMAY_EXPECT(mmay_mir_add_block(&function, &block));
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MMAY_MIR_CONSTANT_STRING;
    instruction.target = left;
    instruction.string_constant = left_id;
    MMAY_EXPECT(mmay_mir_append_instruction(&function, block, &instruction));
    instruction.target = right;
    instruction.string_constant = right_id;
    MMAY_EXPECT(mmay_mir_append_instruction(&function, block, &instruction));
    instruction = mmay_test_mir_binary(
        MMAY_MIR_EQUAL_STRING,
        equal,
        left,
        right
    );
    MMAY_EXPECT(mmay_mir_append_instruction(&function, block, &instruction));
    MMAY_EXPECT(mmay_mir_set_terminator(
        &function,
        block,
        mmay_test_mir_return(equal)
    ));
    MMAY_EXPECT(mmay_mir_verify(&function) == MMAY_MIR_VERIFY_OK);
    MMAY_EXPECT(mmay_mir_execution_init(
        &execution,
        mmay_system_allocator(),
        mmay_mir_execution_default_options()
    ));
    options.execution = &execution;
    MMAY_EXPECT(mmay_mir_interpret(
        &function,
        NULL,
        0,
        mmay_system_allocator(),
        options,
        &result
    ) == MMAY_MIR_INTERPRET_OK);
    MMAY_EXPECT(result.type == MMAY_TYPE_BOOL && result.as.boolean);
    mmay_mir_execution_dispose(&execution);

    right_constant = mmay_vector_at(&function.string_constants, right_id);
    MMAY_EXPECT(right_constant != NULL && right_constant->value.length == 3);
    if (right_constant != NULL && right_constant->value.length == 3) {
        right_constant->value.data[2] = 'c';
        MMAY_EXPECT(mmay_mir_execution_init(
            &execution,
            mmay_system_allocator(),
            mmay_mir_execution_default_options()
        ));
        options.execution = &execution;
        MMAY_EXPECT(mmay_mir_interpret(
            &function,
            NULL,
            0,
            mmay_system_allocator(),
            options,
            &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(result.type == MMAY_TYPE_BOOL && !result.as.boolean);
        mmay_mir_execution_dispose(&execution);
        right_constant->value.data[2] = 'b';
    }

    stored_block = mmay_vector_at(&function.blocks, block);
    comparison = stored_block != NULL
        ? mmay_vector_at(&stored_block->instructions, 2)
        : NULL;
    right_type = mmay_vector_at(&function.register_types, right);
    MMAY_EXPECT(comparison != NULL && right_type != NULL);
    if (right_type != NULL) {
        *right_type = MMAY_TYPE_I64;
        MMAY_EXPECT(mmay_mir_verify(&function) == MMAY_MIR_VERIFY_TYPE_MISMATCH);
        *right_type = MMAY_TYPE_STRING;
    }
    if (comparison != NULL) {
        comparison->second = uninitialized;
        MMAY_EXPECT(
            mmay_mir_verify(&function)
                == MMAY_MIR_VERIFY_UNINITIALIZED_REGISTER
        );
        comparison->second = right;
    }
    MMAY_EXPECT(mmay_mir_verify(&function) == MMAY_MIR_VERIFY_OK);
    options = mmay_mir_interpret_default_options();
    MMAY_EXPECT(mmay_mir_interpret(
        &function,
        NULL,
        0,
        mmay_system_allocator(),
        options,
        &result
    ) == MMAY_MIR_INTERPRET_INVALID_ARGUMENT);
    mmay_mir_function_dispose(&function);
}

static void mmay_test_mir_verify_limits_and_failures(MarimaySymbolId symbol) {
    MarimayTypeId parameter = MMAY_TYPE_I64;
    MarimayMirFunction function;
    MarimayMirBlockId block = MMAY_MIR_NO_BLOCK;
    MarimayMirFunctionOptions options = mmay_mir_function_default_options();
    size_t failure_offset = 0;
    bool reached_success = false;

    options.max_verify_work = 1;
    MMAY_EXPECT(mmay_mir_function_init(
        &function,
        mmay_system_allocator(),
        symbol,
        &parameter,
        1,
        MMAY_TYPE_I64,
        options
    ));
    MMAY_EXPECT(mmay_mir_add_block(&function, &block));
    MMAY_EXPECT(mmay_mir_set_terminator(
        &function,
        block,
        mmay_test_mir_return(0)
    ));
    MMAY_EXPECT(mmay_mir_verify(&function) == MMAY_MIR_VERIFY_LIMIT_REACHED);
    mmay_mir_function_dispose(&function);

    for (failure_offset = 0; failure_offset < 16; failure_offset += 1) {
        MarimayFailAllocator state;
        MarimayAllocator allocator;
        MarimayMirVerifyStatus status;
        size_t baseline_allocations = 0;
        size_t baseline_bytes = 0;
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
            &parameter,
            1,
            MMAY_TYPE_I64,
            mmay_mir_function_default_options()
        ));
        MMAY_EXPECT(mmay_mir_add_block(&function, &block));
        MMAY_EXPECT(mmay_mir_set_terminator(
            &function,
            block,
            mmay_test_mir_return(0)
        ));
        baseline_allocations = state.live_allocations;
        baseline_bytes = state.live_bytes;
        state.failure_index = state.attempts + failure_offset;
        status = mmay_mir_verify(&function);
        if (status == MMAY_MIR_VERIFY_OK) {
            reached_success = true;
        } else {
            MMAY_EXPECT(status == MMAY_MIR_VERIFY_ALLOCATION_FAILED);
            MMAY_EXPECT(state.live_allocations == baseline_allocations);
            MMAY_EXPECT(state.live_bytes == baseline_bytes);
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

static void mmay_test_mir_verify_long_cfg(MarimaySymbolId symbol) {
    enum { BLOCK_COUNT = 256 };
    MarimayMirFunction function;
    MarimayMirFunctionOptions options = mmay_mir_function_default_options();
    MarimayMirRegister value = MMAY_MIR_NO_REGISTER;
    MarimayMirBlockId blocks[BLOCK_COUNT];
    MarimayMirInstruction instruction;
    size_t index = 0;
    options.max_verify_work = 10000;
    MMAY_EXPECT(mmay_mir_function_init(
        &function,
        mmay_system_allocator(),
        symbol,
        NULL,
        0,
        MMAY_TYPE_I64,
        options
    ));
    MMAY_EXPECT(mmay_mir_add_register(&function, MMAY_TYPE_I64, &value));
    for (index = 0; index < BLOCK_COUNT; index += 1) {
        MMAY_EXPECT(mmay_mir_add_block(&function, &blocks[index]));
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MMAY_MIR_CONSTANT_I64;
    instruction.target = value;
    instruction.first = MMAY_MIR_NO_REGISTER;
    instruction.second = MMAY_MIR_NO_REGISTER;
    instruction.integer = 7;
    MMAY_EXPECT(mmay_mir_append_instruction(
        &function,
        blocks[0],
        &instruction
    ));
    for (index = 0; index + 1 < BLOCK_COUNT; index += 1) {
        MMAY_EXPECT(mmay_mir_set_terminator(
            &function,
            blocks[index],
            mmay_test_mir_goto(blocks[index + 1])
        ));
    }
    MMAY_EXPECT(mmay_mir_set_terminator(
        &function,
        blocks[BLOCK_COUNT - 1],
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
    mmay_mir_function_dispose(&function);
}


void mmay_test_mir_scalar_core(MarimaySymbolId symbol) {
    MarimayMirFunction function;
    MarimayTypeId parameters[] = {MMAY_TYPE_I64, MMAY_TYPE_I64, MMAY_TYPE_I64};
    MarimayMirRegister product = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister sum = MMAY_MIR_NO_REGISTER;
    MarimayMirBlockId entry = MMAY_MIR_NO_BLOCK;
    MarimayMirInstruction instruction;
    MarimayMirValue arguments[3];
    MarimayMirValue result;

    MMAY_EXPECT(mmay_mir_function_init(
        &function,
        mmay_system_allocator(),
        symbol,
        parameters,
        3,
        MMAY_TYPE_I64,
        mmay_mir_function_default_options()
    ));
    MMAY_EXPECT(mmay_mir_add_register(&function, MMAY_TYPE_I64, &product));
    MMAY_EXPECT(mmay_mir_add_register(&function, MMAY_TYPE_I64, &sum));
    MMAY_EXPECT(mmay_mir_add_block(&function, &entry));
    instruction = mmay_test_mir_binary(
        MMAY_MIR_MULTIPLY_CHECKED_I64,
        product,
        1,
        2
    );
    MMAY_EXPECT(mmay_mir_append_instruction(&function, entry, &instruction));
    instruction = mmay_test_mir_binary(
        MMAY_MIR_ADD_CHECKED_I64,
        sum,
        0,
        product
    );
    MMAY_EXPECT(mmay_mir_append_instruction(&function, entry, &instruction));
    MMAY_EXPECT(mmay_mir_set_terminator(
        &function,
        entry,
        mmay_test_mir_return(sum)
    ));
    MMAY_EXPECT(mmay_mir_verify(&function) == MMAY_MIR_VERIFY_OK);
    arguments[0].type = MMAY_TYPE_I64;
    arguments[0].as.i64 = 2;
    arguments[1].type = MMAY_TYPE_I64;
    arguments[1].as.i64 = 3;
    arguments[2].type = MMAY_TYPE_I64;
    arguments[2].as.i64 = 4;
    MMAY_EXPECT(mmay_mir_interpret(
        &function,
        arguments,
        3,
        mmay_system_allocator(),
        mmay_mir_interpret_default_options(),
        &result
    ) == MMAY_MIR_INTERPRET_OK);
    MMAY_EXPECT(result.type == MMAY_TYPE_I64);
    MMAY_EXPECT(result.as.i64 == 14);
    mmay_mir_function_dispose(&function);

    {
        MarimayTypeId overflow_parameters[] = {MMAY_TYPE_I64};
        MarimayMirRegister one = MMAY_MIR_NO_REGISTER;
        MarimayMirRegister added = MMAY_MIR_NO_REGISTER;
        MarimayMirBlockId block = MMAY_MIR_NO_BLOCK;
        MarimayMirValue maximum;
        MMAY_EXPECT(mmay_mir_function_init(
            &function,
            mmay_system_allocator(),
            symbol,
            overflow_parameters,
            1,
            MMAY_TYPE_I64,
            mmay_mir_function_default_options()
        ));
        MMAY_EXPECT(mmay_mir_add_register(&function, MMAY_TYPE_I64, &one));
        MMAY_EXPECT(mmay_mir_add_register(&function, MMAY_TYPE_I64, &added));
        MMAY_EXPECT(mmay_mir_add_block(&function, &block));
        memset(&instruction, 0, sizeof(instruction));
        instruction.kind = MMAY_MIR_CONSTANT_I64;
        instruction.target = one;
        instruction.integer = 1;
        MMAY_EXPECT(mmay_mir_append_instruction(&function, block, &instruction));
        instruction = mmay_test_mir_binary(
            MMAY_MIR_ADD_CHECKED_I64,
            added,
            0,
            one
        );
        MMAY_EXPECT(mmay_mir_append_instruction(&function, block, &instruction));
        MMAY_EXPECT(mmay_mir_set_terminator(
            &function,
            block,
            mmay_test_mir_return(added)
        ));
        maximum.type = MMAY_TYPE_I64;
        maximum.as.i64 = INT64_MAX;
        MMAY_EXPECT(mmay_mir_verify(&function) == MMAY_MIR_VERIFY_OK);
        MMAY_EXPECT(mmay_mir_interpret(
            &function,
            &maximum,
            1,
            mmay_system_allocator(),
            mmay_mir_interpret_default_options(),
            &result
        ) == MMAY_MIR_INTERPRET_OVERFLOW);
        mmay_mir_function_dispose(&function);
    }

    {
        MarimayTypeId invalid_parameters[] = {MMAY_TYPE_BOOL};
        MarimayMirRegister value = MMAY_MIR_NO_REGISTER;
        MarimayMirBlockId block = MMAY_MIR_NO_BLOCK;
        MMAY_EXPECT(mmay_mir_function_init(
            &function,
            mmay_system_allocator(),
            symbol,
            invalid_parameters,
            1,
            MMAY_TYPE_I64,
            mmay_mir_function_default_options()
        ));
        MMAY_EXPECT(mmay_mir_add_register(&function, MMAY_TYPE_I64, &value));
        MMAY_EXPECT(mmay_mir_add_block(&function, &block));
        instruction = mmay_test_mir_binary(
            MMAY_MIR_ADD_CHECKED_I64,
            value,
            0,
            0
        );
        MMAY_EXPECT(mmay_mir_append_instruction(&function, block, &instruction));
        MMAY_EXPECT(mmay_mir_set_terminator(
            &function,
            block,
            mmay_test_mir_return(value)
        ));
        MMAY_EXPECT(mmay_mir_verify(&function) == MMAY_MIR_VERIFY_TYPE_MISMATCH);
        mmay_mir_function_dispose(&function);
    }

    {
        MarimayTypeId branch_parameters[] = {MMAY_TYPE_BOOL};
        MarimayMirRegister selected = MMAY_MIR_NO_REGISTER;
        MarimayMirBlockId branch = MMAY_MIR_NO_BLOCK;
        MarimayMirBlockId when_true = MMAY_MIR_NO_BLOCK;
        MarimayMirBlockId when_false = MMAY_MIR_NO_BLOCK;
        MarimayMirBlockId join = MMAY_MIR_NO_BLOCK;
        MarimayMirValue condition;
        MMAY_EXPECT(mmay_mir_function_init(
            &function,
            mmay_system_allocator(),
            symbol,
            branch_parameters,
            1,
            MMAY_TYPE_I64,
            mmay_mir_function_default_options()
        ));
        MMAY_EXPECT(mmay_mir_add_register(&function, MMAY_TYPE_I64, &selected));
        MMAY_EXPECT(mmay_mir_add_block(&function, &branch));
        MMAY_EXPECT(mmay_mir_add_block(&function, &when_true));
        MMAY_EXPECT(mmay_mir_add_block(&function, &when_false));
        MMAY_EXPECT(mmay_mir_add_block(&function, &join));
        MMAY_EXPECT(mmay_mir_set_terminator(
            &function,
            branch,
            mmay_test_mir_branch(0, when_true, when_false)
        ));
        memset(&instruction, 0, sizeof(instruction));
        instruction.kind = MMAY_MIR_CONSTANT_I64;
        instruction.target = selected;
        instruction.integer = 10;
        MMAY_EXPECT(mmay_mir_append_instruction(
            &function,
            when_true,
            &instruction
        ));
        MMAY_EXPECT(mmay_mir_set_terminator(
            &function,
            when_true,
            mmay_test_mir_goto(join)
        ));
        instruction.integer = 20;
        MMAY_EXPECT(mmay_mir_append_instruction(
            &function,
            when_false,
            &instruction
        ));
        MMAY_EXPECT(mmay_mir_set_terminator(
            &function,
            when_false,
            mmay_test_mir_goto(join)
        ));
        MMAY_EXPECT(mmay_mir_set_terminator(
            &function,
            join,
            mmay_test_mir_return(selected)
        ));
        MMAY_EXPECT(mmay_mir_verify(&function) == MMAY_MIR_VERIFY_OK);
        condition.type = MMAY_TYPE_BOOL;
        condition.as.boolean = true;
        MMAY_EXPECT(mmay_mir_interpret(
            &function,
            &condition,
            1,
            mmay_system_allocator(),
            mmay_mir_interpret_default_options(),
            &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(result.as.i64 == 10);
        condition.as.boolean = false;
        MMAY_EXPECT(mmay_mir_interpret(
            &function,
            &condition,
            1,
            mmay_system_allocator(),
            mmay_mir_interpret_default_options(),
            &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(result.as.i64 == 20);
        mmay_mir_function_dispose(&function);
    }

    {
        MarimayTypeId branch_parameters[] = {MMAY_TYPE_BOOL};
        MarimayMirRegister selected = MMAY_MIR_NO_REGISTER;
        MarimayMirBlockId branch = MMAY_MIR_NO_BLOCK;
        MarimayMirBlockId when_true = MMAY_MIR_NO_BLOCK;
        MarimayMirBlockId when_false = MMAY_MIR_NO_BLOCK;
        MarimayMirBlockId join = MMAY_MIR_NO_BLOCK;
        MMAY_EXPECT(mmay_mir_function_init(
            &function,
            mmay_system_allocator(),
            symbol,
            branch_parameters,
            1,
            MMAY_TYPE_I64,
            mmay_mir_function_default_options()
        ));
        MMAY_EXPECT(mmay_mir_add_register(&function, MMAY_TYPE_I64, &selected));
        MMAY_EXPECT(mmay_mir_add_block(&function, &branch));
        MMAY_EXPECT(mmay_mir_add_block(&function, &when_true));
        MMAY_EXPECT(mmay_mir_add_block(&function, &when_false));
        MMAY_EXPECT(mmay_mir_add_block(&function, &join));
        MMAY_EXPECT(mmay_mir_set_terminator(
            &function,
            branch,
            mmay_test_mir_branch(0, when_true, when_false)
        ));
        memset(&instruction, 0, sizeof(instruction));
        instruction.kind = MMAY_MIR_CONSTANT_I64;
        instruction.target = selected;
        instruction.integer = 10;
        MMAY_EXPECT(mmay_mir_append_instruction(
            &function,
            when_true,
            &instruction
        ));
        MMAY_EXPECT(mmay_mir_set_terminator(
            &function,
            when_true,
            mmay_test_mir_goto(join)
        ));
        MMAY_EXPECT(mmay_mir_set_terminator(
            &function,
            when_false,
            mmay_test_mir_goto(join)
        ));
        MMAY_EXPECT(mmay_mir_set_terminator(
            &function,
            join,
            mmay_test_mir_return(selected)
        ));
        MMAY_EXPECT(
            mmay_mir_verify(&function)
                == MMAY_MIR_VERIFY_UNINITIALIZED_REGISTER
        );
        mmay_mir_function_dispose(&function);
    }
}
void mmay_test_mir_verify(MarimaySymbolId symbol) {
    mmay_test_mir_verify_limits_and_failures(symbol);
    mmay_test_mir_verify_long_cfg(symbol);
}

void mmay_test_mir_strings_f64(MarimaySymbolId symbol) {
    mmay_test_mir_strings_and_panic(symbol);
    mmay_test_mir_string_equality(symbol);
    mmay_test_mir_f64(symbol);
}
