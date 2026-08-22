#include "test_mir_support.h"

#include <string.h>

#include "mmay/base/allocator.h"

static void mmay_test_mir_enums(MarimaySymbolId symbol) {
    const MarimayTypeId enum_type = MMAY_TYPE_FIRST_DYNAMIC + 20;
    MarimayMirEnumVariant variants[3];
    MarimayMirEnumPayload payloads[4];
    MarimayMirFunction function;
    MarimayMirRegister first = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister ready = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister x = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister y = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister uninitialized = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister unit_value = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister positional_value = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister named_value = MMAY_MIR_NO_REGISTER;
    MarimayMirBlockId block = MMAY_MIR_NO_BLOCK;
    MarimayMirEnumId enum_layout = MMAY_MIR_NO_ENUM;
    MarimayMirEnumConstructionId unit = MMAY_MIR_NO_ENUM_CONSTRUCTION;
    MarimayMirEnumConstructionId positional = MMAY_MIR_NO_ENUM_CONSTRUCTION;
    MarimayMirEnumConstructionId named = MMAY_MIR_NO_ENUM_CONSTRUCTION;
    MarimayMirEnumArgument positional_arguments[2];
    MarimayMirEnumArgument named_arguments[2];
    MarimayMirInstruction instruction;
    MarimayMirExecution execution;
    MarimayMirExecutionOptions execution_options =
        mmay_mir_execution_default_options();
    MarimayMirInterpretOptions interpret_options =
        mmay_mir_interpret_default_options();
    MarimayMirValue result;
    MarimayMirValue payload;
    size_t selected_variant = SIZE_MAX;
    size_t payload_count = SIZE_MAX;

    variants[0].kind = MMAY_MIR_ENUM_VARIANT_UNIT;
    variants[0].payload_start = 0;
    variants[0].payload_count = 0;
    variants[1].kind = MMAY_MIR_ENUM_VARIANT_POSITIONAL;
    variants[1].payload_start = 0;
    variants[1].payload_count = 2;
    variants[2].kind = MMAY_MIR_ENUM_VARIANT_NAMED;
    variants[2].payload_start = 2;
    variants[2].payload_count = 2;
    payloads[0].type = MMAY_TYPE_I64;
    payloads[1].type = MMAY_TYPE_BOOL;
    payloads[2].type = MMAY_TYPE_I64;
    payloads[3].type = MMAY_TYPE_I64;
    MMAY_EXPECT(mmay_mir_function_init(
        &function,
        mmay_system_allocator(),
        symbol,
        NULL,
        0,
        enum_type,
        mmay_mir_function_default_options()
    ));
    MMAY_EXPECT(mmay_mir_add_register(&function, MMAY_TYPE_I64, &first));
    MMAY_EXPECT(mmay_mir_add_register(&function, MMAY_TYPE_BOOL, &ready));
    MMAY_EXPECT(mmay_mir_add_register(&function, MMAY_TYPE_I64, &x));
    MMAY_EXPECT(mmay_mir_add_register(&function, MMAY_TYPE_I64, &y));
    MMAY_EXPECT(mmay_mir_add_register(
        &function,
        MMAY_TYPE_I64,
        &uninitialized
    ));
    MMAY_EXPECT(mmay_mir_add_register(
        &function,
        enum_type,
        &unit_value
    ));
    MMAY_EXPECT(mmay_mir_add_register(
        &function,
        enum_type,
        &positional_value
    ));
    MMAY_EXPECT(mmay_mir_add_register(
        &function,
        enum_type,
        &named_value
    ));
    MMAY_EXPECT(mmay_mir_add_block(&function, &block));
    MMAY_EXPECT(mmay_mir_add_enum(
        &function,
        enum_type,
        variants,
        3,
        payloads,
        4,
        &enum_layout
    ));
    {
        MarimayMirEnumId duplicate = MMAY_MIR_NO_ENUM;
        MarimayMirEnumVariant mismatch[3] = {
            variants[0], variants[1], variants[2]
        };
        MMAY_EXPECT(mmay_mir_add_enum(
            &function,
            enum_type,
            variants,
            3,
            payloads,
            4,
            &duplicate
        ));
        MMAY_EXPECT(duplicate == enum_layout);
        mismatch[2].kind = MMAY_MIR_ENUM_VARIANT_POSITIONAL;
        MMAY_EXPECT(!mmay_mir_add_enum(
            &function,
            enum_type,
            mismatch,
            3,
            payloads,
            4,
            &duplicate
        ));
    }
    MMAY_EXPECT(mmay_mir_add_enum_construction(
        &function,
        enum_layout,
        0,
        NULL,
        0,
        &unit
    ));
    positional_arguments[0].register_id = first;
    positional_arguments[0].type = MMAY_TYPE_I64;
    positional_arguments[1].register_id = ready;
    positional_arguments[1].type = MMAY_TYPE_BOOL;
    MMAY_EXPECT(mmay_mir_add_enum_construction(
        &function,
        enum_layout,
        1,
        positional_arguments,
        2,
        &positional
    ));
    named_arguments[0].register_id = x;
    named_arguments[0].type = MMAY_TYPE_I64;
    named_arguments[1].register_id = y;
    named_arguments[1].type = MMAY_TYPE_I64;
    MMAY_EXPECT(mmay_mir_add_enum_construction(
        &function,
        enum_layout,
        2,
        named_arguments,
        2,
        &named
    ));
    {
        MarimayMirEnumConstructionId invalid = MMAY_MIR_NO_ENUM_CONSTRUCTION;
        MMAY_EXPECT(!mmay_mir_add_enum_construction(
            &function,
            enum_layout,
            2,
            named_arguments,
            1,
            &invalid
        ));
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MMAY_MIR_ENUM_NEW;
    instruction.target = unit_value;
    instruction.enum_id = enum_layout;
    instruction.enum_construction = unit;
    MMAY_EXPECT(mmay_mir_append_instruction(&function, block, &instruction));
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MMAY_MIR_CONSTANT_I64;
    instruction.target = first;
    instruction.integer = 41;
    MMAY_EXPECT(mmay_mir_append_instruction(&function, block, &instruction));
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MMAY_MIR_CONSTANT_BOOL;
    instruction.target = ready;
    instruction.boolean = true;
    MMAY_EXPECT(mmay_mir_append_instruction(&function, block, &instruction));
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MMAY_MIR_ENUM_NEW;
    instruction.target = positional_value;
    instruction.enum_id = enum_layout;
    instruction.enum_construction = positional;
    MMAY_EXPECT(mmay_mir_append_instruction(&function, block, &instruction));
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MMAY_MIR_CONSTANT_I64;
    instruction.target = x;
    instruction.integer = 10;
    MMAY_EXPECT(mmay_mir_append_instruction(&function, block, &instruction));
    instruction.target = y;
    instruction.integer = 20;
    MMAY_EXPECT(mmay_mir_append_instruction(&function, block, &instruction));
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MMAY_MIR_ENUM_NEW;
    instruction.target = named_value;
    instruction.enum_id = enum_layout;
    instruction.enum_construction = named;
    MMAY_EXPECT(mmay_mir_append_instruction(&function, block, &instruction));
    MMAY_EXPECT(mmay_mir_set_terminator(
        &function,
        block,
        mmay_test_mir_return(named_value)
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
    ) == MMAY_MIR_INTERPRET_OK);
    MMAY_EXPECT(result.type == enum_type);
    MMAY_EXPECT(mmay_mir_execution_owns_reference(&execution, result));
    MMAY_EXPECT(execution.enums.length == 3);
    MMAY_EXPECT(mmay_mir_execution_inspect_enum(
        &execution,
        result,
        &selected_variant,
        &payload_count
    ));
    MMAY_EXPECT(selected_variant == 2);
    MMAY_EXPECT(payload_count == 2);
    MMAY_EXPECT(mmay_mir_execution_enum_payload(
        &execution,
        result,
        0,
        &payload
    ));
    MMAY_EXPECT(payload.type == MMAY_TYPE_I64);
    MMAY_EXPECT(payload.as.i64 == 10);
    MMAY_EXPECT(mmay_mir_execution_enum_payload(
        &execution,
        result,
        1,
        &payload
    ));
    MMAY_EXPECT(payload.as.i64 == 20);
    MMAY_EXPECT(!mmay_mir_execution_enum_payload(
        &execution,
        result,
        2,
        &payload
    ));
    mmay_mir_execution_dispose(&execution);

    {
        MarimayMirEnumArgument *argument = mmay_vector_at(
            &function.enum_arguments,
            2
        );
        MarimayTypeId *target_type = mmay_vector_at(
            &function.register_types,
            named_value
        );
        MMAY_EXPECT(argument != NULL);
        MMAY_EXPECT(target_type != NULL);
        if (argument != NULL) {
            argument->register_id = uninitialized;
            MMAY_EXPECT(
                mmay_mir_verify(&function)
                    == MMAY_MIR_VERIFY_UNINITIALIZED_REGISTER
            );
            argument->register_id = x;
        }
        if (target_type != NULL) {
            *target_type = MMAY_TYPE_BOOL;
            MMAY_EXPECT(
                mmay_mir_verify(&function) == MMAY_MIR_VERIFY_TYPE_MISMATCH
            );
            *target_type = enum_type;
        }
        MMAY_EXPECT(mmay_mir_verify(&function) == MMAY_MIR_VERIFY_OK);
    }

    execution_options = mmay_mir_execution_default_options();
    execution_options.max_objects = 2;
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
    MMAY_EXPECT(execution.enums.length == 2);
    mmay_mir_execution_dispose(&execution);

    {
        size_t failure_offset = 0;
        bool reached_success = false;
        for (failure_offset = 0; failure_offset < 16; failure_offset += 1) {
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
}

static void mmay_test_mir_enum_projection(MarimaySymbolId symbol) {
    const MarimayTypeId enum_type = MMAY_TYPE_FIRST_DYNAMIC + 21;
    MarimayMirEnumVariant variants[3];
    MarimayMirEnumPayload payloads[4];
    MarimayMirFunction constructor;
    MarimayMirFunction projection;
    MarimayMirEnumArgument arguments[2];
    MarimayMirEnumId constructor_layout = MMAY_MIR_NO_ENUM;
    MarimayMirEnumId projection_layout = MMAY_MIR_NO_ENUM;
    MarimayMirEnumConstructionId construction = MMAY_MIR_NO_ENUM_CONSTRUCTION;
    MarimayMirRegister left = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister right = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister value = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister matched = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister guarded = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister projected = MMAY_MIR_NO_REGISTER;
    MarimayMirBlockId constructor_block = MMAY_MIR_NO_BLOCK;
    MarimayMirBlockId projection_block = MMAY_MIR_NO_BLOCK;
    MarimayMirInstruction instruction;
    MarimayMirExecution execution;
    MarimayMirInterpretOptions options = mmay_mir_interpret_default_options();
    MarimayMirValue enum_value;
    MarimayMirValue result;
    MarimayMirValue forged;

    variants[0].kind = MMAY_MIR_ENUM_VARIANT_UNIT;
    variants[0].payload_start = 0;
    variants[0].payload_count = 0;
    variants[1].kind = MMAY_MIR_ENUM_VARIANT_POSITIONAL;
    variants[1].payload_start = 0;
    variants[1].payload_count = 2;
    variants[2].kind = MMAY_MIR_ENUM_VARIANT_NAMED;
    variants[2].payload_start = 2;
    variants[2].payload_count = 2;
    payloads[0].type = MMAY_TYPE_I64;
    payloads[1].type = MMAY_TYPE_BOOL;
    payloads[2].type = MMAY_TYPE_I64;
    payloads[3].type = MMAY_TYPE_I64;

    MMAY_EXPECT(mmay_mir_function_init(
        &constructor,
        mmay_system_allocator(),
        symbol,
        NULL,
        0,
        enum_type,
        mmay_mir_function_default_options()
    ));
    MMAY_EXPECT(mmay_mir_add_register(&constructor, MMAY_TYPE_I64, &left));
    MMAY_EXPECT(mmay_mir_add_register(&constructor, MMAY_TYPE_I64, &right));
    MMAY_EXPECT(mmay_mir_add_register(&constructor, enum_type, &value));
    MMAY_EXPECT(mmay_mir_add_block(&constructor, &constructor_block));
    MMAY_EXPECT(mmay_mir_add_enum(
        &constructor,
        enum_type,
        variants,
        3,
        payloads,
        4,
        &constructor_layout
    ));
    arguments[0].register_id = left;
    arguments[0].type = MMAY_TYPE_I64;
    arguments[1].register_id = right;
    arguments[1].type = MMAY_TYPE_I64;
    MMAY_EXPECT(mmay_mir_add_enum_construction(
        &constructor,
        constructor_layout,
        2,
        arguments,
        2,
        &construction
    ));
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MMAY_MIR_CONSTANT_I64;
    instruction.target = left;
    instruction.integer = 10;
    MMAY_EXPECT(mmay_mir_append_instruction(
        &constructor,
        constructor_block,
        &instruction
    ));
    instruction.target = right;
    instruction.integer = 20;
    MMAY_EXPECT(mmay_mir_append_instruction(
        &constructor,
        constructor_block,
        &instruction
    ));
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MMAY_MIR_ENUM_NEW;
    instruction.target = value;
    instruction.enum_id = constructor_layout;
    instruction.enum_construction = construction;
    MMAY_EXPECT(mmay_mir_append_instruction(
        &constructor,
        constructor_block,
        &instruction
    ));
    MMAY_EXPECT(mmay_mir_set_terminator(
        &constructor,
        constructor_block,
        mmay_test_mir_return(value)
    ));
    MMAY_EXPECT(mmay_mir_verify(&constructor) == MMAY_MIR_VERIFY_OK);

    MMAY_EXPECT(mmay_mir_function_init(
        &projection,
        mmay_system_allocator(),
        symbol,
        &enum_type,
        1,
        MMAY_TYPE_I64,
        mmay_mir_function_default_options()
    ));
    MMAY_EXPECT(mmay_mir_add_register(
        &projection,
        MMAY_TYPE_BOOL,
        &matched
    ));
    MMAY_EXPECT(mmay_mir_add_register(
        &projection,
        enum_type,
        &guarded
    ));
    MMAY_EXPECT(mmay_mir_add_register(
        &projection,
        MMAY_TYPE_I64,
        &projected
    ));
    MMAY_EXPECT(mmay_mir_add_block(&projection, &projection_block));
    MMAY_EXPECT(mmay_mir_add_enum(
        &projection,
        enum_type,
        variants,
        3,
        payloads,
        4,
        &projection_layout
    ));
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MMAY_MIR_ENUM_IS_VARIANT;
    instruction.target = matched;
    instruction.first = 0;
    instruction.second = MMAY_MIR_NO_REGISTER;
    instruction.enum_id = projection_layout;
    instruction.variant = 2;
    MMAY_EXPECT(mmay_mir_append_instruction(
        &projection,
        projection_block,
        &instruction
    ));
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MMAY_MIR_ENUM_REQUIRE_VARIANT;
    instruction.target = guarded;
    instruction.first = 0;
    instruction.second = MMAY_MIR_NO_REGISTER;
    instruction.enum_id = projection_layout;
    instruction.variant = 2;
    MMAY_EXPECT(mmay_mir_append_instruction(
        &projection,
        projection_block,
        &instruction
    ));
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MMAY_MIR_ENUM_GET_PAYLOAD;
    instruction.target = projected;
    instruction.first = guarded;
    instruction.second = MMAY_MIR_NO_REGISTER;
    instruction.enum_id = projection_layout;
    instruction.variant = 2;
    instruction.field = 1;
    MMAY_EXPECT(mmay_mir_append_instruction(
        &projection,
        projection_block,
        &instruction
    ));
    MMAY_EXPECT(mmay_mir_set_terminator(
        &projection,
        projection_block,
        mmay_test_mir_return(projected)
    ));
    MMAY_EXPECT(mmay_mir_verify(&projection) == MMAY_MIR_VERIFY_OK);

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
        &enum_value
    ) == MMAY_MIR_INTERPRET_OK);
    MMAY_EXPECT(mmay_mir_interpret(
        &projection,
        &enum_value,
        1,
        mmay_system_allocator(),
        options,
        &result
    ) == MMAY_MIR_INTERPRET_OK);
    MMAY_EXPECT(result.type == MMAY_TYPE_I64 && result.as.i64 == 20);
    forged = enum_value;
    forged.as.reference = MMAY_MIR_INVALID_REFERENCE;
    MMAY_EXPECT(mmay_mir_interpret(
        &projection,
        &forged,
        1,
        mmay_system_allocator(),
        options,
        &result
    ) == MMAY_MIR_INTERPRET_INVALID_REFERENCE);
    {
        MarimayMirBlock *stored_block = mmay_vector_at(
            &projection.blocks,
            projection_block
        );
        MarimayMirInstruction *require = stored_block != NULL
            ? mmay_vector_at(&stored_block->instructions, 1)
            : NULL;
        MarimayMirInstruction *get = stored_block != NULL
            ? mmay_vector_at(&stored_block->instructions, 2)
            : NULL;
        MMAY_EXPECT(require != NULL && get != NULL);
        if (require != NULL) {
            require->variant = 1;
            MMAY_EXPECT(mmay_mir_verify(&projection) == MMAY_MIR_VERIFY_OK);
            MMAY_EXPECT(mmay_mir_interpret(
                &projection,
                &enum_value,
                1,
                mmay_system_allocator(),
                options,
                &result
            ) == MMAY_MIR_INTERPRET_INVALID_MIR);
            require->variant = 2;
        }
        if (get != NULL) {
            get->field = 2;
            MMAY_EXPECT(
                mmay_mir_verify(&projection)
                    == MMAY_MIR_VERIFY_INVALID_ARGUMENT
            );
            get->field = 1;
        }
        {
            MarimayMirEnumPayload *layout_payload = mmay_vector_at(
                &projection.enum_payloads,
                3
            );
            MarimayTypeId *projected_type = mmay_vector_at(
                &projection.register_types,
                projected
            );
            MMAY_EXPECT(layout_payload != NULL && projected_type != NULL);
            if (layout_payload != NULL && projected_type != NULL) {
                layout_payload->type = MMAY_TYPE_BOOL;
                *projected_type = MMAY_TYPE_BOOL;
                projection.result_type = MMAY_TYPE_BOOL;
                MMAY_EXPECT(
                    mmay_mir_verify(&projection) == MMAY_MIR_VERIFY_OK
                );
                MMAY_EXPECT(mmay_mir_interpret(
                    &projection,
                    &enum_value,
                    1,
                    mmay_system_allocator(),
                    options,
                    &result
                ) == MMAY_MIR_INTERPRET_INVALID_MIR);
                layout_payload->type = MMAY_TYPE_I64;
                *projected_type = MMAY_TYPE_I64;
                projection.result_type = MMAY_TYPE_I64;
            }
        }
        MMAY_EXPECT(mmay_mir_verify(&projection) == MMAY_MIR_VERIFY_OK);
    }
    mmay_mir_execution_dispose(&execution);
    mmay_mir_function_dispose(&projection);
    mmay_mir_function_dispose(&constructor);
}

void mmay_test_mir_enums_projection(MarimaySymbolId symbol) {
    mmay_test_mir_enums(symbol);
    mmay_test_mir_enum_projection(symbol);
}
