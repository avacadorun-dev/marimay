#include "scalar_flow_internal.h"

#include <string.h>

/* Stop on a cycle without changing its entry: a divergent empty cycle must
 * not become a return or disappear. Work is charged even for cyclic input. */
static bool scalar_flow_target(MarimayMirScalarFlow *flow, MarimayMirBlockId *edge) {
    MarimayMirBlockId target = *edge;
    for (size_t count = 0; count < flow->function->blocks.length; count += 1) {
        if (!mmay_mir_scalar_flow_work(flow, 1)) return false;
        const MarimayMirBlock *block = mmay_vector_at_const(&flow->function->blocks, target);
        if (block->instructions.length != 0 || block->terminator.kind != MMAY_MIR_GOTO) {
            if (*edge != target) { *edge = target; flow->changed = true; }
            return true;
        }
        target = block->terminator.target;
    }
    return true;
}

MarimayMirScalarFlowStatus mmay_mir_scalar_flow_control(MarimayMirScalarFlow *flow) {
    MarimayMirFunction *function = flow->function;
    size_t count = function->blocks.length;
    size_t map_bytes = 0;
    if (!mmay_size_multiply(count, sizeof(MarimayMirBlockId), &map_bytes))
        return MMAY_MIR_SCALAR_FLOW_LIMIT_REACHED;
    MarimayMirBlockId *map = mmay_allocate(&function->allocator, map_bytes);
    MarimayMirBlockId *queue = mmay_allocate(&function->allocator, map_bytes);
    MarimayMirScalarFlowStatus status = MMAY_MIR_SCALAR_FLOW_ALLOCATION_FAILED;
    if (map == NULL || queue == NULL) goto cleanup;
    status = MMAY_MIR_SCALAR_FLOW_LIMIT_REACHED;
    for (size_t index = 0; index < count; index += 1) {
        MarimayMirBlock *block = mmay_vector_at(&function->blocks, index);
        MarimayMirTerminator *term = &block->terminator;
        map[index] = MMAY_MIR_NO_BLOCK;
        if (term->kind == MMAY_MIR_BRANCH && term->target == term->otherwise) {
            term->kind = MMAY_MIR_GOTO;
            flow->changed = true;
        }
        if ((term->kind == MMAY_MIR_GOTO || term->kind == MMAY_MIR_BRANCH)
            && !scalar_flow_target(flow, &term->target)) goto cleanup;
        if (term->kind == MMAY_MIR_BRANCH
            && !scalar_flow_target(flow, &term->otherwise)) goto cleanup;
    }
    size_t head = 0, tail = 1;
    queue[0] = 0;
    map[0] = 0;
    while (head < tail) {
        if (!mmay_mir_scalar_flow_work(flow, 1)) goto cleanup;
        const MarimayMirBlock *block = mmay_vector_at_const(&function->blocks, queue[head++]);
        MarimayMirBlockId edges[2] = {block->terminator.target, block->terminator.otherwise};
        size_t edge_count = block->terminator.kind == MMAY_MIR_BRANCH ? 2
            : block->terminator.kind == MMAY_MIR_GOTO ? 1 : 0;
        for (size_t index = 0; index < edge_count; index += 1) {
            if (map[edges[index]] == MMAY_MIR_NO_BLOCK) {
                map[edges[index]] = 0;
                queue[tail++] = edges[index];
            }
        }
    }
    if (tail != count) {
        size_t kept = 0;
        for (size_t index = 0; index < count; index += 1) {
            if (map[index] != MMAY_MIR_NO_BLOCK) map[index] = (MarimayMirBlockId)kept++;
        }
        MarimayMirBlock *blocks = (MarimayMirBlock *)function->blocks.data;
        size_t instructions = 0;
        for (size_t index = 0; index < count; index += 1) {
            if (!mmay_mir_scalar_flow_work(flow, 1)) goto cleanup;
            if (map[index] == MMAY_MIR_NO_BLOCK) {
                mmay_vector_dispose(&blocks[index].instructions);
                memset(&blocks[index], 0, sizeof(blocks[index]));
                continue;
            }
            MarimayMirTerminator *term = &blocks[index].terminator;
            if (term->kind == MMAY_MIR_GOTO || term->kind == MMAY_MIR_BRANCH) term->target = map[term->target];
            if (term->kind == MMAY_MIR_BRANCH) term->otherwise = map[term->otherwise];
            instructions += blocks[index].instructions.length;
            if (map[index] != index) {
                blocks[map[index]] = blocks[index];
                memset(&blocks[index], 0, sizeof(blocks[index]));
            }
        }
        function->blocks.length = kept;
        function->instruction_count = instructions;
        flow->changed = true;
    }
    status = MMAY_MIR_SCALAR_FLOW_OK;
cleanup:
    mmay_deallocate(&function->allocator, queue, map_bytes);
    mmay_deallocate(&function->allocator, map, map_bytes);
    return status;
}
