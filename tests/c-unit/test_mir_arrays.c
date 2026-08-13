#include "test_support.h"

#include <stdint.h>
#include <string.h>

#include "ruda/base/allocator.h"
#include "ruda/mir/mir.h"

typedef struct RudaMirArrayRhsTestContext {
    RudaSymbolId expected;
    size_t calls;
    uint64_t result_bits;
} RudaMirArrayRhsTestContext;

typedef struct RudaMirManagedCallTestContext {
    RudaSymbolId expected;
    RudaMirValue result;
    size_t calls;
} RudaMirManagedCallTestContext;

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

static RudaMirInterpretStatus ruda_test_mir_managed_call_handler(
    void *context,
    RudaSymbolId callee,
    const RudaMirValue *arguments,
    size_t argument_count,
    RudaMirValue *result
) {
    RudaMirManagedCallTestContext *state = context;
    (void)arguments;
    if (state == NULL || result == NULL
        || !ruda_symbol_id_equal(callee, state->expected)
        || argument_count != 0) {
        return RUDA_MIR_INTERPRET_INVALID_ARGUMENT;
    }
    state->calls += 1;
    *result = state->result;
    return RUDA_MIR_INTERPRET_OK;
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

void ruda_test_mir_arrays(RudaSymbolId symbol) {
    static const uint64_t one = UINT64_C(0x3ff0000000000000);
    static const uint64_t two = UINT64_C(0x4000000000000000);
    static const uint64_t three = UINT64_C(0x4008000000000000);
    static const uint64_t four = UINT64_C(0x4010000000000000);
    static const uint64_t five = UINT64_C(0x4014000000000000);
    static const unsigned char negative_message[] =
        "array index -1 is outside length 1";
    static const unsigned char upper_message[] =
        "array index 1 is outside length 1";
    static const unsigned char minimum_message[] =
        "array index -9223372036854775808 is outside length 1";
    static const unsigned char maximum_message[] =
        "array index 9223372036854775807 is outside length 1";
    const RudaTypeId array_type = RUDA_TYPE_FIRST_DYNAMIC + 30;
    RudaMirFunction function;
    RudaMirArrayId array = RUDA_MIR_NO_ARRAY;
    RudaMirArrayConstructionId construction =
        RUDA_MIR_NO_ARRAY_CONSTRUCTION;
    RudaMirArrayConstructionId empty_construction =
        RUDA_MIR_NO_ARRAY_CONSTRUCTION;
    RudaMirArrayArgument arguments[3];
    RudaMirRegister values[3];
    RudaMirRegister array_value = RUDA_MIR_NO_REGISTER;
    RudaMirRegister empty_array_value = RUDA_MIR_NO_REGISTER;
    RudaMirRegister alias = RUDA_MIR_NO_REGISTER;
    RudaMirRegister length = RUDA_MIR_NO_REGISTER;
    RudaMirRegister offset = RUDA_MIR_NO_REGISTER;
    RudaMirRegister index = RUDA_MIR_NO_REGISTER;
    RudaMirRegister replacement = RUDA_MIR_NO_REGISTER;
    RudaMirRegister set_result = RUDA_MIR_NO_REGISTER;
    RudaMirRegister projected = RUDA_MIR_NO_REGISTER;
    RudaMirRegister uninitialized = RUDA_MIR_NO_REGISTER;
    RudaMirBlockId block = RUDA_MIR_NO_BLOCK;
    RudaMirInstruction instruction;
    RudaMirExecution execution;
    RudaMirInterpretOptions interpret_options =
        ruda_mir_interpret_default_options();
    RudaMirValue result;
    RudaMirValue handle;
    RudaMirValue collision_handle;
    RudaMirValue element;
    RudaMirBlock *stored_block = NULL;
    RudaMirInstruction *stored_set = NULL;
    RudaMirArray *stored_array = NULL;
    size_t inspected_length = 0;
    size_t value_index = 0;

    for (value_index = 0; value_index < 3; value_index += 1) {
        values[value_index] = RUDA_MIR_NO_REGISTER;
    }
    RUDA_EXPECT(ruda_mir_function_init(
        &function,
        ruda_system_allocator(),
        symbol,
        NULL,
        0,
        RUDA_TYPE_F64,
        ruda_mir_function_default_options()
    ));
    RUDA_EXPECT(!ruda_mir_add_array(
        &function,
        RUDA_TYPE_I64,
        RUDA_TYPE_F64,
        &array
    ));
    RUDA_EXPECT(array == RUDA_MIR_NO_ARRAY);
    RUDA_EXPECT(ruda_mir_add_array(
        &function,
        array_type,
        RUDA_TYPE_F64,
        &array
    ));
    {
        RudaMirArrayId duplicate = RUDA_MIR_NO_ARRAY;
        RUDA_EXPECT(ruda_mir_add_array(
            &function,
            array_type,
            RUDA_TYPE_F64,
            &duplicate
        ));
        RUDA_EXPECT(duplicate == array);
        RUDA_EXPECT(!ruda_mir_add_array(
            &function,
            array_type,
            RUDA_TYPE_I64,
            &duplicate
        ));
    }
    for (value_index = 0; value_index < 3; value_index += 1) {
        RUDA_EXPECT(ruda_mir_add_register(
            &function,
            RUDA_TYPE_F64,
            &values[value_index]
        ));
        arguments[value_index].register_id = values[value_index];
        arguments[value_index].type = RUDA_TYPE_F64;
    }
    RUDA_EXPECT(ruda_mir_add_register(
        &function,
        array_type,
        &array_value
    ));
    RUDA_EXPECT(ruda_mir_add_register(
        &function,
        array_type,
        &empty_array_value
    ));
    RUDA_EXPECT(ruda_mir_add_register(&function, array_type, &alias));
    RUDA_EXPECT(ruda_mir_add_register(&function, RUDA_TYPE_I64, &length));
    RUDA_EXPECT(ruda_mir_add_register(&function, RUDA_TYPE_I64, &offset));
    RUDA_EXPECT(ruda_mir_add_register(&function, RUDA_TYPE_I64, &index));
    RUDA_EXPECT(ruda_mir_add_register(
        &function,
        RUDA_TYPE_F64,
        &replacement
    ));
    RUDA_EXPECT(ruda_mir_add_register(
        &function,
        RUDA_TYPE_F64,
        &set_result
    ));
    RUDA_EXPECT(ruda_mir_add_register(
        &function,
        RUDA_TYPE_F64,
        &projected
    ));
    RUDA_EXPECT(ruda_mir_add_register(
        &function,
        RUDA_TYPE_F64,
        &uninitialized
    ));
    RUDA_EXPECT(ruda_mir_add_array_construction(
        &function,
        array,
        arguments,
        3,
        &construction
    ));
    RUDA_EXPECT(ruda_mir_add_array_construction(
        &function,
        array,
        NULL,
        0,
        &empty_construction
    ));
    RUDA_EXPECT(ruda_mir_add_block(&function, &block));
    for (value_index = 0; value_index < 3; value_index += 1) {
        memset(&instruction, 0, sizeof(instruction));
        instruction.kind = RUDA_MIR_CONSTANT_F64;
        instruction.target = values[value_index];
        instruction.f64_bits = value_index == 0 ? one
            : value_index == 1 ? two : three;
        RUDA_EXPECT(ruda_mir_append_instruction(
            &function,
            block,
            &instruction
        ));
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_ARRAY_NEW;
    instruction.target = array_value;
    instruction.array = array;
    instruction.array_construction = construction;
    RUDA_EXPECT(ruda_mir_append_instruction(&function, block, &instruction));
    instruction = ruda_test_mir_binary(
        RUDA_MIR_COPY,
        alias,
        array_value,
        RUDA_MIR_NO_REGISTER
    );
    RUDA_EXPECT(ruda_mir_append_instruction(&function, block, &instruction));
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_ARRAY_LENGTH;
    instruction.target = length;
    instruction.first = alias;
    instruction.array = array;
    RUDA_EXPECT(ruda_mir_append_instruction(&function, block, &instruction));
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_CONSTANT_I64;
    instruction.target = offset;
    instruction.integer = 2;
    RUDA_EXPECT(ruda_mir_append_instruction(&function, block, &instruction));
    instruction = ruda_test_mir_binary(
        RUDA_MIR_SUBTRACT_CHECKED_I64,
        index,
        length,
        offset
    );
    RUDA_EXPECT(ruda_mir_append_instruction(&function, block, &instruction));
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_CONSTANT_F64;
    instruction.target = replacement;
    instruction.f64_bits = four;
    RUDA_EXPECT(ruda_mir_append_instruction(&function, block, &instruction));
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_ARRAY_SET;
    instruction.target = set_result;
    instruction.first = array_value;
    instruction.second = index;
    instruction.third = replacement;
    instruction.array = array;
    instruction.span.start = 10;
    instruction.span.end = 18;
    RUDA_EXPECT(ruda_mir_append_instruction(&function, block, &instruction));
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_ARRAY_GET;
    instruction.target = projected;
    instruction.first = alias;
    instruction.second = index;
    instruction.array = array;
    instruction.span.start = 20;
    instruction.span.end = 28;
    RUDA_EXPECT(ruda_mir_append_instruction(&function, block, &instruction));
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_ARRAY_NEW;
    instruction.target = empty_array_value;
    instruction.array = array;
    instruction.array_construction = empty_construction;
    RUDA_EXPECT(ruda_mir_append_instruction(&function, block, &instruction));
    RUDA_EXPECT(ruda_mir_set_terminator(
        &function,
        block,
        ruda_test_mir_return(projected)
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
        ruda_mir_execution_default_options()
    ));
    interpret_options.execution = &execution;
    {
        RudaMirFunction record_constructor;
        RudaMirRecordId record = RUDA_MIR_NO_RECORD;
        RudaMirRecordConstructionId record_construction =
            RUDA_MIR_NO_RECORD_CONSTRUCTION;
        RudaMirRegister record_value = RUDA_MIR_NO_REGISTER;
        RudaMirBlockId record_block = RUDA_MIR_NO_BLOCK;
        RUDA_EXPECT(ruda_mir_function_init(
            &record_constructor,
            ruda_system_allocator(),
            symbol,
            NULL,
            0,
            array_type,
            ruda_mir_function_default_options()
        ));
        RUDA_EXPECT(ruda_mir_add_register(
            &record_constructor,
            array_type,
            &record_value
        ));
        RUDA_EXPECT(ruda_mir_add_block(&record_constructor, &record_block));
        RUDA_EXPECT(ruda_mir_add_record(
            &record_constructor,
            array_type,
            NULL,
            0,
            &record
        ));
        RUDA_EXPECT(ruda_mir_add_record_construction(
            &record_constructor,
            record,
            NULL,
            0,
            &record_construction
        ));
        memset(&instruction, 0, sizeof(instruction));
        instruction.kind = RUDA_MIR_RECORD_NEW;
        instruction.target = record_value;
        instruction.record = record;
        instruction.record_construction = record_construction;
        RUDA_EXPECT(ruda_mir_append_instruction(
            &record_constructor,
            record_block,
            &instruction
        ));
        RUDA_EXPECT(ruda_mir_set_terminator(
            &record_constructor,
            record_block,
            ruda_test_mir_return(record_value)
        ));
        RUDA_EXPECT(
            ruda_mir_verify(&record_constructor) == RUDA_MIR_VERIFY_OK
        );
        RUDA_EXPECT(ruda_mir_interpret(
            &record_constructor,
            NULL,
            0,
            ruda_system_allocator(),
            interpret_options,
            &collision_handle
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(ruda_mir_execution_owns_reference(
            &execution,
            collision_handle
        ));
        ruda_mir_function_dispose(&record_constructor);
    }
    RUDA_EXPECT(ruda_mir_interpret(
        &function,
        NULL,
        0,
        ruda_system_allocator(),
        interpret_options,
        &result
    ) == RUDA_MIR_INTERPRET_OK);
    RUDA_EXPECT(result.type == RUDA_TYPE_F64 && result.as.f64_bits == four);
    RUDA_EXPECT(ruda_mir_execution_array_at(&execution, 0, &handle));
    RUDA_EXPECT(!ruda_mir_reference_equal(
        handle.as.reference,
        collision_handle.as.reference
    ));
    RUDA_EXPECT(!ruda_mir_execution_array_length(
        &execution,
        collision_handle,
        &inspected_length
    ));
    RUDA_EXPECT(ruda_mir_execution_owns_reference(&execution, handle));
    RUDA_EXPECT(ruda_mir_execution_array_length(
        &execution,
        handle,
        &inspected_length
    ));
    RUDA_EXPECT(inspected_length == 3);
    RUDA_EXPECT(ruda_mir_execution_array_element(
        &execution,
        handle,
        1,
        &element
    ));
    RUDA_EXPECT(element.type == RUDA_TYPE_F64 && element.as.f64_bits == four);
    RUDA_EXPECT(!ruda_mir_execution_array_element(
        &execution,
        handle,
        3,
        &element
    ));
    RUDA_EXPECT(ruda_mir_execution_array_at(&execution, 1, &handle));
    RUDA_EXPECT(ruda_mir_execution_array_length(
        &execution,
        handle,
        &inspected_length
    ));
    RUDA_EXPECT(inspected_length == 0);
    {
        const RudaTypeId outer_type = array_type + 2;
        RudaMirFunction outer_constructor;
        RudaMirArrayId outer_array = RUDA_MIR_NO_ARRAY;
        RudaMirArrayConstructionId outer_construction =
            RUDA_MIR_NO_ARRAY_CONSTRUCTION;
        RudaMirArrayArgument outer_argument;
        RudaMirRegister outer_value = RUDA_MIR_NO_REGISTER;
        RudaMirBlockId outer_block = RUDA_MIR_NO_BLOCK;
        RudaMirValue inner;
        RudaMirValue outer;
        RudaMirValue nested;
        RudaMirValue foreign_inner;
        RudaMirValue foreign_result;
        RudaMirExecution foreign_execution;
        RudaMirFunction managed_call;
        RudaSymbolId managed_callee;
        RudaMirCallId managed_call_id = RUDA_MIR_NO_CALL;
        RudaMirRegister managed_call_result = RUDA_MIR_NO_REGISTER;
        RudaMirBlockId managed_call_block = RUDA_MIR_NO_BLOCK;
        RudaMirManagedCallTestContext managed_call_context;
        RudaTypeId parameter = array_type;
        RUDA_EXPECT(ruda_mir_execution_array_at(&execution, 0, &inner));
        RUDA_EXPECT(ruda_mir_function_init(
            &outer_constructor,
            ruda_system_allocator(),
            symbol,
            &parameter,
            1,
            outer_type,
            ruda_mir_function_default_options()
        ));
        RUDA_EXPECT(ruda_mir_add_register(
            &outer_constructor,
            outer_type,
            &outer_value
        ));
        RUDA_EXPECT(ruda_mir_add_array(
            &outer_constructor,
            outer_type,
            array_type,
            &outer_array
        ));
        outer_argument.register_id = 0;
        outer_argument.type = array_type;
        RUDA_EXPECT(ruda_mir_add_array_construction(
            &outer_constructor,
            outer_array,
            &outer_argument,
            1,
            &outer_construction
        ));
        RUDA_EXPECT(ruda_mir_add_block(&outer_constructor, &outer_block));
        memset(&instruction, 0, sizeof(instruction));
        instruction.kind = RUDA_MIR_ARRAY_NEW;
        instruction.target = outer_value;
        instruction.array = outer_array;
        instruction.array_construction = outer_construction;
        RUDA_EXPECT(ruda_mir_append_instruction(
            &outer_constructor,
            outer_block,
            &instruction
        ));
        RUDA_EXPECT(ruda_mir_set_terminator(
            &outer_constructor,
            outer_block,
            ruda_test_mir_return(outer_value)
        ));
        RUDA_EXPECT(
            ruda_mir_verify(&outer_constructor) == RUDA_MIR_VERIFY_OK
        );
        RUDA_EXPECT(ruda_mir_interpret(
            &outer_constructor,
            &inner,
            1,
            ruda_system_allocator(),
            interpret_options,
            &outer
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(ruda_mir_execution_array_element(
            &execution,
            outer,
            0,
            &nested
        ));
        RUDA_EXPECT(nested.type == inner.type
            && ruda_mir_reference_equal(
                nested.as.reference,
                inner.as.reference
            ));
        RUDA_EXPECT(ruda_mir_execution_init(
            &foreign_execution,
            ruda_system_allocator(),
            ruda_mir_execution_default_options()
        ));
        interpret_options.execution = &foreign_execution;
        RUDA_EXPECT(ruda_mir_interpret(
            &function,
            NULL,
            0,
            ruda_system_allocator(),
            interpret_options,
            &foreign_result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(ruda_mir_execution_array_at(
            &foreign_execution,
            0,
            &foreign_inner
        ));
        RUDA_EXPECT(ruda_mir_execution_owns_reference(
            &foreign_execution,
            foreign_inner
        ));
        RUDA_EXPECT(!ruda_mir_execution_owns_reference(
            &foreign_execution,
            inner
        ));
        RUDA_EXPECT(!ruda_mir_reference_equal(
            foreign_inner.as.reference,
            inner.as.reference
        ));
        RUDA_EXPECT(ruda_symbol_id_from_parts(
            "test",
            "mir.ruda",
            "managedCall",
            0,
            &managed_callee
        ));
        RUDA_EXPECT(ruda_mir_function_init(
            &managed_call,
            ruda_system_allocator(),
            symbol,
            NULL,
            0,
            array_type,
            ruda_mir_function_default_options()
        ));
        RUDA_EXPECT(ruda_mir_add_register(
            &managed_call,
            array_type,
            &managed_call_result
        ));
        RUDA_EXPECT(ruda_mir_add_call(
            &managed_call,
            managed_callee,
            array_type,
            NULL,
            0,
            &managed_call_id
        ));
        RUDA_EXPECT(ruda_mir_add_block(
            &managed_call,
            &managed_call_block
        ));
        memset(&instruction, 0, sizeof(instruction));
        instruction.kind = RUDA_MIR_CALL;
        instruction.target = managed_call_result;
        instruction.call = managed_call_id;
        RUDA_EXPECT(ruda_mir_append_instruction(
            &managed_call,
            managed_call_block,
            &instruction
        ));
        RUDA_EXPECT(ruda_mir_set_terminator(
            &managed_call,
            managed_call_block,
            ruda_test_mir_return(managed_call_result)
        ));
        RUDA_EXPECT(ruda_mir_verify(&managed_call) == RUDA_MIR_VERIFY_OK);
        managed_call_context.expected = managed_callee;
        managed_call_context.result = inner;
        managed_call_context.calls = 0;
        interpret_options.call_context = &managed_call_context;
        interpret_options.call = ruda_test_mir_managed_call_handler;
        RUDA_EXPECT(ruda_mir_interpret(
            &managed_call,
            NULL,
            0,
            ruda_system_allocator(),
            interpret_options,
            &foreign_result
        ) == RUDA_MIR_INTERPRET_INVALID_REFERENCE);
        RUDA_EXPECT(managed_call_context.calls == 1);
        managed_call_context.result = foreign_inner;
        RUDA_EXPECT(ruda_mir_interpret(
            &managed_call,
            NULL,
            0,
            ruda_system_allocator(),
            interpret_options,
            &foreign_result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(managed_call_context.calls == 2);
        RUDA_EXPECT(ruda_mir_reference_equal(
            foreign_result.as.reference,
            foreign_inner.as.reference
        ));
        ruda_mir_function_dispose(&managed_call);
        interpret_options.call_context = NULL;
        interpret_options.call = NULL;
        RUDA_EXPECT(ruda_mir_interpret(
            &outer_constructor,
            &inner,
            1,
            ruda_system_allocator(),
            interpret_options,
            &outer
        ) == RUDA_MIR_INTERPRET_INVALID_REFERENCE);
        ruda_mir_execution_dispose(&foreign_execution);
        interpret_options.execution = &execution;
        ruda_mir_function_dispose(&outer_constructor);
    }
    RUDA_EXPECT(ruda_mir_execution_array_at(&execution, 0, &handle));
    ruda_mir_execution_dispose(&execution);
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
    RUDA_EXPECT(!ruda_mir_execution_owns_reference(&execution, handle));
    ruda_mir_execution_dispose(&execution);

    stored_block = ruda_vector_at(&function.blocks, block);
    stored_set = stored_block != NULL
        ? ruda_vector_at(&stored_block->instructions, 9)
        : NULL;
    stored_array = ruda_vector_at(&function.arrays, array);
    RUDA_EXPECT(stored_set != NULL && stored_array != NULL);
    if (stored_set != NULL) {
        stored_set->third = uninitialized;
        RUDA_EXPECT(
            ruda_mir_verify(&function)
                == RUDA_MIR_VERIFY_UNINITIALIZED_REGISTER
        );
        stored_set->third = replacement;
    }
    if (stored_array != NULL) {
        stored_array->element_type = RUDA_TYPE_INVALID;
        RUDA_EXPECT(
            ruda_mir_verify(&function) == RUDA_MIR_VERIFY_INVALID_ARGUMENT
        );
        stored_array->element_type = RUDA_TYPE_F64;
    }
    {
        RudaMirRecordId collision = RUDA_MIR_NO_RECORD;
        RudaMirRecord *record = NULL;
        RUDA_EXPECT(ruda_mir_add_record(
            &function,
            array_type,
            NULL,
            0,
            &collision
        ));
        RUDA_EXPECT(
            ruda_mir_verify(&function) == RUDA_MIR_VERIFY_INVALID_ARGUMENT
        );
        record = ruda_vector_at(&function.records, collision);
        RUDA_EXPECT(record != NULL);
        if (record != NULL) {
            record->type = array_type + 1;
        }
        RUDA_EXPECT(ruda_mir_verify(&function) == RUDA_MIR_VERIFY_OK);
    }
    {
        RudaMirExecutionOptions options =
            ruda_mir_execution_default_options();
        options.max_array_length = 2;
        RUDA_EXPECT(ruda_mir_execution_init(
            &execution,
            ruda_system_allocator(),
            options
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
        RUDA_EXPECT(execution.arrays.length == 0);
        RUDA_EXPECT(execution.total_array_elements == 0);
        ruda_mir_execution_dispose(&execution);

        options = ruda_mir_execution_default_options();
        options.max_total_array_elements = 2;
        RUDA_EXPECT(ruda_mir_execution_init(
            &execution,
            ruda_system_allocator(),
            options
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
        RUDA_EXPECT(execution.arrays.length == 0);
        RUDA_EXPECT(execution.total_array_elements == 0);
        ruda_mir_execution_dispose(&execution);

        options = ruda_mir_execution_default_options();
        options.max_arrays = 2;
        RUDA_EXPECT(ruda_mir_execution_init(
            &execution,
            ruda_system_allocator(),
            options
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
        RUDA_EXPECT(ruda_mir_interpret(
            &function,
            NULL,
            0,
            ruda_system_allocator(),
            interpret_options,
            &result
        ) == RUDA_MIR_INTERPRET_OBJECT_LIMIT);
        RUDA_EXPECT(execution.arrays.length == 2);
        RUDA_EXPECT(execution.total_array_elements == 3);
        ruda_mir_execution_dispose(&execution);

        options.max_arrays = 0;
        RUDA_EXPECT(!ruda_mir_execution_init(
            &execution,
            ruda_system_allocator(),
            options
        ));
    }
    {
        bool reached_success = false;
        size_t failure_offset = 0;
        for (failure_offset = 0; failure_offset < 16; failure_offset += 1) {
            RudaFailAllocator state;
            RudaAllocator allocator;
            RudaMirExecution failed_execution;
            RudaMirInterpretStatus status;
            ruda_fail_allocator_init(
                &state,
                ruda_system_allocator(),
                SIZE_MAX
            );
            allocator = ruda_fail_allocator(&state);
            RUDA_EXPECT(ruda_mir_execution_init(
                &failed_execution,
                allocator,
                ruda_mir_execution_default_options()
            ));
            state.failure_index = state.attempts + failure_offset;
            interpret_options.execution = &failed_execution;
            status = ruda_mir_interpret(
                &function,
                NULL,
                0,
                allocator,
                interpret_options,
                &result
            );
            if (status == RUDA_MIR_INTERPRET_OK) {
                reached_success = true;
            } else {
                RUDA_EXPECT(status == RUDA_MIR_INTERPRET_ALLOCATION_FAILED);
            }
            ruda_mir_execution_dispose(&failed_execution);
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
        RudaMirPanic panic;
        RudaMirArrayArgument argument;
        RudaMirRegister value = RUDA_MIR_NO_REGISTER;
        RudaMirRegister array_register = RUDA_MIR_NO_REGISTER;
        RudaMirRegister bounds_index = RUDA_MIR_NO_REGISTER;
        RudaMirRegister output = RUDA_MIR_NO_REGISTER;
        RudaMirInstruction *stored_index = NULL;
        RudaMirInstruction *stored_access = NULL;
        memset(&panic, 0, sizeof(panic));
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
        instruction.integer = -1;
        RUDA_EXPECT(ruda_mir_append_instruction(&function, block, &instruction));
        memset(&instruction, 0, sizeof(instruction));
        instruction.kind = RUDA_MIR_ARRAY_GET;
        instruction.target = output;
        instruction.first = array_register;
        instruction.second = bounds_index;
        instruction.array = array;
        instruction.span.start = 7;
        instruction.span.end = 16;
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
        interpret_options.panic = &panic;
        RUDA_EXPECT(ruda_mir_interpret(
            &function,
            NULL,
            0,
            ruda_system_allocator(),
            interpret_options,
            &result
        ) == RUDA_MIR_INTERPRET_PANIC);
        RUDA_EXPECT(panic.code != NULL && strcmp(panic.code, "G4002") == 0);
        RUDA_EXPECT(panic.message.length == sizeof(negative_message) - 1);
        RUDA_EXPECT(panic.message.data != NULL && memcmp(
            panic.message.data,
            negative_message,
            sizeof(negative_message) - 1
        ) == 0);
        RUDA_EXPECT(ruda_symbol_id_equal(panic.symbol, symbol));
        RUDA_EXPECT(panic.span.start == 7 && panic.span.end == 16);
        {
            RudaFailAllocator counting_state;
            RudaFailAllocator failing_state;
            RudaAllocator counting_allocator;
            RudaAllocator failing_allocator;
            RudaMirPanic counted_panic;
            RudaMirPanic failed_panic;
            size_t message_allocation = 0;
            memset(&counted_panic, 0, sizeof(counted_panic));
            memset(&failed_panic, 0, sizeof(failed_panic));
            ruda_fail_allocator_init(
                &counting_state,
                ruda_system_allocator(),
                SIZE_MAX
            );
            counting_allocator = ruda_fail_allocator(&counting_state);
            interpret_options.panic = &counted_panic;
            RUDA_EXPECT(ruda_mir_interpret(
                &function,
                NULL,
                0,
                counting_allocator,
                interpret_options,
                &result
            ) == RUDA_MIR_INTERPRET_PANIC);
            RUDA_EXPECT(counting_state.attempts != 0);
            message_allocation = counting_state.attempts - 1;
            ruda_mir_panic_dispose(&counted_panic);
            RUDA_EXPECT(counting_state.live_allocations == 0);
            RUDA_EXPECT(counting_state.live_bytes == 0);
            ruda_fail_allocator_init(
                &failing_state,
                ruda_system_allocator(),
                message_allocation
            );
            failing_allocator = ruda_fail_allocator(&failing_state);
            interpret_options.panic = &failed_panic;
            RUDA_EXPECT(ruda_mir_interpret(
                &function,
                NULL,
                0,
                failing_allocator,
                interpret_options,
                &result
            ) == RUDA_MIR_INTERPRET_ALLOCATION_FAILED);
            RUDA_EXPECT(failed_panic.message.data == NULL);
            ruda_mir_panic_dispose(&failed_panic);
            RUDA_EXPECT(failing_state.live_allocations == 0);
            RUDA_EXPECT(failing_state.live_bytes == 0);
            interpret_options.panic = &panic;
        }
        stored_block = ruda_vector_at(&function.blocks, block);
        stored_index = stored_block != NULL
            ? ruda_vector_at(&stored_block->instructions, 2)
            : NULL;
        stored_access = stored_block != NULL
            ? ruda_vector_at(&stored_block->instructions, 3)
            : NULL;
        RUDA_EXPECT(stored_index != NULL && stored_access != NULL);
        if (stored_index != NULL) {
            stored_index->integer = 1;
        }
        RUDA_EXPECT(ruda_mir_interpret(
            &function,
            NULL,
            0,
            ruda_system_allocator(),
            interpret_options,
            &result
        ) == RUDA_MIR_INTERPRET_PANIC);
        RUDA_EXPECT(panic.message.length == sizeof(upper_message) - 1);
        RUDA_EXPECT(panic.message.data != NULL && memcmp(
            panic.message.data,
            upper_message,
            sizeof(upper_message) - 1
        ) == 0);
        if (stored_index != NULL) {
            stored_index->integer = INT64_MIN;
        }
        RUDA_EXPECT(ruda_mir_interpret(
            &function,
            NULL,
            0,
            ruda_system_allocator(),
            interpret_options,
            &result
        ) == RUDA_MIR_INTERPRET_PANIC);
        RUDA_EXPECT(panic.message.length == sizeof(minimum_message) - 1);
        RUDA_EXPECT(panic.message.data != NULL && memcmp(
            panic.message.data,
            minimum_message,
            sizeof(minimum_message) - 1
        ) == 0);
        if (stored_index != NULL) {
            stored_index->integer = INT64_MAX;
        }
        RUDA_EXPECT(ruda_mir_interpret(
            &function,
            NULL,
            0,
            ruda_system_allocator(),
            interpret_options,
            &result
        ) == RUDA_MIR_INTERPRET_PANIC);
        RUDA_EXPECT(panic.message.length == sizeof(maximum_message) - 1);
        RUDA_EXPECT(panic.message.data != NULL && memcmp(
            panic.message.data,
            maximum_message,
            sizeof(maximum_message) - 1
        ) == 0);
        if (stored_index != NULL) {
            stored_index->integer = 1;
        }
        if (stored_access != NULL) {
            stored_access->kind = RUDA_MIR_ARRAY_SET;
            stored_access->third = value;
        }
        RUDA_EXPECT(ruda_mir_verify(&function) == RUDA_MIR_VERIFY_OK);
        interpret_options.panic = NULL;
        RUDA_EXPECT(ruda_mir_interpret(
            &function,
            NULL,
            0,
            ruda_system_allocator(),
            interpret_options,
            &result
        ) == RUDA_MIR_INTERPRET_PANIC);
        RUDA_EXPECT(ruda_mir_execution_array_at(
            &execution,
            execution.arrays.length - 1,
            &handle
        ));
        RUDA_EXPECT(ruda_mir_execution_array_element(
            &execution,
            handle,
            0,
            &element
        ));
        RUDA_EXPECT(element.as.f64_bits == one);
        ruda_mir_panic_dispose(&panic);
        ruda_mir_execution_dispose(&execution);
        ruda_mir_function_dispose(&function);
    }

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
