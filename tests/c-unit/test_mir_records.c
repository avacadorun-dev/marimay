#include "test_mir_support.h"

#include <string.h>

#include "mmay/base/allocator.h"

static void mmay_test_mir_records(MarimaySymbolId symbol) {
    const MarimayTypeId record_type = MMAY_TYPE_FIRST_DYNAMIC;
    MarimayMirRecordField fields[2];
    MarimayMirFunction function;
    MarimayMirRegister first = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister second = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister record_value = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister alias = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister replacement = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister set_result = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister projected = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister uninitialized = MMAY_MIR_NO_REGISTER;
    MarimayMirBlockId block = MMAY_MIR_NO_BLOCK;
    MarimayMirRecordId record = MMAY_MIR_NO_RECORD;
    MarimayMirRecordConstructionId construction =
        MMAY_MIR_NO_RECORD_CONSTRUCTION;
    MarimayMirRecordArgument construction_arguments[2];
    MarimayMirInstruction instruction;
    MarimayMirExecution execution;
    MarimayMirInterpretOptions interpret_options =
        mmay_mir_interpret_default_options();
    MarimayMirValue result;

    fields[0].type = MMAY_TYPE_I64;
    fields[0].is_mutable = false;
    fields[1].type = MMAY_TYPE_I64;
    fields[1].is_mutable = true;
    MMAY_EXPECT(mmay_mir_function_init(
        &function,
        mmay_system_allocator(),
        symbol,
        NULL,
        0,
        MMAY_TYPE_I64,
        mmay_mir_function_default_options()
    ));
    MMAY_EXPECT(mmay_mir_add_register(&function, MMAY_TYPE_I64, &first));
    MMAY_EXPECT(mmay_mir_add_register(&function, MMAY_TYPE_I64, &second));
    MMAY_EXPECT(mmay_mir_add_register(
        &function,
        record_type,
        &record_value
    ));
    MMAY_EXPECT(mmay_mir_add_register(&function, record_type, &alias));
    MMAY_EXPECT(mmay_mir_add_register(
        &function,
        MMAY_TYPE_I64,
        &replacement
    ));
    MMAY_EXPECT(mmay_mir_add_register(
        &function,
        MMAY_TYPE_I64,
        &set_result
    ));
    MMAY_EXPECT(mmay_mir_add_register(
        &function,
        MMAY_TYPE_I64,
        &projected
    ));
    MMAY_EXPECT(mmay_mir_add_register(
        &function,
        MMAY_TYPE_I64,
        &uninitialized
    ));
    MMAY_EXPECT(mmay_mir_add_block(&function, &block));
    MMAY_EXPECT(mmay_mir_add_record(
        &function,
        record_type,
        fields,
        2,
        &record
    ));
    {
        MarimayMirRecordId duplicate = MMAY_MIR_NO_RECORD;
        MarimayMirRecordField mismatch[2] = {fields[0], fields[1]};
        MMAY_EXPECT(mmay_mir_add_record(
            &function,
            record_type,
            fields,
            2,
            &duplicate
        ));
        MMAY_EXPECT(duplicate == record);
        mismatch[1].is_mutable = false;
        MMAY_EXPECT(!mmay_mir_add_record(
            &function,
            record_type,
            mismatch,
            2,
            &duplicate
        ));
    }
    construction_arguments[0].register_id = first;
    construction_arguments[0].type = MMAY_TYPE_I64;
    construction_arguments[1].register_id = second;
    construction_arguments[1].type = MMAY_TYPE_I64;
    MMAY_EXPECT(mmay_mir_add_record_construction(
        &function,
        record,
        construction_arguments,
        2,
        &construction
    ));
    {
        MarimayMirRecordConstructionId invalid =
            MMAY_MIR_NO_RECORD_CONSTRUCTION;
        MMAY_EXPECT(!mmay_mir_add_record_construction(
            &function,
            record,
            construction_arguments,
            1,
            &invalid
        ));
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MMAY_MIR_CONSTANT_I64;
    instruction.target = first;
    instruction.integer = 10;
    MMAY_EXPECT(mmay_mir_append_instruction(&function, block, &instruction));
    instruction.target = second;
    instruction.integer = 20;
    MMAY_EXPECT(mmay_mir_append_instruction(&function, block, &instruction));
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MMAY_MIR_RECORD_NEW;
    instruction.target = record_value;
    instruction.record = record;
    instruction.record_construction = construction;
    MMAY_EXPECT(mmay_mir_append_instruction(&function, block, &instruction));
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MMAY_MIR_COPY;
    instruction.target = alias;
    instruction.first = record_value;
    MMAY_EXPECT(mmay_mir_append_instruction(&function, block, &instruction));
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MMAY_MIR_CONSTANT_I64;
    instruction.target = replacement;
    instruction.integer = 30;
    MMAY_EXPECT(mmay_mir_append_instruction(&function, block, &instruction));
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MMAY_MIR_RECORD_SET;
    instruction.target = set_result;
    instruction.first = record_value;
    instruction.second = replacement;
    instruction.record = record;
    instruction.field = 1;
    MMAY_EXPECT(mmay_mir_append_instruction(&function, block, &instruction));
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MMAY_MIR_RECORD_GET;
    instruction.target = projected;
    instruction.first = alias;
    instruction.record = record;
    instruction.field = 1;
    MMAY_EXPECT(mmay_mir_append_instruction(&function, block, &instruction));
    MMAY_EXPECT(mmay_mir_set_terminator(
        &function,
        block,
        mmay_test_mir_return(projected)
    ));
    MMAY_EXPECT(mmay_mir_verify(&function) == MMAY_MIR_VERIFY_OK);
    {
        MarimayMirRecordId other = MMAY_MIR_NO_RECORD;
        MarimayMirRecord *other_layout = NULL;
        MMAY_EXPECT(mmay_mir_add_record(
            &function,
            record_type + 1,
            fields,
            2,
            &other
        ));
        other_layout = mmay_vector_at(&function.records, other);
        MMAY_EXPECT(other_layout != NULL);
        if (other_layout != NULL) {
            other_layout->type = record_type;
            MMAY_EXPECT(
                mmay_mir_verify(&function)
                    == MMAY_MIR_VERIFY_INVALID_ARGUMENT
            );
            other_layout->type = record_type + 1;
        }
        MMAY_EXPECT(mmay_mir_verify(&function) == MMAY_MIR_VERIFY_OK);
    }
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
    MMAY_EXPECT(mmay_mir_interpret(
        &function,
        NULL,
        0,
        mmay_system_allocator(),
        interpret_options,
        &result
    ) == MMAY_MIR_INTERPRET_OK);
    MMAY_EXPECT(result.type == MMAY_TYPE_I64);
    MMAY_EXPECT(result.as.i64 == 30);
    MMAY_EXPECT(execution.objects.length == 1);
    MMAY_EXPECT(execution.total_fields == 2);
    {
        MarimayMirFunction mismatch;
        MarimayTypeId parameter = record_type;
        MarimayMirRecordField mismatch_fields[2];
        MarimayMirRecordId mismatch_record = MMAY_MIR_NO_RECORD;
        MarimayMirRegister mismatch_value = MMAY_MIR_NO_REGISTER;
        MarimayMirBlockId mismatch_block = MMAY_MIR_NO_BLOCK;
        MarimayMirValue mismatch_argument;
        mismatch_fields[0].type = MMAY_TYPE_I64;
        mismatch_fields[0].is_mutable = true;
        mismatch_fields[1].type = MMAY_TYPE_I64;
        mismatch_fields[1].is_mutable = true;
        MMAY_EXPECT(mmay_mir_function_init(
            &mismatch,
            mmay_system_allocator(),
            symbol,
            &parameter,
            1,
            MMAY_TYPE_I64,
            mmay_mir_function_default_options()
        ));
        MMAY_EXPECT(mmay_mir_add_register(
            &mismatch,
            MMAY_TYPE_I64,
            &mismatch_value
        ));
        MMAY_EXPECT(mmay_mir_add_block(&mismatch, &mismatch_block));
        MMAY_EXPECT(mmay_mir_add_record(
            &mismatch,
            record_type,
            mismatch_fields,
            2,
            &mismatch_record
        ));
        memset(&instruction, 0, sizeof(instruction));
        instruction.kind = MMAY_MIR_RECORD_GET;
        instruction.target = mismatch_value;
        instruction.first = 0;
        instruction.record = mismatch_record;
        instruction.field = 0;
        MMAY_EXPECT(mmay_mir_append_instruction(
            &mismatch,
            mismatch_block,
            &instruction
        ));
        MMAY_EXPECT(mmay_mir_set_terminator(
            &mismatch,
            mismatch_block,
            mmay_test_mir_return(mismatch_value)
        ));
        MMAY_EXPECT(mmay_mir_verify(&mismatch) == MMAY_MIR_VERIFY_OK);
        mismatch_argument.type = record_type;
        mismatch_argument.as.reference.execution_provenance =
            execution.provenance;
        mismatch_argument.as.reference.slot = 0;
        MMAY_EXPECT(mmay_mir_interpret(
            &mismatch,
            &mismatch_argument,
            1,
            mmay_system_allocator(),
            interpret_options,
            &result
        ) == MMAY_MIR_INTERPRET_INVALID_MIR);
        mmay_mir_function_dispose(&mismatch);
    }
    mmay_mir_execution_dispose(&execution);

    {
        MarimayMirBlock *mutable_block = mmay_vector_at(
            &function.blocks,
            block
        );
        MarimayMirInstruction *set = mutable_block != NULL
            ? mmay_vector_at(&mutable_block->instructions, 5)
            : NULL;
        MarimayTypeId *projected_type = mmay_vector_at(
            &function.register_types,
            projected
        );
        MarimayMirRecordArgument *argument = mmay_vector_at(
            &function.record_arguments,
            0
        );
        MMAY_EXPECT(set != NULL);
        MMAY_EXPECT(projected_type != NULL);
        MMAY_EXPECT(argument != NULL);
        if (set != NULL) {
            set->field = 0;
            MMAY_EXPECT(
                mmay_mir_verify(&function) == MMAY_MIR_VERIFY_TYPE_MISMATCH
            );
            set->field = 1;
        }
        if (projected_type != NULL) {
            *projected_type = MMAY_TYPE_BOOL;
            MMAY_EXPECT(
                mmay_mir_verify(&function) == MMAY_MIR_VERIFY_TYPE_MISMATCH
            );
            *projected_type = MMAY_TYPE_I64;
        }
        if (argument != NULL) {
            argument->register_id = uninitialized;
            MMAY_EXPECT(
                mmay_mir_verify(&function)
                    == MMAY_MIR_VERIFY_UNINITIALIZED_REGISTER
            );
            argument->register_id = first;
        }
        MMAY_EXPECT(mmay_mir_verify(&function) == MMAY_MIR_VERIFY_OK);
    }

    {
        size_t failure_offset = 0;
        bool reached_success = false;
        for (failure_offset = 0; failure_offset < 8; failure_offset += 1) {
            MarimayFailAllocator state;
            MarimayAllocator allocator;
            MarimayMirInterpretStatus status;
            mmay_fail_allocator_init(
                &state,
                mmay_system_allocator(),
                SIZE_MAX
            );
            allocator = mmay_fail_allocator(&state);
            MMAY_EXPECT(mmay_mir_execution_init(
                &execution,
                allocator,
                mmay_mir_execution_default_options()
            ));
            state.failure_index = state.attempts + failure_offset;
            interpret_options.execution = &execution;
            status = mmay_mir_interpret(
                &function,
                NULL,
                0,
                mmay_system_allocator(),
                interpret_options,
                &result
            );
            if (status == MMAY_MIR_INTERPRET_OK) {
                reached_success = true;
                MMAY_EXPECT(result.as.i64 == 30);
            } else {
                MMAY_EXPECT(
                    status == MMAY_MIR_INTERPRET_ALLOCATION_FAILED
                );
            }
            mmay_mir_execution_dispose(&execution);
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
        MarimayTypeId parameter = record_type;
        MarimayMirRegister value = MMAY_MIR_NO_REGISTER;
        MarimayMirValue argument;
        MMAY_EXPECT(mmay_mir_function_init(
            &function,
            mmay_system_allocator(),
            symbol,
            &parameter,
            1,
            MMAY_TYPE_I64,
            mmay_mir_function_default_options()
        ));
        MMAY_EXPECT(mmay_mir_add_register(
            &function,
            MMAY_TYPE_I64,
            &value
        ));
        MMAY_EXPECT(mmay_mir_add_block(&function, &block));
        MMAY_EXPECT(mmay_mir_add_record(
            &function,
            record_type,
            fields,
            2,
            &record
        ));
        memset(&instruction, 0, sizeof(instruction));
        instruction.kind = MMAY_MIR_RECORD_GET;
        instruction.target = value;
        instruction.first = 0;
        instruction.record = record;
        instruction.field = 0;
        MMAY_EXPECT(mmay_mir_append_instruction(
            &function,
            block,
            &instruction
        ));
        MMAY_EXPECT(mmay_mir_set_terminator(
            &function,
            block,
            mmay_test_mir_return(value)
        ));
        MMAY_EXPECT(mmay_mir_verify(&function) == MMAY_MIR_VERIFY_OK);
        MMAY_EXPECT(mmay_mir_execution_init(
            &execution,
            mmay_system_allocator(),
            mmay_mir_execution_default_options()
        ));
        interpret_options.execution = &execution;
        argument.type = record_type;
        argument.as.reference = MMAY_MIR_INVALID_REFERENCE;
        MMAY_EXPECT(mmay_mir_interpret(
            &function,
            &argument,
            1,
            mmay_system_allocator(),
            interpret_options,
            &result
        ) == MMAY_MIR_INTERPRET_INVALID_REFERENCE);
        mmay_mir_execution_dispose(&execution);
        mmay_mir_function_dispose(&function);
    }

    {
        MarimayMirExecutionOptions execution_options =
            mmay_mir_execution_default_options();
        MarimayMirRegister value = MMAY_MIR_NO_REGISTER;
        MarimayMirRegister first_record = MMAY_MIR_NO_REGISTER;
        MarimayMirRegister second_record = MMAY_MIR_NO_REGISTER;
        execution_options.max_objects = 1;
        MMAY_EXPECT(mmay_mir_function_init(
            &function,
            mmay_system_allocator(),
            symbol,
            NULL,
            0,
            MMAY_TYPE_I64,
            mmay_mir_function_default_options()
        ));
        MMAY_EXPECT(mmay_mir_add_register(&function, MMAY_TYPE_I64, &value));
        MMAY_EXPECT(mmay_mir_add_register(
            &function,
            record_type,
            &first_record
        ));
        MMAY_EXPECT(mmay_mir_add_register(
            &function,
            record_type,
            &second_record
        ));
        MMAY_EXPECT(mmay_mir_add_block(&function, &block));
        MMAY_EXPECT(mmay_mir_add_record(
            &function,
            record_type,
            fields,
            2,
            &record
        ));
        construction_arguments[0].register_id = value;
        construction_arguments[1].register_id = value;
        MMAY_EXPECT(mmay_mir_add_record_construction(
            &function,
            record,
            construction_arguments,
            2,
            &construction
        ));
        memset(&instruction, 0, sizeof(instruction));
        instruction.kind = MMAY_MIR_CONSTANT_I64;
        instruction.target = value;
        instruction.integer = 1;
        MMAY_EXPECT(mmay_mir_append_instruction(
            &function,
            block,
            &instruction
        ));
        memset(&instruction, 0, sizeof(instruction));
        instruction.kind = MMAY_MIR_RECORD_NEW;
        instruction.target = first_record;
        instruction.record = record;
        instruction.record_construction = construction;
        MMAY_EXPECT(mmay_mir_append_instruction(
            &function,
            block,
            &instruction
        ));
        instruction.target = second_record;
        MMAY_EXPECT(mmay_mir_append_instruction(
            &function,
            block,
            &instruction
        ));
        MMAY_EXPECT(mmay_mir_set_terminator(
            &function,
            block,
            mmay_test_mir_return(value)
        ));
        MMAY_EXPECT(mmay_mir_execution_init(
            &execution,
            mmay_system_allocator(),
            execution_options
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
        MMAY_EXPECT(execution.objects.length == 1);
        MMAY_EXPECT(execution.total_fields == 2);
        mmay_mir_execution_dispose(&execution);
        mmay_mir_function_dispose(&function);
    }

    {
        MarimayMirFunctionOptions options = mmay_mir_function_default_options();
        MarimayMirExecutionOptions execution_options =
            mmay_mir_execution_default_options();
        options.max_record_layouts = 0;
        MMAY_EXPECT(!mmay_mir_function_init(
            &function,
            mmay_system_allocator(),
            symbol,
            NULL,
            0,
            MMAY_TYPE_I64,
            options
        ));
        execution_options.max_objects = 0;
        MMAY_EXPECT(!mmay_mir_execution_init(
            &execution,
            mmay_system_allocator(),
            execution_options
        ));
    }
}

static void mmay_test_mir_record_identity(MarimaySymbolId symbol) {
    const MarimayTypeId record_type = MMAY_TYPE_FIRST_DYNAMIC + 10;
    MarimayMirRecordField field;
    MarimayMirRecordArgument construction_argument;
    MarimayMirFunction constructor;
    MarimayMirFunction comparison;
    MarimayMirRecordId constructor_record = MMAY_MIR_NO_RECORD;
    MarimayMirRecordId comparison_record = MMAY_MIR_NO_RECORD;
    MarimayMirRecordConstructionId construction =
        MMAY_MIR_NO_RECORD_CONSTRUCTION;
    MarimayMirRegister value = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister object = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister same = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister uninitialized = MMAY_MIR_NO_REGISTER;
    MarimayMirBlockId block = MMAY_MIR_NO_BLOCK;
    MarimayMirInstruction instruction;
    MarimayTypeId parameters[2] = {record_type, record_type};
    MarimayMirExecution execution;
    MarimayMirInterpretOptions options = mmay_mir_interpret_default_options();
    MarimayMirValue first;
    MarimayMirValue second;
    MarimayMirValue arguments[2];
    MarimayMirValue result;

    field.type = MMAY_TYPE_I64;
    field.is_mutable = false;
    MMAY_EXPECT(mmay_mir_function_init(
        &constructor,
        mmay_system_allocator(),
        symbol,
        NULL,
        0,
        record_type,
        mmay_mir_function_default_options()
    ));
    MMAY_EXPECT(mmay_mir_add_register(&constructor, MMAY_TYPE_I64, &value));
    MMAY_EXPECT(mmay_mir_add_register(&constructor, record_type, &object));
    MMAY_EXPECT(mmay_mir_add_block(&constructor, &block));
    MMAY_EXPECT(mmay_mir_add_record(
        &constructor,
        record_type,
        &field,
        1,
        &constructor_record
    ));
    construction_argument.register_id = value;
    construction_argument.type = MMAY_TYPE_I64;
    MMAY_EXPECT(mmay_mir_add_record_construction(
        &constructor,
        constructor_record,
        &construction_argument,
        1,
        &construction
    ));
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MMAY_MIR_CONSTANT_I64;
    instruction.target = value;
    instruction.integer = 42;
    MMAY_EXPECT(mmay_mir_append_instruction(
        &constructor,
        block,
        &instruction
    ));
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MMAY_MIR_RECORD_NEW;
    instruction.target = object;
    instruction.record = constructor_record;
    instruction.record_construction = construction;
    MMAY_EXPECT(mmay_mir_append_instruction(
        &constructor,
        block,
        &instruction
    ));
    MMAY_EXPECT(mmay_mir_set_terminator(
        &constructor,
        block,
        mmay_test_mir_return(object)
    ));
    MMAY_EXPECT(mmay_mir_verify(&constructor) == MMAY_MIR_VERIFY_OK);

    block = MMAY_MIR_NO_BLOCK;
    MMAY_EXPECT(mmay_mir_function_init(
        &comparison,
        mmay_system_allocator(),
        symbol,
        parameters,
        2,
        MMAY_TYPE_BOOL,
        mmay_mir_function_default_options()
    ));
    MMAY_EXPECT(mmay_mir_add_register(&comparison, MMAY_TYPE_BOOL, &same));
    MMAY_EXPECT(mmay_mir_add_register(
        &comparison,
        record_type,
        &uninitialized
    ));
    MMAY_EXPECT(mmay_mir_add_block(&comparison, &block));
    MMAY_EXPECT(mmay_mir_add_record(
        &comparison,
        record_type,
        &field,
        1,
        &comparison_record
    ));
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MMAY_MIR_RECORD_IDENTITY_SAME;
    instruction.target = same;
    instruction.first = 0;
    instruction.second = 1;
    instruction.record = comparison_record;
    MMAY_EXPECT(mmay_mir_append_instruction(
        &comparison,
        block,
        &instruction
    ));
    MMAY_EXPECT(mmay_mir_set_terminator(
        &comparison,
        block,
        mmay_test_mir_return(same)
    ));
    MMAY_EXPECT(mmay_mir_verify(&comparison) == MMAY_MIR_VERIFY_OK);
    {
        MarimayMirBlock *mutable_block = mmay_vector_at(
            &comparison.blocks,
            block
        );
        MarimayMirInstruction *identity = mutable_block != NULL
            ? mmay_vector_at(&mutable_block->instructions, 0)
            : NULL;
        MarimayTypeId *target_type = mmay_vector_at(
            &comparison.register_types,
            same
        );
        MarimayTypeId *second_type = mmay_vector_at(
            &comparison.register_types,
            1
        );
        MMAY_EXPECT(identity != NULL);
        MMAY_EXPECT(target_type != NULL);
        MMAY_EXPECT(second_type != NULL);
        if (identity != NULL) {
            identity->second = uninitialized;
            MMAY_EXPECT(
                mmay_mir_verify(&comparison)
                    == MMAY_MIR_VERIFY_UNINITIALIZED_REGISTER
            );
            identity->second = 1;
            identity->record = MMAY_MIR_NO_RECORD;
            MMAY_EXPECT(
                mmay_mir_verify(&comparison)
                    == MMAY_MIR_VERIFY_INVALID_ARGUMENT
            );
            identity->record = comparison_record;
        }
        if (target_type != NULL) {
            *target_type = MMAY_TYPE_I64;
            MMAY_EXPECT(
                mmay_mir_verify(&comparison)
                    == MMAY_MIR_VERIFY_TYPE_MISMATCH
            );
            *target_type = MMAY_TYPE_BOOL;
        }
        if (second_type != NULL) {
            *second_type = record_type + 1;
            MMAY_EXPECT(
                mmay_mir_verify(&comparison)
                    == MMAY_MIR_VERIFY_TYPE_MISMATCH
            );
            *second_type = record_type;
        }
    }
    MMAY_EXPECT(mmay_mir_verify(&comparison) == MMAY_MIR_VERIFY_OK);
    MMAY_EXPECT(mmay_mir_execution_init(
        &execution,
        mmay_system_allocator(),
        mmay_mir_execution_default_options()
    ));
    options.execution = &execution;
    MMAY_EXPECT(mmay_mir_interpret(
        &constructor,
        NULL,
        0,
        mmay_system_allocator(),
        options,
        &first
    ) == MMAY_MIR_INTERPRET_OK);
    MMAY_EXPECT(mmay_mir_interpret(
        &constructor,
        NULL,
        0,
        mmay_system_allocator(),
        options,
        &second
    ) == MMAY_MIR_INTERPRET_OK);
    MMAY_EXPECT(!mmay_mir_reference_equal(
        first.as.reference,
        second.as.reference
    ));
    arguments[0] = first;
    arguments[1] = first;
    MMAY_EXPECT(mmay_mir_interpret(
        &comparison,
        arguments,
        2,
        mmay_system_allocator(),
        options,
        &result
    ) == MMAY_MIR_INTERPRET_OK);
    MMAY_EXPECT(result.type == MMAY_TYPE_BOOL);
    MMAY_EXPECT(result.as.boolean);
    arguments[1] = second;
    MMAY_EXPECT(mmay_mir_interpret(
        &comparison,
        arguments,
        2,
        mmay_system_allocator(),
        options,
        &result
    ) == MMAY_MIR_INTERPRET_OK);
    MMAY_EXPECT(!result.as.boolean);
    arguments[1].as.reference = MMAY_MIR_INVALID_REFERENCE;
    MMAY_EXPECT(mmay_mir_interpret(
        &comparison,
        arguments,
        2,
        mmay_system_allocator(),
        options,
        &result
    ) == MMAY_MIR_INTERPRET_INVALID_REFERENCE);
    field.is_mutable = true;
    {
        MarimayMirRecordField *mismatched = mmay_vector_at(
            &comparison.record_fields,
            0
        );
        MMAY_EXPECT(mismatched != NULL);
        if (mismatched != NULL) {
            *mismatched = field;
        }
    }
    arguments[1] = first;
    MMAY_EXPECT(mmay_mir_interpret(
        &comparison,
        arguments,
        2,
        mmay_system_allocator(),
        options,
        &result
    ) == MMAY_MIR_INTERPRET_INVALID_MIR);
    mmay_mir_execution_dispose(&execution);
    mmay_mir_function_dispose(&comparison);
    mmay_mir_function_dispose(&constructor);
}


void mmay_test_mir_records_identity(MarimaySymbolId symbol) {
    mmay_test_mir_records(symbol);
    mmay_test_mir_record_identity(symbol);
}
