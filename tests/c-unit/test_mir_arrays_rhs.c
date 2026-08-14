#include "test_mir_arrays_support.h"

#include <string.h>

#include "ruda/base/allocator.h"

typedef struct RudaMirArrayRhsTestContext {
    RudaSymbolId expected;
    size_t calls;
    uint64_t result_bits;
} RudaMirArrayRhsTestContext;

static RudaMirInterpretStatus ruda_test_mir_array_rhs_handler(
    void *context,
    RudaSymbolId callee,
    const RudaMirValue *arguments,
    size_t argument_count,
    RudaMirValue *result
) {
    RudaMirArrayRhsTestContext *state = context;
    (void)arguments;
    if (state == NULL || result == NULL
        || !ruda_symbol_id_equal(callee, state->expected)
        || argument_count != 0) {
        return RUDA_MIR_INTERPRET_INVALID_ARGUMENT;
    }
    state->calls += 1;
    result->type = RUDA_TYPE_F64;
    result->as.f64_bits = state->result_bits;
    return RUDA_MIR_INTERPRET_OK;
}

void ruda_test_mir_arrays_rhs(
    RudaSymbolId symbol,
    RudaTypeId array_type,
    uint64_t one,
    uint64_t five
) {
    RudaMirFunction function;
    RudaMirArrayId array = RUDA_MIR_NO_ARRAY;
    RudaMirArrayConstructionId construction =
        RUDA_MIR_NO_ARRAY_CONSTRUCTION;
    RudaMirBlockId block = RUDA_MIR_NO_BLOCK;
    RudaMirInstruction instruction;
    RudaMirExecution execution;
    RudaMirInterpretOptions interpret_options =
        ruda_mir_interpret_default_options();
    RudaMirValue result;
    RudaMirValue handle;
    RudaMirValue element;
    RudaMirBlock *stored_block = NULL;
    {
        RudaMirArrayRhsTestContext context;
        RudaMirPanic panic;
        RudaMirArrayArgument argument;
        RudaMirCallId rhs_call = RUDA_MIR_NO_CALL;
        RudaMirRegister value = RUDA_MIR_NO_REGISTER;
        RudaMirRegister array_register = RUDA_MIR_NO_REGISTER;
        RudaMirRegister bounds_index = RUDA_MIR_NO_REGISTER;
        RudaMirRegister checked_array = RUDA_MIR_NO_REGISTER;
        RudaMirRegister rhs = RUDA_MIR_NO_REGISTER;
        RudaMirRegister output = RUDA_MIR_NO_REGISTER;
        RudaMirInstruction *stored_index = NULL;
        memset(&panic, 0, sizeof(panic));
        context.expected = symbol;
        context.calls = 0;
        context.result_bits = five;
        array = RUDA_MIR_NO_ARRAY;
        construction = RUDA_MIR_NO_ARRAY_CONSTRUCTION;
        block = RUDA_MIR_NO_BLOCK;
        RUDA_EXPECT(ruda_mir_function_init(
            &function,
            ruda_system_allocator(),
            symbol,
            NULL,
            0,
            RUDA_TYPE_F64,
            ruda_mir_function_default_options()
        ));
        RUDA_EXPECT(ruda_mir_add_register(&function, RUDA_TYPE_F64, &value));
        RUDA_EXPECT(ruda_mir_add_register(
            &function,
            array_type,
            &array_register
        ));
        RUDA_EXPECT(ruda_mir_add_register(
            &function,
            RUDA_TYPE_I64,
            &bounds_index
        ));
        RUDA_EXPECT(ruda_mir_add_register(
            &function,
            array_type,
            &checked_array
        ));
        RUDA_EXPECT(ruda_mir_add_register(&function, RUDA_TYPE_F64, &rhs));
        RUDA_EXPECT(ruda_mir_add_register(&function, RUDA_TYPE_F64, &output));
        RUDA_EXPECT(ruda_mir_add_array(
            &function,
            array_type,
            RUDA_TYPE_F64,
            &array
        ));
        argument.register_id = value;
        argument.type = RUDA_TYPE_F64;
        RUDA_EXPECT(ruda_mir_add_array_construction(
            &function,
            array,
            &argument,
            1,
            &construction
        ));
        RUDA_EXPECT(ruda_mir_add_call(
            &function,
            symbol,
            RUDA_TYPE_F64,
            NULL,
            0,
            &rhs_call
        ));
        RUDA_EXPECT(ruda_mir_add_block(&function, &block));
        memset(&instruction, 0, sizeof(instruction));
        instruction.kind = RUDA_MIR_CONSTANT_F64;
        instruction.target = value;
        instruction.f64_bits = one;
        RUDA_EXPECT(ruda_mir_append_instruction(&function, block, &instruction));
        memset(&instruction, 0, sizeof(instruction));
        instruction.kind = RUDA_MIR_ARRAY_NEW;
        instruction.target = array_register;
        instruction.array = array;
        instruction.array_construction = construction;
        RUDA_EXPECT(ruda_mir_append_instruction(&function, block, &instruction));
        memset(&instruction, 0, sizeof(instruction));
        instruction.kind = RUDA_MIR_CONSTANT_I64;
        instruction.target = bounds_index;
        instruction.integer = 1;
        RUDA_EXPECT(ruda_mir_append_instruction(&function, block, &instruction));
        memset(&instruction, 0, sizeof(instruction));
        instruction.kind = RUDA_MIR_ARRAY_REQUIRE_INDEX;
        instruction.target = checked_array;
        instruction.first = array_register;
        instruction.second = bounds_index;
        instruction.array = array;
        instruction.span.start = 40;
        instruction.span.end = 49;
        RUDA_EXPECT(ruda_mir_append_instruction(&function, block, &instruction));
        memset(&instruction, 0, sizeof(instruction));
        instruction.kind = RUDA_MIR_CALL;
        instruction.target = rhs;
        instruction.call = rhs_call;
        RUDA_EXPECT(ruda_mir_append_instruction(&function, block, &instruction));
        memset(&instruction, 0, sizeof(instruction));
        instruction.kind = RUDA_MIR_ARRAY_SET;
        instruction.target = output;
        instruction.first = checked_array;
        instruction.second = bounds_index;
        instruction.third = rhs;
        instruction.array = array;
        instruction.span.start = 40;
        instruction.span.end = 54;
        RUDA_EXPECT(ruda_mir_append_instruction(&function, block, &instruction));
        RUDA_EXPECT(ruda_mir_set_terminator(
            &function,
            block,
            ruda_test_mir_return(output)
        ));
        RUDA_EXPECT(ruda_mir_verify(&function) == RUDA_MIR_VERIFY_OK);
        RUDA_EXPECT(ruda_mir_execution_init(
            &execution,
            ruda_system_allocator(),
            ruda_mir_execution_default_options()
        ));
        interpret_options = ruda_mir_interpret_default_options();
        interpret_options.execution = &execution;
        interpret_options.call_context = &context;
        interpret_options.call = ruda_test_mir_array_rhs_handler;
        interpret_options.panic = &panic;
        RUDA_EXPECT(ruda_mir_interpret(
            &function,
            NULL,
            0,
            ruda_system_allocator(),
            interpret_options,
            &result
        ) == RUDA_MIR_INTERPRET_PANIC);
        RUDA_EXPECT(context.calls == 0);
        RUDA_EXPECT(panic.code != NULL && strcmp(panic.code, "G4002") == 0);
        RUDA_EXPECT(panic.span.start == 40 && panic.span.end == 49);
        stored_block = ruda_vector_at(&function.blocks, block);
        stored_index = stored_block != NULL
            ? ruda_vector_at(&stored_block->instructions, 2)
            : NULL;
        RUDA_EXPECT(stored_index != NULL);
        if (stored_index != NULL) {
            stored_index->integer = 0;
        }
        RUDA_EXPECT(ruda_mir_interpret(
            &function,
            NULL,
            0,
            ruda_system_allocator(),
            interpret_options,
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(context.calls == 1);
        RUDA_EXPECT(result.type == RUDA_TYPE_F64
            && result.as.f64_bits == five);
        RUDA_EXPECT(ruda_mir_execution_array_at(&execution, 1, &handle));
        RUDA_EXPECT(ruda_mir_execution_array_element(
            &execution,
            handle,
            0,
            &element
        ));
        RUDA_EXPECT(element.as.f64_bits == five);
        ruda_mir_panic_dispose(&panic);
        ruda_mir_execution_dispose(&execution);
        ruda_mir_function_dispose(&function);
    }
}
