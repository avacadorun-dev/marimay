#include "test_mir_support.h"

#include <string.h>

#include "ruda/base/allocator.h"

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
        mismatch_argument.as.reference.execution_provenance =
            execution.provenance;
        mismatch_argument.as.reference.slot = 0;
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
    RUDA_EXPECT(!ruda_mir_reference_equal(
        first.as.reference,
        second.as.reference
    ));
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


void ruda_test_mir_records_identity(RudaSymbolId symbol) {
    ruda_test_mir_records(symbol);
    ruda_test_mir_record_identity(symbol);
}
