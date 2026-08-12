#include "ruda/bytecode/mir_lower.h"

#include <limits.h>
#include <string.h>

RudaMirBytecodeLowerOptions ruda_mir_bytecode_lower_default_options(void) {
    RudaMirBytecodeLowerOptions options;
    options.source = 0;
    options.source_count = 1;
    options.function_options = ruda_bytecode_function_default_options();
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

static bool ruda_mir_bytecode_type(
    RudaTypeId input,
    bool allow_void,
    RudaBytecodeType *output
) {
    if (input == RUDA_TYPE_BOOL) {
        *output = RUDA_BYTECODE_TYPE_BOOL;
        return true;
    }
    if (input == RUDA_TYPE_I64) {
        *output = RUDA_BYTECODE_TYPE_I64;
        return true;
    }
    if (allow_void && input == RUDA_TYPE_VOID) {
        *output = RUDA_BYTECODE_TYPE_VOID;
        return true;
    }
    return false;
}

static RudaMirBytecodeLowerStatus ruda_mir_bytecode_instruction(
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
        case RUDA_MIR_CONSTANT_STRING:
        case RUDA_MIR_EQUAL_STRING:
        case RUDA_MIR_RECORD_NEW:
        case RUDA_MIR_RECORD_GET:
        case RUDA_MIR_RECORD_SET:
        case RUDA_MIR_RECORD_IDENTITY_SAME:
        case RUDA_MIR_ENUM_NEW:
        case RUDA_MIR_ENUM_IS_VARIANT:
        case RUDA_MIR_ENUM_REQUIRE_VARIANT:
        case RUDA_MIR_ENUM_GET_PAYLOAD:
        case RUDA_MIR_CALL:
            return RUDA_MIR_BYTECODE_LOWER_UNSUPPORTED_INSTRUCTION;
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
        case RUDA_MIR_CALL_NEVER:
            return RUDA_MIR_BYTECODE_LOWER_UNSUPPORTED_TERMINATOR;
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
    if (!ruda_mir_bytecode_type(input->result_type, true, &result_type)) {
        return RUDA_MIR_BYTECODE_LOWER_UNSUPPORTED_TYPE;
    }
    for (index = 0; index < input->register_types.length; index += 1) {
        const RudaTypeId *input_type = ruda_vector_at_const(
            &input->register_types,
            index
        );
        RudaBytecodeType ignored;
        if (input_type == NULL
            || !ruda_mir_bytecode_type(*input_type, false, &ignored)) {
            return RUDA_MIR_BYTECODE_LOWER_UNSUPPORTED_TYPE;
        }
    }
    if (!ruda_size_add(
            input->instruction_count,
            input->blocks.length,
            &lowered_instruction_count
        ) || input->register_types.length > options.function_options.max_registers
        || input->blocks.length > options.function_options.max_blocks
        || lowered_instruction_count > options.function_options.max_instructions) {
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
                    *input_type,
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
    ruda_deallocate(&allocator, parameter_types, parameter_bytes);
    parameter_types = NULL;
    parameter_bytes = 0;
    for (index = input->parameter_count;
        index < input->register_types.length; index += 1) {
        const RudaTypeId *input_type = ruda_vector_at_const(
            &input->register_types,
            index
        );
        RudaBytecodeType type;
        uint32_t register_id = UINT32_MAX;
        if (!ruda_mir_bytecode_type(*input_type, false, &type)) {
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
