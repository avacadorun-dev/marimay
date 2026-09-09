#include "vm_image.h"

#include <stdalign.h>
#include <string.h>

static bool mmay_bytecode_image_align(
    size_t offset, size_t alignment, size_t *result
) {
    size_t remainder = offset % alignment;
    size_t padding = remainder == 0 ? 0 : alignment - remainder;
    return mmay_size_add(offset, padding, result);
}

static bool mmay_bytecode_image_function_size(
    const MarimayBytecodeFunction *function, size_t *block_bytes,
    size_t *operation_bytes, size_t *operation_count
) {
    size_t count = 0;
    if (function == NULL || block_bytes == NULL || operation_bytes == NULL
        || operation_count == NULL || function->blocks.length > UINT32_MAX
        || !mmay_size_multiply(function->blocks.length, sizeof(uint32_t),
            block_bytes)) {
        return false;
    }
    for (size_t block = 0; block < function->blocks.length; block += 1) {
        const MarimayBytecodeBlock *body = mmay_vector_at_const(
            &function->blocks, block);
        if (body == NULL || body->instructions.length > UINT32_MAX
            || !mmay_size_add(count, body->instructions.length, &count)
            || count > UINT32_MAX) {
            return false;
        }
    }
    if (!mmay_size_multiply(count, sizeof(MarimayBytecodeImageOperation),
            operation_bytes)) {
        return false;
    }
    *operation_count = count;
    return true;
}

static bool mmay_bytecode_image_storage_size(
    const MarimayBytecodeProgram *program, size_t *result
) {
    size_t offset = 0;
    if (program->functions.length == 0
        || !mmay_size_multiply(program->functions.length,
            sizeof(MarimayBytecodeImageFunction), &offset)) {
        return false;
    }
    for (size_t index = 0; index < program->functions.length; index += 1) {
        const MarimayBytecodeProgramFunction *function =
            mmay_bytecode_program_function(program, index);
        size_t block_bytes = 0, operation_bytes = 0, operation_count = 0;
        if (function == NULL || !mmay_bytecode_image_function_size(
                &function->body, &block_bytes, &operation_bytes,
                &operation_count)
            || !mmay_bytecode_image_align(offset, alignof(uint32_t), &offset)
            || !mmay_size_add(offset, block_bytes, &offset)
            || !mmay_bytecode_image_align(offset,
                alignof(MarimayBytecodeImageOperation), &offset)
            || !mmay_size_add(offset, operation_bytes, &offset)) {
            return false;
        }
    }
    *result = offset;
    return true;
}

static bool mmay_bytecode_image_operand_u32(uint64_t value, uint32_t *output) {
    if (value > UINT32_MAX) return false;
    *output = (uint32_t)value;
    return true;
}

static bool mmay_bytecode_image_opcode_borrows_roots(MarimayOpcode opcode) {
    switch (opcode) {
        case MMAY_OPCODE_CONST_I64:
        case MMAY_OPCODE_CONST_INTEGER:
        case MMAY_OPCODE_CONST_F64:
        case MMAY_OPCODE_CONST_BOOL:
        case MMAY_OPCODE_COPY:
        case MMAY_OPCODE_BOOL_NOT:
        case MMAY_OPCODE_BOOL_EQUAL:
        case MMAY_OPCODE_I64_ADD_CHECKED:
        case MMAY_OPCODE_I64_SUBTRACT_CHECKED:
        case MMAY_OPCODE_I64_MULTIPLY_CHECKED:
        case MMAY_OPCODE_I64_DIVIDE_CHECKED:
        case MMAY_OPCODE_I64_REMAINDER_CHECKED:
        case MMAY_OPCODE_I64_NEGATE_CHECKED:
        case MMAY_OPCODE_I64_ADD_PROVEN:
        case MMAY_OPCODE_I64_SUBTRACT_PROVEN:
        case MMAY_OPCODE_I64_MULTIPLY_PROVEN:
        case MMAY_OPCODE_I64_EQUAL:
        case MMAY_OPCODE_I64_LESS:
        case MMAY_OPCODE_I64_LESS_EQUAL:
        case MMAY_OPCODE_JUMP:
        case MMAY_OPCODE_BRANCH_BOOL:
        case MMAY_OPCODE_ARRAY_LENGTH:
        case MMAY_OPCODE_ARRAY_REQUIRE_INDEX:
        case MMAY_OPCODE_ARRAY_GET:
        case MMAY_OPCODE_ARRAY_SET:
        case MMAY_OPCODE_ARRAY_GET_IN_BOUNDS:
        case MMAY_OPCODE_ARRAY_SET_IN_BOUNDS:
        case MMAY_OPCODE_ARRAY_GENERATION:
        case MMAY_OPCODE_ARRAY_REQUIRE_GENERATION:
        case MMAY_OPCODE_RECORD_GET:
        case MMAY_OPCODE_RECORD_SET:
        case MMAY_OPCODE_RETURN:
        case MMAY_OPCODE_RETURN_VOID:
            return true;
        default:
            return false;
    }
}

static bool mmay_bytecode_image_sequence(
    const MarimayBytecodeImageOperation *operations, size_t available,
    const MarimayOpcode *opcodes, size_t count
) {
    if (available < count) return false;
    for (size_t index = 0; index < count; index += 1) {
        if (operations[index].opcode != (uint8_t)opcodes[index]) return false;
    }
    return true;
}

static void mmay_bytecode_image_mark_block(
    MarimayBytecodeImageOperation *operations, size_t count
) {
    static const MarimayOpcode array_shift[] = {
        MMAY_OPCODE_COPY, MMAY_OPCODE_ARRAY_REQUIRE_INDEX,
        MMAY_OPCODE_I64_SUBTRACT_PROVEN, MMAY_OPCODE_ARRAY_REQUIRE_INDEX,
        MMAY_OPCODE_ARRAY_GET_IN_BOUNDS, MMAY_OPCODE_ARRAY_SET_IN_BOUNDS,
        MMAY_OPCODE_COPY, MMAY_OPCODE_I64_SUBTRACT_PROVEN,
        MMAY_OPCODE_COPY, MMAY_OPCODE_JUMP
    };
    static const MarimayOpcode array_read_branch[] = {
        MMAY_OPCODE_I64_SUBTRACT_PROVEN, MMAY_OPCODE_ARRAY_REQUIRE_INDEX,
        MMAY_OPCODE_ARRAY_GET_IN_BOUNDS, MMAY_OPCODE_I64_LESS,
        MMAY_OPCODE_BRANCH_BOOL
    };
    for (size_t index = 0; index < count; index += 1) {
        MarimayBytecodeImageOperation *operation = &operations[index];
        size_t available = count - index;
        if (mmay_bytecode_image_sequence(operation, available, array_shift,
                sizeof(array_shift) / sizeof(array_shift[0]))
            && operations[index + 4].second == operations[index + 3].first
            && operations[index + 4].third == operations[index + 3].third
            && operations[index + 5].first == operations[index + 1].first
            && operations[index + 5].second == operations[index + 1].third
            && operations[index + 3].second == operations[index + 1].second) {
            operation->fusion = MMAY_BYTECODE_IMAGE_FUSION_ARRAY_SHIFT;
        } else if (mmay_bytecode_image_sequence(operation, available,
                array_read_branch,
                sizeof(array_read_branch) / sizeof(array_read_branch[0]))
            && operations[index + 2].second == operations[index + 1].first
            && operations[index + 2].third == operations[index + 1].third) {
            operation->fusion = MMAY_BYTECODE_IMAGE_FUSION_ARRAY_READ_BRANCH;
        } else if (available >= 3
            && (operation->opcode == MMAY_OPCODE_I64_ADD_PROVEN
                || operation->opcode == MMAY_OPCODE_I64_SUBTRACT_PROVEN
                || operation->opcode == MMAY_OPCODE_I64_MULTIPLY_PROVEN)
            && operations[index + 1].opcode == MMAY_OPCODE_COPY
            && operations[index + 2].opcode == MMAY_OPCODE_JUMP) {
            operation->fusion = MMAY_BYTECODE_IMAGE_FUSION_INDUCTION;
        } else if (available >= 2
            && (operation->opcode == MMAY_OPCODE_I64_EQUAL
                || operation->opcode == MMAY_OPCODE_I64_LESS
                || operation->opcode == MMAY_OPCODE_I64_LESS_EQUAL
                || operation->opcode == MMAY_OPCODE_BOOL_EQUAL)
            && operations[index + 1].opcode == MMAY_OPCODE_BRANCH_BOOL
            && operations[index + 1].first == operation->first) {
            operation->fusion = MMAY_BYTECODE_IMAGE_FUSION_COMPARE_BRANCH;
        }
    }
}

static void mmay_bytecode_image_mark_loops(
    MarimayBytecodeImageFunction *function
) {
    static const MarimayOpcode record_array_head[] = {
        MMAY_OPCODE_RECORD_GET, MMAY_OPCODE_ARRAY_REQUIRE_INDEX,
        MMAY_OPCODE_ARRAY_GET_IN_BOUNDS, MMAY_OPCODE_I64_EQUAL,
        MMAY_OPCODE_BOOL_NOT, MMAY_OPCODE_BRANCH_BOOL
    };
    static const MarimayOpcode record_array_head_direct[] = {
        MMAY_OPCODE_RECORD_GET, MMAY_OPCODE_ARRAY_REQUIRE_INDEX,
        MMAY_OPCODE_ARRAY_GET_IN_BOUNDS, MMAY_OPCODE_I64_EQUAL,
        MMAY_OPCODE_BRANCH_BOOL
    };
    static const MarimayOpcode record_array_follow[] = {
        MMAY_OPCODE_RECORD_GET, MMAY_OPCODE_ARRAY_REQUIRE_INDEX,
        MMAY_OPCODE_ARRAY_GET_IN_BOUNDS, MMAY_OPCODE_COPY, MMAY_OPCODE_JUMP
    };
    static const MarimayOpcode record_array_compress[] = {
        MMAY_OPCODE_RECORD_GET, MMAY_OPCODE_ARRAY_REQUIRE_INDEX,
        MMAY_OPCODE_ARRAY_GET_IN_BOUNDS, MMAY_OPCODE_COPY,
        MMAY_OPCODE_RECORD_GET, MMAY_OPCODE_COPY,
        MMAY_OPCODE_ARRAY_REQUIRE_INDEX, MMAY_OPCODE_ARRAY_SET_IN_BOUNDS,
        MMAY_OPCODE_COPY, MMAY_OPCODE_COPY, MMAY_OPCODE_JUMP
    };
    MarimayBytecodeImageOperation *operations =
        (MarimayBytecodeImageOperation *)(uintptr_t)function->operations;
    for (size_t pc = 0; pc < function->operation_count; pc += 1) {
        MarimayBytecodeImageOperation *head = &operations[pc];
        bool inverted_head = mmay_bytecode_image_sequence(head,
                function->operation_count - pc, record_array_head,
                sizeof(record_array_head) / sizeof(record_array_head[0]));
        bool direct_head = mmay_bytecode_image_sequence(head,
                function->operation_count - pc, record_array_head_direct,
                sizeof(record_array_head_direct)
                    / sizeof(record_array_head_direct[0]));
        if (inverted_head || direct_head) {
            MarimayBytecodeImageOperation *branch = head
                + (inverted_head ? 5 : 4);
            size_t body_pc = inverted_head ? branch->second : branch->third;
            bool head_matches = head[1].second == head[0].first
                && head[2].second == head[1].first
                && head[2].third == head[1].third
                && head[3].second == head[2].first
                && head[3].third == head[1].third
                && (inverted_head
                    ? head[4].second == head[3].first
                        && head[5].first == head[4].first
                    : head[4].first == head[3].first);
            bool follow_matches = body_pc + 4 < function->operation_count
                && mmay_bytecode_image_sequence(&operations[body_pc],
                    function->operation_count - body_pc, record_array_follow,
                    sizeof(record_array_follow)
                        / sizeof(record_array_follow[0]))
                && operations[body_pc + 1].second
                    == operations[body_pc].first
                && operations[body_pc + 2].second
                    == operations[body_pc + 1].first
                && operations[body_pc + 2].third
                    == operations[body_pc + 1].third
                && operations[body_pc + 3].first == head[1].third
                && operations[body_pc + 3].second
                    == operations[body_pc + 2].first
                && operations[body_pc + 4].first == pc
                && operations[body_pc].second == head[0].second
                && operations[body_pc].third == head[0].third;
            bool compress_matches = body_pc + 10 < function->operation_count
                && mmay_bytecode_image_sequence(&operations[body_pc],
                    function->operation_count - body_pc,
                    record_array_compress,
                    sizeof(record_array_compress)
                        / sizeof(record_array_compress[0]))
                && operations[body_pc + 1].second
                    == operations[body_pc].first
                && operations[body_pc + 2].second
                    == operations[body_pc + 1].first
                && operations[body_pc + 2].third
                    == operations[body_pc + 1].third
                && operations[body_pc + 3].second
                    == operations[body_pc + 2].first
                && operations[body_pc + 4].second == head[0].second
                && operations[body_pc + 4].third == head[0].third
                && operations[body_pc + 5].first
                    == operations[body_pc + 6].third
                && operations[body_pc + 5].second == head[1].third
                && operations[body_pc + 6].second
                    == operations[body_pc + 4].first
                && operations[body_pc + 7].first
                    == operations[body_pc + 6].first
                && operations[body_pc + 7].second
                    == operations[body_pc + 6].third
                && operations[body_pc + 7].third == head[3].third
                && operations[body_pc + 8].second
                    == operations[body_pc + 7].third
                && operations[body_pc + 9].first == head[1].third
                && operations[body_pc + 9].second
                    == operations[body_pc + 3].first
                && operations[body_pc + 10].first == pc;
            if (head_matches && (follow_matches || compress_matches)) {
                head->fusion = follow_matches
                    ? MMAY_BYTECODE_IMAGE_FUSION_RECORD_ARRAY_FOLLOW_LOOP
                    : MMAY_BYTECODE_IMAGE_FUSION_RECORD_ARRAY_COMPRESS_LOOP;
                function->has_specialized_loop = true;
                continue;
            }
        }
        if (head->fusion != MMAY_BYTECODE_IMAGE_FUSION_COMPARE_BRANCH
            || pc + 1 >= function->operation_count) continue;
        MarimayBytecodeImageOperation *head_branch = &operations[pc + 1];
        size_t read_pc = head_branch->second;
        if (read_pc + 4 >= function->operation_count
            || operations[read_pc].fusion
                != MMAY_BYTECODE_IMAGE_FUSION_ARRAY_READ_BRANCH) continue;
        MarimayBytecodeImageOperation *read_branch = &operations[read_pc + 4];
        size_t shift_pc = read_branch->second;
        if (shift_pc + 9 >= function->operation_count
            || operations[shift_pc].fusion
                != MMAY_BYTECODE_IMAGE_FUSION_ARRAY_SHIFT
            || operations[shift_pc + 9].first != pc
            || head_branch->third != read_branch->third) continue;
        head->fusion = MMAY_BYTECODE_IMAGE_FUSION_ARRAY_SHIFT_LOOP;
        function->has_specialized_loop = true;
    }
}

static bool mmay_bytecode_image_bind_direct_call(
    const MarimayBytecodeProgram *program,
    const MarimayBytecodeFunction *function,
    const MarimayBytecodeInstruction *source,
    MarimayBytecodeImageOperation *target
) {
    if (source->opcode != MMAY_OPCODE_CALL_DIRECT
        && source->opcode != MMAY_OPCODE_CALL_DIRECT_VOID
        && source->opcode != MMAY_OPCODE_CALL_DIRECT_NEVER) return true;
    uint64_t wire = source->operands[
        source->opcode == MMAY_OPCODE_CALL_DIRECT ? 1 : 0].unsigned_value;
    const MarimayBytecodeCall *call = wire < function->calls.length
        ? mmay_bytecode_call(function, (MarimayBytecodeCallId)wire) : NULL;
    const MarimayBytecodeProgramFunction *callee = call == NULL ? NULL
        : mmay_bytecode_program_function_by_symbol(program, call->callee);
    const MarimayBytecodeProgramFunction *functions = program->functions.data;
    if (callee == NULL || functions == NULL || callee < functions
        || (size_t)(callee - functions) >= program->functions.length
        || (size_t)(callee - functions) > UINT32_MAX) return false;
    target->third = (uint32_t)(callee - functions);
    return true;
}

static bool mmay_bytecode_image_fill_function(
    MarimayBytecodeImageFunction *output,
    const MarimayBytecodeProgram *program,
    const MarimayBytecodeFunction *function
) {
    size_t flat = 0;
    uint32_t *block_starts = (uint32_t *)(uintptr_t)output->block_starts;
    MarimayBytecodeImageOperation *operations =
        (MarimayBytecodeImageOperation *)(uintptr_t)output->operations;
    for (size_t block = 0; block < function->blocks.length; block += 1) {
        const MarimayBytecodeBlock *body = mmay_vector_at_const(
            &function->blocks, block);
        block_starts[block] = (uint32_t)flat;
        flat += body->instructions.length;
    }
    flat = 0;
    output->borrows_caller_roots = true;
    for (size_t block = 0; block < function->blocks.length; block += 1) {
        const MarimayBytecodeBlock *body = mmay_vector_at_const(
            &function->blocks, block);
        size_t block_start = flat;
        for (size_t instruction = 0;
            instruction < body->instructions.length; instruction += 1) {
            const MarimayBytecodeInstruction *source = mmay_vector_at_const(
                &body->instructions, instruction);
            MarimayBytecodeImageOperation *target = &operations[flat++];
            memset(target, 0, sizeof(*target));
            const MarimayOpcodeMetadata *metadata = source == NULL
                ? NULL : mmay_opcode_metadata(source->opcode);
            if (source == NULL || metadata == NULL || source->opcode > UINT8_MAX
                || !mmay_bytecode_image_operand_u32(
                    source->operands[0].unsigned_value, &target->first)
                || ((metadata->operands[1] != MMAY_BYTECODE_OPERAND_I64
                        && metadata->operands[1] != MMAY_BYTECODE_OPERAND_F64_BITS
                        && metadata->operands[1] != MMAY_BYTECODE_OPERAND_U64)
                    && !mmay_bytecode_image_operand_u32(
                        source->operands[1].unsigned_value, &target->second))
                || !mmay_bytecode_image_operand_u32(
                    source->operands[2].unsigned_value, &target->third)) {
                return false;
            }
            target->opcode = (uint8_t)source->opcode;
            if (!mmay_bytecode_image_opcode_borrows_roots(source->opcode)) {
                output->borrows_caller_roots = false;
            }
            target->immediate = metadata->operands[1] == MMAY_BYTECODE_OPERAND_I64
                ? (uint64_t)source->operands[1].signed_value
                : source->operands[1].unsigned_value;
            target->source_block = (uint32_t)block;
            target->source_instruction = (uint32_t)instruction;
            if (source->opcode == MMAY_OPCODE_JUMP) {
                target->first = block_starts[target->first];
            } else if (source->opcode == MMAY_OPCODE_BRANCH_BOOL) {
                target->second = block_starts[target->second];
                target->third = block_starts[target->third];
            }
            if (!mmay_bytecode_image_bind_direct_call(
                    program, function, source, target)) return false;
        }
        mmay_bytecode_image_mark_block(
            &operations[block_start], body->instructions.length);
    }
    mmay_bytecode_image_mark_loops(output);
    return flat == output->operation_count;
}

MarimayBytecodeImageStatus mmay_bytecode_execution_image_init(
    MarimayBytecodeExecutionImage *output,
    const MarimayBytecodeProgram *program, MarimayAllocator allocator
) {
    size_t bytes = 0, offset = 0;
    if (output == NULL) return MMAY_BYTECODE_IMAGE_UNAVAILABLE;
    memset(output, 0, sizeof(*output));
    if (program == NULL || !program->initialized || allocator.allocate == NULL
        || allocator.deallocate == NULL
        || !mmay_bytecode_image_storage_size(program, &bytes)) {
        return MMAY_BYTECODE_IMAGE_UNAVAILABLE;
    }
    output->storage = mmay_allocate(&allocator, bytes);
    if (output->storage == NULL) {
        return MMAY_BYTECODE_IMAGE_ALLOCATION_FAILED;
    }
    memset(output->storage, 0, bytes);
    output->allocator = allocator;
    output->storage_bytes = bytes;
    output->functions = output->storage;
    output->function_count = program->functions.length;
    offset = output->function_count * sizeof(*output->functions);
    for (size_t index = 0; index < output->function_count; index += 1) {
        const MarimayBytecodeProgramFunction *source =
            mmay_bytecode_program_function(program, index);
        MarimayBytecodeImageFunction *target = &output->functions[index];
        size_t block_bytes = 0, operation_bytes = 0, operation_count = 0;
        if (!mmay_bytecode_image_function_size(&source->body, &block_bytes,
                &operation_bytes, &operation_count)
            || !mmay_bytecode_image_align(offset, alignof(uint32_t), &offset)) {
            mmay_bytecode_execution_image_dispose(output);
            return MMAY_BYTECODE_IMAGE_UNAVAILABLE;
        }
        target->block_starts = (const uint32_t *)((unsigned char *)output->storage + offset);
        target->block_count = source->body.blocks.length;
        offset += block_bytes;
        if (!mmay_bytecode_image_align(offset,
                alignof(MarimayBytecodeImageOperation), &offset)) {
            mmay_bytecode_execution_image_dispose(output);
            return MMAY_BYTECODE_IMAGE_UNAVAILABLE;
        }
        target->operations = (const MarimayBytecodeImageOperation *)
            ((unsigned char *)output->storage + offset);
        target->operation_count = operation_count;
        offset += operation_bytes;
        if (!mmay_bytecode_image_fill_function(
                target, program, &source->body)) {
            mmay_bytecode_execution_image_dispose(output);
            return MMAY_BYTECODE_IMAGE_UNAVAILABLE;
        }
    }
    output->initialized = true;
    return MMAY_BYTECODE_IMAGE_OK;
}

const MarimayBytecodeImageOperation *mmay_bytecode_image_operation_at(
    const MarimayBytecodeImageFunction *function, size_t block,
    size_t instruction
) {
    if (function == NULL || block >= function->block_count
        || instruction > UINT32_MAX) return NULL;
    size_t pc = (size_t)function->block_starts[block] + instruction;
    return pc < function->operation_count ? &function->operations[pc] : NULL;
}

void mmay_bytecode_execution_image_dispose(
    MarimayBytecodeExecutionImage *image
) {
    if (image == NULL) return;
    mmay_deallocate(&image->allocator, image->storage, image->storage_bytes);
    memset(image, 0, sizeof(*image));
}
