#include "test_mir_arrays_support.h"

#include <string.h>

#include "mmay/base/allocator.h"

typedef struct MarimayMirArrayRhsTestContext {
    MarimaySymbolId expected;
    size_t calls;
    uint64_t result_bits;
} MarimayMirArrayRhsTestContext;

static MarimayMirInterpretStatus mmay_test_mir_array_rhs_handler(
    void *context,
    MarimaySymbolId callee,
    const MarimayMirValue *arguments,
    size_t argument_count,
    MarimayMirValue *result
) {
    MarimayMirArrayRhsTestContext *state = context;
    (void)arguments;
    if (state == NULL || result == NULL
        || !mmay_symbol_id_equal(callee, state->expected)
        || argument_count != 0) {
        return MMAY_MIR_INTERPRET_INVALID_ARGUMENT;
    }
    state->calls += 1;
    result->type = MMAY_TYPE_F64;
    result->as.f64_bits = state->result_bits;
    return MMAY_MIR_INTERPRET_OK;
}

void mmay_test_mir_arrays_rhs(
    MarimaySymbolId symbol,
    MarimayTypeId array_type,
    uint64_t one,
    uint64_t five
) {
    MarimayMirFunction function;
    MarimayMirArrayId array = MMAY_MIR_NO_ARRAY;
    MarimayMirArrayConstructionId construction =
        MMAY_MIR_NO_ARRAY_CONSTRUCTION;
    MarimayMirBlockId block = MMAY_MIR_NO_BLOCK;
    MarimayMirInstruction instruction;
    MarimayMirExecution execution;
    MarimayMirInterpretOptions interpret_options =
        mmay_mir_interpret_default_options();
    MarimayMirValue result;
    MarimayMirValue handle;
    MarimayMirValue element;
    MarimayMirBlock *stored_block = NULL;
    {
        MarimayMirArrayRhsTestContext context;
        MarimayMirPanic panic;
        MarimayMirArrayArgument argument;
        MarimayMirCallId rhs_call = MMAY_MIR_NO_CALL;
        MarimayMirRegister value = MMAY_MIR_NO_REGISTER;
        MarimayMirRegister array_register = MMAY_MIR_NO_REGISTER;
        MarimayMirRegister bounds_index = MMAY_MIR_NO_REGISTER;
        MarimayMirRegister checked_array = MMAY_MIR_NO_REGISTER;
        MarimayMirRegister rhs = MMAY_MIR_NO_REGISTER;
        MarimayMirRegister output = MMAY_MIR_NO_REGISTER;
        MarimayMirInstruction *stored_index = NULL;
        memset(&panic, 0, sizeof(panic));
        context.expected = symbol;
        context.calls = 0;
        context.result_bits = five;
        array = MMAY_MIR_NO_ARRAY;
        construction = MMAY_MIR_NO_ARRAY_CONSTRUCTION;
        block = MMAY_MIR_NO_BLOCK;
        MMAY_EXPECT(mmay_mir_function_init(
            &function,
            mmay_system_allocator(),
            symbol,
            NULL,
            0,
            MMAY_TYPE_F64,
            mmay_mir_function_default_options()
        ));
        MMAY_EXPECT(mmay_mir_add_register(&function, MMAY_TYPE_F64, &value));
        MMAY_EXPECT(mmay_mir_add_register(
            &function,
            array_type,
            &array_register
        ));
        MMAY_EXPECT(mmay_mir_add_register(
            &function,
            MMAY_TYPE_I64,
            &bounds_index
        ));
        MMAY_EXPECT(mmay_mir_add_register(
            &function,
            array_type,
            &checked_array
        ));
        MMAY_EXPECT(mmay_mir_add_register(&function, MMAY_TYPE_F64, &rhs));
        MMAY_EXPECT(mmay_mir_add_register(&function, MMAY_TYPE_F64, &output));
        MMAY_EXPECT(mmay_mir_add_array(
            &function,
            array_type,
            MMAY_TYPE_F64,
            &array
        ));
        argument.register_id = value;
        argument.type = MMAY_TYPE_F64;
        MMAY_EXPECT(mmay_mir_add_array_construction(
            &function,
            array,
            &argument,
            1,
            &construction
        ));
        MMAY_EXPECT(mmay_mir_add_call(
            &function,
            symbol,
            MMAY_TYPE_F64,
            NULL,
            0,
            &rhs_call
        ));
        MMAY_EXPECT(mmay_mir_add_block(&function, &block));
        memset(&instruction, 0, sizeof(instruction));
        instruction.kind = MMAY_MIR_CONSTANT_F64;
        instruction.target = value;
        instruction.f64_bits = one;
        MMAY_EXPECT(mmay_mir_append_instruction(&function, block, &instruction));
        memset(&instruction, 0, sizeof(instruction));
        instruction.kind = MMAY_MIR_ARRAY_NEW;
        instruction.target = array_register;
        instruction.array = array;
        instruction.array_construction = construction;
        MMAY_EXPECT(mmay_mir_append_instruction(&function, block, &instruction));
        memset(&instruction, 0, sizeof(instruction));
        instruction.kind = MMAY_MIR_CONSTANT_I64;
        instruction.target = bounds_index;
        instruction.integer = 1;
        MMAY_EXPECT(mmay_mir_append_instruction(&function, block, &instruction));
        memset(&instruction, 0, sizeof(instruction));
        instruction.kind = MMAY_MIR_ARRAY_REQUIRE_INDEX;
        instruction.target = checked_array;
        instruction.first = array_register;
        instruction.second = bounds_index;
        instruction.array = array;
        instruction.span.start = 40;
        instruction.span.end = 49;
        MMAY_EXPECT(mmay_mir_append_instruction(&function, block, &instruction));
        memset(&instruction, 0, sizeof(instruction));
        instruction.kind = MMAY_MIR_CALL;
        instruction.target = rhs;
        instruction.call = rhs_call;
        MMAY_EXPECT(mmay_mir_append_instruction(&function, block, &instruction));
        memset(&instruction, 0, sizeof(instruction));
        instruction.kind = MMAY_MIR_ARRAY_SET;
        instruction.target = output;
        instruction.first = checked_array;
        instruction.second = bounds_index;
        instruction.third = rhs;
        instruction.array = array;
        instruction.span.start = 40;
        instruction.span.end = 54;
        MMAY_EXPECT(mmay_mir_append_instruction(&function, block, &instruction));
        MMAY_EXPECT(mmay_mir_set_terminator(
            &function,
            block,
            mmay_test_mir_return(output)
        ));
        MMAY_EXPECT(mmay_mir_verify(&function) == MMAY_MIR_VERIFY_OK);
        MMAY_EXPECT(mmay_mir_execution_init(
            &execution,
            mmay_system_allocator(),
            mmay_mir_execution_default_options()
        ));
        interpret_options = mmay_mir_interpret_default_options();
        interpret_options.execution = &execution;
        interpret_options.call_context = &context;
        interpret_options.call = mmay_test_mir_array_rhs_handler;
        interpret_options.panic = &panic;
        MMAY_EXPECT(mmay_mir_interpret(
            &function,
            NULL,
            0,
            mmay_system_allocator(),
            interpret_options,
            &result
        ) == MMAY_MIR_INTERPRET_PANIC);
        MMAY_EXPECT(context.calls == 0);
        MMAY_EXPECT(panic.code != NULL && strcmp(panic.code, "G4002") == 0);
        MMAY_EXPECT(panic.span.start == 40 && panic.span.end == 49);
        stored_block = mmay_vector_at(&function.blocks, block);
        stored_index = stored_block != NULL
            ? mmay_vector_at(&stored_block->instructions, 2)
            : NULL;
        MMAY_EXPECT(stored_index != NULL);
        if (stored_index != NULL) {
            stored_index->integer = 0;
        }
        MMAY_EXPECT(mmay_mir_interpret(
            &function,
            NULL,
            0,
            mmay_system_allocator(),
            interpret_options,
            &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(context.calls == 1);
        MMAY_EXPECT(result.type == MMAY_TYPE_F64
            && result.as.f64_bits == five);
        MMAY_EXPECT(mmay_mir_execution_array_at(&execution, 1, &handle));
        MMAY_EXPECT(mmay_mir_execution_array_element(
            &execution,
            handle,
            0,
            &element
        ));
        MMAY_EXPECT(element.as.f64_bits == five);
        mmay_mir_panic_dispose(&panic);
        mmay_mir_execution_dispose(&execution);
        mmay_mir_function_dispose(&function);
    }
}
