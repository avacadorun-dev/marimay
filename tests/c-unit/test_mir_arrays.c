#include "test_mir_arrays_support.h"

#include <stdint.h>
#include <string.h>

#include "mmay/base/allocator.h"
#include "mmay/mir/mir.h"


typedef struct MarimayMirManagedCallTestContext {
    MarimaySymbolId expected;
    MarimayMirValue result;
    size_t calls;
} MarimayMirManagedCallTestContext;

static MarimayMirInterpretStatus mmay_test_mir_managed_call_handler(
    void *context,
    MarimaySymbolId callee,
    const MarimayMirValue *arguments,
    size_t argument_count,
    MarimayMirValue *result
) {
    MarimayMirManagedCallTestContext *state = context;
    (void)arguments;
    if (state == NULL || result == NULL
        || !mmay_symbol_id_equal(callee, state->expected)
        || argument_count != 0) {
        return MMAY_MIR_INTERPRET_INVALID_ARGUMENT;
    }
    state->calls += 1;
    *result = state->result;
    return MMAY_MIR_INTERPRET_OK;
}

void mmay_test_mir_arrays(MarimaySymbolId symbol) {
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
    const MarimayTypeId array_type = MMAY_TYPE_FIRST_DYNAMIC + 30;
    MarimayMirFunction function;
    MarimayMirArrayId array = MMAY_MIR_NO_ARRAY;
    MarimayMirArrayConstructionId construction =
        MMAY_MIR_NO_ARRAY_CONSTRUCTION;
    MarimayMirArrayConstructionId empty_construction =
        MMAY_MIR_NO_ARRAY_CONSTRUCTION;
    MarimayMirArrayArgument arguments[3];
    MarimayMirRegister values[3];
    MarimayMirRegister array_value = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister empty_array_value = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister alias = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister length = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister offset = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister index = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister checked_destination = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister replacement = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister set_result = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister projected = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister uninitialized = MMAY_MIR_NO_REGISTER;
    MarimayMirBlockId block = MMAY_MIR_NO_BLOCK;
    MarimayMirInstruction instruction;
    MarimayMirExecution execution;
    MarimayMirInterpretOptions interpret_options =
        mmay_mir_interpret_default_options();
    MarimayMirValue result;
    MarimayMirValue handle;
    MarimayMirValue collision_handle;
    MarimayMirValue element;
    MarimayMirBlock *stored_block = NULL;
    MarimayMirInstruction *stored_set = NULL;
    MarimayMirArray *stored_array = NULL;
    size_t inspected_length = 0;
    size_t value_index = 0;

    for (value_index = 0; value_index < 3; value_index += 1) {
        values[value_index] = MMAY_MIR_NO_REGISTER;
    }
    MMAY_EXPECT(mmay_mir_function_init(
        &function,
        mmay_system_allocator(),
        symbol,
        NULL,
        0,
        MMAY_TYPE_F64,
        mmay_mir_function_default_options()
    ));
    MMAY_EXPECT(!mmay_mir_add_array(
        &function,
        MMAY_TYPE_I64,
        MMAY_TYPE_F64,
        &array
    ));
    MMAY_EXPECT(array == MMAY_MIR_NO_ARRAY);
    MMAY_EXPECT(mmay_mir_add_array(
        &function,
        array_type,
        MMAY_TYPE_F64,
        &array
    ));
    {
        MarimayMirArrayId duplicate = MMAY_MIR_NO_ARRAY;
        MMAY_EXPECT(mmay_mir_add_array(
            &function,
            array_type,
            MMAY_TYPE_F64,
            &duplicate
        ));
        MMAY_EXPECT(duplicate == array);
        MMAY_EXPECT(!mmay_mir_add_array(
            &function,
            array_type,
            MMAY_TYPE_I64,
            &duplicate
        ));
    }
    for (value_index = 0; value_index < 3; value_index += 1) {
        MMAY_EXPECT(mmay_mir_add_register(
            &function,
            MMAY_TYPE_F64,
            &values[value_index]
        ));
        arguments[value_index].register_id = values[value_index];
        arguments[value_index].type = MMAY_TYPE_F64;
    }
    MMAY_EXPECT(mmay_mir_add_register(
        &function,
        array_type,
        &array_value
    ));
    MMAY_EXPECT(mmay_mir_add_register(
        &function,
        array_type,
        &empty_array_value
    ));
    MMAY_EXPECT(mmay_mir_add_register(&function, array_type, &alias));
    MMAY_EXPECT(mmay_mir_add_register(&function, MMAY_TYPE_I64, &length));
    MMAY_EXPECT(mmay_mir_add_register(&function, MMAY_TYPE_I64, &offset));
    MMAY_EXPECT(mmay_mir_add_register(&function, MMAY_TYPE_I64, &index));
    MMAY_EXPECT(mmay_mir_add_register(
        &function,
        array_type,
        &checked_destination
    ));
    MMAY_EXPECT(mmay_mir_add_register(
        &function,
        MMAY_TYPE_F64,
        &replacement
    ));
    MMAY_EXPECT(mmay_mir_add_register(
        &function,
        MMAY_TYPE_F64,
        &set_result
    ));
    MMAY_EXPECT(mmay_mir_add_register(
        &function,
        MMAY_TYPE_F64,
        &projected
    ));
    MMAY_EXPECT(mmay_mir_add_register(
        &function,
        MMAY_TYPE_F64,
        &uninitialized
    ));
    MMAY_EXPECT(mmay_mir_add_array_construction(
        &function,
        array,
        arguments,
        3,
        &construction
    ));
    MMAY_EXPECT(mmay_mir_add_array_construction(
        &function,
        array,
        NULL,
        0,
        &empty_construction
    ));
    MMAY_EXPECT(mmay_mir_add_block(&function, &block));
    for (value_index = 0; value_index < 3; value_index += 1) {
        memset(&instruction, 0, sizeof(instruction));
        instruction.kind = MMAY_MIR_CONSTANT_F64;
        instruction.target = values[value_index];
        instruction.f64_bits = value_index == 0 ? one
            : value_index == 1 ? two : three;
        MMAY_EXPECT(mmay_mir_append_instruction(
            &function,
            block,
            &instruction
        ));
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MMAY_MIR_ARRAY_NEW;
    instruction.target = array_value;
    instruction.array = array;
    instruction.array_construction = construction;
    MMAY_EXPECT(mmay_mir_append_instruction(&function, block, &instruction));
    instruction = mmay_test_mir_binary(
        MMAY_MIR_COPY,
        alias,
        array_value,
        MMAY_MIR_NO_REGISTER
    );
    MMAY_EXPECT(mmay_mir_append_instruction(&function, block, &instruction));
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MMAY_MIR_ARRAY_LENGTH;
    instruction.target = length;
    instruction.first = alias;
    instruction.array = array;
    MMAY_EXPECT(mmay_mir_append_instruction(&function, block, &instruction));
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MMAY_MIR_CONSTANT_I64;
    instruction.target = offset;
    instruction.integer = 2;
    MMAY_EXPECT(mmay_mir_append_instruction(&function, block, &instruction));
    instruction = mmay_test_mir_binary(
        MMAY_MIR_SUBTRACT_CHECKED_I64,
        index,
        length,
        offset
    );
    MMAY_EXPECT(mmay_mir_append_instruction(&function, block, &instruction));
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MMAY_MIR_ARRAY_REQUIRE_INDEX;
    instruction.target = checked_destination;
    instruction.first = array_value;
    instruction.second = index;
    instruction.array = array;
    instruction.span.start = 10;
    instruction.span.end = 18;
    MMAY_EXPECT(mmay_mir_append_instruction(&function, block, &instruction));
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MMAY_MIR_CONSTANT_F64;
    instruction.target = replacement;
    instruction.f64_bits = four;
    MMAY_EXPECT(mmay_mir_append_instruction(&function, block, &instruction));
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MMAY_MIR_ARRAY_SET;
    instruction.target = set_result;
    instruction.first = checked_destination;
    instruction.second = index;
    instruction.third = replacement;
    instruction.array = array;
    instruction.span.start = 10;
    instruction.span.end = 18;
    MMAY_EXPECT(mmay_mir_append_instruction(&function, block, &instruction));
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MMAY_MIR_ARRAY_GET;
    instruction.target = projected;
    instruction.first = alias;
    instruction.second = index;
    instruction.array = array;
    instruction.span.start = 20;
    instruction.span.end = 28;
    MMAY_EXPECT(mmay_mir_append_instruction(&function, block, &instruction));
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MMAY_MIR_ARRAY_NEW;
    instruction.target = empty_array_value;
    instruction.array = array;
    instruction.array_construction = empty_construction;
    MMAY_EXPECT(mmay_mir_append_instruction(&function, block, &instruction));
    MMAY_EXPECT(mmay_mir_set_terminator(
        &function,
        block,
        mmay_test_mir_return(projected)
    ));
    MMAY_EXPECT(mmay_mir_verify(&function) == MMAY_MIR_VERIFY_OK);
    MMAY_EXPECT(mmay_mir_interpret(
        &function,
        NULL,
        0,
        mmay_system_allocator(),
        interpret_options,
        &result
    ) == MMAY_MIR_INTERPRET_INVALID_ARGUMENT);
    MMAY_EXPECT(mmay_mir_execution_init(
        &execution,
        mmay_system_allocator(),
        mmay_mir_execution_default_options()
    ));
    interpret_options.execution = &execution;
    {
        MarimayMirFunction record_constructor;
        MarimayMirRecordId record = MMAY_MIR_NO_RECORD;
        MarimayMirRecordConstructionId record_construction =
            MMAY_MIR_NO_RECORD_CONSTRUCTION;
        MarimayMirRegister record_value = MMAY_MIR_NO_REGISTER;
        MarimayMirBlockId record_block = MMAY_MIR_NO_BLOCK;
        MMAY_EXPECT(mmay_mir_function_init(
            &record_constructor,
            mmay_system_allocator(),
            symbol,
            NULL,
            0,
            array_type,
            mmay_mir_function_default_options()
        ));
        MMAY_EXPECT(mmay_mir_add_register(
            &record_constructor,
            array_type,
            &record_value
        ));
        MMAY_EXPECT(mmay_mir_add_block(&record_constructor, &record_block));
        MMAY_EXPECT(mmay_mir_add_record(
            &record_constructor,
            array_type,
            NULL,
            0,
            &record
        ));
        MMAY_EXPECT(mmay_mir_add_record_construction(
            &record_constructor,
            record,
            NULL,
            0,
            &record_construction
        ));
        memset(&instruction, 0, sizeof(instruction));
        instruction.kind = MMAY_MIR_RECORD_NEW;
        instruction.target = record_value;
        instruction.record = record;
        instruction.record_construction = record_construction;
        MMAY_EXPECT(mmay_mir_append_instruction(
            &record_constructor,
            record_block,
            &instruction
        ));
        MMAY_EXPECT(mmay_mir_set_terminator(
            &record_constructor,
            record_block,
            mmay_test_mir_return(record_value)
        ));
        MMAY_EXPECT(
            mmay_mir_verify(&record_constructor) == MMAY_MIR_VERIFY_OK
        );
        MMAY_EXPECT(mmay_mir_interpret(
            &record_constructor,
            NULL,
            0,
            mmay_system_allocator(),
            interpret_options,
            &collision_handle
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(mmay_mir_execution_owns_reference(
            &execution,
            collision_handle
        ));
        mmay_mir_function_dispose(&record_constructor);
    }
    MMAY_EXPECT(mmay_mir_interpret(
        &function,
        NULL,
        0,
        mmay_system_allocator(),
        interpret_options,
        &result
    ) == MMAY_MIR_INTERPRET_OK);
    MMAY_EXPECT(result.type == MMAY_TYPE_F64 && result.as.f64_bits == four);
    MMAY_EXPECT(mmay_mir_execution_array_at(&execution, 0, &handle));
    MMAY_EXPECT(!mmay_mir_reference_equal(
        handle.as.reference,
        collision_handle.as.reference
    ));
    MMAY_EXPECT(!mmay_mir_execution_array_length(
        &execution,
        collision_handle,
        &inspected_length
    ));
    MMAY_EXPECT(mmay_mir_execution_owns_reference(&execution, handle));
    MMAY_EXPECT(mmay_mir_execution_array_length(
        &execution,
        handle,
        &inspected_length
    ));
    MMAY_EXPECT(inspected_length == 3);
    MMAY_EXPECT(mmay_mir_execution_array_element(
        &execution,
        handle,
        1,
        &element
    ));
    MMAY_EXPECT(element.type == MMAY_TYPE_F64 && element.as.f64_bits == four);
    MMAY_EXPECT(!mmay_mir_execution_array_element(
        &execution,
        handle,
        3,
        &element
    ));
    MMAY_EXPECT(mmay_mir_execution_array_at(&execution, 1, &handle));
    MMAY_EXPECT(mmay_mir_execution_array_length(
        &execution,
        handle,
        &inspected_length
    ));
    MMAY_EXPECT(inspected_length == 0);
    {
        const MarimayTypeId outer_type = array_type + 2;
        MarimayMirFunction outer_constructor;
        MarimayMirArrayId outer_array = MMAY_MIR_NO_ARRAY;
        MarimayMirArrayConstructionId outer_construction =
            MMAY_MIR_NO_ARRAY_CONSTRUCTION;
        MarimayMirArrayArgument outer_argument;
        MarimayMirRegister outer_value = MMAY_MIR_NO_REGISTER;
        MarimayMirBlockId outer_block = MMAY_MIR_NO_BLOCK;
        MarimayMirValue inner;
        MarimayMirValue outer;
        MarimayMirValue nested;
        MarimayMirValue foreign_inner;
        MarimayMirValue foreign_result;
        MarimayMirExecution foreign_execution;
        MarimayMirFunction managed_call;
        MarimaySymbolId managed_callee;
        MarimayMirCallId managed_call_id = MMAY_MIR_NO_CALL;
        MarimayMirRegister managed_call_result = MMAY_MIR_NO_REGISTER;
        MarimayMirBlockId managed_call_block = MMAY_MIR_NO_BLOCK;
        MarimayMirManagedCallTestContext managed_call_context;
        MarimayTypeId parameter = array_type;
        MMAY_EXPECT(mmay_mir_execution_array_at(&execution, 0, &inner));
        MMAY_EXPECT(mmay_mir_function_init(
            &outer_constructor,
            mmay_system_allocator(),
            symbol,
            &parameter,
            1,
            outer_type,
            mmay_mir_function_default_options()
        ));
        MMAY_EXPECT(mmay_mir_add_register(
            &outer_constructor,
            outer_type,
            &outer_value
        ));
        MMAY_EXPECT(mmay_mir_add_array(
            &outer_constructor,
            outer_type,
            array_type,
            &outer_array
        ));
        outer_argument.register_id = 0;
        outer_argument.type = array_type;
        MMAY_EXPECT(mmay_mir_add_array_construction(
            &outer_constructor,
            outer_array,
            &outer_argument,
            1,
            &outer_construction
        ));
        MMAY_EXPECT(mmay_mir_add_block(&outer_constructor, &outer_block));
        memset(&instruction, 0, sizeof(instruction));
        instruction.kind = MMAY_MIR_ARRAY_NEW;
        instruction.target = outer_value;
        instruction.array = outer_array;
        instruction.array_construction = outer_construction;
        MMAY_EXPECT(mmay_mir_append_instruction(
            &outer_constructor,
            outer_block,
            &instruction
        ));
        MMAY_EXPECT(mmay_mir_set_terminator(
            &outer_constructor,
            outer_block,
            mmay_test_mir_return(outer_value)
        ));
        MMAY_EXPECT(
            mmay_mir_verify(&outer_constructor) == MMAY_MIR_VERIFY_OK
        );
        MMAY_EXPECT(mmay_mir_interpret(
            &outer_constructor,
            &inner,
            1,
            mmay_system_allocator(),
            interpret_options,
            &outer
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(mmay_mir_execution_array_element(
            &execution,
            outer,
            0,
            &nested
        ));
        MMAY_EXPECT(nested.type == inner.type
            && mmay_mir_reference_equal(
                nested.as.reference,
                inner.as.reference
            ));
        MMAY_EXPECT(mmay_mir_execution_init(
            &foreign_execution,
            mmay_system_allocator(),
            mmay_mir_execution_default_options()
        ));
        interpret_options.execution = &foreign_execution;
        MMAY_EXPECT(mmay_mir_interpret(
            &function,
            NULL,
            0,
            mmay_system_allocator(),
            interpret_options,
            &foreign_result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(mmay_mir_execution_array_at(
            &foreign_execution,
            0,
            &foreign_inner
        ));
        MMAY_EXPECT(mmay_mir_execution_owns_reference(
            &foreign_execution,
            foreign_inner
        ));
        MMAY_EXPECT(!mmay_mir_execution_owns_reference(
            &foreign_execution,
            inner
        ));
        MMAY_EXPECT(!mmay_mir_reference_equal(
            foreign_inner.as.reference,
            inner.as.reference
        ));
        MMAY_EXPECT(mmay_symbol_id_from_parts(
            "test",
            "mir.my",
            "managedCall",
            0,
            &managed_callee
        ));
        MMAY_EXPECT(mmay_mir_function_init(
            &managed_call,
            mmay_system_allocator(),
            symbol,
            NULL,
            0,
            array_type,
            mmay_mir_function_default_options()
        ));
        MMAY_EXPECT(mmay_mir_add_register(
            &managed_call,
            array_type,
            &managed_call_result
        ));
        MMAY_EXPECT(mmay_mir_add_call(
            &managed_call,
            managed_callee,
            array_type,
            NULL,
            0,
            &managed_call_id
        ));
        MMAY_EXPECT(mmay_mir_add_block(
            &managed_call,
            &managed_call_block
        ));
        memset(&instruction, 0, sizeof(instruction));
        instruction.kind = MMAY_MIR_CALL;
        instruction.target = managed_call_result;
        instruction.call = managed_call_id;
        MMAY_EXPECT(mmay_mir_append_instruction(
            &managed_call,
            managed_call_block,
            &instruction
        ));
        MMAY_EXPECT(mmay_mir_set_terminator(
            &managed_call,
            managed_call_block,
            mmay_test_mir_return(managed_call_result)
        ));
        MMAY_EXPECT(mmay_mir_verify(&managed_call) == MMAY_MIR_VERIFY_OK);
        managed_call_context.expected = managed_callee;
        managed_call_context.result = inner;
        managed_call_context.calls = 0;
        interpret_options.call_context = &managed_call_context;
        interpret_options.call = mmay_test_mir_managed_call_handler;
        MMAY_EXPECT(mmay_mir_interpret(
            &managed_call,
            NULL,
            0,
            mmay_system_allocator(),
            interpret_options,
            &foreign_result
        ) == MMAY_MIR_INTERPRET_INVALID_REFERENCE);
        MMAY_EXPECT(managed_call_context.calls == 1);
        managed_call_context.result = foreign_inner;
        MMAY_EXPECT(mmay_mir_interpret(
            &managed_call,
            NULL,
            0,
            mmay_system_allocator(),
            interpret_options,
            &foreign_result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(managed_call_context.calls == 2);
        MMAY_EXPECT(mmay_mir_reference_equal(
            foreign_result.as.reference,
            foreign_inner.as.reference
        ));
        mmay_mir_function_dispose(&managed_call);
        interpret_options.call_context = NULL;
        interpret_options.call = NULL;
        MMAY_EXPECT(mmay_mir_interpret(
            &outer_constructor,
            &inner,
            1,
            mmay_system_allocator(),
            interpret_options,
            &outer
        ) == MMAY_MIR_INTERPRET_INVALID_REFERENCE);
        mmay_mir_execution_dispose(&foreign_execution);
        interpret_options.execution = &execution;
        mmay_mir_function_dispose(&outer_constructor);
    }
    MMAY_EXPECT(mmay_mir_execution_array_at(&execution, 0, &handle));
    mmay_mir_execution_dispose(&execution);
    MMAY_EXPECT(mmay_mir_execution_init(
        &execution,
        mmay_system_allocator(),
        mmay_mir_execution_default_options()
    ));
    interpret_options.execution = &execution;
    MMAY_EXPECT(mmay_mir_interpret(
        &function,
        NULL,
        0,
        mmay_system_allocator(),
        interpret_options,
        &result
    ) == MMAY_MIR_INTERPRET_OK);
    MMAY_EXPECT(!mmay_mir_execution_owns_reference(&execution, handle));
    mmay_mir_execution_dispose(&execution);

    stored_block = mmay_vector_at(&function.blocks, block);
    stored_set = stored_block != NULL
        ? mmay_vector_at(&stored_block->instructions, 10)
        : NULL;
    stored_array = mmay_vector_at(&function.arrays, array);
    MMAY_EXPECT(stored_set != NULL && stored_array != NULL);
    if (stored_set != NULL) {
        MarimayMirInstruction *rhs = stored_block != NULL
            ? mmay_vector_at(&stored_block->instructions, 9)
            : NULL;
        MarimayMirInstruction saved_rhs;
        memset(&saved_rhs, 0, sizeof(saved_rhs));
        if (rhs != NULL) {
            saved_rhs = *rhs;
        }
        stored_set->third = uninitialized;
        MMAY_EXPECT(
            mmay_mir_verify(&function)
                == MMAY_MIR_VERIFY_UNINITIALIZED_REGISTER
        );
        stored_set->third = replacement;
        stored_set->first = array_value;
        MMAY_EXPECT(mmay_mir_verify(&function)
            == MMAY_MIR_VERIFY_INVALID_ARGUMENT);
        stored_set->first = checked_destination;
        stored_set->second = offset;
        MMAY_EXPECT(mmay_mir_verify(&function)
            == MMAY_MIR_VERIFY_INVALID_ARGUMENT);
        stored_set->second = index;
        MMAY_EXPECT(rhs != NULL);
        if (rhs != NULL) {
            memset(rhs, 0, sizeof(*rhs));
            rhs->kind = MMAY_MIR_COPY;
            rhs->target = checked_destination;
            rhs->first = array_value;
            stored_set->third = values[0];
            MMAY_EXPECT(mmay_mir_verify(&function)
                == MMAY_MIR_VERIFY_INVALID_ARGUMENT);
            memset(rhs, 0, sizeof(*rhs));
            rhs->kind = MMAY_MIR_CONSTANT_I64;
            rhs->target = index;
            rhs->integer = 0;
            MMAY_EXPECT(mmay_mir_verify(&function)
                == MMAY_MIR_VERIFY_INVALID_ARGUMENT);
            *rhs = saved_rhs;
            stored_set->third = replacement;
        }
    }
    if (stored_array != NULL) {
        stored_array->element_type = MMAY_TYPE_INVALID;
        MMAY_EXPECT(
            mmay_mir_verify(&function) == MMAY_MIR_VERIFY_INVALID_ARGUMENT
        );
        stored_array->element_type = MMAY_TYPE_F64;
    }
    {
        MarimayMirRecordId collision = MMAY_MIR_NO_RECORD;
        MarimayMirRecord *record = NULL;
        MMAY_EXPECT(mmay_mir_add_record(
            &function,
            array_type,
            NULL,
            0,
            &collision
        ));
        MMAY_EXPECT(
            mmay_mir_verify(&function) == MMAY_MIR_VERIFY_INVALID_ARGUMENT
        );
        record = mmay_vector_at(&function.records, collision);
        MMAY_EXPECT(record != NULL);
        if (record != NULL) {
            record->type = array_type + 1;
        }
        MMAY_EXPECT(mmay_mir_verify(&function) == MMAY_MIR_VERIFY_OK);
    }
    {
        MarimayMirExecutionOptions options =
            mmay_mir_execution_default_options();
        options.max_array_length = 2;
        MMAY_EXPECT(mmay_mir_execution_init(
            &execution,
            mmay_system_allocator(),
            options
        ));
        interpret_options.execution = &execution;
        MMAY_EXPECT(mmay_mir_interpret(
            &function,
            NULL,
            0,
            mmay_system_allocator(),
            interpret_options,
            &result
        ) == MMAY_MIR_INTERPRET_OBJECT_LIMIT);
        MMAY_EXPECT(execution.arrays.length == 0);
        MMAY_EXPECT(execution.total_array_elements == 0);
        mmay_mir_execution_dispose(&execution);

        options = mmay_mir_execution_default_options();
        options.max_total_array_elements = 2;
        MMAY_EXPECT(mmay_mir_execution_init(
            &execution,
            mmay_system_allocator(),
            options
        ));
        interpret_options.execution = &execution;
        MMAY_EXPECT(mmay_mir_interpret(
            &function,
            NULL,
            0,
            mmay_system_allocator(),
            interpret_options,
            &result
        ) == MMAY_MIR_INTERPRET_OBJECT_LIMIT);
        MMAY_EXPECT(execution.arrays.length == 0);
        MMAY_EXPECT(execution.total_array_elements == 0);
        mmay_mir_execution_dispose(&execution);

        options = mmay_mir_execution_default_options();
        options.max_arrays = 2;
        MMAY_EXPECT(mmay_mir_execution_init(
            &execution,
            mmay_system_allocator(),
            options
        ));
        interpret_options.execution = &execution;
        MMAY_EXPECT(mmay_mir_interpret(
            &function,
            NULL,
            0,
            mmay_system_allocator(),
            interpret_options,
            &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(mmay_mir_interpret(
            &function,
            NULL,
            0,
            mmay_system_allocator(),
            interpret_options,
            &result
        ) == MMAY_MIR_INTERPRET_OBJECT_LIMIT);
        MMAY_EXPECT(execution.arrays.length == 2);
        MMAY_EXPECT(execution.total_array_elements == 3);
        mmay_mir_execution_dispose(&execution);

        options.max_arrays = 0;
        MMAY_EXPECT(!mmay_mir_execution_init(
            &execution,
            mmay_system_allocator(),
            options
        ));
    }
    {
        bool reached_success = false;
        size_t failure_offset = 0;
        for (failure_offset = 0; failure_offset < 16; failure_offset += 1) {
            MarimayFailAllocator state;
            MarimayAllocator allocator;
            MarimayMirExecution failed_execution;
            MarimayMirInterpretStatus status;
            mmay_fail_allocator_init(
                &state,
                mmay_system_allocator(),
                SIZE_MAX
            );
            allocator = mmay_fail_allocator(&state);
            MMAY_EXPECT(mmay_mir_execution_init(
                &failed_execution,
                allocator,
                mmay_mir_execution_default_options()
            ));
            state.failure_index = state.attempts + failure_offset;
            interpret_options.execution = &failed_execution;
            status = mmay_mir_interpret(
                &function,
                NULL,
                0,
                allocator,
                interpret_options,
                &result
            );
            if (status == MMAY_MIR_INTERPRET_OK) {
                reached_success = true;
            } else {
                MMAY_EXPECT(status == MMAY_MIR_INTERPRET_ALLOCATION_FAILED);
            }
            mmay_mir_execution_dispose(&failed_execution);
            MMAY_EXPECT(state.live_allocations == 0);
            MMAY_EXPECT(state.live_bytes == 0);
            if (reached_success) {
                break;
            }
        }
        MMAY_EXPECT(reached_success);
    }
    mmay_mir_function_dispose(&function);

    {
        MarimayMirPanic panic;
        MarimayMirArrayArgument argument;
        MarimayMirRegister value = MMAY_MIR_NO_REGISTER;
        MarimayMirRegister array_register = MMAY_MIR_NO_REGISTER;
        MarimayMirRegister bounds_index = MMAY_MIR_NO_REGISTER;
        MarimayMirRegister output = MMAY_MIR_NO_REGISTER;
        MarimayMirInstruction *stored_index = NULL;
        MarimayMirInstruction *stored_access = NULL;
        memset(&panic, 0, sizeof(panic));
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
        instruction.integer = -1;
        MMAY_EXPECT(mmay_mir_append_instruction(&function, block, &instruction));
        memset(&instruction, 0, sizeof(instruction));
        instruction.kind = MMAY_MIR_ARRAY_GET;
        instruction.target = output;
        instruction.first = array_register;
        instruction.second = bounds_index;
        instruction.array = array;
        instruction.span.start = 7;
        instruction.span.end = 16;
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
        interpret_options.panic = &panic;
        MMAY_EXPECT(mmay_mir_interpret(
            &function,
            NULL,
            0,
            mmay_system_allocator(),
            interpret_options,
            &result
        ) == MMAY_MIR_INTERPRET_PANIC);
        MMAY_EXPECT(panic.code != NULL && strcmp(panic.code, "G4002") == 0);
        MMAY_EXPECT(panic.message.length == sizeof(negative_message) - 1);
        MMAY_EXPECT(panic.message.data != NULL && memcmp(
            panic.message.data,
            negative_message,
            sizeof(negative_message) - 1
        ) == 0);
        MMAY_EXPECT(mmay_symbol_id_equal(panic.symbol, symbol));
        MMAY_EXPECT(panic.span.start == 7 && panic.span.end == 16);
        {
            MarimayFailAllocator counting_state;
            MarimayFailAllocator failing_state;
            MarimayAllocator counting_allocator;
            MarimayAllocator failing_allocator;
            MarimayMirPanic counted_panic;
            MarimayMirPanic failed_panic;
            size_t message_allocation = 0;
            memset(&counted_panic, 0, sizeof(counted_panic));
            memset(&failed_panic, 0, sizeof(failed_panic));
            mmay_fail_allocator_init(
                &counting_state,
                mmay_system_allocator(),
                SIZE_MAX
            );
            counting_allocator = mmay_fail_allocator(&counting_state);
            interpret_options.panic = &counted_panic;
            MMAY_EXPECT(mmay_mir_interpret(
                &function,
                NULL,
                0,
                counting_allocator,
                interpret_options,
                &result
            ) == MMAY_MIR_INTERPRET_PANIC);
            MMAY_EXPECT(counting_state.attempts != 0);
            message_allocation = counting_state.attempts - 1;
            mmay_mir_panic_dispose(&counted_panic);
            MMAY_EXPECT(counting_state.live_allocations == 0);
            MMAY_EXPECT(counting_state.live_bytes == 0);
            mmay_fail_allocator_init(
                &failing_state,
                mmay_system_allocator(),
                message_allocation
            );
            failing_allocator = mmay_fail_allocator(&failing_state);
            interpret_options.panic = &failed_panic;
            MMAY_EXPECT(mmay_mir_interpret(
                &function,
                NULL,
                0,
                failing_allocator,
                interpret_options,
                &result
            ) == MMAY_MIR_INTERPRET_ALLOCATION_FAILED);
            MMAY_EXPECT(failed_panic.message.data == NULL);
            mmay_mir_panic_dispose(&failed_panic);
            MMAY_EXPECT(failing_state.live_allocations == 0);
            MMAY_EXPECT(failing_state.live_bytes == 0);
            interpret_options.panic = &panic;
        }
        stored_block = mmay_vector_at(&function.blocks, block);
        stored_index = stored_block != NULL
            ? mmay_vector_at(&stored_block->instructions, 2)
            : NULL;
        stored_access = stored_block != NULL
            ? mmay_vector_at(&stored_block->instructions, 3)
            : NULL;
        MMAY_EXPECT(stored_index != NULL && stored_access != NULL);
        if (stored_index != NULL) {
            stored_index->integer = 1;
        }
        MMAY_EXPECT(mmay_mir_interpret(
            &function,
            NULL,
            0,
            mmay_system_allocator(),
            interpret_options,
            &result
        ) == MMAY_MIR_INTERPRET_PANIC);
        MMAY_EXPECT(panic.message.length == sizeof(upper_message) - 1);
        MMAY_EXPECT(panic.message.data != NULL && memcmp(
            panic.message.data,
            upper_message,
            sizeof(upper_message) - 1
        ) == 0);
        if (stored_index != NULL) {
            stored_index->integer = INT64_MIN;
        }
        MMAY_EXPECT(mmay_mir_interpret(
            &function,
            NULL,
            0,
            mmay_system_allocator(),
            interpret_options,
            &result
        ) == MMAY_MIR_INTERPRET_PANIC);
        MMAY_EXPECT(panic.message.length == sizeof(minimum_message) - 1);
        MMAY_EXPECT(panic.message.data != NULL && memcmp(
            panic.message.data,
            minimum_message,
            sizeof(minimum_message) - 1
        ) == 0);
        if (stored_index != NULL) {
            stored_index->integer = INT64_MAX;
        }
        MMAY_EXPECT(mmay_mir_interpret(
            &function,
            NULL,
            0,
            mmay_system_allocator(),
            interpret_options,
            &result
        ) == MMAY_MIR_INTERPRET_PANIC);
        MMAY_EXPECT(panic.message.length == sizeof(maximum_message) - 1);
        MMAY_EXPECT(panic.message.data != NULL && memcmp(
            panic.message.data,
            maximum_message,
            sizeof(maximum_message) - 1
        ) == 0);
        if (stored_index != NULL) {
            stored_index->integer = 1;
        }
        if (stored_access != NULL) {
            stored_access->kind = MMAY_MIR_ARRAY_SET;
            stored_access->third = value;
        }
        MMAY_EXPECT(mmay_mir_verify(&function)
            == MMAY_MIR_VERIFY_INVALID_ARGUMENT);
        if (stored_access != NULL) {
            stored_access->kind = MMAY_MIR_ARRAY_GET;
            stored_access->third = MMAY_MIR_NO_REGISTER;
        }
        MMAY_EXPECT(mmay_mir_verify(&function) == MMAY_MIR_VERIFY_OK);
        interpret_options.panic = NULL;
        MMAY_EXPECT(mmay_mir_interpret(
            &function,
            NULL,
            0,
            mmay_system_allocator(),
            interpret_options,
            &result
        ) == MMAY_MIR_INTERPRET_PANIC);
        MMAY_EXPECT(mmay_mir_execution_array_at(
            &execution,
            execution.arrays.length - 1,
            &handle
        ));
        MMAY_EXPECT(mmay_mir_execution_array_element(
            &execution,
            handle,
            0,
            &element
        ));
        MMAY_EXPECT(element.as.f64_bits == one);
        mmay_mir_panic_dispose(&panic);
        mmay_mir_execution_dispose(&execution);
        mmay_mir_function_dispose(&function);
    }

    mmay_test_mir_arrays_rhs(symbol, array_type, one, five);
}
