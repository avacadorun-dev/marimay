#include "mir_internal.h"

#include <string.h>

static RudaMirVerifyStatus ruda_mir_defer_propagate(
    const RudaMirFunction *function,
    RudaMirBlockId target,
    const RudaMirCallId *stack,
    size_t depth,
    RudaMirCallId *incoming,
    size_t *depths,
    unsigned char *queued
) {
    size_t width = function->calls.length == 0 ? 1 : function->calls.length;
    RudaMirCallId *target_stack = incoming + (size_t)target * width;
    if (depths[target] == SIZE_MAX) {
        depths[target] = depth;
        if (depth != 0) {
            memcpy(target_stack, stack, depth * sizeof(*stack));
        }
        queued[target] = 1;
        return RUDA_MIR_VERIFY_OK;
    }
    return depths[target] == depth
            && (depth == 0 || memcmp(
                target_stack, stack, depth * sizeof(*stack)
            ) == 0)
        ? RUDA_MIR_VERIFY_OK
        : RUDA_MIR_VERIFY_INVALID_CONTROL_FLOW;
}

RudaMirVerifyStatus ruda_mir_verify_defer_control_flow(
    const RudaMirFunction *function,
    size_t *work
) {
    size_t block_count = function->blocks.length;
    size_t width = function->calls.length == 0 ? 1 : function->calls.length;
    size_t stack_cells = 0;
    size_t stack_bytes = 0;
    size_t scratch_bytes = 0;
    size_t depth_bytes = 0;
    RudaMirCallId *incoming = NULL;
    RudaMirCallId *scratch = NULL;
    size_t *depths = NULL;
    unsigned char *queued = NULL;
    size_t block_index = 0;
    RudaMirVerifyStatus status = RUDA_MIR_VERIFY_OK;
    if (!ruda_size_multiply(block_count, width, &stack_cells)
        || stack_cells > RUDA_MIR_MAX_DATAFLOW_CELLS
        || !ruda_size_multiply(
            stack_cells, sizeof(*incoming), &stack_bytes
        ) || !ruda_size_multiply(
            width, sizeof(*scratch), &scratch_bytes
        ) || !ruda_size_multiply(
            block_count, sizeof(*depths), &depth_bytes
        )) {
        return RUDA_MIR_VERIFY_LIMIT_REACHED;
    }
    incoming = ruda_allocate(&function->allocator, stack_bytes);
    scratch = ruda_allocate(&function->allocator, scratch_bytes);
    depths = ruda_allocate(&function->allocator, depth_bytes);
    queued = ruda_allocate_array(
        &function->allocator, block_count, sizeof(*queued)
    );
    if (incoming == NULL || scratch == NULL || depths == NULL
        || queued == NULL) {
        status = RUDA_MIR_VERIFY_ALLOCATION_FAILED;
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
        const RudaMirBlock *block = NULL;
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
        depth = depths[block_index];
        if (depth != 0) {
            memcpy(
                scratch,
                incoming + block_index * width,
                depth * sizeof(*scratch)
            );
        }
        for (instruction_index = 0;
            instruction_index < block->instructions.length;
            instruction_index += 1) {
            const RudaMirInstruction *instruction = ruda_vector_at_const(
                &block->instructions, instruction_index
            );
            if (!ruda_mir_consume_verify_work(
                    work, 1, function->options.max_verify_work
                )) {
                status = RUDA_MIR_VERIFY_LIMIT_REACHED;
                goto cleanup;
            }
            if (instruction->kind == RUDA_MIR_DEFER_REGISTER) {
                if (depth >= width) {
                    status = RUDA_MIR_VERIFY_INVALID_CONTROL_FLOW;
                    goto cleanup;
                }
                scratch[depth] = instruction->call;
                depth += 1;
            } else if (instruction->kind == RUDA_MIR_DEFER_CALL) {
                if (depth == 0 || scratch[depth - 1] != instruction->call) {
                    status = RUDA_MIR_VERIFY_INVALID_CONTROL_FLOW;
                    goto cleanup;
                }
                depth -= 1;
            }
        }
        if (block->terminator.kind == RUDA_MIR_RETURN && depth != 0) {
            status = RUDA_MIR_VERIFY_INVALID_CONTROL_FLOW;
            goto cleanup;
        }
        if (block->terminator.kind == RUDA_MIR_GOTO) {
            status = ruda_mir_defer_propagate(
                function, block->terminator.target, scratch, depth,
                incoming, depths, queued
            );
        } else if (block->terminator.kind == RUDA_MIR_BRANCH) {
            status = ruda_mir_defer_propagate(
                function, block->terminator.target, scratch, depth,
                incoming, depths, queued
            );
            if (status == RUDA_MIR_VERIFY_OK) {
                status = ruda_mir_defer_propagate(
                    function, block->terminator.otherwise, scratch, depth,
                    incoming, depths, queued
                );
            }
        }
        if (status != RUDA_MIR_VERIFY_OK) {
            goto cleanup;
        }
    }

cleanup:
    ruda_deallocate(&function->allocator, queued, block_count);
    ruda_deallocate(&function->allocator, depths, depth_bytes);
    ruda_deallocate(&function->allocator, scratch, scratch_bytes);
    ruda_deallocate(&function->allocator, incoming, stack_bytes);
    return status;
}
