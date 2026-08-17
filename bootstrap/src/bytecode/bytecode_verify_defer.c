#include "bytecode_internal.h"

#include <string.h>

static bool ruda_bytecode_defer_consume(
    size_t *work,
    size_t amount,
    size_t limit
) {
    size_t next = 0;
    if (!ruda_size_add(*work, amount, &next) || next > limit) {
        return false;
    }
    *work = next;
    return true;
}

static RudaBytecodeVerifyStatus ruda_bytecode_defer_propagate(
    const RudaBytecodeFunction *function,
    size_t target,
    const RudaBytecodeCallId *stack,
    size_t depth,
    RudaBytecodeCallId *incoming,
    size_t *depths,
    unsigned char *queued
) {
    size_t width = function->calls.length == 0 ? 1 : function->calls.length;
    RudaBytecodeCallId *target_stack = incoming + target * width;
    if (depths[target] == SIZE_MAX) {
        depths[target] = depth;
        if (depth != 0) {
            memcpy(target_stack, stack, depth * sizeof(*stack));
        }
        queued[target] = 1;
        return RUDA_BYTECODE_VERIFY_OK;
    }
    return depths[target] == depth
            && (depth == 0 || memcmp(
                target_stack, stack, depth * sizeof(*stack)
            ) == 0)
        ? RUDA_BYTECODE_VERIFY_OK
        : RUDA_BYTECODE_VERIFY_INVALID_CONTROL_FLOW;
}

RudaBytecodeVerifyStatus ruda_bytecode_verify_defer_control_flow(
    const RudaBytecodeFunction *function,
    size_t available_work,
    size_t *work_used
) {
    size_t block_count = function->blocks.length;
    size_t width = function->calls.length == 0 ? 1 : function->calls.length;
    size_t stack_cells = 0;
    size_t stack_bytes = 0;
    size_t scratch_bytes = 0;
    size_t depth_bytes = 0;
    RudaBytecodeCallId *incoming = NULL;
    RudaBytecodeCallId *scratch = NULL;
    size_t *depths = NULL;
    unsigned char *queued = NULL;
    size_t block_index = 0;
    size_t work = 0;
    RudaBytecodeVerifyStatus status = RUDA_BYTECODE_VERIFY_OK;
    if (work_used == NULL || available_work == 0
        || !ruda_size_multiply(block_count, width, &stack_cells)
        || stack_cells > available_work
        || !ruda_size_multiply(stack_cells, sizeof(*incoming), &stack_bytes)
        || !ruda_size_multiply(width, sizeof(*scratch), &scratch_bytes)
        || !ruda_size_multiply(block_count, sizeof(*depths), &depth_bytes)) {
        return RUDA_BYTECODE_VERIFY_LIMIT_REACHED;
    }
    *work_used = 0;
    incoming = ruda_allocate(&function->allocator, stack_bytes);
    scratch = ruda_allocate(&function->allocator, scratch_bytes);
    depths = ruda_allocate(&function->allocator, depth_bytes);
    queued = ruda_allocate_array(
        &function->allocator, block_count, sizeof(*queued)
    );
    if (incoming == NULL || scratch == NULL || depths == NULL
        || queued == NULL) {
        status = RUDA_BYTECODE_VERIFY_ALLOCATION_FAILED;
        goto cleanup;
    }
    memset(incoming, 0, stack_bytes);
    memset(queued, 0, block_count);
    for (block_index = 0; block_index < block_count; block_index += 1) {
        depths[block_index] = SIZE_MAX;
    }
    depths[0] = 0;
    queued[0] = 1;
    for (;;) {
        const RudaBytecodeBlock *block = NULL;
        const RudaBytecodeInstruction *terminator = NULL;
        size_t depth = 0;
        size_t instruction_index = 0;
        for (block_index = 0; block_index < block_count; block_index += 1) {
            if (queued[block_index] != 0) {
                break;
            }
        }
        if (block_index == block_count) {
            break;
        }
        queued[block_index] = 0;
        block = ruda_vector_at_const(&function->blocks, block_index);
        terminator = ruda_bytecode_block_terminator(block);
        depth = depths[block_index];
        if (depth != 0) {
            memcpy(
                scratch, incoming + block_index * width,
                depth * sizeof(*scratch)
            );
        }
        for (instruction_index = 0;
            instruction_index + 1 < block->instructions.length;
            instruction_index += 1) {
            const RudaBytecodeInstruction *instruction = ruda_vector_at_const(
                &block->instructions, instruction_index
            );
            RudaBytecodeCallId call = (RudaBytecodeCallId)
                instruction->operands[1].unsigned_value;
            if (!ruda_bytecode_defer_consume(
                    &work, 1, available_work
                )) {
                status = RUDA_BYTECODE_VERIFY_LIMIT_REACHED;
                goto cleanup;
            }
            if (instruction->opcode == RUDA_OPCODE_DEFER_REGISTER) {
                if (depth >= width) {
                    status = RUDA_BYTECODE_VERIFY_INVALID_CONTROL_FLOW;
                    goto cleanup;
                }
                scratch[depth] = call;
                depth += 1;
            } else if (instruction->opcode == RUDA_OPCODE_DEFER_CALL) {
                if (depth == 0 || scratch[depth - 1] != call) {
                    status = RUDA_BYTECODE_VERIFY_INVALID_CONTROL_FLOW;
                    goto cleanup;
                }
                depth -= 1;
            }
        }
        if ((terminator->opcode == RUDA_OPCODE_RETURN
                || terminator->opcode == RUDA_OPCODE_RETURN_VOID)
            && depth != 0) {
            status = RUDA_BYTECODE_VERIFY_INVALID_CONTROL_FLOW;
            goto cleanup;
        }
        if (terminator->opcode == RUDA_OPCODE_JUMP) {
            status = ruda_bytecode_defer_propagate(
                function, (size_t)terminator->operands[0].unsigned_value,
                scratch, depth, incoming, depths, queued
            );
        } else if (terminator->opcode == RUDA_OPCODE_BRANCH_BOOL) {
            status = ruda_bytecode_defer_propagate(
                function, (size_t)terminator->operands[1].unsigned_value,
                scratch, depth, incoming, depths, queued
            );
            if (status == RUDA_BYTECODE_VERIFY_OK) {
                status = ruda_bytecode_defer_propagate(
                    function, (size_t)terminator->operands[2].unsigned_value,
                    scratch, depth, incoming, depths, queued
                );
            }
        }
        if (status != RUDA_BYTECODE_VERIFY_OK) {
            goto cleanup;
        }
    }
    *work_used = work;

cleanup:
    ruda_deallocate(&function->allocator, queued, block_count);
    ruda_deallocate(&function->allocator, depths, depth_bytes);
    ruda_deallocate(&function->allocator, scratch, scratch_bytes);
    ruda_deallocate(&function->allocator, incoming, stack_bytes);
    return status;
}
