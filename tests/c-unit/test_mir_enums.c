#include "test_mir_support.h"

#include <string.h>

#include "ruda/base/allocator.h"

static void ruda_test_mir_enums(RudaSymbolId symbol) {
    const RudaTypeId enum_type = RUDA_TYPE_FIRST_DYNAMIC + 20;
    RudaMirEnumVariant variants[3];
    RudaMirEnumPayload payloads[4];
    RudaMirFunction function;
    RudaMirRegister first = RUDA_MIR_NO_REGISTER;
    RudaMirRegister ready = RUDA_MIR_NO_REGISTER;
    RudaMirRegister x = RUDA_MIR_NO_REGISTER;
    RudaMirRegister y = RUDA_MIR_NO_REGISTER;
    RudaMirRegister uninitialized = RUDA_MIR_NO_REGISTER;
    RudaMirRegister unit_value = RUDA_MIR_NO_REGISTER;
    RudaMirRegister positional_value = RUDA_MIR_NO_REGISTER;
    RudaMirRegister named_value = RUDA_MIR_NO_REGISTER;
    RudaMirBlockId block = RUDA_MIR_NO_BLOCK;
    RudaMirEnumId enum_layout = RUDA_MIR_NO_ENUM;
    RudaMirEnumConstructionId unit = RUDA_MIR_NO_ENUM_CONSTRUCTION;
    RudaMirEnumConstructionId positional = RUDA_MIR_NO_ENUM_CONSTRUCTION;
    RudaMirEnumConstructionId named = RUDA_MIR_NO_ENUM_CONSTRUCTION;
    RudaMirEnumArgument positional_arguments[2];
    RudaMirEnumArgument named_arguments[2];
    RudaMirInstruction instruction;
    RudaMirExecution execution;
    RudaMirExecutionOptions execution_options =
        ruda_mir_execution_default_options();
    RudaMirInterpretOptions interpret_options =
        ruda_mir_interpret_default_options();
    RudaMirValue result;
    RudaMirValue payload;
    size_t selected_variant = SIZE_MAX;
    size_t payload_count = SIZE_MAX;

    variants[0].kind = RUDA_MIR_ENUM_VARIANT_UNIT;
    variants[0].payload_start = 0;
    variants[0].payload_count = 0;
    variants[1].kind = RUDA_MIR_ENUM_VARIANT_POSITIONAL;
    variants[1].payload_start = 0;
    variants[1].payload_count = 2;
    variants[2].kind = RUDA_MIR_ENUM_VARIANT_NAMED;
    variants[2].payload_start = 2;
    variants[2].payload_count = 2;
    payloads[0].type = RUDA_TYPE_I64;
    payloads[1].type = RUDA_TYPE_BOOL;
    payloads[2].type = RUDA_TYPE_I64;
    payloads[3].type = RUDA_TYPE_I64;
    RUDA_EXPECT(ruda_mir_function_init(
        &function,
        ruda_system_allocator(),
        symbol,
        NULL,
        0,
        enum_type,
        ruda_mir_function_default_options()
    ));
    RUDA_EXPECT(ruda_mir_add_register(&function, RUDA_TYPE_I64, &first));
    RUDA_EXPECT(ruda_mir_add_register(&function, RUDA_TYPE_BOOL, &ready));
    RUDA_EXPECT(ruda_mir_add_register(&function, RUDA_TYPE_I64, &x));
    RUDA_EXPECT(ruda_mir_add_register(&function, RUDA_TYPE_I64, &y));
    RUDA_EXPECT(ruda_mir_add_register(
        &function,
        RUDA_TYPE_I64,
        &uninitialized
    ));
    RUDA_EXPECT(ruda_mir_add_register(
        &function,
        enum_type,
        &unit_value
    ));
    RUDA_EXPECT(ruda_mir_add_register(
        &function,
        enum_type,
        &positional_value
    ));
    RUDA_EXPECT(ruda_mir_add_register(
        &function,
        enum_type,
        &named_value
    ));
    RUDA_EXPECT(ruda_mir_add_block(&function, &block));
    RUDA_EXPECT(ruda_mir_add_enum(
        &function,
        enum_type,
        variants,
        3,
        payloads,
        4,
        &enum_layout
    ));
    {
        RudaMirEnumId duplicate = RUDA_MIR_NO_ENUM;
        RudaMirEnumVariant mismatch[3] = {
            variants[0], variants[1], variants[2]
        };
        RUDA_EXPECT(ruda_mir_add_enum(
            &function,
            enum_type,
            variants,
            3,
            payloads,
            4,
            &duplicate
        ));
        RUDA_EXPECT(duplicate == enum_layout);
        mismatch[2].kind = RUDA_MIR_ENUM_VARIANT_POSITIONAL;
        RUDA_EXPECT(!ruda_mir_add_enum(
            &function,
            enum_type,
            mismatch,
            3,
            payloads,
            4,
            &duplicate
        ));
    }
    RUDA_EXPECT(ruda_mir_add_enum_construction(
        &function,
        enum_layout,
        0,
        NULL,
        0,
        &unit
    ));
    positional_arguments[0].register_id = first;
    positional_arguments[0].type = RUDA_TYPE_I64;
    positional_arguments[1].register_id = ready;
    positional_arguments[1].type = RUDA_TYPE_BOOL;
    RUDA_EXPECT(ruda_mir_add_enum_construction(
        &function,
        enum_layout,
        1,
        positional_arguments,
        2,
        &positional
    ));
    named_arguments[0].register_id = x;
    named_arguments[0].type = RUDA_TYPE_I64;
    named_arguments[1].register_id = y;
    named_arguments[1].type = RUDA_TYPE_I64;
    RUDA_EXPECT(ruda_mir_add_enum_construction(
        &function,
        enum_layout,
        2,
        named_arguments,
        2,
        &named
    ));
    {
        RudaMirEnumConstructionId invalid = RUDA_MIR_NO_ENUM_CONSTRUCTION;
        RUDA_EXPECT(!ruda_mir_add_enum_construction(
            &function,
            enum_layout,
            2,
            named_arguments,
            1,
            &invalid
        ));
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_ENUM_NEW;
    instruction.target = unit_value;
    instruction.enum_id = enum_layout;
    instruction.enum_construction = unit;
    RUDA_EXPECT(ruda_mir_append_instruction(&function, block, &instruction));
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_CONSTANT_I64;
    instruction.target = first;
    instruction.integer = 41;
    RUDA_EXPECT(ruda_mir_append_instruction(&function, block, &instruction));
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_CONSTANT_BOOL;
    instruction.target = ready;
    instruction.boolean = true;
    RUDA_EXPECT(ruda_mir_append_instruction(&function, block, &instruction));
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_ENUM_NEW;
    instruction.target = positional_value;
    instruction.enum_id = enum_layout;
    instruction.enum_construction = positional;
    RUDA_EXPECT(ruda_mir_append_instruction(&function, block, &instruction));
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_CONSTANT_I64;
    instruction.target = x;
    instruction.integer = 10;
    RUDA_EXPECT(ruda_mir_append_instruction(&function, block, &instruction));
    instruction.target = y;
    instruction.integer = 20;
    RUDA_EXPECT(ruda_mir_append_instruction(&function, block, &instruction));
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_ENUM_NEW;
    instruction.target = named_value;
    instruction.enum_id = enum_layout;
    instruction.enum_construction = named;
    RUDA_EXPECT(ruda_mir_append_instruction(&function, block, &instruction));
    RUDA_EXPECT(ruda_mir_set_terminator(
        &function,
        block,
        ruda_test_mir_return(named_value)
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
    ) == RUDA_MIR_INTERPRET_OK);
    RUDA_EXPECT(result.type == enum_type);
    RUDA_EXPECT(ruda_mir_execution_owns_reference(&execution, result));
    RUDA_EXPECT(execution.enums.length == 3);
    RUDA_EXPECT(ruda_mir_execution_inspect_enum(
        &execution,
        result,
        &selected_variant,
        &payload_count
    ));
    RUDA_EXPECT(selected_variant == 2);
    RUDA_EXPECT(payload_count == 2);
    RUDA_EXPECT(ruda_mir_execution_enum_payload(
        &execution,
        result,
        0,
        &payload
    ));
    RUDA_EXPECT(payload.type == RUDA_TYPE_I64);
    RUDA_EXPECT(payload.as.i64 == 10);
    RUDA_EXPECT(ruda_mir_execution_enum_payload(
        &execution,
        result,
        1,
        &payload
    ));
    RUDA_EXPECT(payload.as.i64 == 20);
    RUDA_EXPECT(!ruda_mir_execution_enum_payload(
        &execution,
        result,
        2,
        &payload
    ));
    ruda_mir_execution_dispose(&execution);

    {
        RudaMirEnumArgument *argument = ruda_vector_at(
            &function.enum_arguments,
            2
        );
        RudaTypeId *target_type = ruda_vector_at(
            &function.register_types,
            named_value
        );
        RUDA_EXPECT(argument != NULL);
        RUDA_EXPECT(target_type != NULL);
        if (argument != NULL) {
            argument->register_id = uninitialized;
            RUDA_EXPECT(
                ruda_mir_verify(&function)
                    == RUDA_MIR_VERIFY_UNINITIALIZED_REGISTER
            );
            argument->register_id = x;
        }
        if (target_type != NULL) {
            *target_type = RUDA_TYPE_BOOL;
            RUDA_EXPECT(
                ruda_mir_verify(&function) == RUDA_MIR_VERIFY_TYPE_MISMATCH
            );
            *target_type = enum_type;
        }
        RUDA_EXPECT(ruda_mir_verify(&function) == RUDA_MIR_VERIFY_OK);
    }

    execution_options = ruda_mir_execution_default_options();
    execution_options.max_objects = 2;
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
    RUDA_EXPECT(execution.enums.length == 2);
    ruda_mir_execution_dispose(&execution);

    {
        size_t failure_offset = 0;
        bool reached_success = false;
        for (failure_offset = 0; failure_offset < 16; failure_offset += 1) {
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
}

static void ruda_test_mir_enum_projection(RudaSymbolId symbol) {
    const RudaTypeId enum_type = RUDA_TYPE_FIRST_DYNAMIC + 21;
    RudaMirEnumVariant variants[3];
    RudaMirEnumPayload payloads[4];
    RudaMirFunction constructor;
    RudaMirFunction projection;
    RudaMirEnumArgument arguments[2];
    RudaMirEnumId constructor_layout = RUDA_MIR_NO_ENUM;
    RudaMirEnumId projection_layout = RUDA_MIR_NO_ENUM;
    RudaMirEnumConstructionId construction = RUDA_MIR_NO_ENUM_CONSTRUCTION;
    RudaMirRegister left = RUDA_MIR_NO_REGISTER;
    RudaMirRegister right = RUDA_MIR_NO_REGISTER;
    RudaMirRegister value = RUDA_MIR_NO_REGISTER;
    RudaMirRegister matched = RUDA_MIR_NO_REGISTER;
    RudaMirRegister guarded = RUDA_MIR_NO_REGISTER;
    RudaMirRegister projected = RUDA_MIR_NO_REGISTER;
    RudaMirBlockId constructor_block = RUDA_MIR_NO_BLOCK;
    RudaMirBlockId projection_block = RUDA_MIR_NO_BLOCK;
    RudaMirInstruction instruction;
    RudaMirExecution execution;
    RudaMirInterpretOptions options = ruda_mir_interpret_default_options();
    RudaMirValue enum_value;
    RudaMirValue result;
    RudaMirValue forged;

    variants[0].kind = RUDA_MIR_ENUM_VARIANT_UNIT;
    variants[0].payload_start = 0;
    variants[0].payload_count = 0;
    variants[1].kind = RUDA_MIR_ENUM_VARIANT_POSITIONAL;
    variants[1].payload_start = 0;
    variants[1].payload_count = 2;
    variants[2].kind = RUDA_MIR_ENUM_VARIANT_NAMED;
    variants[2].payload_start = 2;
    variants[2].payload_count = 2;
    payloads[0].type = RUDA_TYPE_I64;
    payloads[1].type = RUDA_TYPE_BOOL;
    payloads[2].type = RUDA_TYPE_I64;
    payloads[3].type = RUDA_TYPE_I64;

    RUDA_EXPECT(ruda_mir_function_init(
        &constructor,
        ruda_system_allocator(),
        symbol,
        NULL,
        0,
        enum_type,
        ruda_mir_function_default_options()
    ));
    RUDA_EXPECT(ruda_mir_add_register(&constructor, RUDA_TYPE_I64, &left));
    RUDA_EXPECT(ruda_mir_add_register(&constructor, RUDA_TYPE_I64, &right));
    RUDA_EXPECT(ruda_mir_add_register(&constructor, enum_type, &value));
    RUDA_EXPECT(ruda_mir_add_block(&constructor, &constructor_block));
    RUDA_EXPECT(ruda_mir_add_enum(
        &constructor,
        enum_type,
        variants,
        3,
        payloads,
        4,
        &constructor_layout
    ));
    arguments[0].register_id = left;
    arguments[0].type = RUDA_TYPE_I64;
    arguments[1].register_id = right;
    arguments[1].type = RUDA_TYPE_I64;
    RUDA_EXPECT(ruda_mir_add_enum_construction(
        &constructor,
        constructor_layout,
        2,
        arguments,
        2,
        &construction
    ));
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_CONSTANT_I64;
    instruction.target = left;
    instruction.integer = 10;
    RUDA_EXPECT(ruda_mir_append_instruction(
        &constructor,
        constructor_block,
        &instruction
    ));
    instruction.target = right;
    instruction.integer = 20;
    RUDA_EXPECT(ruda_mir_append_instruction(
        &constructor,
        constructor_block,
        &instruction
    ));
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_ENUM_NEW;
    instruction.target = value;
    instruction.enum_id = constructor_layout;
    instruction.enum_construction = construction;
    RUDA_EXPECT(ruda_mir_append_instruction(
        &constructor,
        constructor_block,
        &instruction
    ));
    RUDA_EXPECT(ruda_mir_set_terminator(
        &constructor,
        constructor_block,
        ruda_test_mir_return(value)
    ));
    RUDA_EXPECT(ruda_mir_verify(&constructor) == RUDA_MIR_VERIFY_OK);

    RUDA_EXPECT(ruda_mir_function_init(
        &projection,
        ruda_system_allocator(),
        symbol,
        &enum_type,
        1,
        RUDA_TYPE_I64,
        ruda_mir_function_default_options()
    ));
    RUDA_EXPECT(ruda_mir_add_register(
        &projection,
        RUDA_TYPE_BOOL,
        &matched
    ));
    RUDA_EXPECT(ruda_mir_add_register(
        &projection,
        enum_type,
        &guarded
    ));
    RUDA_EXPECT(ruda_mir_add_register(
        &projection,
        RUDA_TYPE_I64,
        &projected
    ));
    RUDA_EXPECT(ruda_mir_add_block(&projection, &projection_block));
    RUDA_EXPECT(ruda_mir_add_enum(
        &projection,
        enum_type,
        variants,
        3,
        payloads,
        4,
        &projection_layout
    ));
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_ENUM_IS_VARIANT;
    instruction.target = matched;
    instruction.first = 0;
    instruction.second = RUDA_MIR_NO_REGISTER;
    instruction.enum_id = projection_layout;
    instruction.variant = 2;
    RUDA_EXPECT(ruda_mir_append_instruction(
        &projection,
        projection_block,
        &instruction
    ));
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_ENUM_REQUIRE_VARIANT;
    instruction.target = guarded;
    instruction.first = 0;
    instruction.second = RUDA_MIR_NO_REGISTER;
    instruction.enum_id = projection_layout;
    instruction.variant = 2;
    RUDA_EXPECT(ruda_mir_append_instruction(
        &projection,
        projection_block,
        &instruction
    ));
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_ENUM_GET_PAYLOAD;
    instruction.target = projected;
    instruction.first = guarded;
    instruction.second = RUDA_MIR_NO_REGISTER;
    instruction.enum_id = projection_layout;
    instruction.variant = 2;
    instruction.field = 1;
    RUDA_EXPECT(ruda_mir_append_instruction(
        &projection,
        projection_block,
        &instruction
    ));
    RUDA_EXPECT(ruda_mir_set_terminator(
        &projection,
        projection_block,
        ruda_test_mir_return(projected)
    ));
    RUDA_EXPECT(ruda_mir_verify(&projection) == RUDA_MIR_VERIFY_OK);

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
        &enum_value
    ) == RUDA_MIR_INTERPRET_OK);
    RUDA_EXPECT(ruda_mir_interpret(
        &projection,
        &enum_value,
        1,
        ruda_system_allocator(),
        options,
        &result
    ) == RUDA_MIR_INTERPRET_OK);
    RUDA_EXPECT(result.type == RUDA_TYPE_I64 && result.as.i64 == 20);
    forged = enum_value;
    forged.as.reference = RUDA_MIR_INVALID_REFERENCE;
    RUDA_EXPECT(ruda_mir_interpret(
        &projection,
        &forged,
        1,
        ruda_system_allocator(),
        options,
        &result
    ) == RUDA_MIR_INTERPRET_INVALID_REFERENCE);
    {
        RudaMirBlock *stored_block = ruda_vector_at(
            &projection.blocks,
            projection_block
        );
        RudaMirInstruction *require = stored_block != NULL
            ? ruda_vector_at(&stored_block->instructions, 1)
            : NULL;
        RudaMirInstruction *get = stored_block != NULL
            ? ruda_vector_at(&stored_block->instructions, 2)
            : NULL;
        RUDA_EXPECT(require != NULL && get != NULL);
        if (require != NULL) {
            require->variant = 1;
            RUDA_EXPECT(ruda_mir_verify(&projection) == RUDA_MIR_VERIFY_OK);
            RUDA_EXPECT(ruda_mir_interpret(
                &projection,
                &enum_value,
                1,
                ruda_system_allocator(),
                options,
                &result
            ) == RUDA_MIR_INTERPRET_INVALID_MIR);
            require->variant = 2;
        }
        if (get != NULL) {
            get->field = 2;
            RUDA_EXPECT(
                ruda_mir_verify(&projection)
                    == RUDA_MIR_VERIFY_INVALID_ARGUMENT
            );
            get->field = 1;
        }
        {
            RudaMirEnumPayload *layout_payload = ruda_vector_at(
                &projection.enum_payloads,
                3
            );
            RudaTypeId *projected_type = ruda_vector_at(
                &projection.register_types,
                projected
            );
            RUDA_EXPECT(layout_payload != NULL && projected_type != NULL);
            if (layout_payload != NULL && projected_type != NULL) {
                layout_payload->type = RUDA_TYPE_BOOL;
                *projected_type = RUDA_TYPE_BOOL;
                projection.result_type = RUDA_TYPE_BOOL;
                RUDA_EXPECT(
                    ruda_mir_verify(&projection) == RUDA_MIR_VERIFY_OK
                );
                RUDA_EXPECT(ruda_mir_interpret(
                    &projection,
                    &enum_value,
                    1,
                    ruda_system_allocator(),
                    options,
                    &result
                ) == RUDA_MIR_INTERPRET_INVALID_MIR);
                layout_payload->type = RUDA_TYPE_I64;
                *projected_type = RUDA_TYPE_I64;
                projection.result_type = RUDA_TYPE_I64;
            }
        }
        RUDA_EXPECT(ruda_mir_verify(&projection) == RUDA_MIR_VERIFY_OK);
    }
    ruda_mir_execution_dispose(&execution);
    ruda_mir_function_dispose(&projection);
    ruda_mir_function_dispose(&constructor);
}

void ruda_test_mir_enums_projection(RudaSymbolId symbol) {
    ruda_test_mir_enums(symbol);
    ruda_test_mir_enum_projection(symbol);
}
