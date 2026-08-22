#include "mir_internal.h"

#include <string.h>

static MarimayMirVerifyStatus mmay_mir_defer_propagate(
    const MarimayMirFunction *function,
    MarimayMirBlockId target,
    const MarimayMirCallId *stack,
    size_t depth,
    MarimayMirCallId *incoming,
    size_t *depths,
    unsigned char *queued
) {
    size_t width = function->calls.length == 0 ? 1 : function->calls.length;
    MarimayMirCallId *target_stack = incoming + (size_t)target * width;
    if (depths[target] == SIZE_MAX) {
        depths[target] = depth;
        if (depth != 0) {
            memcpy(target_stack, stack, depth * sizeof(*stack));
        }
        queued[target] = 1;
        return MMAY_MIR_VERIFY_OK;
    }
    return depths[target] == depth
            && (depth == 0 || memcmp(
                target_stack, stack, depth * sizeof(*stack)
            ) == 0)
        ? MMAY_MIR_VERIFY_OK
        : MMAY_MIR_VERIFY_INVALID_CONTROL_FLOW;
}

MarimayMirVerifyStatus mmay_mir_verify_defer_control_flow(
    const MarimayMirFunction *function,
    size_t *work
) {
    size_t block_count = function->blocks.length;
    size_t width = function->calls.length == 0 ? 1 : function->calls.length;
    size_t stack_cells = 0;
    size_t stack_bytes = 0;
    size_t scratch_bytes = 0;
    size_t depth_bytes = 0;
    MarimayMirCallId *incoming = NULL;
    MarimayMirCallId *scratch = NULL;
    size_t *depths = NULL;
    unsigned char *queued = NULL;
    size_t block_index = 0;
    MarimayMirVerifyStatus status = MMAY_MIR_VERIFY_OK;
    if (!mmay_size_multiply(block_count, width, &stack_cells)
        || stack_cells > MMAY_MIR_MAX_DATAFLOW_CELLS
        || !mmay_size_multiply(
            stack_cells, sizeof(*incoming), &stack_bytes
        ) || !mmay_size_multiply(
            width, sizeof(*scratch), &scratch_bytes
        ) || !mmay_size_multiply(
            block_count, sizeof(*depths), &depth_bytes
        )) {
        return MMAY_MIR_VERIFY_LIMIT_REACHED;
    }
    incoming = mmay_allocate(&function->allocator, stack_bytes);
    scratch = mmay_allocate(&function->allocator, scratch_bytes);
    depths = mmay_allocate(&function->allocator, depth_bytes);
    queued = mmay_allocate_array(
        &function->allocator, block_count, sizeof(*queued)
    );
    if (incoming == NULL || scratch == NULL || depths == NULL
        || queued == NULL) {
        status = MMAY_MIR_VERIFY_ALLOCATION_FAILED;
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
        const MarimayMirBlock *block = NULL;
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
        block = mmay_vector_at_const(&function->blocks, block_index);
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
            const MarimayMirInstruction *instruction = mmay_vector_at_const(
                &block->instructions, instruction_index
            );
            if (!mmay_mir_consume_verify_work(
                    work, 1, function->options.max_verify_work
                )) {
                status = MMAY_MIR_VERIFY_LIMIT_REACHED;
                goto cleanup;
            }
            if (instruction->kind == MMAY_MIR_DEFER_REGISTER) {
                if (depth >= width) {
                    status = MMAY_MIR_VERIFY_INVALID_CONTROL_FLOW;
                    goto cleanup;
                }
                scratch[depth] = instruction->call;
                depth += 1;
            } else if (instruction->kind == MMAY_MIR_DEFER_CALL) {
                if (depth == 0 || scratch[depth - 1] != instruction->call) {
                    status = MMAY_MIR_VERIFY_INVALID_CONTROL_FLOW;
                    goto cleanup;
                }
                depth -= 1;
            }
        }
        if (block->terminator.kind == MMAY_MIR_RETURN && depth != 0) {
            status = MMAY_MIR_VERIFY_INVALID_CONTROL_FLOW;
            goto cleanup;
        }
        if (block->terminator.kind == MMAY_MIR_GOTO) {
            status = mmay_mir_defer_propagate(
                function, block->terminator.target, scratch, depth,
                incoming, depths, queued
            );
        } else if (block->terminator.kind == MMAY_MIR_BRANCH) {
            status = mmay_mir_defer_propagate(
                function, block->terminator.target, scratch, depth,
                incoming, depths, queued
            );
            if (status == MMAY_MIR_VERIFY_OK) {
                status = mmay_mir_defer_propagate(
                    function, block->terminator.otherwise, scratch, depth,
                    incoming, depths, queued
                );
            }
        }
        if (status != MMAY_MIR_VERIFY_OK) {
            goto cleanup;
        }
    }

cleanup:
    mmay_deallocate(&function->allocator, queued, block_count);
    mmay_deallocate(&function->allocator, depths, depth_bytes);
    mmay_deallocate(&function->allocator, scratch, scratch_bytes);
    mmay_deallocate(&function->allocator, incoming, stack_bytes);
    return status;
}
