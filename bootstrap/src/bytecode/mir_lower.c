#include "ruda/bytecode/mir_lower.h"

#include "mir_lower_internal.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

RudaMirBytecodeLowerOptions ruda_mir_bytecode_lower_default_options(void) {
    RudaMirBytecodeLowerOptions options;
    options.source = 0;
    options.source_count = 1;
    options.function_options = ruda_bytecode_function_default_options();
    return options;
}

RudaMirBytecodeProgramLowerOptions
ruda_mir_bytecode_program_lower_default_options(void) {
    RudaMirBytecodeProgramLowerOptions options;
    options.source = 0;
    options.source_count = 1;
    options.program_options = ruda_bytecode_program_default_options();
    return options;
}

static bool ruda_mir_bytecode_options_valid(
    RudaAllocator allocator,
    RudaMirBytecodeLowerOptions options
) {
    return allocator.allocate != NULL && allocator.resize != NULL
        && allocator.deallocate != NULL && options.source_count != 0
        && options.source < options.source_count
        && options.source_count <= options.function_options.max_source_files
        && options.function_options.max_registers != 0
        && options.function_options.max_blocks != 0
        && options.function_options.max_instructions != 0
        && options.function_options.max_calls != 0
        && options.function_options.max_call_arguments != 0
        && options.function_options.max_string_constants != 0
        && options.function_options.max_string_bytes != 0
        && options.function_options.max_array_constructions != 0
        && options.function_options.max_array_arguments != 0
        && options.function_options.max_source_files != 0
        && options.function_options.max_encoded_bytes >= 8U
        && options.function_options.max_verify_work != 0;
}

static bool ruda_mir_bytecode_span(
    RudaMirSpan input,
    uint64_t source,
    RudaBytecodeSpan *output
) {
#if SIZE_MAX > UINT64_MAX
    if (input.start > UINT64_MAX || input.end > UINT64_MAX) {
        return false;
    }
#endif
    output->source = source;
    output->start = (uint64_t)input.start;
    output->end = (uint64_t)input.end;
    return true;
}

static RudaMirBytecodeLowerStatus ruda_mir_bytecode_instruction(
    const RudaMirFunction *function,
    const RudaMirInstruction *input,
    uint64_t source,
    RudaBytecodeInstruction *output
) {
    memset(output, 0, sizeof(*output));
    if (!ruda_mir_bytecode_span(input->span, source, &output->span)) {
        return RUDA_MIR_BYTECODE_LOWER_INVALID_ARGUMENT;
    }
    output->operands[0].unsigned_value = input->target;
    switch (input->kind) {
        case RUDA_MIR_CONSTANT_I64:
            output->opcode = RUDA_OPCODE_CONST_I64;
            output->operands[1].signed_value = input->integer;
            break;
        case RUDA_MIR_CONSTANT_BOOL:
            output->opcode = RUDA_OPCODE_CONST_BOOL;
            output->operands[1].unsigned_value = input->boolean ? 1U : 0U;
            break;
        case RUDA_MIR_CONSTANT_F64:
            output->opcode = RUDA_OPCODE_CONST_F64;
            output->operands[1].unsigned_value = input->f64_bits;
            break;
        case RUDA_MIR_CONSTANT_STRING:
            output->opcode = RUDA_OPCODE_CONST_STRING;
            output->operands[1].unsigned_value = input->string_constant;
            break;
        case RUDA_MIR_COPY:
            output->opcode = RUDA_OPCODE_COPY;
            output->operands[1].unsigned_value = input->first;
            break;
        case RUDA_MIR_ADD_CHECKED_I64:
            output->opcode = RUDA_OPCODE_I64_ADD_CHECKED;
            output->operands[1].unsigned_value = input->first;
            output->operands[2].unsigned_value = input->second;
            break;
        case RUDA_MIR_SUBTRACT_CHECKED_I64:
            output->opcode = RUDA_OPCODE_I64_SUBTRACT_CHECKED;
            output->operands[1].unsigned_value = input->first;
            output->operands[2].unsigned_value = input->second;
            break;
        case RUDA_MIR_MULTIPLY_CHECKED_I64:
            output->opcode = RUDA_OPCODE_I64_MULTIPLY_CHECKED;
            output->operands[1].unsigned_value = input->first;
            output->operands[2].unsigned_value = input->second;
            break;
        case RUDA_MIR_DIVIDE_CHECKED_I64:
            output->opcode = RUDA_OPCODE_I64_DIVIDE_CHECKED;
            output->operands[1].unsigned_value = input->first;
            output->operands[2].unsigned_value = input->second;
            break;
        case RUDA_MIR_REMAINDER_CHECKED_I64:
            output->opcode = RUDA_OPCODE_I64_REMAINDER_CHECKED;
            output->operands[1].unsigned_value = input->first;
            output->operands[2].unsigned_value = input->second;
            break;
        case RUDA_MIR_NEGATE_CHECKED_I64:
            output->opcode = RUDA_OPCODE_I64_NEGATE_CHECKED;
            output->operands[1].unsigned_value = input->first;
            break;
        case RUDA_MIR_ADD_F64:
            output->opcode = RUDA_OPCODE_F64_ADD;
            output->operands[1].unsigned_value = input->first;
            output->operands[2].unsigned_value = input->second;
            break;
        case RUDA_MIR_SUBTRACT_F64:
            output->opcode = RUDA_OPCODE_F64_SUBTRACT;
            output->operands[1].unsigned_value = input->first;
            output->operands[2].unsigned_value = input->second;
            break;
        case RUDA_MIR_MULTIPLY_F64:
            output->opcode = RUDA_OPCODE_F64_MULTIPLY;
            output->operands[1].unsigned_value = input->first;
            output->operands[2].unsigned_value = input->second;
            break;
        case RUDA_MIR_DIVIDE_F64:
            output->opcode = RUDA_OPCODE_F64_DIVIDE;
            output->operands[1].unsigned_value = input->first;
            output->operands[2].unsigned_value = input->second;
            break;
        case RUDA_MIR_NEGATE_F64:
            output->opcode = RUDA_OPCODE_F64_NEGATE;
            output->operands[1].unsigned_value = input->first;
            break;
        case RUDA_MIR_NOT_BOOL:
            output->opcode = RUDA_OPCODE_BOOL_NOT;
            output->operands[1].unsigned_value = input->first;
            break;
        case RUDA_MIR_BITWISE_NOT_I64:
            output->opcode = RUDA_OPCODE_I64_BITWISE_NOT;
            output->operands[1].unsigned_value = input->first;
            break;
        case RUDA_MIR_BITWISE_AND_I64:
            output->opcode = RUDA_OPCODE_I64_BITWISE_AND;
            output->operands[1].unsigned_value = input->first;
            output->operands[2].unsigned_value = input->second;
            break;
        case RUDA_MIR_BITWISE_XOR_I64:
            output->opcode = RUDA_OPCODE_I64_BITWISE_XOR;
            output->operands[1].unsigned_value = input->first;
            output->operands[2].unsigned_value = input->second;
            break;
        case RUDA_MIR_BITWISE_OR_I64:
            output->opcode = RUDA_OPCODE_I64_BITWISE_OR;
            output->operands[1].unsigned_value = input->first;
            output->operands[2].unsigned_value = input->second;
            break;
        case RUDA_MIR_SHIFT_LEFT_CHECKED_I64:
            output->opcode = RUDA_OPCODE_I64_SHIFT_LEFT_CHECKED;
            output->operands[1].unsigned_value = input->first;
            output->operands[2].unsigned_value = input->second;
            break;
        case RUDA_MIR_SHIFT_RIGHT_I64:
            output->opcode = RUDA_OPCODE_I64_SHIFT_RIGHT;
            output->operands[1].unsigned_value = input->first;
            output->operands[2].unsigned_value = input->second;
            break;
        case RUDA_MIR_EQUAL_I64:
            output->opcode = RUDA_OPCODE_I64_EQUAL;
            output->operands[1].unsigned_value = input->first;
            output->operands[2].unsigned_value = input->second;
            break;
        case RUDA_MIR_EQUAL_BOOL:
            output->opcode = RUDA_OPCODE_BOOL_EQUAL;
            output->operands[1].unsigned_value = input->first;
            output->operands[2].unsigned_value = input->second;
            break;
        case RUDA_MIR_EQUAL_STRING:
            output->opcode = RUDA_OPCODE_STRING_EQUAL;
            output->operands[1].unsigned_value = input->first;
            output->operands[2].unsigned_value = input->second;
            break;
        case RUDA_MIR_EQUAL_F64:
            output->opcode = RUDA_OPCODE_F64_EQUAL;
            output->operands[1].unsigned_value = input->first;
            output->operands[2].unsigned_value = input->second;
            break;
        case RUDA_MIR_LESS_I64:
            output->opcode = RUDA_OPCODE_I64_LESS;
            output->operands[1].unsigned_value = input->first;
            output->operands[2].unsigned_value = input->second;
            break;
        case RUDA_MIR_LESS_EQUAL_I64:
            output->opcode = RUDA_OPCODE_I64_LESS_EQUAL;
            output->operands[1].unsigned_value = input->first;
            output->operands[2].unsigned_value = input->second;
            break;
        case RUDA_MIR_LESS_F64:
            output->opcode = RUDA_OPCODE_F64_LESS;
            output->operands[1].unsigned_value = input->first;
            output->operands[2].unsigned_value = input->second;
            break;
        case RUDA_MIR_LESS_EQUAL_F64:
            output->opcode = RUDA_OPCODE_F64_LESS_EQUAL;
            output->operands[1].unsigned_value = input->first;
            output->operands[2].unsigned_value = input->second;
            break;
        case RUDA_MIR_RECORD_NEW:
            output->opcode = RUDA_OPCODE_RECORD_NEW;
            output->operands[1].unsigned_value = input->record_construction;
            break;
        case RUDA_MIR_RECORD_GET:
            output->opcode = RUDA_OPCODE_RECORD_GET;
            output->operands[1].unsigned_value = input->first;
            output->operands[2].signed_value = (int64_t)input->field;
            break;
        case RUDA_MIR_RECORD_SET:
            output->opcode = RUDA_OPCODE_RECORD_SET;
            output->operands[0].unsigned_value = input->second;
            output->operands[1].unsigned_value = input->first;
            output->operands[2].signed_value = (int64_t)input->field;
            break;
        case RUDA_MIR_RECORD_IDENTITY_SAME:
            output->opcode = RUDA_OPCODE_RECORD_IDENTITY_SAME;
            output->operands[1].unsigned_value = input->first;
            output->operands[2].unsigned_value = input->second;
            break;
        case RUDA_MIR_ENUM_NEW:
            output->opcode = RUDA_OPCODE_ENUM_NEW;
            output->operands[1].unsigned_value = input->enum_construction;
            break;
        case RUDA_MIR_ENUM_IS_VARIANT:
            output->opcode = RUDA_OPCODE_ENUM_IS_VARIANT;
            output->operands[1].unsigned_value = input->first;
            output->operands[2].signed_value = (int64_t)input->variant;
            break;
        case RUDA_MIR_ENUM_REQUIRE_VARIANT:
            output->opcode = RUDA_OPCODE_ENUM_REQUIRE_VARIANT;
            output->operands[1].unsigned_value = input->first;
            output->operands[2].signed_value = (int64_t)input->variant;
            break;
        case RUDA_MIR_ENUM_GET_PAYLOAD:
            output->opcode = RUDA_OPCODE_ENUM_GET_PAYLOAD;
            output->operands[1].unsigned_value = input->first;
            output->operands[2].signed_value = (int64_t)(
                ((uint64_t)input->variant << 32) | (uint64_t)input->field
            );
            break;
        case RUDA_MIR_ARRAY_NEW:
            output->opcode = RUDA_OPCODE_ARRAY_NEW;
            output->operands[1].unsigned_value = input->array_construction;
            break;
        case RUDA_MIR_ARRAY_LENGTH:
            output->opcode = RUDA_OPCODE_ARRAY_LENGTH;
            output->operands[1].unsigned_value = input->first;
            break;
        case RUDA_MIR_ARRAY_REQUIRE_INDEX:
            output->opcode = RUDA_OPCODE_ARRAY_REQUIRE_INDEX;
            output->operands[1].unsigned_value = input->first;
            output->operands[2].unsigned_value = input->second;
            break;
        case RUDA_MIR_ARRAY_GET:
            output->opcode = RUDA_OPCODE_ARRAY_GET;
            output->operands[1].unsigned_value = input->first;
            output->operands[2].unsigned_value = input->second;
            break;
        case RUDA_MIR_ARRAY_SET:
            output->opcode = RUDA_OPCODE_ARRAY_SET;
            output->operands[0].unsigned_value = input->first;
            output->operands[1].unsigned_value = input->second;
            output->operands[2].unsigned_value = input->third;
            break;
        case RUDA_MIR_CALL: {
            const RudaMirCall *call = input->call < function->calls.length
                ? ruda_vector_at_const(&function->calls, input->call)
                : NULL;
            if (call == NULL) {
                return RUDA_MIR_BYTECODE_LOWER_INVALID_MIR;
            }
            if (call->result_type == RUDA_TYPE_VOID) {
                output->opcode = RUDA_OPCODE_CALL_DIRECT_VOID;
                output->operands[0].unsigned_value = input->call;
            } else {
                output->opcode = RUDA_OPCODE_CALL_DIRECT;
                output->operands[0].unsigned_value = input->target;
                output->operands[1].unsigned_value = input->call;
            }
            break;
        }
        case RUDA_MIR_INTERFACE_NEW:
            output->opcode = RUDA_OPCODE_INTERFACE_NEW;
            output->operands[1].unsigned_value = input->interface_construction;
            break;
        case RUDA_MIR_CALL_INTERFACE:
            output->opcode = RUDA_OPCODE_CALL_INTERFACE;
            output->operands[0].unsigned_value = input->target;
            output->operands[1].unsigned_value = input->call;
            break;
    }
    return RUDA_MIR_BYTECODE_LOWER_OK;
}

static RudaMirBytecodeLowerStatus ruda_mir_bytecode_terminator(
    const RudaMirFunction *function,
    const RudaMirTerminator *input,
    uint64_t source,
    RudaBytecodeInstruction *output
) {
    memset(output, 0, sizeof(*output));
    if (!ruda_mir_bytecode_span(input->span, source, &output->span)) {
        return RUDA_MIR_BYTECODE_LOWER_INVALID_ARGUMENT;
    }
    switch (input->kind) {
        case RUDA_MIR_RETURN:
            if (function->result_type == RUDA_TYPE_VOID) {
                output->opcode = RUDA_OPCODE_RETURN_VOID;
            } else {
                output->opcode = RUDA_OPCODE_RETURN;
                output->operands[0].unsigned_value = input->value;
            }
            return RUDA_MIR_BYTECODE_LOWER_OK;
        case RUDA_MIR_GOTO:
            output->opcode = RUDA_OPCODE_JUMP;
            output->operands[0].unsigned_value = input->target;
            return RUDA_MIR_BYTECODE_LOWER_OK;
        case RUDA_MIR_BRANCH:
            output->opcode = RUDA_OPCODE_BRANCH_BOOL;
            output->operands[0].unsigned_value = input->value;
            output->operands[1].unsigned_value = input->target;
            output->operands[2].unsigned_value = input->otherwise;
            return RUDA_MIR_BYTECODE_LOWER_OK;
        case RUDA_MIR_PANIC:
            output->opcode = RUDA_OPCODE_PANIC_STRING;
            output->operands[0].unsigned_value = input->value;
            return RUDA_MIR_BYTECODE_LOWER_OK;
        case RUDA_MIR_CALL_NEVER:
            output->opcode = RUDA_OPCODE_CALL_DIRECT_NEVER;
            output->operands[0].unsigned_value = input->call;
            return RUDA_MIR_BYTECODE_LOWER_OK;
        case RUDA_MIR_TERMINATOR_UNSET:
            return RUDA_MIR_BYTECODE_LOWER_INVALID_MIR;
    }
    return RUDA_MIR_BYTECODE_LOWER_INVALID_MIR;
}

RudaMirBytecodeLowerStatus ruda_mir_lower_bytecode_function(
    const RudaMirFunction *input,
    RudaAllocator allocator,
    RudaMirBytecodeLowerOptions options,
    RudaBytecodeFunction *output
) {
    RudaBytecodeFunction lowered;
    RudaBytecodeType result_type;
    RudaBytecodeType *parameter_types = NULL;
    size_t parameter_bytes = 0;
    size_t lowered_instruction_count = 0;
    size_t array_set_count = 0;
    size_t index = 0;
    RudaMirBytecodeLowerStatus status = RUDA_MIR_BYTECODE_LOWER_OK;
    if (output == NULL) {
        return RUDA_MIR_BYTECODE_LOWER_INVALID_ARGUMENT;
    }
    if (output->initialized) {
        ruda_bytecode_function_dispose(output);
    } else {
        memset(output, 0, sizeof(*output));
    }
    memset(&lowered, 0, sizeof(lowered));
    if (input == NULL || !input->initialized
        || !ruda_mir_bytecode_options_valid(allocator, options)) {
        return RUDA_MIR_BYTECODE_LOWER_INVALID_ARGUMENT;
    }
    {
        RudaMirVerifyStatus verify_status = ruda_mir_verify(input);
        if (verify_status == RUDA_MIR_VERIFY_ALLOCATION_FAILED) {
            return RUDA_MIR_BYTECODE_LOWER_ALLOCATION_FAILED;
        }
        if (verify_status == RUDA_MIR_VERIFY_LIMIT_REACHED) {
            return RUDA_MIR_BYTECODE_LOWER_LIMIT_REACHED;
        }
        if (verify_status != RUDA_MIR_VERIFY_OK) {
            return RUDA_MIR_BYTECODE_LOWER_INVALID_MIR;
        }
    }
    if (!ruda_mir_bytecode_type(
            input,
            input->result_type,
            true,
            true,
            &result_type
        )) {
        return RUDA_MIR_BYTECODE_LOWER_UNSUPPORTED_TYPE;
    }
    for (index = 0; index < input->register_types.length; index += 1) {
        const RudaTypeId *input_type = ruda_vector_at_const(
            &input->register_types,
            index
        );
        RudaBytecodeType ignored;
        if (input_type == NULL
            || !ruda_mir_bytecode_type(
                input,
                *input_type,
                index >= input->parameter_count,
                false,
                &ignored
            )) {
            return RUDA_MIR_BYTECODE_LOWER_UNSUPPORTED_TYPE;
        }
    }
    for (index = 0; index < input->blocks.length; index += 1) {
        const RudaMirBlock *block = ruda_vector_at_const(&input->blocks, index);
        size_t instruction_index = 0;
        if (block == NULL) {
            return RUDA_MIR_BYTECODE_LOWER_INVALID_MIR;
        }
        for (instruction_index = 0;
            instruction_index < block->instructions.length;
            instruction_index += 1) {
            const RudaMirInstruction *instruction = ruda_vector_at_const(
                &block->instructions, instruction_index
            );
            if (instruction != NULL
                && (instruction->kind == RUDA_MIR_ARRAY_SET
                    || instruction->kind == RUDA_MIR_RECORD_SET)
                && !ruda_size_add(array_set_count, 1, &array_set_count)) {
                return RUDA_MIR_BYTECODE_LOWER_LIMIT_REACHED;
            }
        }
    }
    if (!ruda_size_add(
            input->instruction_count,
            input->blocks.length,
            &lowered_instruction_count
        ) || !ruda_size_add(
            lowered_instruction_count, array_set_count,
            &lowered_instruction_count
        ) || input->register_types.length > options.function_options.max_registers
        || input->blocks.length > options.function_options.max_blocks
        || lowered_instruction_count > options.function_options.max_instructions
        || input->calls.length > options.function_options.max_calls
        || input->call_arguments.length
            > options.function_options.max_call_arguments
        || input->string_constants.length
            > options.function_options.max_string_constants
        || input->string_bytes > options.function_options.max_string_bytes
        || input->array_constructions.length
            > options.function_options.max_array_constructions
        || input->array_arguments.length
            > options.function_options.max_array_arguments) {
        return RUDA_MIR_BYTECODE_LOWER_LIMIT_REACHED;
    }
    if (!ruda_size_multiply(
            input->parameter_count,
            sizeof(RudaBytecodeType),
            &parameter_bytes
        )) {
        return RUDA_MIR_BYTECODE_LOWER_INVALID_ARGUMENT;
    }
    if (parameter_bytes != 0) {
        parameter_types = ruda_allocate(&allocator, parameter_bytes);
        if (parameter_types == NULL) {
            return RUDA_MIR_BYTECODE_LOWER_ALLOCATION_FAILED;
        }
        for (index = 0; index < input->parameter_count; index += 1) {
            const RudaTypeId *input_type = ruda_vector_at_const(
                &input->register_types,
                index
            );
            if (!ruda_mir_bytecode_type(
                    input,
                    *input_type,
                    false,
                    false,
                    &parameter_types[index]
                )) {
                status = RUDA_MIR_BYTECODE_LOWER_INVALID_MIR;
                goto cleanup;
            }
        }
    }
    if (!ruda_bytecode_function_init(
            &lowered,
            allocator,
            result_type,
            options.source_count,
            parameter_types,
            input->parameter_count,
            options.function_options
        )) {
        status = RUDA_MIR_BYTECODE_LOWER_ALLOCATION_FAILED;
        goto cleanup;
    }
    lowered.result_layout = ruda_mir_bytecode_layout_for_type(
        input,
        input->result_type
    );
    ruda_deallocate(&allocator, parameter_types, parameter_bytes);
    parameter_types = NULL;
    parameter_bytes = 0;
    for (index = 0; index < input->parameter_count; index += 1) {
        const RudaTypeId *input_type = ruda_vector_at_const(
            &input->register_types, index
        );
        if (input_type == NULL || !ruda_bytecode_set_register_layout(
                &lowered,
                (uint32_t)index,
                ruda_mir_bytecode_layout_for_type(input, *input_type)
            )) {
            status = RUDA_MIR_BYTECODE_LOWER_INVALID_MIR;
            goto cleanup;
        }
    }
    for (index = 0; index < input->string_constants.length; index += 1) {
        const RudaMirStringConstant *constant = ruda_vector_at_const(
            &input->string_constants,
            index
        );
        uint32_t string_id = UINT32_MAX;
        if (constant == NULL || !ruda_bytecode_add_string_constant(
                &lowered,
                constant->value.data,
                constant->value.length,
                &string_id
            )) {
            status = RUDA_MIR_BYTECODE_LOWER_ALLOCATION_FAILED;
            goto cleanup;
        }
        if ((size_t)string_id != index) {
            status = RUDA_MIR_BYTECODE_LOWER_INVALID_MIR;
            goto cleanup;
        }
    }
    for (index = input->parameter_count;
        index < input->register_types.length; index += 1) {
        const RudaTypeId *input_type = ruda_vector_at_const(
            &input->register_types,
            index
        );
        RudaBytecodeType type;
        uint32_t register_id = UINT32_MAX;
        if (!ruda_mir_bytecode_type(
                input, *input_type, true, false, &type
            )) {
            status = RUDA_MIR_BYTECODE_LOWER_INVALID_MIR;
            goto cleanup;
        }
        if (!ruda_bytecode_add_register(&lowered, type, &register_id)) {
            status = RUDA_MIR_BYTECODE_LOWER_ALLOCATION_FAILED;
            goto cleanup;
        }
        if (register_id != index) {
            status = RUDA_MIR_BYTECODE_LOWER_INVALID_MIR;
            goto cleanup;
        }
        if (!ruda_bytecode_set_register_layout(
                &lowered,
                register_id,
                ruda_mir_bytecode_layout_for_type(input, *input_type)
            )) {
            status = RUDA_MIR_BYTECODE_LOWER_ALLOCATION_FAILED;
            goto cleanup;
        }
    }
    status = ruda_mir_bytecode_lower_nominals(input, &lowered, allocator);
    if (status != RUDA_MIR_BYTECODE_LOWER_OK) {
        goto cleanup;
    }
    for (index = 0; index < input->array_constructions.length; index += 1) {
        const RudaMirArrayConstruction *construction = ruda_vector_at_const(
            &input->array_constructions, index
        );
        const RudaMirArray *array = construction != NULL
                && construction->array < input->arrays.length
            ? ruda_vector_at_const(&input->arrays, construction->array)
            : NULL;
        RudaBytecodeArrayArgument *arguments = NULL;
        size_t argument_bytes = 0;
        size_t argument_index = 0;
        RudaBytecodeType array_type;
        RudaBytecodeArrayConstructionId construction_id =
            RUDA_BYTECODE_NO_ARRAY_CONSTRUCTION;
        if (construction == NULL || array == NULL
            || !ruda_mir_bytecode_type(
                input, array->type, false, false, &array_type
            ) || !ruda_size_multiply(
                construction->argument_count,
                sizeof(RudaBytecodeArrayArgument),
                &argument_bytes
            )) {
            status = array != NULL
                ? RUDA_MIR_BYTECODE_LOWER_UNSUPPORTED_TYPE
                : RUDA_MIR_BYTECODE_LOWER_INVALID_MIR;
            goto cleanup;
        }
        if (argument_bytes != 0) {
            arguments = ruda_allocate(&allocator, argument_bytes);
            if (arguments == NULL) {
                status = RUDA_MIR_BYTECODE_LOWER_ALLOCATION_FAILED;
                goto cleanup;
            }
        }
        for (argument_index = 0;
            argument_index < construction->argument_count;
            argument_index += 1) {
            const RudaMirArrayArgument *argument = ruda_vector_at_const(
                &input->array_arguments,
                construction->argument_start + argument_index
            );
            if (argument == NULL || arguments == NULL
                || !ruda_mir_bytecode_type(
                    input, argument->type, false, false,
                    &arguments[argument_index].type
                )) {
                status = RUDA_MIR_BYTECODE_LOWER_INVALID_MIR;
                break;
            }
            arguments[argument_index].register_id = argument->register_id;
        }
        if (status == RUDA_MIR_BYTECODE_LOWER_OK
            && !ruda_bytecode_add_array_construction(
                &lowered, array_type, arguments,
                construction->argument_count, &construction_id
            )) {
            status = RUDA_MIR_BYTECODE_LOWER_ALLOCATION_FAILED;
        }
        ruda_deallocate(&allocator, arguments, argument_bytes);
        if (status != RUDA_MIR_BYTECODE_LOWER_OK) {
            goto cleanup;
        }
        if ((size_t)construction_id != index) {
            status = RUDA_MIR_BYTECODE_LOWER_INVALID_MIR;
            goto cleanup;
        }
    }
    for (index = 0; index < input->calls.length; index += 1) {
        const RudaMirCall *call = ruda_vector_at_const(&input->calls, index);
        RudaBytecodeCallArgument *arguments = NULL;
        size_t argument_bytes = 0;
        size_t argument_index = 0;
        RudaBytecodeType call_result_type;
        RudaBytecodeCallId call_id = RUDA_BYTECODE_NO_CALL;
        if (call == NULL
            || !ruda_mir_bytecode_type(
                input,
                call->result_type,
                true,
                true,
                &call_result_type
            ) || !ruda_size_multiply(
                call->argument_count,
                sizeof(RudaBytecodeCallArgument),
                &argument_bytes
            )) {
            status = RUDA_MIR_BYTECODE_LOWER_INVALID_MIR;
            goto cleanup;
        }
        if (argument_bytes != 0) {
            arguments = ruda_allocate(&allocator, argument_bytes);
            if (arguments == NULL) {
                status = RUDA_MIR_BYTECODE_LOWER_ALLOCATION_FAILED;
                goto cleanup;
            }
        }
        for (argument_index = 0;
            argument_index < call->argument_count;
            argument_index += 1) {
            const RudaMirCallArgument *argument = ruda_vector_at_const(
                &input->call_arguments,
                call->argument_start + argument_index
            );
            if (arguments == NULL || argument == NULL
                || !ruda_mir_bytecode_type(
                    input,
                    argument->type,
                    false,
                    false,
                    &arguments[argument_index].type
                )) {
                status = RUDA_MIR_BYTECODE_LOWER_INVALID_MIR;
                break;
            }
            arguments[argument_index].register_id = argument->register_id;
            arguments[argument_index].layout = ruda_mir_bytecode_layout_for_type(
                input, argument->type
            );
        }
        if (status == RUDA_MIR_BYTECODE_LOWER_OK
            && !ruda_bytecode_add_call(
                &lowered,
                call->callee,
                call_result_type,
                arguments,
                call->argument_count,
                &call_id
            )) {
            status = RUDA_MIR_BYTECODE_LOWER_ALLOCATION_FAILED;
        }
        ruda_deallocate(&allocator, arguments, argument_bytes);
        if (status != RUDA_MIR_BYTECODE_LOWER_OK) {
            goto cleanup;
        }
        if ((size_t)call_id != index) {
            status = RUDA_MIR_BYTECODE_LOWER_INVALID_MIR;
            goto cleanup;
        }
        {
            RudaBytecodeCall *lowered_call = ruda_vector_at(&lowered.calls, call_id);
            if (lowered_call == NULL) {
                status = RUDA_MIR_BYTECODE_LOWER_INVALID_MIR;
                goto cleanup;
            }
            lowered_call->result_layout = ruda_mir_bytecode_layout_for_type(
                input, call->result_type
            );
            lowered_call->interface_slot = call->interface_slot;
        }
    }
    for (index = 0; index < input->blocks.length; index += 1) {
        uint32_t block_id = UINT32_MAX;
        if (!ruda_bytecode_add_block(&lowered, &block_id)) {
            status = RUDA_MIR_BYTECODE_LOWER_ALLOCATION_FAILED;
            goto cleanup;
        }
        if (block_id != index) {
            status = RUDA_MIR_BYTECODE_LOWER_INVALID_MIR;
            goto cleanup;
        }
    }
    for (index = 0; index < input->blocks.length; index += 1) {
        const RudaMirBlock *block = ruda_vector_at_const(&input->blocks, index);
        size_t instruction_index = 0;
        for (instruction_index = 0;
            instruction_index < block->instructions.length;
            instruction_index += 1) {
            const RudaMirInstruction *instruction = ruda_vector_at_const(
                &block->instructions,
                instruction_index
            );
            RudaBytecodeInstruction lowered_instruction;
            status = ruda_mir_bytecode_instruction(
                input,
                instruction,
                options.source,
                &lowered_instruction
            );
            if (status != RUDA_MIR_BYTECODE_LOWER_OK
                || !ruda_bytecode_append_instruction(
                    &lowered,
                    (uint32_t)index,
                    &lowered_instruction
                )) {
                if (status == RUDA_MIR_BYTECODE_LOWER_OK) {
                    status = RUDA_MIR_BYTECODE_LOWER_ALLOCATION_FAILED;
                }
                goto cleanup;
            }
            if (instruction->kind == RUDA_MIR_ARRAY_SET
                || instruction->kind == RUDA_MIR_RECORD_SET) {
                RudaBytecodeInstruction copy;
                memset(&copy, 0, sizeof(copy));
                copy.opcode = RUDA_OPCODE_COPY;
                copy.operands[0].unsigned_value = instruction->target;
                copy.operands[1].unsigned_value = instruction->kind
                    == RUDA_MIR_ARRAY_SET
                    ? instruction->third
                    : instruction->second;
                copy.span = lowered_instruction.span;
                if (!ruda_bytecode_append_instruction(
                        &lowered, (uint32_t)index, &copy
                    )) {
                    status = RUDA_MIR_BYTECODE_LOWER_ALLOCATION_FAILED;
                    goto cleanup;
                }
            }
        }
        {
            RudaBytecodeInstruction terminator;
            status = ruda_mir_bytecode_terminator(
                input,
                &block->terminator,
                options.source,
                &terminator
            );
            if (status != RUDA_MIR_BYTECODE_LOWER_OK
                || !ruda_bytecode_append_instruction(
                    &lowered,
                    (uint32_t)index,
                    &terminator
                )) {
                if (status == RUDA_MIR_BYTECODE_LOWER_OK) {
                    status = RUDA_MIR_BYTECODE_LOWER_ALLOCATION_FAILED;
                }
                goto cleanup;
            }
        }
    }
    {
        RudaBytecodeVerifyStatus verify_status = ruda_bytecode_verify(&lowered);
        if (verify_status == RUDA_BYTECODE_VERIFY_ALLOCATION_FAILED) {
            status = RUDA_MIR_BYTECODE_LOWER_ALLOCATION_FAILED;
            goto cleanup;
        }
        if (verify_status == RUDA_BYTECODE_VERIFY_LIMIT_REACHED) {
            status = RUDA_MIR_BYTECODE_LOWER_LIMIT_REACHED;
            goto cleanup;
        }
        if (verify_status != RUDA_BYTECODE_VERIFY_OK) {
            status = RUDA_MIR_BYTECODE_LOWER_INVALID_MIR;
            goto cleanup;
        }
    }
    *output = lowered;
    memset(&lowered, 0, sizeof(lowered));

cleanup:
    ruda_deallocate(&allocator, parameter_types, parameter_bytes);
    ruda_bytecode_function_dispose(&lowered);
    return status;
}

static int ruda_mir_bytecode_compare_function_symbols(
    const void *left,
    const void *right
) {
    const RudaMirFunction *const left_function =
        *(const RudaMirFunction *const *)left;
    const RudaMirFunction *const right_function =
        *(const RudaMirFunction *const *)right;
    return memcmp(
        left_function->symbol.bytes,
        right_function->symbol.bytes,
        sizeof(left_function->symbol.bytes)
    );
}

RudaMirBytecodeLowerStatus ruda_mir_lower_bytecode_program(
    const RudaMirFunction *const *functions,
    size_t function_count,
    RudaSymbolId entry,
    RudaAllocator allocator,
    RudaMirBytecodeProgramLowerOptions options,
    RudaBytecodeProgram *output
) {
    RudaBytecodeProgram lowered;
    const RudaMirFunction **ordered = NULL;
    size_t ordered_bytes = 0;
    size_t index = 0;
    RudaMirBytecodeLowerStatus status = RUDA_MIR_BYTECODE_LOWER_OK;
    if (output == NULL) {
        return RUDA_MIR_BYTECODE_LOWER_INVALID_ARGUMENT;
    }
    if (output->initialized) {
        ruda_bytecode_program_dispose(output);
    } else {
        memset(output, 0, sizeof(*output));
    }
    memset(&lowered, 0, sizeof(lowered));
    if ((functions == NULL && function_count != 0) || function_count == 0
        || function_count > options.program_options.max_functions
        || allocator.allocate == NULL || allocator.resize == NULL
        || allocator.deallocate == NULL
        || options.source_count == 0 || options.source >= options.source_count
        || options.source_count
            > options.program_options.function_options.max_source_files
        || !ruda_size_multiply(
            function_count,
            sizeof(const RudaMirFunction *),
            &ordered_bytes
        )) {
        return RUDA_MIR_BYTECODE_LOWER_INVALID_ARGUMENT;
    }
    if (!ruda_bytecode_program_init(
            &lowered,
            allocator,
            options.source_count,
            options.program_options
        )) {
        return RUDA_MIR_BYTECODE_LOWER_INVALID_ARGUMENT;
    }
    ordered = ruda_allocate(&allocator, ordered_bytes);
    if (ordered == NULL) {
        ruda_bytecode_program_dispose(&lowered);
        return RUDA_MIR_BYTECODE_LOWER_ALLOCATION_FAILED;
    }
    for (index = 0; index < function_count; index += 1) {
        if (functions[index] == NULL || !functions[index]->initialized) {
            status = RUDA_MIR_BYTECODE_LOWER_INVALID_ARGUMENT;
            goto cleanup_program;
        }
        ordered[index] = functions[index];
    }
    qsort(
        ordered,
        function_count,
        sizeof(const RudaMirFunction *),
        ruda_mir_bytecode_compare_function_symbols
    );
    for (index = 1; index < function_count; index += 1) {
        if (ruda_mir_bytecode_compare_function_symbols(
                &ordered[index - 1],
                &ordered[index]
            ) == 0) {
            status = RUDA_MIR_BYTECODE_LOWER_INVALID_MIR;
            goto cleanup_program;
        }
    }
    for (index = 0; index < function_count; index += 1) {
        RudaBytecodeFunction function;
        RudaMirBytecodeLowerOptions function_options;
        uint32_t function_id = UINT32_MAX;
        memset(&function, 0, sizeof(function));
        function_options.source = options.source;
        function_options.source_count = options.source_count;
        function_options.function_options =
            options.program_options.function_options;
        status = ruda_mir_lower_bytecode_function(
            ordered[index],
            allocator,
            function_options,
            &function
        );
        if (status == RUDA_MIR_BYTECODE_LOWER_OK
            && !ruda_bytecode_program_take_function(
                &lowered,
                ordered[index]->symbol,
                &function,
                &function_id
            )) {
            status = RUDA_MIR_BYTECODE_LOWER_ALLOCATION_FAILED;
        }
        ruda_bytecode_function_dispose(&function);
        if (status != RUDA_MIR_BYTECODE_LOWER_OK) {
            goto cleanup_program;
        }
        if ((size_t)function_id != index) {
            status = RUDA_MIR_BYTECODE_LOWER_INVALID_MIR;
            goto cleanup_program;
        }
    }
    if (!ruda_bytecode_program_set_entry(&lowered, entry)) {
        status = RUDA_MIR_BYTECODE_LOWER_INVALID_MIR;
        goto cleanup_program;
    }
    {
        RudaBytecodeProgramVerifyStatus verify_status =
            ruda_bytecode_program_verify(&lowered);
        if (verify_status == RUDA_BYTECODE_PROGRAM_VERIFY_ALLOCATION_FAILED) {
            status = RUDA_MIR_BYTECODE_LOWER_ALLOCATION_FAILED;
        } else if (verify_status == RUDA_BYTECODE_PROGRAM_VERIFY_LIMIT_REACHED) {
            status = RUDA_MIR_BYTECODE_LOWER_LIMIT_REACHED;
        } else if (verify_status != RUDA_BYTECODE_PROGRAM_VERIFY_OK) {
            status = RUDA_MIR_BYTECODE_LOWER_INVALID_MIR;
        }
        if (status != RUDA_MIR_BYTECODE_LOWER_OK) {
            goto cleanup_program;
        }
    }
    *output = lowered;
    memset(&lowered, 0, sizeof(lowered));

cleanup_program:
    ruda_bytecode_program_dispose(&lowered);
    ruda_deallocate(&allocator, ordered, ordered_bytes);
    return status;
}
